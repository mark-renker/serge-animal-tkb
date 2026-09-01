// Serge Animal TKB v2 — FOUNDATION PERF SPIKE (satk_perfspike, no JUCE).
// GAUNTLET T16 requires >= 6x realtime single-thread at 48 kHz with the full
// default patch ticking. Before the module builders choose accuracy tiers,
// this spike loads every stub module with REPRESENTATIVE hero-tier costs
// (REDESIGN_PLAN §2): a tanh-class nonlinearity per active jack path and 2x
// oversampling islands on the VCFQ / VCM (x3 sections) / RING / NTO-shaper
// paths, ADAA1/ADAA2 kernels where the plan calls for them, PCG32 noise
// streams, and control-rate OU drift. It then runs the frozen T16 protocol
// (>= 10 s render, min over >= 5 runs, cycles/sample at the assumed P-core
// clock) on the full graph, plus per-module standalone measurements.
//
// The cost subclasses below override ONLY tick()/prepare()/reseed() of the
// frozen stubs — registration (jacks/ctls) is inherited untouched. They are
// throwaway: stage-2 builders replace the stubs' tick() with the real DSP.
// Results are recorded in docs/SPEC.md §10 ("Perf spike").
#include "../src/engine/System.h"
#include <chrono>
#include <cstdio>

namespace spike {

using namespace satk;

constexpr double SR = 48000.0;
constexpr double kAssumedPCoreHz = 3.504e9; // M2 Max nominal P-core clock

struct Perf { double realtime = 0, cyc = 0; };

inline Perf measure (Engine& e, double seconds, int runs)
{
    const long n = (long) (SR * seconds);
    double best = 1e30;
    for (int r = 0; r < runs; ++r)
    {
        const auto t0 = std::chrono::steady_clock::now();
        for (long i = 0; i < n; ++i) e.tick();
        const double dt = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        best = std::min (best, dt);
    }
    return { seconds / best, best * kAssumedPCoreHz / (double) n };
}

// ---------------------------------------------------------------------------
// representative-cost module subclasses (hero tier per REDESIGN_PLAN §2)
// ---------------------------------------------------------------------------

// NTO: phase core + polyBLEP saw + polyBLAMP tri + 2x-OS ADAA1(tanh) shaper
// island on VAR + control-rate OU drift.
struct CostNTO final : NTO
{
    dsp::Oversampler2x os;
    dsp::ADAA1<dsp::TanhKernel> shaper;
    dsp::PCG32 rng;
    dsp::OUDrift d1, d2;
    float phase = 0.0f, driftOct = 0.0f;
    int ctr = 0;

    void reseed() override { rng.seed (moduleSeed, 1); }
    void prepare (double s) override
    {
        NTO::prepare (s);
        os.prepare(); shaper.reset();
        d1.configure (0.5f, 0.003f, sr / 48.0f);
        d2.configure (10.0f, 0.002f, sr / 48.0f);
        phase = 0.0f; ctr = 0;
    }
    void tick() override
    {
        if (++ctr >= 48) { ctr = 0; driftOct = d1.tick (rng) + d2.tick (rng); }
        const float oct = in (jOctA) + in (jOctB)
                        + dsp::av (ctls[cVcFreqAmt].v) * 0.4f * in (jVcFreq)
                        + (ctls[cFine].v - 0.5f) * (200.0f / 1200.0f) + driftOct;
        float f = dsp::expmap (ctls[cFreq].v, 16.0f, 16000.0f) * std::exp2 (oct);
        f += ctls[cLinDepth].v * (in (jLinFm) / kCV) * 3.0f * f; // TZFM (signed)
        const float dt = f / sr;
        phase += dt;
        if (phase >= 1.0f) phase -= 1.0f;
        if (phase <  0.0f) phase += 1.0f;
        const float adt = std::min (0.5f, std::fabs (dt));
        const float saw = 2.0f * phase - 1.0f - dsp::polyBLEP (phase, adt);
        float tri = 4.0f * std::fabs (phase - 0.5f) - 1.0f;
        tri += dsp::polyBLAMP (phase, adt) * adt * 4.0f;
        const float sine = std::sin (6.2831853f * phase);
        const float shape = std::clamp (ctls[cWave].v + in (jVcWave) / kCV, 0.0f, 1.0f);
        const float var = os.process (tri * (1.0f + 3.0f * shape),
                                      [this] (float v) { return shaper.process (v); });
        float pulse = phase < 0.5f ? kCV : 0.0f;
        pulse -= kCV * 0.5f * dsp::polyBLEP (phase, adt);
        jacks[jSine].value  = sine * kAudio;
        jacks[jTri].value   = tri  * kAudio;
        jacks[jSaw].value   = saw  * kAudio;
        jacks[jVar].value   = clampRails (var * kAudio);
        jacks[jPulse].value = pulse;
        leds[0] = phase;
    }
};

// DUSG: linear-drive slew with tanh landing; free-cycles so it is always hot.
struct CostDUSG final : DUSG
{
    using DUSG::DUSG;
    EdgeDetector ed;
    float y = 0.0f;
    bool rising = true;

    void tick() override
    {
        const float rise = dsp::expmap (ctls[cRise].v, 50e-6f, 10.0f) * std::exp2 (-in (jVcRise));
        const float fall = dsp::expmap (ctls[cFall].v, 50e-6f, 10.0f) * std::exp2 (-in (jVcFall));
        if (ed.rising (in (jTrig)) && y < 0.01f) rising = true;
        const float target = rising ? kCV : std::max (0.0f, in (jIn));
        const float tc = rising ? rise : fall;
        const float step = kCV / (std::max (tc, 2.0e-5f) * sr);
        y += step * dsp::fastTanh (1.5f * (target - y));
        if (y >= kCV - 0.02f) rising = false;
        if (! rising && y <= 0.02f) rising = true; // free cycle (worst case)
        y = std::clamp (y, 0.0f, kCV);
        jacks[jOut].value = y;
        jacks[jEnd].value = (rising || y <= 0.02f) ? kCV : 0.0f;
        leds[0] = y / kCV;
    }
};

// SSG: two slew cores + comparator + hot output.
struct CostSSG final : SSG
{
    EdgeDetector sampEd;
    float sm = 0.0f, st = 0.0f, phase = 0.0f;

    void tick() override
    {
        const float smRate = dsp::expmap (ctls[cSmRate].v, 50e-6f, 10.0f) * std::exp2 (-in (jSmVcRate));
        const float stRate = dsp::expmap (ctls[cStRate].v, 50e-6f, 10.0f) * std::exp2 (-in (jStVcRate));
        phase += 1.0f / std::max (1.0f, smRate * sr);
        if (phase >= 1.0f) phase -= 1.0f;
        const float target = in (jSmIn) + (phase < 0.5f ? kCV : 0.0f); // internal drive keeps it hot
        if (! isHigh (in (jHold)))
            sm += (kCV / (std::max (smRate, 2.0e-5f) * sr)) * dsp::fastTanh (1.2f * (target - sm));
        sm = clampRails (sm);
        if (sampEd.rising (in (jSample)))
        {
            const float lim = kCV / (std::max (stRate, 2.0e-5f) * sr) * 8.0f;
            st += std::clamp (in (jStIn) - st, -lim, lim);
        }
        jacks[jSmOut].value      = std::clamp (sm, -kCV, kCV);
        jacks[jCycle].value      = phase < 0.1f ? kCV : 0.0f;
        jacks[jStOut].value      = st;
        jacks[jCoupler].value    = sm > st ? kCV : 0.0f;
        jacks[jHotCoupler].value = dsp::fastTanh ((sm - st) * 2.0f) * 11.0f;
        leds[0] = std::fabs (sm) / kCV;
        leds[1] = sm > st ? 1.0f : 0.0f;
    }
};

// VCFQ: 2x-OS TPT SVF with per-integrator tanh (hero tier).
struct CostVCFQ final : VCFQ
{
    dsp::Oversampler2x os;
    float s1 = 0.0f, s2 = 0.0f, bpv = 0.0f, hpv = 0.0f;

    void prepare (double s) override { VCFQ::prepare (s); os.prepare(); s1 = s2 = 0.0f; }
    void tick() override
    {
        const float fc = std::min (dsp::expmap (ctls[cFreq].v, 20.0f, 12000.0f)
                       * std::exp2 (in (jOct) + dsp::av (ctls[cVcFAmt].v) * in (jVcF)), 0.45f * sr);
        const float q = std::max (0.5f, dsp::expmap (ctls[cQ].v, 0.5f, 40.0f)
                       * std::exp2 (dsp::av (ctls[cVcQAmt].v) * 0.6f * in (jVcQ)));
        const float g = std::tan (3.14159265f * fc / (2.0f * sr)); // coefficient at the 2x rate
        const float k = 1.0f / q;
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float x = in (jIn) * ctls[cGain].v * 2.0f + in (jAgc) * k
                      + in (jTrig) * ctls[cPing].v * 0.2f;
        const float lp = os.process (x, [&] (float v)
        {
            const float hp = (v - (g + k) * s1 - s2) * a1;
            const float v1 = g * hp;
            const float bp = dsp::fastTanh (v1 + s1); s1 = bp + v1;
            const float v2 = g * bp;
            const float lo = dsp::fastTanh (v2 + s2); s2 = lo + v2;
            bpv = bp; hpv = hp;
            return lo;
        });
        jacks[jLp].value    = clampRails (lp);
        jacks[jBp].value    = clampRails (bpv);
        jacks[jHp].value    = clampRails (hpv);
        jacks[jNotch].value = clampRails (lp + hpv);
        leds[0] = std::fabs (lp) / kAudio;
    }
};

// VCM: three sections, each its own 2x-OS island; ADAA1 top, ADAA2 mid
// (fold + rail clip in the kernel), rectifier cascade bottom.
struct CostVCM final : VCM
{
    dsp::Oversampler2x osT, osM, osB;
    dsp::ADAA1<dsp::TanhKernel> topSh;
    dsp::ADAA2<dsp::HardClipKernel> midClip;

    void prepare (double s) override
    {
        VCM::prepare (s);
        osT.prepare(); osM.prepare(); osB.prepare();
        topSh.reset(); midClip.reset(); midClip.kernel.c = 1.0;
    }
    void tick() override
    {
        const float drive = dsp::expmap (ctls[cTopDrive].v, 1.0f, 10.0f) * (1.0f + in (jVcTop) / kCV);
        const float top = osT.process (in (jTopIn) / kAudio * drive,
                                       [&] (float v) { return topSh.process (v); });
        const float foldG = 0.5f * std::exp2 (4.0f * ctls[cFold].v + 0.4f * in (jVcFold));
        const float bias = (ctls[cBias].v - 0.5f) * 2.0f;
        const float mIn = (in (jMidIn1) + in (jMidIn2)) / kAudio * foldG + bias;
        const float mid = osM.process (mIn, [&] (float v)
        {
            return midClip.process (std::sin (1.8f * v) + 0.35f * v);
        });
        const float depth = std::clamp (ctls[cBotDepth].v + in (jVcBot) / kCV, 0.0f, 1.0f);
        const float bIn = (in (jBotIn1) + in (jBotIn2)) / kAudio;
        const float bot = osB.process (bIn, [&] (float v)
        {
            float r = std::fabs (v) - 0.5f;
            r = std::fabs (r) - 0.25f;
            r = std::fabs (r) - 0.125f;
            return dsp::fastTanh (r * (1.0f + 4.0f * depth));
        });
        jacks[jTopOut].value = clampRails (top * kAudio);
        jacks[jMidOut].value = clampRails (mid * kAudio);
        jacks[jBotOut].value = clampRails (bot * kAudio);
        jacks[jBotSq].value  = dsp::fastTanh (8.0f * bot) * kAudio;
        leds[0] = std::min (1.0f, std::fabs (mid));
    }
};

// RING: tanh*tanh four-quadrant model in a 2x-OS island with bleed terms.
struct CostRING final : RING
{
    dsp::Oversampler2x os;

    void prepare (double s) override { RING::prepare (s); os.prepare(); }
    void tick() override
    {
        const float gain = dsp::expmap (ctls[cGain].v, 0.5f, 4.0f);
        const float morph = std::clamp (ctls[cMorph].v + in (jVcMorph) / kCV, 0.0f, 1.0f);
        const float yv = dsp::fastTanh (in (jY) / kAudio * 1.5f);
        const float bleedX = ctls[cTrimX].v * 0.003f, bleedY = ctls[cTrimY].v * 0.003f;
        const float o = os.process (in (jX) / kAudio, [&] (float v)
        {
            const float xs = dsp::fastTanh (1.5f * v);
            return xs * (morph * yv + (1.0f - morph) * 0.7f) + bleedX * v + bleedY * yv;
        });
        jacks[jOut].value = clampRails (o * gain * kAudio);
        leds[0] = std::min (1.0f, std::fabs (o));
    }
};

// NOISE: PCG32 white + 3-pole pink + randomly modulated S/H-source saw.
struct CostNOISE final : NOISE
{
    dsp::PCG32 rngW, rngS;
    float p0 = 0, p1 = 0, p2 = 0, sawPhase = 0, sawRate = 400.0f;

    void reseed() override { rngW.seed (moduleSeed, 10); rngS.seed (moduleSeed, 11); }
    void tick() override
    {
        const float w = rngW.nextBipolar();
        p0 = 0.99765f * p0 + w * 0.0990460f;
        p1 = 0.96300f * p1 + w * 0.2965164f;
        p2 = 0.57000f * p2 + w * 1.0526913f;
        const float pink = (p0 + p1 + p2 + w * 0.1848f) * 0.25f;
        sawPhase += sawRate / sr;
        if (sawPhase >= 1.0f) { sawPhase -= 1.0f; sawRate = 250.0f + 300.0f * rngS.nextFloat01(); }
        jacks[jWhite].value = w * kAudio;
        jacks[jPink].value  = pink * kAudio;
        jacks[jShSrc].value = sawPhase * kCV;
    }
};

// RVG: random clock + stepped + slewed smooth.
struct CostRVG final : RVG
{
    dsp::PCG32 rng;
    EdgeDetector ed;
    float phase = 0, stepped = 0, smooth = 0;

    void reseed() override { rng.seed (moduleSeed, 12); }
    void tick() override
    {
        const float rate = dsp::expmap (ctls[cRate].v, 0.05f, 50.0f) * std::exp2 (in (jVcRate));
        phase += rate / sr;
        bool fire = ed.rising (in (jTrig));
        if (phase >= 1.0f) { phase -= 1.0f; fire = true; }
        if (fire) stepped = rng.nextFloat01() * kCV;
        smooth += (stepped - smooth) * std::min (1.0f, 8.0f * rate / sr);
        jacks[jStepped].value = stepped;
        jacks[jSmooth].value  = smooth;
        jacks[jPulse].value   = phase < 0.1f ? kCV : 0.0f;
        leds[0] = stepped / kCV;
    }
};

// TKB: internal clock, stage/row logic, 21 jack writes + glide slew.
struct CostTKB final : TKB
{
    dsp::PCG32 rng;
    EdgeDetector clkEd, vertEd, rstEd;
    float phase = 0, kv = 0;
    int stage = 0, row = 0, gateN = 0;

    void reseed() override { rng.seed (moduleSeed, 13); }
    void tick() override
    {
        const float rate = dsp::expmap (ctls[cRate].v, 0.1f, 30.0f);
        phase += rate / sr;
        bool step = clkEd.rising (in (jClock));
        if (phase >= 1.0f) { phase -= 1.0f; step = true; } // internal clock always hot
        const int length = (int) ctls[cLength].v + 1;
        if (step && ! isHigh (in (jHold)))
        {
            const bool down = isHigh (in (jUpDown));
            const bool rnd  = ctls[cRandom].v > 0.5f || isHigh (in (jRandom));
            if (rnd) stage = (int) (rng.next() % (uint32_t) std::max (1, length));
            else     stage = down ? (stage + length - 1) % length : (stage + 1) % length;
            gateN = (int) (dsp::expmap (ctls[cGateLen].v, 0.005f, 1.0f) * sr);
        }
        if (vertEd.rising (in (jVertClock))) row = (row + 1) & 3;
        if (rstEd.rising (in (jReset))) stage = 0;
        const float range = dsp::linmap (ctls[cRange].v, 0.5f, 5.0f);
        for (int r = 0; r < 4; ++r)
            jacks[(size_t) (jA + r)].value = ctls[(size_t) (cRowA1 + r * 16 + stage)].v * range;
        jacks[jAbcd].value = ctls[(size_t) (cRowA1 + row * 16 + stage)].v * range;
        const float kvT = (float) stage / 12.0f;
        const float gl = ctls[cGlide].v;
        kv += (kvT - kv) * (gl > 0.001f ? std::min (1.0f, 1.0f / (gl * sr)) : 1.0f);
        jacks[jKv].value       = kv;
        jacks[jPressure].value = ctls[cPressure].v * kCV;
        jacks[jGate].value     = gateN > 0 ? kCV : 0.0f;
        if (gateN > 0) --gateN;
        jacks[jKp].value = 0.0f;
        for (int s = 0; s < 16; ++s)
        {
            jacks[(size_t) (jStage1 + s)].value = s == stage ? kCV : 0.0f;
            leds[(size_t) s] = s == stage ? 1.0f : 0.0f;
        }
        for (int r = 0; r < 4; ++r) leds[(size_t) (16 + r)] = r == row ? 1.0f : 0.0f;
    }
};

// KEY: pass-through + glide one-pole.
struct CostKEY final : KEY
{
    float kv = 0.0f;
    void tick() override
    {
        KEY::tick();
        kv += (pitchV - kv) * 0.01f;
        jacks[jKv].value = kv;
    }
};

// VCA: exp law + soft saturation.
struct CostVCA final : VCA
{
    void tick() override
    {
        const float vc = std::clamp (in (jVc) + ctls[cLevel].v * kCV, 0.0f, kCV);
        float g;
        if (ctls[cResponse].v > 0.5f)
            g = vc < 0.5f ? 0.0f : std::min (2.0f, std::exp ((vc - kCV) * 1.3816f) * 2.0f);
        else
            g = std::min (2.0f, vc / kCV * 2.0f);
        jacks[jOut].value = dsp::fastTanh (in (jIn) * g / kRails) * kRails;
        leds[0] = g * 0.5f;
    }
};

// ---------------------------------------------------------------------------
// full representative system: frozen registration order, default patch plus
// extra cables so that every module has live feeds ("all modules ticking").
// ---------------------------------------------------------------------------
struct CostSystem
{
    Engine e;
    CostNTO* nto; CostDUSG* dusg1; CostDUSG* dusg2; CostSSG* ssg;
    CostVCFQ* vcfq; CostVCM* vcm; CostRING* ring; CostNOISE* noise;
    CostRVG* rvg; CostTKB* tkb; CostKEY* key; CostVCA* vca1; CostVCA* vca2;
    OUT* out;

    CostSystem()
    {
        nto   = e.add<CostNTO>   ("nto",   "NTO");
        dusg1 = e.add<CostDUSG>  ("dusg1", "DUSG1", 0.2454f, 0.6978f);
        dusg2 = e.add<CostDUSG>  ("dusg2", "DUSG2", 0.5659f, 0.5659f);
        ssg   = e.add<CostSSG>   ("ssg",   "SSG");
        vcfq  = e.add<CostVCFQ>  ("vcfq",  "VCFQ");
        vcm   = e.add<CostVCM>   ("vcm",   "VCM");
        ring  = e.add<CostRING>  ("ring",  "RING");
        noise = e.add<CostNOISE> ("noise", "NOISE");
        rvg   = e.add<CostRVG>   ("rvg",   "RVG");
        tkb   = e.add<CostTKB>   ("tkb",   "TKB");
        key   = e.add<CostKEY>   ("key",   "KEY");
        vca1  = e.add<CostVCA>   ("vca1",  "VCA1");
        vca2  = e.add<CostVCA>   ("vca2",  "VCA2");
        out   = e.add<OUT>       ("out",   "OUT");

        out->monSrc.assign ((size_t) OUT::kNumMonitorSources, nullptr);
        out->monSrc[15] = &vcm->jacks[VCM::jMidOut].value;
        out->velSrc = &key->jacks[KEY::jVel].value;

        auto con = [this] (Module* a, int ja, Module* b, int jb)
        { e.connect (e.indexOf (a), ja, e.indexOf (b), jb); };

        // the frozen default patch (SPEC §6)...
        con (key,   KEY::jGate,   dusg1, DUSG::jTrig);
        con (nto,   NTO::jVar,    vcm,   VCM::jMidIn1);
        con (vcm,   VCM::jMidOut, vcfq,  VCFQ::jIn);
        con (vcfq,  VCFQ::jLp,    vca1,  VCA::jIn);
        con (dusg1, DUSG::jOut,   vca1,  VCA::jVc);
        con (vca1,  VCA::jOut,    out,   OUT::jIn1);
        con (vca2,  VCA::jOut,    out,   OUT::jIn2);
        // ...plus feeds so every module works on live signals (T16 wording:
        // "full default patch, all modules ticking")
        con (noise, NOISE::jWhite,  ring, RING::jX);
        con (nto,   NTO::jSine,     ring, RING::jY);
        con (ring,  RING::jOut,     vca2, VCA::jIn);
        con (dusg2, DUSG::jOut,     vca2, VCA::jVc);
        con (ssg,   SSG::jSmOut,    nto,  NTO::jVcFreq);
        con (rvg,   RVG::jStepped,  vcfq, VCFQ::jVcF);
        con (tkb,   TKB::jA,        nto,  NTO::jOctB);
        con (tkb,   TKB::jGate,     dusg2, DUSG::jTrig);
        con (noise, NOISE::jShSrc,  ssg,  SSG::jStIn);
        con (dusg1, DUSG::jEnd,     ssg,  SSG::jSample);
        con (rvg,   RVG::jPulse,    tkb,  TKB::jClock);
        con (tkb,   TKB::jKv,       nto,  NTO::jOctA);
        con (nto,   NTO::jSaw,      vcm,  VCM::jTopIn);
        con (noise, NOISE::jPink,   vcm,  VCM::jBotIn1);
        con (ssg,   SSG::jCoupler,  rvg,  RVG::jTrig);

        e.prepare (SR);
        out->ctls[OUT::cMonSrc].v = 15.0f; // monitor path exercised
        key->gateV = kCV; key->pitchV = 0.25f; key->velV = 4.0f;
    }
};

template <typename T, typename... A>
Perf singleModule (const char* id, A&&... args)
{
    Engine e;
    e.add<T> (id, id, std::forward<A> (args)...);
    e.prepare (SR);
    return measure (e, 10.0, 3);
}

} // namespace spike

int main()
{
    using namespace spike;
    const double budget6x = kAssumedPCoreHz / SR / 6.0; // cycles/sample at the 6x gate

    std::printf ("Serge Animal TKB v2 — foundation perf spike (representative hero-tier costs)\n");
    std::printf ("protocol: T16 (>= 10 s render, min over runs); assumed P-core %.3f GHz;\n", kAssumedPCoreHz / 1e9);
    std::printf ("6x-realtime budget = %.0f cycles/sample\n\n", budget6x);

    // stub-graph baseline (framework overhead: virtual dispatch, feeds, mixer)
    satk::System stub;
    stub.patchDefaults();
    stub.key->gateV = satk::kCV;
    const Perf base = measure (stub.engine, 10.0, 3);
    std::printf ("%-22s %8.1f cyc/smp   %7.0fx realtime  (graph overhead)\n\n",
                 "stub System", base.cyc, base.realtime);

    // per-module standalone hero-tier costs
    struct Row { const char* name; Perf p; int count; };
    Row rows[] = {
        { "NTO",       singleModule<CostNTO>   ("nto"),                    1 },
        { "DUSG (x2)", singleModule<CostDUSG>  ("dusg1", 0.2454f, 0.6978f), 2 },
        { "SSG",       singleModule<CostSSG>   ("ssg"),                    1 },
        { "VCFQ",      singleModule<CostVCFQ>  ("vcfq"),                   1 },
        { "VCM",       singleModule<CostVCM>   ("vcm"),                    1 },
        { "RING",      singleModule<CostRING>  ("ring"),                   1 },
        { "NOISE",     singleModule<CostNOISE> ("noise"),                  1 },
        { "RVG",       singleModule<CostRVG>   ("rvg"),                    1 },
        { "TKB",       singleModule<CostTKB>   ("tkb"),                    1 },
        { "KEY",       singleModule<CostKEY>   ("key"),                    1 },
        { "VCA (x2)",  singleModule<CostVCA>   ("vca1"),                   2 },
    };
    std::printf ("per-module standalone (module + its engine wrapper):\n");
    double predicted = 0.0;
    for (auto& r : rows)
    {
        std::printf ("  %-12s %8.1f cyc/smp  x%d   (%.2f%% of 6x budget each)\n",
                     r.name, r.p.cyc, r.count, 100.0 * r.p.cyc / budget6x);
        predicted += r.p.cyc * r.count;
    }
    std::printf ("  sum of parts (x counts): %.0f cyc/smp (+ graph overhead %.0f)\n\n",
                 predicted, base.cyc);

    // full representative system, frozen T16 protocol: >= 10 s, min of >= 5 runs
    CostSystem sys;
    const Perf full = measure (sys.e, 10.0, 5);
    std::printf ("FULL representative system (T16 protocol, min of 5 x 10 s):\n");
    std::printf ("  %.1f cycles/sample, %.1fx realtime single-thread @ 48 kHz\n",
                 full.cyc, full.realtime);
    std::printf ("  6x-realtime headroom factor: %.1fx over the gate\n", full.realtime / 6.0);
    std::printf ("  (silence guard: outL=%g)\n", (double) sys.out->outL);

    if (full.realtime < 6.0)
    {
        std::printf ("\n*** WARNING: below the 6x realtime T16 gate — per-module tier downgrades required ***\n");
        return 1;
    }
    if (full.realtime < 12.0)
        std::printf ("\nNOTE: under 2x headroom over the gate — pick tiers carefully.\n");
    else
        std::printf ("\nHero tier fits comfortably for every module.\n");
    return 0;
}
