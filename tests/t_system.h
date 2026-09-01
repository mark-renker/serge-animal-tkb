// Serge Animal TKB v2 — system-level gauntlet tests, live at foundation:
//   T00 — DspUtils sanity (the frozen primitives themselves)
//   T01 — silence / finiteness / denormal-free master path
//   T13 — feedback-stress scaffolding (full pathological patch lands with the
//         stage-2 modules; the finite-within-rails checker is the reusable part)
//   T14 — engine state round-trip incl. instance seed (bit-identical renders)
//   T16 — perf measurement protocol (min-of-5 runs, cycles/sample) + stub-cost
//         perf spike baseline
#pragma once
#include "Harness.h"
#include <cstring>

namespace t_system {

using namespace gauntlet;
using namespace satk;

// ---- T13 reusable checker ---------------------------------------------------
// Ticks the rig for `seconds` and verifies every jack stays finite and within
// the ±12 V rails (+ small epsilon). Returns peak |V| seen via `peakOut`.
inline bool allFiniteWithinRails (Rig& r, double seconds, float& peakOut)
{
    bool ok = true; float peak = 0.0f;
    const int n = (int) (SR * seconds);
    for (int i = 0; i < n; ++i)
    {
        r.sys.engine.tick();
        if ((i & 255) == 0)
            for (auto& m : r.sys.engine.mods)
                for (auto& j : m->jacks)
                {
                    if (! std::isfinite (j.value) || std::fabs (j.value) > kRails + 0.01f) ok = false;
                    peak = std::max (peak, std::fabs (j.value));
                }
        if (! ok) break;
    }
    peakOut = peak;
    return ok;
}

inline void runT00()
{
    GROUP ("T00 foundation: DspUtils sanity (frozen primitives)");

    // SplitMix64: reference vector (seed 0 -> 0xe220a8397b1dcdaf) + determinism
    {
        uint64_t s = 0;
        bool refOk = dsp::splitmix64 (s) == 0xe220a8397b1dcdafULL;
        dsp::SplitMix64 a (42), b (42), c (43);
        bool det = a.next() == b.next() && a.next() == b.next();
        bool diff = dsp::SplitMix64 (42).next() != c.next();
        CHECK (refOk && det && diff, "SplitMix64 reference vector + determinism");
    }
    // PCG32: determinism across instances, distinct streams, float range
    {
        dsp::PCG32 a, b, c;
        a.seed (7, 1); b.seed (7, 1); c.seed (7, 2);
        bool det = a.next() == b.next() && a.next() == b.next();
        bool streams = false;
        for (int i = 0; i < 8; ++i) if (a.next() != c.next()) streams = true;
        bool range = true;
        for (int i = 0; i < 10000; ++i)
        { float f = a.nextFloat01(); if (f < 0.0f || f >= 1.0f) range = false; }
        CHECK (det && streams && range, "PCG32 determinism, streams, [0,1) range");
    }
    // fastTanh: accuracy + hard bound
    {
        double maxErr = 0; bool bounded = true;
        for (int i = 0; i <= 3200; ++i)
        {
            float x = -8.0f + (float) i * 0.005f;
            float y = dsp::fastTanh (x);
            maxErr = std::max (maxErr, (double) std::fabs (y - std::tanh (x)));
            if (std::fabs (y) >= 1.0f) bounded = false;
        }
        CHECK (maxErr < 1e-4 && bounded, "fastTanh: |err| < 1e-4, |out| < 1", maxErr);
    }
    // Ornstein-Uhlenbeck: bounded, mean-reverting, stationary std ~ sigma
    {
        dsp::PCG32 rng; rng.seed (99, 5);
        dsp::OUDrift ou; ou.configure (0.5f, 1.0f, 1000.0f);
        double acc = 0, acc2 = 0; float peak = 0; const int n = 200000;
        for (int i = 0; i < n; ++i)
        {
            float v = ou.tick (rng);
            acc += v; acc2 += (double) v * v;
            peak = std::max (peak, std::fabs (v));
        }
        double mean = acc / n, sd = std::sqrt (acc2 / n - mean * mean);
        CHECK (peak < 6.0f && std::fabs (mean) < 0.15 && sd > 0.7 && sd < 1.3,
               "OU drift bounded, mean-reverting, std ~ sigma", sd, peak);
    }
    // Halfband 2x island: DC gain 1, 1 kHz tone amplitude preserved
    {
        dsp::Oversampler2x os; os.prepare();
        float y = 0;
        for (int i = 0; i < 2000; ++i) y = os.process (1.0f, [] (float v) { return v; });
        bool dcOk = std::fabs (y - 1.0f) < 1e-3f;

        dsp::Oversampler2x os2; os2.prepare();
        Goertzel g; g.init (1000.0);
        for (int i = 0; i < 9600; ++i)
        {
            float x = std::sin (2.0f * 3.14159265f * 1000.0f * (float) i / (float) SR);
            float v = os2.process (x, [] (float w) { return w; });
            if (i >= 1600) g.push (v);
        }
        bool toneOk = std::fabs (g.amp() - 1.0) < 0.012; // within ~0.1 dB
        CHECK (dcOk && toneOk, "halfband 2x: DC exact, 1 kHz within 0.1 dB", g.amp());
    }
    // polyBLEP: band-limited 1975 Hz saw — strongest folded alias in 0-6 kHz
    // well under the naive saw's (utility sanity; the full -60 dBc floor is T03)
    {
        auto sawAliases = [] (bool blep)
        {
            const double f0 = 1975.0;
            const float dt = (float) (f0 / SR);
            float t = 0.0f;
            const int n = 24000;
            std::vector<float> buf ((size_t) n);
            for (int i = 0; i < n; ++i)
            {
                float s = 2.0f * t - 1.0f;
                if (blep) s -= dsp::polyBLEP (t, dt);
                buf[(size_t) i] = s;
                t += dt; if (t >= 1.0f) t -= 1.0f;
            }
            Goertzel gf; gf.init (f0);
            for (auto v : buf) gf.push (v);
            double fund = gf.amp(), worst = 0;
            for (int k = 13; k <= 60; ++k)
            {
                double f = k * f0;
                double fa = std::fabs (f - SR * std::round (f / SR));
                if (fa < 100.0 || fa > 6000.0) continue;
                // skip bins that coincide with true harmonics (±20 Hz)
                double h = std::round (fa / f0);
                if (std::fabs (fa - h * f0) < 20.0) continue;
                Goertzel ga; ga.init (fa);
                for (auto v : buf) ga.push (v);
                worst = std::max (worst, ga.amp());
            }
            return db (worst, fund);
        };
        double naive = sawAliases (false), blepped = sawAliases (true);
        CHECK (blepped < -40.0 && blepped < naive - 15.0,
               "polyBLEP saw: aliases < -40 dBc and >15 dB under naive", blepped, naive);
    }
    // polyBLAMP kernel: zero outside the 2-sample window, continuous, bounded
    {
        const float dt = 0.01f;
        bool zeroMid = dsp::polyBLAMP (0.5f, dt) == 0.0f;
        bool small = std::fabs (dsp::polyBLAMP (dt * 0.999f, dt)) < 1e-6f
                  && std::fabs (dsp::polyBLAMP (1.0f - dt * 0.999f, dt)) < 1e-6f;
        bool peakOk = std::fabs (dsp::polyBLAMP (0.0f, dt)) <= 0.3334f
                   && std::fabs (dsp::polyBLAMP (0.9999f, dt)) <= 0.3334f;
        CHECK (zeroMid && small && peakOk, "polyBLAMP kernel: support, continuity, bound");
    }
    // ADAA1 (tanh) static accuracy on a slow ramp; ADAA2 (hard clip) bounded
    {
        dsp::ADAA1<dsp::TanhKernel> adaa1;
        adaa1.reset (-3.0f);
        double maxErr = 0; float px = -3.0f;
        for (int i = 1; i <= 10000; ++i)
        {
            float x = -3.0f + 6.0f * (float) i / 10000.0f;
            float y = adaa1.process (x);
            maxErr = std::max (maxErr, (double) std::fabs (y - std::tanh (0.5f * (x + px))));
            px = x;
        }
        dsp::ADAA2<dsp::HardClipKernel> adaa2;
        adaa2.kernel.c = 1.0f;
        bool ok2 = true;
        for (int i = 0; i < 20000; ++i)
        {
            float x = 2.5f * std::sin (2.0f * 3.14159265f * 2000.0f * (float) i / (float) SR);
            float y = adaa2.process (x);
            if (! std::isfinite (y) || std::fabs (y) > 1.0001f) ok2 = false;
        }
        CHECK (maxErr < 1e-3 && ok2, "ADAA1 tanh accurate; ADAA2 clip bounded", maxErr);
    }
}

inline void runT01()
{
    GROUP ("T01 silence");
    Rig r; r.step (2000);
    bool zero = true, fin = true, denorm = false;
    for (int i = 0; i < 1000; ++i)
    {
        r.sys.engine.tick();
        if (r.sys.out->outL != 0.0f || r.sys.out->outR != 0.0f) zero = false;
        for (auto& m : r.sys.engine.mods)
            for (auto& j : m->jacks)
            {
                if (! std::isfinite (j.value)) fin = false;
                float av = std::fabs (j.value);
                if (av > 0.0f && av < 1e-30f) denorm = true;
            }
    }
    CHECK (zero, "unpatched system renders exact 0.0 output");
    CHECK (fin, "all jack values finite");
    CHECK (! denorm, "no denormals on any jack (master path denormal-free)");
}

inline void runT13Scaffold()
{
    GROUP ("T13 feedback stress (scaffolding — full patch lands with modules)");
    Rig r; auto& s = r.sys;
    // cross-module loops over the frozen jack map (stubs today, hot later)
    s.patch (s.dusg1, DUSG::jEnd,     s.dusg1, DUSG::jTrig);
    s.patch (s.ssg,   SSG::jCoupler,  s.ssg,   SSG::jSmIn);
    s.patch (s.nto,   NTO::jVar,      s.vcm,   VCM::jMidIn1);
    s.patch (s.vcm,   VCM::jMidOut,   s.nto,   NTO::jLinFm);
    s.patch (s.vcfq,  VCFQ::jBp,      s.vcfq,  VCFQ::jVcF);
    s.patch (s.ring,  RING::jOut,     s.vcfq,  VCFQ::jIn);
    s.patch (s.vcfq,  VCFQ::jLp,      s.ring,  RING::jX);
    float peak = 0.0f;
    bool ok = allFiniteWithinRails (r, 1.0, peak);
    CHECK (ok, "feedback patch: finite, within rails (stub engine)", peak);
}

inline void runT14()
{
    GROUP ("T14 engine state round-trip (ctls + cables + instance seed)");
    const uint64_t kSeed = 0x1234ABCD5678EF90ULL;

    // author a state on a scratch rig: default cables + extras + random ctls
    Rig a;
    a.sys.patchDefaults();
    a.sys.patch (a.sys.ssg, SSG::jCoupler, a.sys.ssg, SSG::jSmIn);
    a.sys.engine.setSeed (kSeed);
    {
        dsp::PCG32 rng; rng.seed (2026, 1);
        for (auto& m : a.sys.engine.mods)
            for (auto& c : m->ctls)
                c.v = c.steps > 0 ? (float) (rng.next() % (uint32_t) c.steps)
                                  : rng.nextFloat01();
    }
    EngineBlob blob = saveBlob (a.sys);

    // two fresh engines from the same blob
    Rig b, c;
    loadBlob (b.sys, blob);
    loadBlob (c.sys, blob);

    bool seedOk = b.sys.engine.seed() == kSeed && c.sys.engine.seed() == kSeed;
    CHECK (seedOk, "instance seed survives the round-trip");

    bool modSeedsMatch = true, modSeedsDistinct = false;
    for (size_t i = 0; i < b.sys.engine.mods.size(); ++i)
    {
        if (b.sys.engine.mods[i]->moduleSeed != c.sys.engine.mods[i]->moduleSeed)
            modSeedsMatch = false;
        if (i > 0 && b.sys.engine.mods[i]->moduleSeed != b.sys.engine.mods[0]->moduleSeed)
            modSeedsDistinct = true;
    }
    CHECK (modSeedsMatch && modSeedsDistinct,
           "per-module seeds derived identically, distinct per module");

    bool ctlsOk = true;
    {
        size_t i = 0;
        for (auto& m : b.sys.engine.mods)
            for (auto& ct : m->ctls)
                if (ct.v != blob.ctls[i++]) ctlsOk = false;
    }
    CHECK (ctlsOk && b.sys.engine.cables.size() == blob.cables.size(),
           "ctls + cables restored exactly",
           (double) b.sys.engine.cables.size(), (double) blob.cables.size());

    bool same = true;
    for (int i = 0; i < 512; ++i)
    {
        const float gate = i > 5 ? kCV : 0.0f;
        b.sys.key->gateV = c.sys.key->gateV = gate;
        b.sys.key->pitchV = c.sys.key->pitchV = 0.25f;
        b.sys.engine.tick(); c.sys.engine.tick();
        if (std::memcmp (&b.sys.out->outL, &c.sys.out->outL, sizeof (float)) != 0
         || std::memcmp (&b.sys.out->outR, &c.sys.out->outR, sizeof (float)) != 0)
        { same = false; break; }
    }
    CHECK (same, "two fresh engines render bit-identical first 512 samples");

    // different seed must produce a different per-module seed derivation
    Rig d; loadBlob (d.sys, blob);
    d.sys.engine.setSeed (kSeed + 1);
    bool derivDiffers = d.sys.engine.mods[0]->moduleSeed != b.sys.engine.mods[0]->moduleSeed;
    CHECK (derivDiffers, "different instance seed -> different module seeds");
}

inline void runT16()
{
    GROUP ("T16 perf protocol (min-of-5 x 10 s; STUB-COST perf spike baseline)");
    Rig r;
    r.sys.patchDefaults();
    r.sys.key->gateV = kCV; // everything ticking
    PerfResult p = measurePerf (r.sys, 10.0, 5);
    std::printf ("  perf spike (stub engine): %.1fx realtime, %.1f cycles/sample "
                 "(assumed %.3f GHz P-core)\n",
                 p.realtimeRatio, p.cyclesPerSample, kAssumedPCoreHz / 1e9);
    CHECK (p.realtimeRatio >= 6.0, ">= 6x realtime single-thread (stub baseline)",
           p.realtimeRatio);
}

inline void run()
{
    runT00();
    runT01();
    runT13Scaffold();
    runT14();
    runT16();
}

inline const bool registered = gauntlet::registerTest ("t_system", &run);

} // namespace t_system
