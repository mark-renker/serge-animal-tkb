// Serge Animal TKB v2 — FROZEN shared DSP primitives (REDESIGN_PLAN §3).
// Header-only, pure C++17, no JUCE, no EngineCore dependency.
//
// *** THIS FILE IS FROZEN AT FOUNDATION. ***
// The six module builders code against it and may USE, never MODIFY or FORK,
// these primitives. Any change requires a foundation-level amendment logged in
// gauntlet/LOG.md. Sanity assertions live in tests/t_system.h (group T00).
//
// Contents:
//   RNG / determinism : splitmix64, SplitMix64, fnv1a, seedMix, PCG32
//   Analog layer      : OUDrift (Ornstein-Uhlenbeck, control-rate)
//   Nonlinearity      : fastTanh, ADAA1<K>, ADAA2<K>, TanhKernel, HardClipKernel
//   Band-limiting     : polyBLEP, polyBLAMP
//   Oversampling      : Upsampler2x, Downsampler2x, Oversampler2x (halfband FIR)
//   Mapping           : expmap, linmap, av
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace satk::dsp {

// ======================================================================
// RNG / determinism
// ======================================================================

// SplitMix64 step (Steele/Lea/Flood). Advances `x` and returns the next value.
// Used for STATIC per-instance component tolerances: zero runtime cost, and
// the same seed always yields the same tolerance set (gates T14/T17).
inline uint64_t splitmix64 (uint64_t& x)
{
    x += 0x9e3779b97f4a7c15ULL;
    uint64_t z = x;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

// Convenience object form. Draw tolerances at reseed()/prepare() time only.
struct SplitMix64
{
    uint64_t s = 0;
    explicit SplitMix64 (uint64_t seed = 0) : s (seed) {}
    uint64_t next()             { return splitmix64 (s); }
    // uniform in [0,1)
    float nextFloat01()         { return (float) (next() >> 40) * (1.0f / 16777216.0f); }
    // uniform in [-1,1)
    float nextBipolar()         { return nextFloat01() * 2.0f - 1.0f; }
    // multiplicative tolerance: uniform in [1-frac, 1+frac] (e.g. frac=0.01 → ±1%)
    float tolerance (float frac) { return 1.0f + nextBipolar() * frac; }
};

// FNV-1a 64-bit string hash — stable across runs/platforms. The Engine derives
// each module's seed as seedMix(instanceSeed, fnv1a(module.id)).
inline uint64_t fnv1a (const char* str)
{
    uint64_t h = 1469598103934665603ULL;
    while (*str) { h ^= (uint64_t) (uint8_t) *str++; h *= 1099511628211ULL; }
    return h;
}

// Combine two 64-bit values into a well-mixed seed.
inline uint64_t seedMix (uint64_t a, uint64_t b)
{
    uint64_t s = a ^ (b + 0x9e3779b97f4a7c15ULL + (a << 6) + (a >> 2));
    return splitmix64 (s);
}

// PCG32 (O'Neill, pcg-random.org, Apache-2.0 algorithm). One instance per
// RUNTIME random stream (noise audio, drift excitation, TKB random select...).
// Distinct streams MUST use distinct `stream` values so that e.g. changing
// drift settings never changes the noise render (REDESIGN_PLAN thread rules).
struct PCG32
{
    uint64_t state = 0x853c49e6748fea9bULL;
    uint64_t inc   = 0xda3e39cb94b95bdbULL;

    void seed (uint64_t initstate, uint64_t stream)
    {
        state = 0u;
        inc = (stream << 1) | 1u;
        next();
        state += initstate;
        next();
    }
    uint32_t next()
    {
        uint64_t old = state;
        state = old * 6364136223846793005ULL + inc;
        uint32_t xorshifted = (uint32_t) (((old >> 18) ^ old) >> 27);
        uint32_t rot = (uint32_t) (old >> 59);
        return (xorshifted >> rot) | (xorshifted << ((32u - rot) & 31u));
    }
    float nextFloat01() { return (float) (next() >> 8) * (1.0f / 16777216.0f); } // [0,1)
    float nextBipolar() { return nextFloat01() * 2.0f - 1.0f; }                  // [-1,1)
    // Approximate unit-variance gaussian (Irwin-Hall n=4, bounded ±3.46).
    // Plenty for drift/jitter; NOT for precision statistics.
    float nextGauss()
    {
        float s = nextFloat01() + nextFloat01() + nextFloat01() + nextFloat01();
        return (s - 2.0f) * 1.7320508f;
    }
};

// ======================================================================
// Analog layer: Ornstein-Uhlenbeck drift (control-rate)
// ======================================================================
// Mean-reverting bounded random walk: exact discretization
//     d[n] = rho*d[n-1] + sigma*sqrt(1-rho^2)*gauss,  rho = exp(-1/(tau*rate))
// Stationary standard deviation is exactly `sigma` (in whatever unit you use —
// cents, Hz-ratio, volts). Sum 2-3 of these with tau ≈ 0.5 s / 10 s / 3 min at
// ~1 kHz control rate for thermal drift; interpolate between control ticks.
struct OUDrift
{
    float rho = 0.0f, k = 0.0f, d = 0.0f;

    void configure (float tauSeconds, float sigma, float ctrlRateHz)
    {
        const float a = 1.0f / std::max (1.0e-6f, tauSeconds * ctrlRateHz);
        rho = std::exp (-a);
        k = sigma * std::sqrt (std::max (0.0f, 1.0f - rho * rho));
    }
    void reset (float v = 0.0f) { d = v; }
    float tick (PCG32& rng)
    {
        d = rho * d + k * rng.nextGauss();
        if (std::fabs (d) < 1.0e-20f) d = 0.0f; // denormal guard
        return d;
    }
    float value() const { return d; }
};

// ======================================================================
// Nonlinearity
// ======================================================================

// Fast tanh: Padé(7,6) with input clamp. Monotonic, |out| < 1, max abs error
// vs std::tanh < 1e-5 over the clamped range (asserted in T00).
inline float fastTanh (float x)
{
    x = std::clamp (x, -4.97f, 4.97f);
    const float x2 = x * x;
    const float num = x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)));
    const float den = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + 28.0f * x2));
    return num / den;
}

// ---- ADAA (antiderivative anti-aliasing), Bilbao/Esqueda/Parker/Välimäki ----
// Kernel concept: a struct K providing
//     double f  (double x) const;   // the nonlinearity
//     double F1 (double x) const;   // first antiderivative of f
//     double F2 (double x) const;   // second antiderivative (ADAA2 only)
// Kernels must be memoryless and DOUBLE precision: the antiderivative
// differencing cancels catastrophically in float (Chowdhury, "Practical
// Considerations for ADAA"); the wrappers below keep double state internally
// and expose a float sample interface. Wrap the WHOLE composite nonlinearity
// of a stage in one kernel (fold + rail clip together), not each piece.

// 1st-order ADAA: y = (F1(x) - F1(x1)) / (x - x1), midpoint fallback when the
// difference is ill-conditioned. Introduces ~0.5 sample delay through the NL.
template <typename K>
struct ADAA1
{
    K kernel;
    double x1 = 0.0;
    static constexpr double eps = 1.0e-6;

    void reset (double x0 = 0.0) { x1 = x0; }
    float process (float xin)
    {
        const double x = (double) xin;
        const double d = x - x1;
        double y;
        if (std::fabs (d) < eps) y = kernel.f (0.5 * (x + x1));
        else                     y = (kernel.F1 (x) - kernel.F1 (x1)) / d;
        x1 = x;
        return (float) y;
    }
};

// 2nd-order ADAA with the ill-conditioning fallbacks from Chowdhury,
// "Practical Considerations for Antiderivative Anti-Aliasing" (and the ADAA
// reference repo). ~1 sample delay through the NL.
template <typename K>
struct ADAA2
{
    K kernel;
    double x1 = 0.0, x2 = 0.0;
    static constexpr double eps = 1.0e-6;

    void reset (double x0 = 0.0) { x1 = x2 = x0; d2prev = 0.0; primedD = false; }

    float process (float xin)
    {
        const double x = (double) xin;
        const double d1 = calcD (x, x1);
        const double y  = fallback (x, d1);
        d2prev = d1;
        x2 = x1; x1 = x;
        return (float) y;
    }

private:
    double d2prev = 0.0;
    bool primedD = false;

    // D(a,b) = (F2(a) - F2(b)) / (a - b), with F1 midpoint fallback
    double calcD (double a, double b) const
    {
        if (std::fabs (a - b) < eps) return kernel.F1 (0.5 * (a + b));
        return (kernel.F2 (a) - kernel.F2 (b)) / (a - b);
    }
    double fallback (double x, double d1)
    {
        const double dPrev = primedD ? d2prev : calcD (x1, x2);
        primedD = true;
        const double den = x - x2;
        if (std::fabs (den) >= eps) return (2.0 / den) * (d1 - dPrev);
        // x ≈ x2: expand around xbar
        const double xbar = 0.5 * (x + x2);
        const double delta = xbar - x1;
        if (std::fabs (delta) < eps) return kernel.f (0.5 * (xbar + x1));
        return (2.0 / delta) * (kernel.F1 (xbar) + (kernel.F2 (x1) - kernel.F2 (xbar)) / delta);
    }
};

// Reference kernel: tanh (F2 of tanh has no closed elementary form; use with
// ADAA1 only).
struct TanhKernel
{
    double f (double x) const { return std::tanh (x); }
    // F1 = log(cosh x), computed overflow-safe.
    double F1 (double x) const
    {
        const double ax = std::fabs (x);
        return ax + std::log1p (std::exp (-2.0 * ax)) - 0.6931471805599453;
    }
};

// Reference kernel: symmetric hard clip at ±c (op-amp rails inside a folder
// stage). Analytic F1/F2 → usable with ADAA1 and ADAA2.
struct HardClipKernel
{
    double c = 1.0; // clip level
    double f (double x) const { return std::clamp (x, -c, c); }
    double F1 (double x) const
    {
        const double ax = std::fabs (x);
        if (ax <= c) return 0.5 * x * x;
        return c * ax - 0.5 * c * c;
    }
    double F2 (double x) const
    {
        const double ax = std::fabs (x);
        if (ax <= c) return x * x * x * (1.0 / 6.0);
        const double s = x > 0.0 ? 1.0 : -1.0;
        return s * (0.5 * c * x * x - 0.5 * c * c * ax + c * c * c * (1.0 / 6.0));
    }
};

// ======================================================================
// Band-limiting: polyBLEP / polyBLAMP (2-point polynomial residuals)
// ======================================================================
// `t` is the waveform phase in [0,1), `dt` = |phase increment| per sample
// (callers using through-zero FM pass fabs(inc)). Residual is nonzero only
// within one sample of the wrap point t=0.
//
// polyBLEP: residual scaled for the -2 step of the unit saw (2t-1) at wrap.
//   Canonical usage: saw = 2*t - 1 - polyBLEP(t, dt).
//   For a general waveform that DROPS by height h at the wrap point:
//   y -= (h * 0.5f) * polyBLEP(t, dt);  (h negative = upward step).
inline float polyBLEP (float t, float dt)
{
    if (dt <= 0.0f) return 0.0f;
    if (t < dt)          { const float x = t / dt;          return x + x - x * x - 1.0f; }
    if (t > 1.0f - dt)   { const float x = (t - 1.0f) / dt; return x * x + x + x + 1.0f; }
    return 0.0f;
}

// polyBLAMP: residual kernel for a SLOPE discontinuity at t=0 (triangle
// corners, slew-limit corners). Returns the raw cubic kernel; the caller
// scales it by (dt * deltaSlope / 2) where deltaSlope is the change of slope
// in units-per-PHASE (not per sample):
//   y += polyBLAMP(t, dt) * dt * deltaSlopePerPhase * 0.5f;
// Example (unit triangle, slopes +4/-4 per phase, corner at t=0 has
// deltaSlope = +8): tri += polyBLAMP(t, dt) * dt * 4.0f.
inline float polyBLAMP (float t, float dt)
{
    if (dt <= 0.0f) return 0.0f;
    if (t < dt)          { const float x = t / dt - 1.0f;          return -(x * x * x) * (1.0f / 3.0f); }
    if (t > 1.0f - dt)   { const float x = (t - 1.0f) / dt + 1.0f; return  (x * x * x) * (1.0f / 3.0f); }
    return 0.0f;
}

// ======================================================================
// Oversampling: halfband 2× wrapper for per-module nonlinear islands
// ======================================================================
// 23-tap Blackman-windowed halfband FIR generated at prepare() time (no magic
// coefficient tables). DC gain exactly 1. Combined up+down latency ≈ 11
// base-rate samples through an island; acceptable and documented (aliasing
// gates matter, absolute latency inside the patch does not).

namespace detail
{
    template <int L> // L odd
    inline void halfbandTaps (float (&h)[L])
    {
        static_assert (L % 2 == 1, "halfband length must be odd");
        constexpr int M = (L - 1) / 2;
        constexpr double pi = 3.14159265358979323846;
        double sum = 0.0;
        for (int n = 0; n < L; ++n)
        {
            const int k = n - M;
            const double ideal = (k == 0) ? 0.5 : std::sin (pi * k / 2.0) / (pi * k);
            const double w = 0.42 - 0.5 * std::cos (2.0 * pi * n / (L - 1))
                                  + 0.08 * std::cos (4.0 * pi * n / (L - 1));
            h[n] = (float) (ideal * w);
            sum += ideal * w;
        }
        for (int n = 0; n < L; ++n) h[n] = (float) (h[n] / sum); // exact DC gain 1
    }

    template <int L>
    struct FIRState
    {
        float h[L] {};
        float buf[L] {};
        int pos = 0;
        void prepare() { halfbandTaps (h); reset(); }
        void reset() { for (auto& b : buf) b = 0.0f; pos = 0; }
        void push (float x) { pos = (pos + L - 1) % L; buf[pos] = x; }
        float dot() const
        {
            float acc = 0.0f;
            int i = pos;
            for (int n = 0; n < L; ++n) { acc += h[n] * buf[i]; if (++i == L) i = 0; }
            return acc;
        }
    };
} // namespace detail

// One input sample -> two output samples at 2× rate.
struct Upsampler2x
{
    static constexpr int kTaps = 23;
    detail::FIRState<kTaps> fir;
    void prepare() { fir.prepare(); }
    void reset()   { fir.reset(); }
    void process (float x, float& outA, float& outB)
    {
        fir.push (2.0f * x); outA = fir.dot();   // ×2 restores amplitude after zero-stuffing
        fir.push (0.0f);     outB = fir.dot();
    }
};

// Two input samples at 2× rate -> one output sample.
struct Downsampler2x
{
    static constexpr int kTaps = 23;
    detail::FIRState<kTaps> fir;
    void prepare() { fir.prepare(); }
    void reset()   { fir.reset(); }
    float process (float a, float b)
    {
        fir.push (a);
        fir.push (b);
        return fir.dot();
    }
};

// Convenience island wrapper: y = down( nl(up0), nl(up1) ).
// `nl` is called twice per base-rate sample, at the 2× rate.
struct Oversampler2x
{
    Upsampler2x up;
    Downsampler2x down;
    void prepare() { up.prepare(); down.prepare(); }
    void reset()   { up.reset(); down.reset(); }
    template <typename Fn>
    float process (float x, Fn&& nl)
    {
        float a, b;
        up.process (x, a, b);
        return down.process (nl (a), nl (b));
    }
};

// ======================================================================
// Mapping helpers
// ======================================================================
inline float expmap (float v01, float lo, float hi) // exponential knob mapping
{ return lo * std::pow (hi / lo, std::clamp (v01, 0.0f, 1.0f)); }
inline float linmap (float v01, float lo, float hi)
{ return lo + (hi - lo) * std::clamp (v01, 0.0f, 1.0f); }
inline float av (float ctl01) { return ctl01 * 2.0f - 1.0f; } // attenuverter -1..+1

} // namespace satk::dsp
