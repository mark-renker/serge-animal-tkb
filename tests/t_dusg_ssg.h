// Serge Animal TKB v2 — DUSG/SSG gauntlet tests: T05 (slope timing, 1V/oct
// rate CV, cycle >= 4 kHz, linear-by-design mid-slope, END/GATE level law,
// non-retrigger, ASR) and T06 (SSG smooth/stepped/coupler, hot outs, HOLD).
// Plus the DUSG1 piece of the T15 default-patch contract (envelope path).
// All probes inject test-owned floats straight into jack feeds (never through
// KEY KV, whose glide floor is a ~1.6 kHz one-pole).
#pragma once
#include "Harness.h"
#include <algorithm>
#include <cstring>

namespace t_dusg_ssg {

using namespace gauntlet;
using namespace satk;

// Feed a test-owned float into an In jack (bypasses the cable graph; any later
// rebuild() — patch/disconnect — wipes it, so inject AFTER patching).
inline void injectSrc (Module* m, int jack, const float* src)
{
    m->feeds[(size_t) jack].src.push_back (src);
}

// knob position for a dialed time on the expo 50 us..10 s map
inline float dial (double seconds)
{ return (float) (std::log (seconds / 50.0e-6) / std::log (10.0 / 50.0e-6)); }

// One DUSG (dusg2: no default cables touch it) with injected inputs.
struct DRig
{
    Rig r;
    DUSG* d;
    float trig = 0.0f, sig = 0.0f, vcR = 0.0f, vcF = 0.0f;
    explicit DRig (bool selfFeedbackRise = false)
    {
        d = r.sys.dusg2;
        if (selfFeedbackRise) r.sys.patch (d, DUSG::jOut, d, DUSG::jVcRise);
        injectSrc (d, DUSG::jTrig, &trig);
        injectSrc (d, DUSG::jIn, &sig);
        injectSrc (d, DUSG::jVcRise, &vcR);
        injectSrc (d, DUSG::jVcFall, &vcF);
        r.step (16); // warm-up: jacks read 0 until the first tick
    }
    void set (double rise, double fall)
    { d->ctls[DUSG::cRise].v = dial (rise); d->ctls[DUSG::cFall].v = dial (fall); }
    float out() const { return d->jacks[DUSG::jOut].value; }
    float end() const { return d->jacks[DUSG::jEnd].value; }
    void tick() { r.sys.engine.tick(); }
};

// Trigger once and measure rise (trigger -> END falls) and fall (END falls ->
// END rises again), in seconds. Sample resolution (+-1 sample).
inline bool measureTransient (DRig& g, double maxSeconds, double& rise, double& fall)
{
    const long n = (long) (maxSeconds * SR);
    long iRise = -1, iFall = -1;
    for (long i = 0; i < n && iFall < 0; ++i)
    {
        g.trig = i < 48 ? kCV : 0.0f;
        g.tick();
        const bool hi = g.end() > kLogicHi;
        if (iRise < 0 && ! hi) iRise = i;
        else if (iRise >= 0 && hi) iFall = i;
    }
    rise = iRise / SR;
    fall = (iFall - iRise) / SR;
    return iRise > 0 && iFall > iRise;
}

// ---------------------------------------------------------------------------
// T05 (a) timing over 10 ms - 5 s, sub-ms at knob + CV extremes
// ---------------------------------------------------------------------------
inline void testTiming()
{
    const double times[] = { 0.010, 0.100, 1.0, 5.0 };
    double worst = 0.0; bool ok = true;
    for (double t : times)
    {
        DRig g; g.set (t, t);
        double rise = 0, fall = 0;
        const bool m = measureTransient (g, 3.0 * t + 0.1, rise, fall);
        const double er = 100.0 * std::fabs (rise / t - 1.0);
        const double ef = 100.0 * std::fabs (fall / t - 1.0);
        std::printf ("    dialed %7.3f s: rise %.5f s (%.3f%%)  fall %.5f s (%.3f%%)\n",
                     t, rise, er, fall, ef);
        worst = std::max (worst, std::max (er, ef));
        if (! m) ok = false;
    }
    CHECK (ok && worst <= 10.0, "T05a: rise/fall within +-10% of dialed, 10 ms..5 s (worst %)",
           worst, 10.0);

    // asymmetric dial: rise and fall are independent
    {
        DRig g; g.set (0.020, 0.300);
        double rise = 0, fall = 0;
        measureTransient (g, 1.0, rise, fall);
        CHECK (std::fabs (rise / 0.020 - 1.0) < 0.1 && std::fabs (fall / 0.300 - 1.0) < 0.1,
               "T05a: independent rise 20 ms / fall 300 ms", rise, fall);
    }

    // knob minimum + VC +3 V -> clamps at 20 us: well below 1 ms
    {
        DRig g; g.d->ctls[DUSG::cRise].v = 0.0f; g.d->ctls[DUSG::cFall].v = 0.0f;
        g.vcR = 3.0f; g.vcF = 3.0f; g.tick();
        double rise = 0, fall = 0;
        // short trigger so it does not linger high past the transient
        long iR = -1, iF = -1;
        double peak = 0.0;
        for (long i = 0; i < 480 && iF < 0; ++i)
        {
            g.trig = i < 1 ? kCV : 0.0f; g.tick();
            peak = std::max (peak, (double) g.out());
            const bool hi = g.end() > kLogicHi;
            if (iR < 0 && ! hi) iR = i; else if (iR >= 0 && hi) iF = i;
        }
        rise = (iR + 1) / SR; fall = (iF - iR) / SR;
        CHECK (iR >= 0 && iF > iR && rise < 1.0e-3 && fall < 1.0e-3,
               "T05a: times extend below 1 ms at knob + CV extremes (rise, fall s)", rise, fall);
    }
}

// ---------------------------------------------------------------------------
// T05 (b) 1 V/oct rate CV: time halves per volt within 3%
// ---------------------------------------------------------------------------
inline void testRateCv()
{
    double base = 0, baseF = 0, worst = 0;
    {
        DRig g; g.set (0.200, 0.200);
        measureTransient (g, 1.0, base, baseF);
    }
    const float volts[] = { -1.0f, 1.0f, 2.0f, 3.0f };
    for (float v : volts)
    {
        DRig g; g.set (0.200, 0.200); g.vcR = v; g.vcF = v;
        double rise = 0, fall = 0;
        measureTransient (g, 2.0, rise, fall);
        const double want = std::exp2 (-(double) v);
        const double er = 100.0 * std::fabs (rise / base / want - 1.0);
        const double ef = 100.0 * std::fabs (fall / baseF / want - 1.0);
        std::printf ("    VC %+.0f V: rise ratio %.4f fall ratio %.4f (want %.4f)\n",
                     v, rise / base, fall / baseF, want);
        worst = std::max (worst, std::max (er, ef));
    }
    CHECK (worst <= 3.0, "T05b: VC RISE/FALL 1 V/oct, time halves per volt (worst %)",
           worst, 3.0);
}

// ---------------------------------------------------------------------------
// T05 (c) cycle mode reaches >= 4 kHz (and dialed cycle pitch is accurate)
// ---------------------------------------------------------------------------
inline void testCycle()
{
    {
        DRig g; g.d->ctls[DUSG::cCycle].v = 1.0f;
        g.d->ctls[DUSG::cRise].v = 0.0f; g.d->ctls[DUSG::cFall].v = 0.0f;
        float lo = 1e9f, hi = -1e9f;
        const double f = edgeRate (g.r, g.d, DUSG::jEnd, 48000, [&] (int)
        { lo = std::min (lo, g.out()); hi = std::max (hi, g.out()); });
        CHECK (f >= 4000.0, "T05c: cycle mode at knob minimum reaches >= 4 kHz (Hz)", f, 4000.0);
        CHECK (hi - lo > 2.5f, "T05c: cycle output still swings at max rate (V p-p)", hi - lo);
    }
    {
        DRig g; g.d->ctls[DUSG::cCycle].v = 1.0f; g.set (125.0e-6, 125.0e-6);
        const double f = edgeRate (g.r, g.d, DUSG::jEnd, 48000);
        CHECK (std::fabs (f / 4000.0 - 1.0) < 0.01,
               "T05c: dialed 125+125 us cycles at 4 kHz within 1% (sub-sample carry)", f, 4000.0);
    }
    {
        DRig g; g.d->ctls[DUSG::cCycle].v = 1.0f; g.set (0.010, 0.010);
        const double f = edgeRate (g.r, g.d, DUSG::jEnd, 96000);
        CHECK (std::fabs (f / 50.0 - 1.0) < 0.01, "T05c: 10+10 ms cycles at 50 Hz (LFO)", f, 50.0);
    }
}

// ---------------------------------------------------------------------------
// T05 (d) linear-by-design mid slope, tanh soft landing, feedback bends it
// ---------------------------------------------------------------------------
struct SlopeStats { double devPct = 0, peak = 0; bool monotonic = true; bool landed = false; };

inline SlopeStats captureRise (bool feedback, double riseT)
{
    DRig g (feedback); g.set (riseT, 1.0);
    std::vector<double> y;
    const long n = (long) (riseT * SR * 1.5) + 200;
    for (long i = 0; i < n; ++i)
    {
        g.trig = i < 48 ? kCV : 0.0f; g.tick();
        if (g.end() < kCV) break; // rise over (the edge sample already holds fall time)
        y.push_back (g.out());
    }
    SlopeStats s;
    for (size_t i = 1; i < y.size(); ++i)
        if (y[i] < y[i - 1]) s.monotonic = false;
    s.peak = *std::max_element (y.begin(), y.end());
    // mid 80% of the swing (0.5..4.5 V): least-squares line, max deviation
    std::vector<std::pair<double, double>> pts;
    for (size_t i = 0; i < y.size(); ++i)
        if (y[i] >= 0.5 && y[i] <= 4.5) pts.push_back ({ (double) i, y[i] });
    if (pts.size() >= 3)
    {
        double sx = 0, sy = 0, sxx = 0, sxy = 0; const double N = (double) pts.size();
        for (auto& p : pts) { sx += p.first; sy += p.second; sxx += p.first * p.first; sxy += p.first * p.second; }
        const double b = (N * sxy - sx * sy) / (N * sxx - sx * sx), a = (sy - b * sx) / N;
        double dev = 0;
        for (auto& p : pts) dev = std::max (dev, std::fabs (p.second - (a + b * p.first)));
        s.devPct = 100.0 * dev / kCV;
    }
    // soft landing: the final increments shrink well below the linear slope
    if (y.size() > 40)
    {
        const double linStep = kCV / (riseT * SR);
        const double lastStep = y[y.size() - 2] - y[y.size() - 3];
        s.landed = lastStep > 0.0 && lastStep < 0.5 * linStep;
    }
    return s;
}

inline void testShape()
{
    const SlopeStats lin = captureRise (false, 0.100);
    std::printf ("    100 ms rise: mid-80%% deviation %.4f%% FS, peak %.6f V\n", lin.devPct, lin.peak);
    CHECK (lin.devPct <= 3.0, "T05d: mid 80% of a triggered rise within 3% FS of a line",
           lin.devPct, 3.0);
    CHECK (lin.monotonic && lin.peak <= (double) kCV,
           "T05d: approach monotonic, no overshoot of the 5 V target", lin.peak);
    CHECK (lin.landed, "T05d: tanh current-limit soft landing (last step < 0.5x slope)");

    const SlopeStats bent = captureRise (true, 0.100);
    std::printf ("    OUT->VC RISE feedback: deviation %.2f%% FS\n", bent.devPct);
    CHECK (bent.devPct >= 10.0 && bent.monotonic && bent.peak <= (double) kCV,
           "T05d: OUT->VC RISE feedback patch measurably bends the slope (% FS)", bent.devPct);
}

// ---------------------------------------------------------------------------
// T05 (e) END/GATE is a level; cycle duty tracks rise/(rise+fall), 5..95%
// ---------------------------------------------------------------------------
inline void testEndLevel()
{
    DRig g; g.set (0.020, 0.050);
    const bool restHigh = g.end() > kLogicHi; // at bottom before any trigger
    int bad = 0, nRise = 0, nFall = 0, nBottom = 0;
    float prev = g.out();
    for (long i = 0; i < (long) (0.2 * SR); ++i)
    {
        g.trig = i < 48 ? kCV : 0.0f; g.tick();
        const float y = g.out(), e = g.end();
        const bool hi = e > kLogicHi, mid = e > 0.0f && e < kCV; // mid = edge sample
        if (y > prev)                    { ++nRise;   if (! hi && ! mid) ++bad; }
        else if (y < prev && y > 0.01f)  { ++nFall;   if (hi && ! mid) ++bad; }
        else if (y <= 0.0025f)           { ++nBottom; if (! hi) ++bad; }
        prev = y;
    }
    CHECK (restHigh && bad == 0 && nRise > 900 && nFall > 2000 && nBottom > 1000,
           "T05e: END/GATE high in rise + at bottom, low in fall (violations)", bad);

    double lo = 1, hi = 0, worst = 0;
    const double fracs[] = { 0.05, 0.25, 0.50, 0.75, 0.95 };
    for (double fr : fracs)
    {
        DRig c; c.d->ctls[DUSG::cCycle].v = 1.0f; c.set (fr * 0.020, (1.0 - fr) * 0.020);
        c.r.step (4800);
        double acc = 0; const long n = 48000; // 50 whole periods
        for (long i = 0; i < n; ++i) { c.tick(); acc += c.end(); }
        const double duty = acc / (n * (double) kCV);
        std::printf ("    rise/(rise+fall) %.2f -> END duty %.4f\n", fr, duty);
        lo = std::min (lo, duty); hi = std::max (hi, duty);
        worst = std::max (worst, std::fabs (duty - fr));
    }
    CHECK (worst <= 0.01, "T05e: cycle duty tracks rise/(rise+fall) 5..95% (worst abs err)",
           worst, 0.01);
}

// ---------------------------------------------------------------------------
// T05 (f) non-retriggerable during a transient
// ---------------------------------------------------------------------------
inline void testNonRetrigger()
{
    const long n = (long) (0.15 * SR);
    std::vector<float> a, b;
    {
        DRig g; g.set (0.020, 0.050);
        for (long i = 0; i < n; ++i) { g.trig = i < 48 ? kCV : 0.0f; g.tick(); a.push_back (g.out()); }
    }
    DRig g; g.set (0.020, 0.050);
    for (long i = 0; i < n; ++i)
    {
        const long t = i % 480; // a 1 ms pulse every 10 ms: mid-rise AND mid-fall
        g.trig = (i < (long) (0.065 * SR) && t < 48) ? kCV : 0.0f;
        g.tick(); b.push_back (g.out());
    }
    CHECK (std::memcmp (a.data(), b.data(), a.size() * sizeof (float)) == 0,
           "T05f: triggers during rise and fall are ignored (bit-identical)");
    // once the transient completes, a trigger fires a new one
    for (long i = 0; i < 480; ++i) { g.trig = i < 48 ? kCV : 0.0f; g.tick(); }
    CHECK (g.out() > 1.0f, "T05f: re-armed after the transient completes (V at +10 ms)", g.out());
}

// ---------------------------------------------------------------------------
// T05 (g) gate into IN = ASR sustain overriding TRIG; IN as DC follower
// ---------------------------------------------------------------------------
inline void testAsr()
{
    DRig g; g.set (0.010, 0.050);
    float minSustain = 10.0f; bool endHighInSustain = true;
    long iDone = -1; bool endLowInRelease = true;
    for (long i = 0; i < (long) (0.5 * SR); ++i)
    {
        const double t = i / SR;
        g.sig = t < 0.300 ? kCV : 0.0f;                     // gate into IN
        g.trig = (std::fmod (t, 0.1) < 0.001 && t > 0.05 && t < 0.29) ? kCV : 0.0f; // TRIGs in sustain
        g.tick();
        if (t > 0.015 && t < 0.300)
        {
            minSustain = std::min (minSustain, g.out());
            if (g.end() < kLogicHi) endHighInSustain = false;
        }
        if (t > 0.302 && g.out() > 0.01f && g.end() > kLogicHi) endLowInRelease = false;
        if (t > 0.300 && iDone < 0 && g.out() <= 0.0025f) iDone = i;
    }
    const double release = iDone / SR - 0.300;
    CHECK (minSustain >= 4.99f, "T05g: gate into IN sustains at the top, TRIGs overridden (min V)",
           minSustain);
    CHECK (std::fabs (release / 0.050 - 1.0) < 0.1,
           "T05g: release at the FALL rate after the gate ends (s)", release, 0.050);
    CHECK (endHighInSustain && endLowInRelease,
           "T05g: END/GATE high through A+S, low during R");

    // DC follower: slew-limited toward a static IN level, then exact
    DRig f; f.set (0.100, 0.100); f.sig = 2.0f;
    f.r.step ((int) (0.020 * SR));
    const float at20 = f.out();
    f.r.step ((int) (0.100 * SR));
    CHECK (std::fabs (at20 - 1.0f) < 0.05f && f.out() == 2.0f,
           "T05g: IN = DC follower, 5 V/RISE slope then exact (V at 20 ms)", at20, f.out());
}

// ---------------------------------------------------------------------------
// T15 piece: DUSG1 is the voice envelope of the default patch. NTO and VCM
// are still stubs at this stage, so a 220 Hz 2.5 V sine is injected into the
// VCA1 audio input as a stand-in for the VCFQ LP path; the envelope / VCA /
// mixer / velocity chain is the real default patch.
// ---------------------------------------------------------------------------
inline void testDefaultPatchEnvelope()
{
    Rig r; auto& s = r.sys;
    s.patchDefaults();
    float src = 0.0f; long n = 0;
    injectSrc (s.vca1, VCA::jIn, &src);
    auto pre = [&] (int) { src = (float) (kAudio * std::sin (2.0 * 3.14159265358979 * 220.0 * (double) n++ / SR)); };
    r.step (64, pre);
    s.key->pitchV = 0.0f; s.key->gateV = kCV;
    double acc = 0, peak = 0, envPeak = 0;
    for (int i = 0; i < (int) SR; ++i)
    {
        pre (0); s.engine.tick();
        acc += (double) s.out->outL * s.out->outL;
        peak = std::max (peak, (double) std::fabs (s.out->outL));
        if (i < 96) envPeak = std::max (envPeak, (double) s.dusg1->jacks[DUSG::jOut].value);
    }
    const double rms = std::sqrt (acc / SR);
    std::printf ("    surrogate note-60 render (1 s, L): rms %.4f peak %.4f; DUSG1 peak %.4f V in 2 ms\n",
                 rms, peak, envPeak);
    CHECK (rms >= 0.02 && envPeak >= 4.99,
           "T15 piece: KEY gate fires DUSG1 envelope -> audible (RMS within 1 s)", rms, 0.02);
    s.key->gateV = 0.0f;
    r.step ((int) (0.5 * SR), pre);
    double tail = 0;
    for (int i = 0; i < 4800; ++i) { pre (0); s.engine.tick(); tail = std::max (tail, (double) std::fabs (s.out->outL)); }
    CHECK (tail < 0.001, "T15 piece: note-off -> decays below -60 dBFS", tail, 0.001);

    // remove the DUSG1 trigger cable -> exact 0.0
    s.engine.disconnectAt (s.engine.indexOf (s.key), KEY::jGate);
    injectSrc (s.vca1, VCA::jIn, &src); // rebuild() wiped the probe
    bool zero = true;
    s.key->gateV = kCV;
    for (int i = 0; i < (int) SR; ++i)
    {
        pre (0); s.engine.tick();
        if (s.out->outL != 0.0f || s.out->outR != 0.0f
            || s.dusg1->jacks[DUSG::jOut].value != 0.0f) zero = false;
    }
    CHECK (zero, "T15 piece: DUSG1 trigger cable removed -> exact 0.0 output");
}

// ---------------------------------------------------------------------------
// T06 SSG
// ---------------------------------------------------------------------------
struct SRig
{
    Rig r;
    SSG* g;
    float smIn = 0, smVc = 0, hold = 0, stIn = 0, sample = 0, stVc = 0;
    explicit SRig (bool selfCycle)
    {
        g = r.sys.ssg;
        if (selfCycle) r.sys.patch (g, SSG::jCycle, g, SSG::jSmIn);
        else injectSrc (g, SSG::jSmIn, &smIn);
        injectSrc (g, SSG::jSmVcRate, &smVc);
        injectSrc (g, SSG::jHold, &hold);
        injectSrc (g, SSG::jStIn, &stIn);
        injectSrc (g, SSG::jSample, &sample);
        injectSrc (g, SSG::jStVcRate, &stVc);
        r.step (16);
    }
    float j (int k) const { return g->jacks[(size_t) k].value; }
};

inline void testSsgSmooth()
{
    // self-patch CYCLE -> SMOOTH IN: triangle 0 -> 4..5 V, one pulse per cycle
    {
        SRig s (true);
        s.r.step (4800);
        float lo = 1e9f, hi = -1e9f; int upX = 0, cycEdges = 0; float prev = s.j (SSG::jSmOut);
        bool prevC = s.j (SSG::jCycle) > kLogicHi;
        for (int i = 0; i < 96000; ++i)
        {
            s.r.sys.engine.tick();
            const float y = s.j (SSG::jSmOut);
            lo = std::min (lo, y); hi = std::max (hi, y);
            if (prev < 2.5f && y >= 2.5f) ++upX;
            const bool c = s.j (SSG::jCycle) > kLogicHi;
            if (c && ! prevC) ++cycEdges;
            prev = y; prevC = c;
        }
        std::printf ("    self-cycle: smooth %.3f..%.3f V, %d cycles, %d CYCLE pulses in 2 s\n",
                     lo, hi, upX, cycEdges);
        CHECK (upX >= 5 && lo <= 0.1f && hi >= 4.0f && hi <= 5.0f,
               "T06: smooth self-patch self-oscillates, 0 -> 4..5 V triangle (peak V)", hi);
        CHECK (std::abs (cycEdges - upX) <= 1, "T06: CYCLE emits one pulse per cycle",
               cycEdges, upX);
    }
    // rate span: < 1 Hz at a slow knob, >= 4 kHz at the fast end
    double fSlow = 0, fFast = 0;
    {
        SRig s (true); s.g->ctls[SSG::cSmRate].v = 0.8f;
        fSlow = edgeRate (s.r, s.g, SSG::jCycle, (int) (8 * SR));
    }
    {
        SRig s (true); s.g->ctls[SSG::cSmRate].v = 0.0f;
        fFast = edgeRate (s.r, s.g, SSG::jCycle, 48000);
    }
    std::printf ("    smooth cycle rate span: %.3f Hz .. %.1f Hz\n", fSlow, fFast);
    CHECK (fSlow > 0.0 && fSlow < 1.0 && fFast >= 4000.0,
           "T06: smooth cycle rate spans < 1 Hz .. >= 4 kHz (Hz)", fSlow, fFast);

    // VC RATE 1 V/oct on the lag time (0 -> 5 V step, time to cross 2.5 V)
    auto cross = [] (float vc) {
        SRig s (false); s.g->ctls[SSG::cSmRate].v = dial (0.100); s.smVc = vc;
        s.smIn = kCV;
        for (int i = 0; i < 48000; ++i) { s.r.sys.engine.tick(); if (s.j (SSG::jSmOut) >= 2.5f) return i; }
        return -1;
    };
    const int c0 = cross (0.0f), c1 = cross (1.0f), c2 = cross (2.0f);
    const double e = 100.0 * std::max (std::fabs ((double) c1 / c0 - 0.5) / 0.5,
                                       std::fabs ((double) c2 / c0 - 0.25) / 0.25);
    CHECK (c0 > 0 && e <= 3.0, "T06: SMOOTH VC RATE 1 V/oct (worst %)", e, 3.0);

    // HOLD freezes exactly; release resumes
    {
        SRig s (true);
        s.r.step (2000);
        s.hold = kCV; s.r.sys.engine.tick();
        const float frozen = s.j (SSG::jSmOut);
        bool exact = true;
        for (int i = 0; i < 24000; ++i)
        { s.r.sys.engine.tick(); if (std::memcmp (&frozen, &s.g->jacks[SSG::jSmOut].value, sizeof (float)) != 0) exact = false; }
        s.hold = 0.0f; s.r.step (480);
        CHECK (exact && s.j (SSG::jSmOut) != frozen,
               "T06: HOLD freezes the smooth output exactly (track & hold)", frozen);
    }
}

inline void testSsgStepped()
{
    SRig s (false);
    // capture one sample, then drive a moving input with NO sample pulses
    s.stIn = 3.0f; s.sample = kCV; s.r.sys.engine.tick(); s.sample = 0.0f;
    s.r.step (960);
    const float held = s.j (SSG::jStOut);
    bool stat = true; long n = 0;
    for (int i = 0; i < 48000; ++i)
    {
        s.stIn = (float) (2.5 + 2.0 * std::sin (0.01 * (double) n++));
        s.r.sys.engine.tick();
        if (s.j (SSG::jStOut) != held) stat = false;
    }
    CHECK (stat && held == 3.0f, "T06: stepped = S&H, static without SAMPLE pulses (V)", held);

    // sampling follows the input on each pulse
    bool tracks = true;
    for (int k = 0; k < 20; ++k)
    {
        s.stIn = (float) (k % 5); s.sample = kCV; s.r.sys.engine.tick(); s.sample = 0.0f;
        s.r.step (479);
        if (s.j (SSG::jStOut) != s.stIn) tracks = false;
    }
    CHECK (tracks, "T06: SAMPLE pulses acquire STEPPED IN (exact after acquisition)");

    // VC step-size limiting: 0 -> 5 V jump, time to cross 2.5 V halves per volt
    auto cross = [] (float vc) {
        SRig t (false); t.g->ctls[SSG::cStRate].v = dial (0.100); t.stVc = vc;
        t.stIn = kCV; t.sample = kCV;
        for (int i = 0; i < 48000; ++i) { t.r.sys.engine.tick(); if (t.j (SSG::jStOut) >= 2.5f) return i; }
        return -1;
    };
    const int c0 = cross (0.0f), c1 = cross (1.0f);
    const double e = 100.0 * std::fabs ((double) c1 / c0 - 0.5) / 0.5;
    CHECK (c0 > 2300 && c0 < 2500 && e <= 3.0,
           "T06: ST VC RATE step-size limiting, 1 V/oct (samples to 2.5 V)", c0, c1);
}

inline void testSsgCoupler()
{
    SRig s (true); // smooth cycles 0..4.75 V
    s.stIn = 2.5f; s.sample = kCV; s.r.sys.engine.tick(); s.sample = 0.0f;
    bool binary = true; float hmin = 1e9f, hmax = -1e9f; int toggles = 0; bool prev = false;
    for (int i = 0; i < 48000; ++i)
    {
        s.r.sys.engine.tick();
        const float c = s.j (SSG::jCoupler), h = s.j (SSG::jHotCoupler);
        if (c != 0.0f && c != kCV) binary = false;
        hmin = std::min (hmin, h); hmax = std::max (hmax, h);
        const bool hi = c > kLogicHi;
        if (hi != prev) ++toggles;
        prev = hi;
    }
    CHECK (binary && toggles >= 10, "T06: coupler comparator smooth>stepped, 0/5 V (toggles)",
           toggles);
    CHECK (hmin <= -10.0f && hmax >= 10.0f && hmax <= kRails && hmin >= -kRails,
           "T06: HOT coupler swings >= +-10 V (min/max V)", hmin, hmax);
}

inline void run()
{
    GROUP ("T05 DUSG (slope generator)");
    testTiming();
    testRateCv();
    testCycle();
    testShape();
    testEndLevel();
    testNonRetrigger();
    testAsr();
    testDefaultPatchEnvelope();

    GROUP ("T06 SSG (smooth & stepped generator)");
    testSsgSmooth();
    testSsgStepped();
    testSsgCoupler();
}

inline const bool registered = gauntlet::registerTest ("t_dusg_ssg", &run);

} // namespace t_dusg_ssg
