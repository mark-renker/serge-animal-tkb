// Serge Animal TKB v2 — system-level gauntlet tests, live at foundation:
//   T00 — DspUtils sanity (the frozen primitives themselves)
//   T01 — silence / finiteness / denormal-free master path
//   T13 — feedback-stress scaffolding (full pathological patch lands with the
//         stage-2 modules; the finite-within-rails checker is the reusable part)
//   T14 — engine state round-trip incl. instance seed (bit-identical renders)
//   T15 — default patch & monitor contract on the REAL modules (full voice
//         KEY -> DUSG1 -> NTO var -> VCM mid -> VCFQ LP -> VCA1 -> OUT)
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
    // Low-latency IIR halfband pair (amendment 2026-11-02): unity DC, flat
    // passband, image rejection on upsampling, round-trip latency < 5 samples
    {
        auto roundTrip = [] (double f, int n, int from)
        {
            dsp::IirUpsampler2x up; dsp::IirDownsampler2x dn; up.prepare(); dn.prepare();
            Goertzel g; g.init (f);
            for (int i = 0; i < n; ++i)
            {
                float a, b;
                up.process ((float) std::sin (2.0 * 3.14159265358979 * f * i / SR), a, b);
                const float y = dn.process (a, b);
                if (i >= from) g.push (y);
            }
            return g.amp();
        };
        dsp::IirUpsampler2x up; dsp::IirDownsampler2x dn; up.prepare(); dn.prepare();
        float y = 0.0f;
        for (int i = 0; i < 4000; ++i) { float a, b; up.process (1.0f, a, b); y = dn.process (a, b); }
        const bool dcOk = std::fabs (y - 1.0f) < 1e-4f;
        const double a1k = roundTrip (1000.0, 9600, 1600), a18k = roundTrip (18000.0, 9600, 1600);
        const bool flatOk = std::fabs (a1k - 1.0) < 0.012 && std::fabs (a18k - 1.0) < 0.012;
        CHECK (dcOk && flatOk, "IIR halfband 2x: DC exact, 1k/18k within 0.1 dB", a1k, a18k);

        // image of a 10 kHz tone in the 96 kHz stream sits at 48 - 10 = 38 kHz
        dsp::IirUpsampler2x u2; u2.prepare();
        Goertzel gs, gi; gs.init (10000.0 / 2.0); gi.init (38000.0 / 2.0); // Goertzel runs at SR: halve 2x-rate freqs
        for (int i = 0; i < 9600; ++i)
        {
            float a, b;
            u2.process ((float) std::sin (2.0 * 3.14159265358979 * 10000.0 * i / SR), a, b);
            if (i >= 1600) { gs.push (a); gs.push (b); gi.push (a); gi.push (b); }
        }
        const double rej = 20.0 * std::log10 (gi.amp() / gs.amp());
        CHECK (rej < -80.0, "IIR halfband 2x: upsampling image <= -80 dB", rej);

        // round-trip latency: centroid of the impulse response (base samples)
        dsp::IirUpsampler2x u3; dsp::IirDownsampler2x d3; u3.prepare(); d3.prepare();
        double m0 = 0, m1 = 0;
        for (int i = 0; i < 256; ++i)
        {
            float a, b; u3.process (i == 0 ? 1.0f : 0.0f, a, b);
            const double h = d3.process (a, b);
            m0 += h; m1 += h * i;
        }
        CHECK (m1 / m0 < 5.0, "IIR halfband 2x: round-trip latency < 5 samples", m1 / m0);
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

// ---- T15 default patch & monitor (real modules, GAUNTLET T15) ---------------
struct NoteRender { double rms = 0, peak = 0, tailPeak = 0; bool exactZero = true; };

// Fresh rig with the frozen default cables; hold MIDI note 60 for `holdS`,
// release, render `relS` more. tailPeak = peak over the last half of release.
inline NoteRender renderNote60 (float velV, bool dropTrigger, double holdS = 1.0, double relS = 1.0)
{
    Rig r;
    r.sys.patchDefaults();
    if (dropTrigger)
        r.sys.engine.disconnectAt (r.sys.engine.indexOf (r.sys.dusg1), DUSG::jTrig);
    r.sys.key->pitchV = 0.0f; // note 60 = 0 V
    r.sys.key->velV = velV;
    NoteRender nr;
    const int nH = (int) (SR * holdS), nR = (int) (SR * relS);
    double sq = 0;
    r.sys.key->gateV = 5.0f;
    for (int i = 0; i < nH; ++i)
    {
        r.sys.engine.tick();
        const float x = r.sys.out->outL, y = r.sys.out->outR;
        sq += (double) x * x;
        nr.peak = std::max (nr.peak, (double) std::fabs (x));
        if (x != 0.0f || y != 0.0f) nr.exactZero = false;
    }
    r.sys.key->gateV = 0.0f;
    for (int i = 0; i < nR; ++i)
    {
        r.sys.engine.tick();
        const float x = r.sys.out->outL, y = r.sys.out->outR;
        if (x != 0.0f || y != 0.0f) nr.exactZero = false;
        if (i >= nR / 2) nr.tailPeak = std::max (nr.tailPeak, (double) std::max (std::fabs (x), std::fabs (y)));
    }
    nr.rms = std::sqrt (sq / nH);
    return nr;
}

inline void runT15()
{
    GROUP ("T15 default patch & monitor bus (real modules)");

    const NoteRender n = renderNote60 (4.0f, false);
    std::printf ("  note 60, vel 4 V: rms(1 s) %.4f peak %.4f, tail %.2e  [SPEC loudness target ~0.07 / 0.37]\n",
                 n.rms, n.peak, n.tailPeak);
    CHECK (n.rms >= 0.02, "T15: default patch + note 60 -> RMS >= 0.02 within 1 s", n.rms, 0.02);
    CHECK (n.tailPeak < 0.001, "T15: note-off -> decays below -60 dBFS", n.tailPeak, 0.001);
    CHECK (n.peak <= 1.0, "T15: default voice stays inside full scale", n.peak, 1.0);

    const NoteRender cut = renderNote60 (4.0f, true);
    CHECK (cut.exactZero, "T15: DUSG1 trigger cable removed -> exact 0.0 output");

    // velocity scales the final mix (v1 contract): 0 V -> silence, louder is louder
    const NoteRender v0 = renderNote60 (0.0f, false);
    const NoteRender vLo = renderNote60 (2.0f, false);
    const NoteRender vHi = renderNote60 (5.0f, false);
    CHECK (v0.exactZero, "T15: velocity 0 V -> exact 0.0 output");
    const double ratio = vHi.rms / std::max (1e-12, vLo.rms);
    std::printf ("  velocity 2 V rms %.4f, 5 V rms %.4f, ratio %.3f (linear law = 2.5)\n", vLo.rms, vHi.rms, ratio);
    CHECK (ratio > 1.8 && ratio < 2.6, "T15: velocity scales the final mix (~linear, 2 V -> 5 V)", ratio, 2.5);

    // determinism of the full voice (same seed -> bit-identical), drift on
    {
        const NoteRender a = renderNote60 (4.0f, false, 0.25, 0.0);
        const NoteRender b = renderNote60 (4.0f, false, 0.25, 0.0);
        CHECK (a.rms == b.rms && a.peak == b.peak, "T15: default voice renders bit-identically (same seed)");
    }

    // Monitor bus: with NO cables into the mixer, every monitor source must reach
    // the output. A stimulus patch animates every source (static CVs would be
    // removed by the output DC blocker, so each source is made to move).
    auto stimulate = [] (Rig& r)
    {
        auto& s = r.sys;
        s.patch (s.key,   KEY::jGate,    s.dusg1, DUSG::jTrig);
        s.patch (s.key,   KEY::jGate,    s.dusg2, DUSG::jTrig);
        s.patch (s.ssg,   SSG::jCycle,   s.ssg,   SSG::jSmIn);     // self-cycling smooth
        s.patch (s.noise, NOISE::jWhite, s.ssg,   SSG::jStIn);
        s.patch (s.rvg,   RVG::jPulse,   s.ssg,   SSG::jSample);
        s.patch (s.noise, NOISE::jWhite, s.vcfq,  VCFQ::jIn);
        s.patch (s.nto,   NTO::jSine,    s.vcm,   VCM::jTopIn);
        s.patch (s.nto,   NTO::jSine,    s.vcm,   VCM::jMidIn1);
        s.patch (s.nto,   NTO::jSine,    s.vcm,   VCM::jBotIn1);
        s.patch (s.nto,   NTO::jSine,    s.ring,  RING::jX);
        s.patch (s.nto,   NTO::jSaw,     s.ring,  RING::jY);
        s.patch (s.nto,   NTO::jSine,    s.vca1,  VCA::jIn);
        s.patch (s.nto,   NTO::jSaw,     s.vca2,  VCA::jIn);
        s.patch (s.dusg1, DUSG::jOut,    s.vca1,  VCA::jVc);
        s.patch (s.dusg2, DUSG::jOut,    s.vca2,  VCA::jVc);
        s.patch (s.rvg,   RVG::jPulse,   s.tkb,   TKB::jVertClock);
        s.ssg->ctls[SSG::cSmRate].v = 0.2f;   // fast smooth cycle
        s.tkb->ctls[TKB::cRun].v = 1.0f;
        s.tkb->ctls[TKB::cRate].v = 0.75f;    // ~7 Hz steps; 100 ms gate must toggle (DC is blocked)
        for (int i = 0; i < 64; ++i)          // non-constant rows
            s.tkb->ctls[(size_t) (TKB::cRowA1 + i)].v = (float) ((i * 37) % 16) / 15.0f;
    };
    auto pre = [] (Rig& r, int i)
    {
        auto& s = r.sys;
        const int ph = i % 4800;               // 10 Hz note / pad cycle
        const bool on = ph < 2400;
        s.key->gateV = on ? 5.0f : 0.0f;
        s.key->pitchV = ((i / 4800) % 2) ? 7.0f / 12.0f : 0.0f;
        const int pad = (i / 4800) % 16;
        for (int p = 0; p < 16; ++p) s.tkb->ctls[(size_t) (TKB::cPad1 + p)].v = (on && p == pad) ? 1.0f : 0.0f;
        s.tkb->ctls[TKB::cPressure].v = on ? 0.3f + 0.6f * (float) ph / 2400.0f : 0.0f;
    };

    int audible = 0, deaf = 0; double quietest = 1e9; int quietestIdx = -1;
    for (int src = 1; src < OUT::kNumMonitorSources; ++src)
    {
        Rig r;
        stimulate (r);
        r.sys.out->ctls[OUT::cMonSrc].v = (float) src;
        r.sys.out->ctls[OUT::cMonLvl].v = 1.0f;
        const int n = (int) SR; // 1 s
        double sq = 0;
        for (int i = 0; i < n; ++i)
        {
            pre (r, i);
            r.sys.engine.tick();
            if (i >= n / 4) sq += (double) r.sys.out->outL * r.sys.out->outL;
        }
        const double rms = std::sqrt (sq / (n - n / 4));
        if (rms > 1e-3) ++audible;
        else { ++deaf; std::printf ("    monitor %2d '%s' rms %.2e (inaudible)\n", src, OUT::kMonitorSources[src], rms); }
        if (rms < quietest) { quietest = rms; quietestIdx = src; }
    }
    std::printf ("  monitor: %d/%d sources audible; quietest #%d '%s' rms %.2e\n", audible,
                 OUT::kNumMonitorSources - 1, quietestIdx,
                 quietestIdx > 0 ? OUT::kMonitorSources[quietestIdx] : "-", quietest);
    CHECK (deaf == 0, "T15: every monitor source is audible at the output (> -60 dBFS)",
           (double) audible, (double) (OUT::kNumMonitorSources - 1));

    // monitor OFF with no mixer cables -> exact silence
    {
        Rig r; stimulate (r);
        r.sys.out->ctls[OUT::cMonSrc].v = 0.0f;
        bool zero = true;
        for (int i = 0; i < (int) (SR / 4); ++i)
        {
            pre (r, i); r.sys.engine.tick();
            if (r.sys.out->outL != 0.0f || r.sys.out->outR != 0.0f) zero = false;
        }
        CHECK (zero, "T15: monitor OFF + no mixer cables -> exact 0.0");
    }
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
    runT15();
    runT16();
}

inline const bool registered = gauntlet::registerTest ("t_system", &run);

} // namespace t_system
