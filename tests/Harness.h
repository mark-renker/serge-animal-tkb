// Serge Animal TKB v2 — gauntlet harness utilities (ported from PF50's
// tests/GauntletTests.cpp). Pure C++17, no JUCE. Shared by all per-module
// test files; the registry runner lives in GauntletTests.cpp.
#pragma once
#include "../src/engine/System.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

namespace gauntlet {

inline int passed = 0;
inline int failed = 0;

inline void CHECK (bool ok, const char* name, double a = 0, double b = 0)
{
    if (ok) { ++passed; std::printf ("  PASS  %-52s", name); }
    else    { ++failed; std::printf ("  FAIL  %-52s", name); }
    if (a != 0 || b != 0) std::printf ("  [%.6g / %.6g]", a, b);
    std::printf ("\n");
}
inline void GROUP (const char* g) { std::printf ("\n== %s ==\n", g); }

constexpr double SR = 48000.0;

// Drives the whole instrument headlessly. Patch with sys.patch(mod, jack, ...),
// poke ctls directly (r.sys.nto->ctls[NTO::cFreq].v = ...), drive external
// inputs by writing KEY fields inside the per-sample `pre` lambda, read any
// jack's .value.
struct Rig
{
    satk::System sys;
    Rig() { sys.prepare (SR); }
    void step (int n, const std::function<void (int)>& pre = {})
    {
        for (int i = 0; i < n; ++i) { if (pre) pre (i); sys.engine.tick(); }
    }
};

// measure rate of rising edges on a jack over n samples
inline double edgeRate (Rig& r, satk::Module* m, int jack, int n,
                        const std::function<void (int)>& pre = {})
{
    int edges = 0; bool prev = false; int first = -1, last = -1;
    for (int i = 0; i < n; ++i)
    {
        if (pre) pre (i);
        r.sys.engine.tick();
        bool h = m->jacks[(size_t) jack].value > satk::kLogicHi;
        if (h && ! prev) { ++edges; if (first < 0) first = i; last = i; }
        prev = h;
    }
    if (edges < 2) return 0.0;
    return (edges - 1) * SR / double (last - first);
}

// Goertzel amplitude at frequency f (Hz) over pushed samples
struct Goertzel
{
    double s1 = 0, s2 = 0, coeff = 0; int n = 0;
    void init (double f)
    { coeff = 2.0 * std::cos (2.0 * 3.14159265358979 * f / SR); s1 = s2 = 0; n = 0; }
    void push (double x) { double s0 = x + coeff * s1 - s2; s2 = s1; s1 = s0; ++n; }
    double amp() const
    {
        double p = s1 * s1 + s2 * s2 - coeff * s1 * s2;
        return 2.0 * std::sqrt (std::max (0.0, p)) / std::max (1, n);
    }
};

inline double toneAmp (Rig& r, satk::Module* m, int jack, double f, int n,
                       const std::function<void (int)>& pre = {})
{
    Goertzel g; g.init (f);
    for (int i = 0; i < n; ++i)
    { if (pre) pre (i); r.sys.engine.tick(); g.push (m->jacks[(size_t) jack].value); }
    return g.amp();
}

inline double db (double a, double b)
{ return 20.0 * std::log10 (std::max (a, 1e-12) / std::max (b, 1e-12)); }

// ---------------------------------------------------------------------------
// T16 measurement protocol (frozen, reusable): >= `seconds` of render per run,
// minimum over >= `runs` runs, wall time via std::chrono::steady_clock (mach
// absolute time on macOS). cyclesPerSample assumes the M2 Max P-core nominal
// clock below; the GATE quantity is realtimeRatio >= 6 at 48 kHz.
// ---------------------------------------------------------------------------
constexpr double kAssumedPCoreHz = 3.504e9; // M2 Max nominal P-core clock

struct PerfResult
{
    double secondsRendered = 0, minWallSeconds = 0;
    double realtimeRatio = 0, cyclesPerSample = 0;
};

inline PerfResult measurePerf (satk::System& sys, double seconds = 10.0, int runs = 5)
{
    const long nSamples = (long) (SR * seconds);
    double best = 1e30;
    for (int run = 0; run < runs; ++run)
    {
        auto t0 = std::chrono::steady_clock::now();
        for (long i = 0; i < nSamples; ++i) sys.engine.tick();
        double dt = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        best = std::min (best, dt);
    }
    PerfResult p;
    p.secondsRendered = seconds;
    p.minWallSeconds = best;
    p.realtimeRatio = seconds / best;
    p.cyclesPerSample = best * kAssumedPCoreHz / (double) nSamples;
    return p;
}

// ---------------------------------------------------------------------------
// Engine-level state blob for T14: ctls + cables + instance seed. (The plugin
// serializes the same three things via its ValueTree; this is the engine-level
// contract that two fresh engines restored from one blob render bit-identical
// first 512 samples.)
// ---------------------------------------------------------------------------
struct EngineBlob
{
    uint64_t seed = 0;
    std::vector<satk::Cable> cables;
    std::vector<float> ctls; // in frozen module/ctl declaration order
};

inline EngineBlob saveBlob (satk::System& sys)
{
    EngineBlob b;
    b.seed = sys.engine.seed();
    b.cables = sys.engine.cables;
    for (auto& m : sys.engine.mods)
        for (auto& c : m->ctls) b.ctls.push_back (c.v);
    return b;
}

inline void loadBlob (satk::System& sys, const EngineBlob& b)
{
    sys.engine.setSeed (b.seed);
    sys.engine.cables = b.cables;
    size_t i = 0;
    for (auto& m : sys.engine.mods)
        for (auto& c : m->ctls) c.v = b.ctls[i++];
    sys.prepare (SR); // applies seeds, resets module state, rebuilds feeds
}

// ---------------------------------------------------------------------------
// registry runner
// ---------------------------------------------------------------------------
struct Test { const char* name; void (*fn)(); };
inline std::vector<Test>& registry() { static std::vector<Test> r; return r; }
inline bool registerTest (const char* name, void (*fn)())
{ registry().push_back ({ name, fn }); return true; }

} // namespace gauntlet
