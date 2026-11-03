// Serge Animal TKB v2 — TKB/KEY gauntlet tests: T12 (stage advance/wrap,
// last-touched reset, random select, vertical clock/ABCD, HOLD, UP/DOWN,
// pad gate+pressure, semitone KV + GLIDE, one-hot stage pulses) plus the KEY
// GLIDE piece of the T15 MIDI contract. Numbers verbatim from
// gauntlet/GAUNTLET.md T12. Inputs are driven through KEY's plugin-written
// fields (gateV/clkV/pitchV) patched onto TKB's red inputs — KEY registers
// AFTER TKB, so TKB sees each drive one sample late (frozen §2 feedback rule);
// all pulse widths below are >> 1 sample.
//
// Render evidence (SATK_RENDER=1): 10 s TKB sequence WAV, internal clock
// ~4 Hz, row A melody, gates audible via a local 1 V/oct sine audition osc
// (self-contained test tooling — the v1 manifest has no TKB-specific render;
// this is the v2 TKB evidence file). Written to
// gauntlet/renders/v2work/tkb-key/ relative to the working directory.
#pragma once
#include "Harness.h"
#include <cstdlib>
#include <filesystem>
#include <string>

namespace t_tkb_key {

using namespace gauntlet;
using namespace satk;

// ---- helpers ---------------------------------------------------------------

// index of the single high one-hot stage out (-1 if none/multiple)
inline int activeStage (TKB* t, bool* oneHot = nullptr)
{
    int idx = -1, count = 0;
    for (int i = 0; i < 16; ++i)
        if (t->jacks[(size_t) (TKB::jStage1 + i)].value > kLogicHi) { idx = i; ++count; }
    if (oneHot != nullptr) *oneHot = count == 1;
    return count == 1 ? idx : -1;
}

// drive `field` (a KEY plugin field) high for `hi` samples then low for `lo`
inline void pulseField (Rig& r, float& field, int hi = 12, int lo = 12)
{
    field = kCV; r.step (hi);
    field = 0.0f; r.step (lo);
}

// ---- T12 subtests ----------------------------------------------------------

inline void testAdvanceWrapOneHot()
{
    Rig r; auto* t = r.sys.tkb; auto* k = r.sys.key;
    k->velV = 0.0f;
    r.sys.patch (k, KEY::jGate, t, TKB::jClock);
    t->ctls[TKB::cLength].v = 7.0f; // "8"
    r.step (64);

    bool seqOk = true, oneHotOk = true;
    for (int p = 1; p <= 20; ++p)
    {
        // 12 high / 12 low, checking one-hot on every sample
        for (int i = 0; i < 24; ++i)
        {
            k->gateV = i < 12 ? kCV : 0.0f;
            r.sys.engine.tick();
            bool oh = false; activeStage (t, &oh);
            if (! oh) oneHotOk = false;
        }
        if (activeStage (t) != p % 8) seqOk = false;
    }
    CHECK (seqOk, "T12: clock advances stages 1->LENGTH and wraps (LENGTH=8, v1-ism)");
    CHECK (oneHotOk, "T12: per-stage pulse outs one-hot (exactly one ST high)");
}

inline void testResetLastTouched()
{
    Rig r; auto* t = r.sys.tkb; auto* k = r.sys.key;
    k->velV = 0.0f;
    r.sys.patch (k, KEY::jGate, t, TKB::jClock);
    r.sys.patch (k, KEY::jClk,  t, TKB::jReset);
    t->ctls[TKB::cLength].v = 7.0f;
    r.step (64);

    // touch pad 5 (idx 4) -> selects stage 5 and records "last touched"
    t->ctls[TKB::cPad1 + 4].v = 1.0f; r.step (20);
    t->ctls[TKB::cPad1 + 4].v = 0.0f; r.step (20);
    bool padSel = activeStage (t) == 4;

    for (int p = 0; p < 3; ++p) pulseField (r, k->gateV); // clock away from it
    bool moved = activeStage (t) == 7;

    pulseField (r, k->clkV); // RESET
    CHECK (padSel && moved && activeStage (t) == 4,
           "T12: RESET jumps to the last-touched stage (not stage 1)",
           activeStage (t), 4);
}

inline void testRandomSelect()
{
    auto sequence = [] (uint64_t seed, bool reseed, int* out)
    {
        Rig r; auto* t = r.sys.tkb; auto* k = r.sys.key;
        k->velV = 0.0f;
        r.sys.patch (k, KEY::jGate, t, TKB::jClock);
        if (reseed) { r.sys.engine.setSeed (seed); r.sys.prepare (SR); }
        t->ctls[TKB::cLength].v = 7.0f; // "8"
        t->ctls[TKB::cRandom].v = 1.0f; // armed
        r.step (64);
        for (int p = 0; p < 32; ++p)
        {
            pulseField (r, k->gateV);
            out[p] = activeStage (t);
        }
    };

    int a[32], b[32], c[32];
    sequence (0, false, a);                     // default engine seed
    sequence (0, false, b);                     // same seed -> identical
    sequence (0xBEEF2026ULL, true, c);          // different seed -> different

    bool inLen = true, deterministic = true, jumps = false, differs = false;
    int distinct = 0; bool seen[8] = {};
    for (int i = 0; i < 32; ++i)
    {
        if (a[i] < 0 || a[i] > 7) inLen = false;
        else if (! seen[a[i]]) { seen[a[i]] = true; ++distinct; }
        if (a[i] != b[i]) deterministic = false;
        if (a[i] != c[i]) differs = true;
        if (i > 0 && a[i] != (a[i - 1] + 1) % 8) jumps = true;
    }
    CHECK (inLen && distinct >= 3 && jumps,
           "T12: RANDOM armed + pulse -> random stage within LENGTH", distinct);
    CHECK (deterministic && differs,
           "T12: random select engine-seed deterministic (same seed = same walk)");
}

inline void testVerticalClockAbcd()
{
    Rig r; auto* t = r.sys.tkb; auto* k = r.sys.key;
    k->velV = 0.0f;
    r.sys.patch (k, KEY::jGate, t, TKB::jVertClock);
    r.sys.patch (k, KEY::jClk,  t, TKB::jVertReset);
    // distinct row values at stage 1; RANGE default 2 oct -> volts = knob*2
    t->ctls[TKB::cRowA1].v = 0.1f;
    t->ctls[TKB::cRowB1].v = 0.3f;
    t->ctls[TKB::cRowC1].v = 0.6f;
    t->ctls[TKB::cRowD1].v = 0.9f;
    r.step (200);

    const double expect[4] = { 0.2, 0.6, 1.2, 1.8 }; // ± ladder tolerance
    auto near = [] (double v, double e) { return std::fabs (v - e) < 0.02; };

    bool cycleOk = near (t->jacks[TKB::jAbcd].value, expect[0]); // row A
    bool aFixed  = near (t->jacks[TKB::jA].value, expect[0]);
    for (int p = 1; p <= 5; ++p) // B, C, D, wrap A, B (64-step trick)
    {
        pulseField (r, k->gateV, 12, 60);
        if (! near (t->jacks[TKB::jAbcd].value, expect[p % 4])) cycleOk = false;
        if (! near (t->jacks[TKB::jA].value, expect[0])) aFixed = false;
    }
    CHECK (cycleOk, "T12: vertical clock cycles rows A->B->C->D on ABCD (64-step)",
           t->jacks[TKB::jAbcd].value);
    CHECK (aFixed, "T12: A out stays row A while vertical sequencer runs");

    pulseField (r, k->clkV, 12, 60); // VERT RESET -> row A
    CHECK (near (t->jacks[TKB::jAbcd].value, expect[0]),
           "T12: VERT RESET returns the vertical sequencer to row A",
           t->jacks[TKB::jAbcd].value, expect[0]);
}

inline void testHoldFreeze()
{
    Rig r; auto* t = r.sys.tkb; auto* k = r.sys.key;
    k->velV = 0.0f;
    r.sys.patch (k, KEY::jGate, t, TKB::jClock);
    r.sys.patch (k, KEY::jClk,  t, TKB::jHold);
    t->ctls[TKB::cLength].v = 7.0f;
    r.step (64);

    pulseField (r, k->gateV); pulseField (r, k->gateV); // -> stage 3 (idx 2)
    bool pre = activeStage (t) == 2;

    k->clkV = kCV; r.step (12);                          // HOLD high
    for (int p = 0; p < 5; ++p) pulseField (r, k->gateV);
    bool frozen = activeStage (t) == 2;

    k->clkV = 0.0f; r.step (12);                         // release
    pulseField (r, k->gateV);
    CHECK (pre && frozen && activeStage (t) == 3,
           "T12: HOLD freezes (clocks ignored while high, resume on release)",
           activeStage (t), 3);
}

inline void testUpDownReverse()
{
    Rig r; auto* t = r.sys.tkb; auto* k = r.sys.key;
    k->velV = 0.0f;
    r.sys.patch (k, KEY::jGate, t, TKB::jClock);
    r.sys.patch (k, KEY::jClk,  t, TKB::jUpDown);
    t->ctls[TKB::cLength].v = 7.0f;
    r.step (64);

    k->clkV = kCV; r.step (12); // UP/DOWN high = step downward
    pulseField (r, k->gateV);
    bool wrap = activeStage (t) == 7; // wrap 1 -> LENGTH
    pulseField (r, k->gateV);
    CHECK (wrap && activeStage (t) == 6,
           "T12: UP/DOWN reverses (high = step down, wrap 1->LENGTH)",
           activeStage (t), 6);
}

inline void testPadTouch()
{
    Rig r; auto* t = r.sys.tkb;
    t->ctls[TKB::cPressure].v = 0.8f;
    r.step (64);
    bool idle = t->jacks[TKB::jKp].value == 0.0f
             && t->jacks[TKB::jPressure].value == 0.0f;

    t->ctls[TKB::cPad1 + 10].v = 1.0f; // touch pad 11
    r.step (10);
    bool kpHigh = t->jacks[TKB::jKp].value > kLogicHi;       // gate (contact bounce <= 2 smp)
    bool gateFired = t->jacks[TKB::jGate].value > kLogicHi;  // step gate on select
    bool selected = activeStage (t) == 10;                   // selects its stage
    r.step (500);                                             // ~10 ms finger-contact slew
    const double press = t->jacks[TKB::jPressure].value;
    bool pressOk = std::fabs (press - 0.8 * kCV) < 0.05;      // PRESSURE ctl x 5 V

    t->ctls[TKB::cPad1 + 10].v = 0.0f; // release
    r.step (500);
    bool released = t->jacks[TKB::jKp].value == 0.0f
                 && t->jacks[TKB::jPressure].value < 0.05f;

    CHECK (idle && kpHigh && pressOk && released,
           "T12: pad touch (ctl) emits gate (KP) + pressure CV (0..5 V)", press, 4.0);
    CHECK (selected && gateFired,
           "T12: pad touch selects its stage (+ step GATE fires)");
}

inline void testKvSemitoneDistinct()
{
    Rig r; auto* t = r.sys.tkb;
    r.step (64);

    t->ctls[TKB::cRowA1 + 7].v = 0.9f; // row-A stage-8 knob -> A out 1.8 V
    t->ctls[TKB::cPad1 + 7].v = 1.0f;  // touch pad 8
    r.step (200);
    t->ctls[TKB::cPad1 + 7].v = 0.0f;
    r.step (200);

    const double kv = t->jacks[TKB::jKv].value;
    const double a  = t->jacks[TKB::jA].value;
    bool semis = std::fabs (kv - 7.0 / 12.0) < 1.0e-4; // (pad-1)/12 V, calibrated
    bool distinct = std::fabs (kv - a) > 0.5;          // != row-A stage CV (1.8 V here)

    t->ctls[TKB::cPad1 + 0].v = 1.0f; // pad 1 -> 0.000 V
    r.step (200);
    t->ctls[TKB::cPad1 + 0].v = 0.0f;
    r.step (200);
    bool zero = std::fabs (t->jacks[TKB::jKv].value) < 1.0e-6;

    CHECK (semis && zero,
           "T12: KV = last-touched pad, semitone calibrated ((pad-1)/12 V)", kv, 7.0 / 12.0);
    CHECK (distinct, "T12: KV distinct from row-A stage CV", kv, a);
}

inline void testKvGlide()
{
    Rig r; auto* t = r.sys.tkb;
    t->ctls[TKB::cGlide].v = 0.5f; // tau = 0.5 s one-pole slew
    r.step (64);

    t->ctls[TKB::cPad1 + 12].v = 1.0f; // pad 13 -> target 1.000 V from 0 V
    bool monotonic = true; float prev = 0.0f;
    for (int i = 0; i < 24000; ++i) // exactly tau seconds
    {
        r.sys.engine.tick();
        const float v = t->jacks[TKB::jKv].value;
        if (v < prev - 1.0e-6f) monotonic = false;
        prev = v;
    }
    const double atTau = prev; // expect 1 - e^-1 = 0.632
    r.step (48000 * 4);
    const double settled = t->jacks[TKB::jKv].value;
    t->ctls[TKB::cPad1 + 12].v = 0.0f;

    CHECK (monotonic && std::fabs (atTau - 0.632) < 0.02,
           "T12: KV GLIDE slew (one-pole, tau = GLIDE ctl seconds)", atTau, 0.632);
    CHECK (std::fabs (settled - 1.0) < 0.003,
           "T12: KV GLIDE settles on the semitone target", settled, 1.0);
}

inline void testInternalClockAndGateLen()
{
    // internal clock: RUN + RATE (expo 0.1..30 Hz), GATE LEN gate per advance
    Rig r; auto* t = r.sys.tkb;
    t->ctls[TKB::cRun].v = 1.0f;
    t->ctls[TKB::cRate].v = 0.646742f; // -> 4.000 Hz
    const double rate = edgeRate (r, t, TKB::jGate, (int) (SR * 5.0));
    CHECK (std::fabs (rate - 4.0) < 0.12,
           "T12: internal clock RUN/RATE (expo 0.1..30 Hz) at ~4 Hz", rate, 4.0);

    // default GATE LEN 100 ms: measure one gate-high run length
    Rig r2; auto* t2 = r2.sys.tkb;
    t2->ctls[TKB::cRun].v = 1.0f; // default RATE 2 Hz, gate duty 0.2
    int runLen = 0; bool prev = false; bool measured = false;
    for (int i = 0; i < (int) SR * 2 && ! measured; ++i)
    {
        r2.sys.engine.tick();
        const bool h = t2->jacks[TKB::jGate].value > kLogicHi;
        if (h) ++runLen;
        if (! h && prev) measured = true;
        prev = h;
    }
    CHECK (measured && std::fabs (runLen / SR - 0.1) < 0.002,
           "T12: GATE emits GATE LEN gate on each step advance (100 ms default)",
           runLen / SR, 0.1);
}

// ---- KEY: GLIDE slew (T15 MIDI-contract piece owned by this builder) --------

inline void testKeyGlide()
{
    // glide 0 -> v1 law tau 0.1 ms: settles within ~2 ms
    Rig r; auto* k = r.sys.key;
    k->pitchV = 0.25f; k->gateV = kCV;
    r.step (96); // 2 ms
    bool fastOk = std::fabs (k->jacks[KEY::jKv].value - 0.25f) < 1.0e-3;
    CHECK (fastOk, "KEY: GLIDE 0 -> KV follows pitch (v1 tau 0.1 ms, < 2 ms settle)",
           k->jacks[KEY::jKv].value, 0.25);

    // glide 1.0 -> tau 0.5 s one-pole portamento (v1 law, ctl x 0.5 s)
    Rig r2; auto* k2 = r2.sys.key;
    k2->ctls[KEY::cGlide].v = 1.0f;
    k2->pitchV = 0.0f; r2.step (64);
    k2->pitchV = 1.0f;
    bool monotonic = true; float prevV = 0.0f;
    for (int i = 0; i < 24000; ++i)
    {
        r2.sys.engine.tick();
        const float v = k2->jacks[KEY::jKv].value;
        if (v < prevV - 1.0e-6f) monotonic = false;
        prevV = v;
    }
    CHECK (monotonic && std::fabs (prevV - 0.632f) < 0.02,
           "KEY: GLIDE one-pole portamento on KV (tau = ctl x 0.5 s)", prevV, 0.632);

    // pass-through contract unchanged: gate/vel/clk verbatim, LED follows gate
    bool pass = k2->jacks[KEY::jGate].value == k2->gateV
             && k2->jacks[KEY::jVel].value == k2->velV
             && k2->jacks[KEY::jClk].value == k2->clkV;
    CHECK (pass, "KEY: GATE/VEL/CLK fields pass through verbatim");
}

// ---- perf (T16 protocol on bare per-module engines; spike table SPEC §10) ---

inline double perfBareCycles (satk::Engine& e, double seconds = 10.0, int runs = 5)
{
    const long n = (long) (SR * seconds);
    double best = 1e30;
    for (int run = 0; run < runs; ++run)
    {
        auto t0 = std::chrono::steady_clock::now();
        for (long i = 0; i < n; ++i) e.tick();
        const double dt = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        best = std::min (best, dt);
    }
    return best * kAssumedPCoreHz / (double) n;
}

inline double gMeasuredTkbCycles = 0.0, gMeasuredKeyCycles = 0.0;

inline void testPerf()
{
    satk::Engine et;
    auto* t = et.add<TKB> ("tkb", "TOUCH ACTIVATED KEYBOARD SEQUENCER");
    et.prepare (SR);
    t->ctls[TKB::cRun].v = 1.0f;
    t->ctls[TKB::cRate].v = 1.0f;      // 30 Hz — worst-case stepping
    t->ctls[TKB::cGlide].v = 0.3f;
    t->ctls[TKB::cPad1 + 2].v = 1.0f;  // pad held: KP/pressure paths active
    gMeasuredTkbCycles = perfBareCycles (et);

    satk::Engine ek;
    auto* k = ek.add<KEY> ("key", "KEY / MIDI");
    ek.prepare (SR);
    k->ctls[KEY::cGlide].v = 0.5f;
    k->pitchV = 0.3f; k->gateV = kCV;
    gMeasuredKeyCycles = perfBareCycles (ek);

    std::printf ("  perf (bare module, min of 5 x 10 s): TKB %.1f cyc/smp (spike 55),"
                 " KEY %.1f cyc/smp (spike 16)\n", gMeasuredTkbCycles, gMeasuredKeyCycles);
    CHECK (gMeasuredTkbCycles <= 165.0,
           "perf: TKB within 3x its SPEC.md S10 spike figure (55 -> 165 cyc/smp)",
           gMeasuredTkbCycles, 165.0);
    CHECK (gMeasuredKeyCycles <= 48.0,
           "perf: KEY within 3x its SPEC.md S10 spike figure (16 -> 48 cyc/smp)",
           gMeasuredKeyCycles, 48.0);
}

// ---- render evidence (SATK_RENDER=1): 10 s TKB sequence WAV -----------------

inline bool writeWav16 (const std::string& path,
                        const std::vector<float>& l, const std::vector<float>& r)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (f == nullptr) return false;
    const uint32_t n = (uint32_t) l.size();
    const uint32_t dataBytes = n * 4, sr = 48000, byteRate = sr * 4;
    const uint16_t fmt = 1, ch = 2, block = 4, bits = 16;
    const uint32_t riffLen = 36 + dataBytes, fmtLen = 16;
    std::fwrite ("RIFF", 1, 4, f); std::fwrite (&riffLen, 4, 1, f);
    std::fwrite ("WAVE", 1, 4, f);
    std::fwrite ("fmt ", 1, 4, f); std::fwrite (&fmtLen, 4, 1, f);
    std::fwrite (&fmt, 2, 1, f);   std::fwrite (&ch, 2, 1, f);
    std::fwrite (&sr, 4, 1, f);    std::fwrite (&byteRate, 4, 1, f);
    std::fwrite (&block, 2, 1, f); std::fwrite (&bits, 2, 1, f);
    std::fwrite ("data", 1, 4, f); std::fwrite (&dataBytes, 4, 1, f);
    for (uint32_t i = 0; i < n; ++i)
    {
        const auto conv = [] (float v)
        { return (int16_t) std::lround (std::clamp (v, -1.0f, 1.0f) * 32767.0f); };
        int16_t s[2] = { conv (l[i]), conv (r[i]) };
        std::fwrite (s, 2, 2, f);
    }
    std::fclose (f);
    return true;
}

inline void maybeRender()
{
    const char* env = std::getenv ("SATK_RENDER");
    if (env == nullptr || env[0] != '1') return;

    GROUP ("TKB render evidence (SATK_RENDER=1)");
    Rig r; auto* t = r.sys.tkb; // default engine seed — deterministic
    t->ctls[TKB::cRun].v = 1.0f;
    t->ctls[TKB::cRate].v = 0.646742f; // internal clock ~4 Hz
    t->ctls[TKB::cLength].v = 7.0f;    // 8 stages
    // row A melody in semitones (RANGE default 2 oct: knob = st/24 -> st/12 V)
    const int st[8] = { 0, 3, 7, 10, 12, 7, 3, 5 };
    for (int i = 0; i < 8; ++i)
        t->ctls[TKB::cRowA1 + i].v = (float) st[i] / 24.0f;

    // audition patch (self-contained test tooling): sine osc tracking TKB A at
    // 1 V/oct from C4, amplitude keyed by TKB GATE (3 ms lag) — "row A melody,
    // gates audible".
    const int n = (int) SR * 10;
    std::vector<float> outL ((size_t) n), outR ((size_t) n);
    double ph = 0.0; float amp = 0.0f;
    const float aAmp = std::exp (-1.0f / ((float) SR * 3.0e-3f));
    float peak = 0.0f; double rms = 0.0;
    for (int i = 0; i < n; ++i)
    {
        r.sys.engine.tick();
        const float aV   = t->jacks[TKB::jA].value;
        const float gate = t->jacks[TKB::jGate].value > kLogicHi ? 0.35f : 0.0f;
        const double f = 261.63 * std::pow (2.0, (double) aV);
        ph += f / SR; if (ph >= 1.0) ph -= 1.0;
        amp = gate + (amp - gate) * aAmp;
        const float s = (float) std::sin (2.0 * 3.14159265358979 * ph) * amp;
        outL[(size_t) i] = outR[(size_t) i] = s;
        peak = std::max (peak, std::fabs (s)); rms += (double) s * s;
    }
    rms = std::sqrt (rms / n);

    std::error_code ec;
    std::filesystem::create_directories ("gauntlet/renders/v2work/tkb-key", ec);
    const std::string path = "gauntlet/renders/v2work/tkb-key/tkb_sequence.wav";
    const bool ok = ! ec && writeWav16 (path, outL, outR);
    std::printf ("  %s %s (10 s, 48 kHz, peak %.3f, rms %.4f)\n",
                 ok ? "wrote" : "FAILED to write", path.c_str(), peak, rms);
    CHECK (ok && peak > 0.1f && rms > 0.01, "render: tkb_sequence.wav written, non-silent",
           peak, rms);
}

// ---- runner ------------------------------------------------------------------

inline void run()
{
    GROUP ("T12 TKB: sequencer / pads / KV");
    testAdvanceWrapOneHot();
    testResetLastTouched();
    testRandomSelect();
    testVerticalClockAbcd();
    testHoldFreeze();
    testUpDownReverse();
    testPadTouch();
    testKvSemitoneDistinct();
    testKvGlide();
    testInternalClockAndGateLen();

    GROUP ("KEY: GLIDE (T15 MIDI-contract piece)");
    testKeyGlide();

    GROUP ("TKB/KEY perf (T16 protocol, bare module)");
    testPerf();

    maybeRender();
}

inline const bool registered = gauntlet::registerTest ("t_tkb_key", &run);

} // namespace t_tkb_key
