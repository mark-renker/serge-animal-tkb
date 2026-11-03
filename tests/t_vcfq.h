// Serge Animal TKB v2 — VCFQ gauntlet tests, gate T07 (GAUNTLET.md rev B):
//   LP slope 12 +/- 2.5 dB/oct (1 -> 3 octaves above fc); simultaneous
//   LP/BP/HP/Notch outputs correct; resonance compensation on the dedicated
//   AGC input ONLY (standard GAIN input deliberately uncompensated),
//   monotonic in Q and never unbounded (fitted law logged in LOG.md:
//   agc(Q) = (1 + Q0)/(Q0 + Q), Q0 = 2); ping: pulse to trigger -> damped
//   sine at fc, decay time monotonically increasing with Q; self-oscillation
//   via BP -> IN patch stable and amplitude-limited, never NaN/Inf.
// Render evidence (SATK_RENDER=1): gauntlet/renders/v2work/vcfq/vcfq_ping.wav
// mirroring gauntlet/renders/v1/MANIFEST.md file 5.
#pragma once
#include "Harness.h"
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace t_vcfq {

using namespace gauntlet;
using namespace satk;

constexpr double kFc  = 800.0;                 // default FREQ knob (SPEC §4.4)
constexpr double kPi  = 3.14159265358979;

// inverse knob maps (expo 0.5..40 and 20 Hz..12 kHz, SPEC §4.4)
inline float qKnob (double q) { return (float) (std::log (q / 0.5) / std::log (80.0)); }

// Feed a test-owned float straight into an In jack. Audio-rate probes must NOT
// go through KEY KV: its v1 glide floor (tau 0.1 ms) is a ~1.6 kHz one-pole
// that would colour the measured response (it skewed the LP slope by ~4.6 dB/oct).
inline void injectSrc (Module* m, int jack, const float* src)
{
    m->feeds[(size_t) jack].src.push_back (src);
}

// Drive vcfq.jIn (or another In jack) from KEY KV with a sine, settle, then
// Goertzel one output jack. Continuous phase across settle+measure.
inline double sineResponse (int inJack, int outJack, double f, double amp,
                            double q, int settle = 9600, int win = 24000)
{
    Rig r; auto* v = r.sys.vcfq;
    float src = 0.0f;
    injectSrc (v, inJack, &src);
    v->ctls[VCFQ::cQ].v = qKnob (q);
    long n = 0;
    auto pre = [&] (int) { src = (float) (amp * std::sin (2.0 * kPi * f * (double) n++ / SR)); };
    r.step (settle, pre);
    Goertzel gz; gz.init (f);
    for (int i = 0; i < win; ++i)
    { pre (0); r.sys.engine.tick(); gz.push (v->jacks[(size_t) outJack].value); }
    return gz.amp();
}

// Fire one ping (KEY gate -> TRIG) at the given Q knob and capture BP.
inline void capturePing (double q, double octV, float pingLvl,
                         std::vector<float>& bp, int n)
{
    Rig r; auto* v = r.sys.vcfq;
    r.sys.patch (r.sys.key, KEY::jGate, v, VCFQ::jTrig);
    r.sys.patch (r.sys.key, KEY::jKv,   v, VCFQ::jOct);
    v->ctls[VCFQ::cQ].v    = qKnob (q);
    v->ctls[VCFQ::cPing].v = pingLvl;
    bp.assign ((size_t) n, 0.0f);
    for (int i = 0; i < n; ++i)
    {
        r.sys.key->gateV  = i < 480 ? kCV : 0.0f; // 10 ms trigger pulse
        r.sys.key->pitchV = (float) octV;         // 1 V/oct input
        r.sys.engine.tick();
        bp[(size_t) i] = v->jacks[VCFQ::jBp].value;
    }
}

inline double goertzelBuf (const std::vector<float>& x, double f, int from, int len)
{
    Goertzel g; g.init (f);
    for (int i = from; i < from + len && i < (int) x.size(); ++i) g.push (x[(size_t) i]);
    return g.amp();
}

// peak -> -40 dB ring-down time (seconds); nan if never rings
inline double decayTime (const std::vector<float>& x, float& peakOut)
{
    float peak = 0.0f; int peakIdx = 0;
    for (int i = 0; i < (int) x.size(); ++i)
        if (std::fabs (x[(size_t) i]) > peak) { peak = std::fabs (x[(size_t) i]); peakIdx = i; }
    peakOut = peak;
    if (peak <= 0.0f) return -1.0;
    const float thr = peak * 0.01f; // -40 dB
    int last = peakIdx;
    for (int i = peakIdx; i < (int) x.size(); ++i)
        if (std::fabs (x[(size_t) i]) > thr) last = i;
    return (last - peakIdx) / SR;
}

// ---------------------------------------------------------------------------
inline void testSlopeAndSimultaneous()
{
    // simultaneous outputs, one run, drive at fc (Q = 2 default): LP=BP=HP
    // magnitude = Q*A at resonance, notch nulls
    {
        Rig r; auto* v = r.sys.vcfq;
        float src = 0.0f;
        injectSrc (v, VCFQ::jIn, &src);
        long n = 0;
        auto pre = [&] (int) { src = (float) (0.5 * std::sin (2.0 * kPi * kFc * (double) n++ / SR)); };
        r.step (9600, pre);
        Goertzel gl, gb, gh, gn;
        gl.init (kFc); gb.init (kFc); gh.init (kFc); gn.init (kFc);
        for (int i = 0; i < 24000; ++i)
        {
            pre (0); r.sys.engine.tick();
            gl.push (v->jacks[VCFQ::jLp].value);   gb.push (v->jacks[VCFQ::jBp].value);
            gh.push (v->jacks[VCFQ::jHp].value);   gn.push (v->jacks[VCFQ::jNotch].value);
        }
        const double want = 2.0 * 0.5; // Q * A
        bool res = std::fabs (db (gl.amp(), want)) < 2.0
                && std::fabs (db (gb.amp(), want)) < 2.0
                && std::fabs (db (gh.amp(), want)) < 2.0;
        CHECK (res, "simultaneous LP/BP/HP = Q*A at fc (+/- 2 dB)", gl.amp(), gh.amp());
        CHECK (db (gn.amp(), 0.5) < -25.0,
               "simultaneous Notch output nulls at fc (<= -25 dB)", db (gn.amp(), 0.5));
    }
    // LP slope 12 +/- 2.5 dB/oct measured 1 -> 3 octaves above fc
    {
        const double a1 = sineResponse (VCFQ::jIn, VCFQ::jLp, kFc * 2.0, 0.5, 2.0);
        const double a3 = sineResponse (VCFQ::jIn, VCFQ::jLp, kFc * 8.0, 0.5, 2.0);
        const double slope = db (a1, a3) / 2.0;
        CHECK (slope > 9.5 && slope < 14.5,
               "LP slope 12 +/- 2.5 dB/oct (1->3 octaves above fc)", slope);
    }
    // HP mirrors below fc (correctness of the simultaneous HP response)
    {
        const double b1 = sineResponse (VCFQ::jIn, VCFQ::jHp, kFc / 2.0, 0.5, 2.0);
        const double b3 = sineResponse (VCFQ::jIn, VCFQ::jHp, kFc / 8.0, 0.5, 2.0);
        const double slope = db (b1, b3) / 2.0;
        CHECK (slope > 9.5 && slope < 14.5,
               "HP slope 12 +/- 2.5 dB/oct (1->3 octaves below fc)", slope);
    }
}

inline void testOctCalibration()
{
    // 1 V/oct input: ring pitch of a high-Q ping doubles per volt (SPEC §4.4)
    bool ok = true; double worstRatio = 1.0e9;
    for (int oct = 0; oct <= 2; ++oct)
    {
        std::vector<float> bp;
        capturePing (35.0, (double) oct, 0.6f, bp, 4800);
        const double f0 = kFc * std::pow (2.0, oct);
        const int win = oct == 0 ? 2400 : 1200; // shorter ring at higher fc
        const double c  = goertzelBuf (bp, f0, 0, win);
        const double lo = goertzelBuf (bp, f0 * std::pow (2.0, -1.0 / 12.0), 0, win);
        const double hi = goertzelBuf (bp, f0 * std::pow (2.0,  1.0 / 12.0), 0, win);
        if (! (c > lo && c > hi)) ok = false;
        worstRatio = std::min (worstRatio, db (c, std::max (lo, hi)));
    }
    CHECK (ok, "1V/OCT calibrated: ping rings at fc*2^V (0..2 V)", worstRatio);
}

inline void testAgc()
{
    const double qs[] = { 1.0, 2.0, 6.0, 15.0, 35.0 };

    // AGC input gain (LP passband @ 100 Hz) monotonically decreasing in Q
    double agcG[5];
    bool monotonic = true;
    for (int i = 0; i < 5; ++i)
    {
        agcG[i] = sineResponse (VCFQ::jAgc, VCFQ::jLp, 100.0, 2.5, qs[i]);
        if (i > 0 && ! (agcG[i] < agcG[i - 1])) monotonic = false;
    }
    CHECK (monotonic, "AGC compensation monotonic in Q (law (1+Q0)/(Q0+Q), Q0=2)",
           agcG[0] / 2.5, agcG[4] / 2.5);

    // AGC path bounded at resonance for every Q up to CV-extended 60
    {
        double worst = 0.0;
        for (double q : qs)
            worst = std::max (worst, sineResponse (VCFQ::jAgc, VCFQ::jBp, kFc, 2.5, q));
        // Q = 60: knob 40 max, VC Q CV extends (0.6 oct/V * 5 V, VC Q AMT full)
        {
            Rig r; auto* v = r.sys.vcfq;
            r.sys.patch (r.sys.key, KEY::jKv,  v, VCFQ::jAgc);
            r.sys.patch (r.sys.key, KEY::jClk, v, VCFQ::jVcQ);
            v->ctls[VCFQ::cQ].v = 1.0f; v->ctls[VCFQ::cVcQAmt].v = 1.0f;
            long n = 0;
            auto pre = [&] (int)
            {
                r.sys.key->pitchV = (float) (2.5 * std::sin (2.0 * kPi * kFc * (double) n++ / SR));
                r.sys.key->clkV = kCV;
            };
            r.step (9600, pre);
            Goertzel gz; gz.init (kFc);
            for (int i = 0; i < 24000; ++i)
            { pre (0); r.sys.engine.tick(); gz.push (v->jacks[VCFQ::jBp].value); }
            worst = std::max (worst, gz.amp());
        }
        CHECK (worst < 10.0, "AGC input output bounded to Q=60 (never unbounded)", worst);
    }

    // Standard GAIN input deliberately uncompensated: resonant BP grows with
    // Q (>= 18 dB from Q=1 to Q=15), while its passband gain is Q-independent
    {
        double bp1  = sineResponse (VCFQ::jIn, VCFQ::jBp, kFc, 0.2, 1.0);
        double bp6  = sineResponse (VCFQ::jIn, VCFQ::jBp, kFc, 0.2, 6.0);
        double bp15 = sineResponse (VCFQ::jIn, VCFQ::jBp, kFc, 0.2, 15.0);
        bool grows = bp1 < bp6 && bp6 < bp15 && db (bp15, bp1) >= 18.0;
        CHECK (grows, "GAIN input deliberately UNcompensated (grows with Q)",
               db (bp15, bp1));
        double lpA = sineResponse (VCFQ::jIn, VCFQ::jLp, 100.0, 2.5, 1.0);
        double lpB = sineResponse (VCFQ::jIn, VCFQ::jLp, 100.0, 2.5, 35.0);
        CHECK (std::fabs (db (lpA, lpB)) < 0.5,
               "GAIN-path passband gain independent of Q (+/- 0.5 dB)", db (lpA, lpB));
    }
}

inline void testPing()
{
    // ping -> damped sine at fc, decay monotonically increasing with Q
    // (v1 reference decays: 3.8 / 11.1 / 27.5 / 63.3 ms at Q = 2/6/15/35)
    const double qs[] = { 2.0, 6.0, 15.0, 35.0 };
    double decays[4]; float peaks[4];
    bool finiteOk = true;
    for (int i = 0; i < 4; ++i)
    {
        std::vector<float> bp;
        capturePing (qs[i], 0.0, 0.9f, bp, 24000);
        for (float s : bp) if (! std::isfinite (s)) finiteOk = false;
        decays[i] = decayTime (bp, peaks[i]);
        if (i == 2) // damped sine AT fc: fundamental beats neighbouring octaves
        {
            const double c  = goertzelBuf (bp, kFc, 0, 2400);
            const double lo = goertzelBuf (bp, kFc * 0.5, 0, 2400);
            const double hi = goertzelBuf (bp, kFc * 2.0, 0, 2400);
            CHECK (c > 4.0 * lo && c > 4.0 * hi,
                   "ping: pulse -> damped sine at fc (BP rings at 800 Hz)", c);
        }
    }
    bool monotonic = decays[0] < decays[1] && decays[1] < decays[2] && decays[2] < decays[3];
    CHECK (monotonic && finiteOk,
           "ping decay time monotonically increasing with Q",
           decays[0] * 1000.0, decays[3] * 1000.0);
    bool bounded = true;
    for (float p : peaks) if (! (p > 0.05f && p <= 11.0f)) bounded = false;
    CHECK (bounded, "ping amplitude audible and bounded (<= 11 V)", peaks[0], peaks[3]);
}

inline void testSelfOsc()
{
    // BP -> IN self-oscillation patch: stable, amplitude-limited (integrator
    // saturation), never NaN/Inf (GAUNTLET T07). Run one patch, return BP.
    auto runPatch = [] (double q, float gainCtl, std::vector<float>& bp, bool& finiteOk)
    {
        Rig r; auto* v = r.sys.vcfq;
        r.sys.patch (v, VCFQ::jBp, v, VCFQ::jIn);            // the self-osc patch
        r.sys.patch (r.sys.key, KEY::jGate, v, VCFQ::jTrig); // ping to kick it
        v->ctls[VCFQ::cQ].v    = qKnob (q);
        v->ctls[VCFQ::cGain].v = gainCtl;
        const int n = 48000;
        bp.assign ((size_t) n, 0.0f);
        finiteOk = true;
        for (int i = 0; i < n; ++i)
        {
            r.sys.key->gateV = i < 480 ? kCV : 0.0f;
            r.sys.engine.tick();
            const float s = v->jacks[VCFQ::jBp].value;
            if (! std::isfinite (s)) finiteOk = false;
            bp[(size_t) i] = s;
        }
    };
    auto settled = [] (const std::vector<float>& bp, float& pA, float& pB, double& rms)
    {
        pA = pB = 0.0f; rms = 0.0;
        for (int i = 24000; i < 36000; ++i) pA = std::max (pA, std::fabs (bp[(size_t) i]));
        for (int i = 36000; i < 48000; ++i)
        { pB = std::max (pB, std::fabs (bp[(size_t) i])); rms += (double) bp[(size_t) i] * bp[(size_t) i]; }
        rms = std::sqrt (rms / 12000.0);
        return rms > 0.5                                    // it oscillates
            && pB <= 10.5f && pA <= 10.5f                   // saturation ceiling
            && pB > 0.8f * pA && pB < 1.25f * pA;           // settled amplitude
    };

    // (a) the playable case: loop gain just over critical (Q 10, GAIN 0.13 ->
    //     |loop| ~ 1.3 at fc). Hardware: "a nice round SINE" at fc; integrator
    //     gain compression flattens the pitch slightly (dsp-techniques §2).
    //     Rev-B amendment 2026-11-02 (LOG.md): was GAIN 1.5, a ~15x overdriven
    //     loop that relaxation-oscillates far below fc on any 2-pole model.
    {
        std::vector<float> bp; bool finiteOk;
        runPatch (10.0, 0.065f, bp, finiteOk);
        float pA, pB; double rms;
        const bool ok = settled (bp, pA, pB, rms);
        CHECK (finiteOk && ok, "BP->IN self-osc (near-critical) stable, amplitude-limited, no NaN", pB, rms);
        int zc = 0;
        for (int i = 36001; i < 48000; ++i)
            if (bp[(size_t) i - 1] < 0.0f && bp[(size_t) i] >= 0.0f) ++zc;
        const double f = zc / 0.25;
        const double c  = goertzelBuf (bp, kFc, 36000, 12000);
        const double lo = goertzelBuf (bp, kFc * 0.5, 36000, 12000);
        const double hi = goertzelBuf (bp, kFc * 2.0, 36000, 12000);
        CHECK (c > lo && c > hi && f > 0.85 * kFc && f < 1.02 * kFc,
               "self-osc frequency tracks fc (0.85..1.02 x 800 Hz)", f, kFc);
    }
    // (b) overdriven: GAIN 1.5 (~15x loop gain). Pitch is free to collapse
    //     (relaxation regime); it must stay stable, bounded and finite.
    {
        std::vector<float> bp; bool finiteOk;
        runPatch (10.0, 0.75f, bp, finiteOk);
        float pA, pB; double rms;
        const bool ok = settled (bp, pA, pB, rms);
        CHECK (finiteOk && ok, "BP->IN self-osc (overdriven) stable, amplitude-limited, no NaN", pB, rms);
    }
    // abuse: Q=60 via CV + gain 2 + rail-level drive + repeated pings + loop
    {
        Rig r; auto* v = r.sys.vcfq;
        r.sys.patch (v, VCFQ::jBp, v, VCFQ::jIn);
        r.sys.patch (r.sys.key, KEY::jKv,   v, VCFQ::jIn);
        r.sys.patch (r.sys.key, KEY::jGate, v, VCFQ::jTrig);
        r.sys.patch (r.sys.key, KEY::jClk,  v, VCFQ::jVcQ);
        v->ctls[VCFQ::cQ].v = 1.0f; v->ctls[VCFQ::cVcQAmt].v = 1.0f;
        v->ctls[VCFQ::cGain].v = 1.0f; v->ctls[VCFQ::cPing].v = 1.0f;
        bool ok = true; long n = 0;
        for (int i = 0; i < 48000; ++i)
        {
            r.sys.key->pitchV = (float) (10.0 * std::sin (2.0 * kPi * kFc * (double) n++ / SR));
            r.sys.key->gateV  = (i % 2400) < 480 ? kCV : 0.0f;
            r.sys.key->clkV   = kCV;
            r.sys.engine.tick();
            for (int j = VCFQ::jLp; j <= VCFQ::jNotch; ++j)
            {
                const float s = v->jacks[(size_t) j].value;
                if (! std::isfinite (s) || std::fabs (s) > kRails) ok = false;
            }
        }
        CHECK (ok, "never NaN/Inf within rails (Q=60 CV, gain 2, hot drive)");
    }
}

// ---------------------------------------------------------------------------
// T16-protocol module perf: bare Engine with one VCFQ, live audio + audio-rate
// FM (worst case: per-sample coefficient updates). Min wall time over 5 runs
// of >= 10 s render, cycles/sample at the assumed 3.504 GHz P-core clock.
// SPEC §10 spike figure for VCFQ: 352 cyc/sample; hard flag bound = 3x = 1056.
// ---------------------------------------------------------------------------
struct PerfSrc : Module
{
    PerfSrc()
    {
        addJack ("SIG",  JackType::Out, JackColor::Black);
        addJack ("GATE", JackType::Out, JackColor::Red);
    }
    void tick() override
    {
        ph += 800.0f / (float) SR; if (ph >= 1.0f) ph -= 1.0f;
        jacks[0].value = 2.0f * std::sin (6.2831853f * ph);
        jacks[1].value = (n++ % 24000) < 480 ? kCV : 0.0f;
    }
    float ph = 0.0f; long n = 0;
};

inline double vcfqCyclesPerSample = 0.0;

inline void testPerf()
{
    Engine e;
    auto* src = e.add<PerfSrc> ("perfsrc", "PERF SRC");
    auto* v   = e.add<VCFQ>    ("vcfq",    "VARIABLE Q VCF");
    e.prepare (SR);
    e.connect (0, 0, 1, VCFQ::jIn);
    e.connect (0, 0, 1, VCFQ::jVcF);  // audio-rate FM: per-sample tan/exp2
    e.connect (0, 1, 1, VCFQ::jTrig);
    v->ctls[VCFQ::cVcFAmt].v = 0.6f;
    (void) src;
    const long n = (long) (SR * 10.0);
    double best = 1.0e30;
    for (int run = 0; run < 5; ++run)
    {
        auto t0 = std::chrono::steady_clock::now();
        for (long i = 0; i < n; ++i) e.tick();
        best = std::min (best, std::chrono::duration<double> (
                             std::chrono::steady_clock::now() - t0).count());
    }
    vcfqCyclesPerSample = best * kAssumedPCoreHz / (double) n;
    std::printf ("  VCFQ perf (bare engine, audio-rate FM): %.1f cyc/sample "
                 "(spike 352, 3x bound 1056)\n", vcfqCyclesPerSample);
    CHECK (vcfqCyclesPerSample < 1056.0,
           "VCFQ cycles/sample within 3x of SPEC 10 spike (352)", vcfqCyclesPerSample);
}

// ---------------------------------------------------------------------------
// Render evidence (SATK_RENDER=1): vcfq_ping.wav, v2 counterpart of v1
// manifest file 5 — 8 s, 48 kHz, fc 800 Hz, ping level 0.9, 2 Hz trigger,
// Q stepped every 2 s: 2, 6, 15, 35; VCFQ BP -> monitor at 0.9.
// ---------------------------------------------------------------------------
inline void writeWav16 (const std::string& fn, const std::vector<float>& L,
                        const std::vector<float>& R, int sr)
{
    FILE* f = std::fopen (fn.c_str(), "wb");
    if (f == nullptr) { std::printf ("  RENDER: cannot open %s\n", fn.c_str()); return; }
    const uint32_t n = (uint32_t) L.size(), dataSz = n * 4u, riff = 36u + dataSz;
    const uint16_t ch = 2, bits = 16, blk = 4; const uint16_t pcm = 1;
    const uint32_t fmtSz = 16, byps = (uint32_t) sr * blk;
    std::fwrite ("RIFF", 1, 4, f); std::fwrite (&riff, 4, 1, f);
    std::fwrite ("WAVEfmt ", 1, 8, f);
    std::fwrite (&fmtSz, 4, 1, f); std::fwrite (&pcm, 2, 1, f); std::fwrite (&ch, 2, 1, f);
    std::fwrite (&sr, 4, 1, f); std::fwrite (&byps, 4, 1, f);
    std::fwrite (&blk, 2, 1, f); std::fwrite (&bits, 2, 1, f);
    std::fwrite ("data", 1, 4, f); std::fwrite (&dataSz, 4, 1, f);
    for (uint32_t i = 0; i < n; ++i) // fixed gain 1.0 (comparable to v1 renders)
    {
        const int16_t s[2] = {
            (int16_t) std::lround (std::clamp (L[i], -1.0f, 1.0f) * 32000.0f),
            (int16_t) std::lround (std::clamp (R[i], -1.0f, 1.0f) * 32000.0f) };
        std::fwrite (s, 2, 2, f);
    }
    std::fclose (f);
    double rms = 0.0; float peak = 0.0f;
    for (float s : L) { rms += (double) s * s; peak = std::max (peak, std::fabs (s)); }
    std::printf ("  RENDER %s  %.2fs  peak %.3f  rmsL %.4f\n",
                 fn.c_str(), n / (double) sr, peak, std::sqrt (rms / std::max<size_t> (1, n)));
}

inline void renderPing()
{
    namespace fs = std::filesystem;
    fs::path dir = fs::path (__FILE__).parent_path().parent_path()
                 / "gauntlet" / "renders" / "v2work" / "vcfq";
    if (const char* ov = std::getenv ("SATK_RENDER_DIR")) dir = ov;
    std::error_code ec; fs::create_directories (dir, ec);

    Rig r; auto* v = r.sys.vcfq;
    r.sys.patch (r.sys.key, KEY::jGate, v, VCFQ::jTrig); // 2 Hz pulse -> TRIG
    v->ctls[VCFQ::cPing].v = 0.9f;
    r.sys.out->ctls[OUT::cMonSrc].v = 12.0f;             // VCFQ BP -> monitor
    r.sys.out->ctls[OUT::cMonLvl].v = 0.9f;
    const double qSteps[] = { 2.0, 6.0, 15.0, 35.0 };    // 2 s per step
    const int n = (int) (SR * 8.0);
    std::vector<float> L ((size_t) n), R ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        v->ctls[VCFQ::cQ].v = qKnob (qSteps[std::min (3, (int) (i / (2.0 * SR)))]);
        r.sys.key->gateV = (i % 24000) < 480 ? kCV : 0.0f; // 2 Hz, 10 ms pulses
        r.sys.engine.tick();
        L[(size_t) i] = r.sys.out->outL; R[(size_t) i] = r.sys.out->outR;
    }
    writeWav16 ((dir / "vcfq_ping.wav").string(), L, R, (int) SR);
}

// ---------------------------------------------------------------------------
inline void run()
{
    GROUP ("T07 VCFQ (LP slope, 4 outs, AGC-only compensation, ping, self-osc)");
    testSlopeAndSimultaneous();
    testOctCalibration();
    testAgc();
    testPing();
    testSelfOsc();
    testPerf();
    if (const char* env = std::getenv ("SATK_RENDER"))
        if (env[0] == '1') renderPing();
}

inline const bool registered = gauntlet::registerTest ("t_vcfq", &run);

} // namespace t_vcfq
