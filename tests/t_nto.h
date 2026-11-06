// Serge Animal TKB v2 — NTO gauntlet tests (GAUNTLET.md rev B):
//   T02 1V/oct INTERVAL error <= 3 cents over -3..+3 V around C4 (drift off);
//       amplitude within 1 dB of nominal + correct waveform at the 16 Hz and
//       16 kHz knob extremes; via CV the core reaches >= 32 kHz, finite and
//       non-wrapped.
//   T03 saw @ 1975 Hz / 48 kHz: strongest non-harmonic component <= -60 dBc
//       in 0-6 kHz AND <= -30 dBc over 0-24 kHz (harmonics excluded +/- 20 Hz);
//       through-zero linear FM crossing zero: no NaN, bounded, spectrum
//       symmetric about the carrier for symmetric (sine) modulation.
//   T04 waveshape CV swept over 9 steps -> VAR spectral centroid strictly
//       increasing; THD <= 2% at minimum shape (VAR and SINE); soft sync locks
//       within 100 ms at +/-5% of 1:1, 2:1, 1:2 and does NOT lock 20% off.
//   T17 (NTO half) same seed -> bit-identical; with drift on, interval error
//       <= 4 cents and absolute pitch within +/-6 cents over 10 minutes;
//       different seeds -> measurably different tolerance offsets.
// Probes inject straight into the NTO's jack feeds (never via KEY KV: its
// 0.1 ms glide floor is a ~1.6 kHz lowpass). Analysis: radix-2 FFT with a
// 4-term Blackman-Harris window (-92 dB sidelobes), zero-crossing pitch.
// Render evidence (SATK_RENDER=1): gauntlet/renders/v2work/nto/nto_evidence.wav
#pragma once
#include "Harness.h"
#include <complex>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace t_nto {

using namespace gauntlet;
using namespace satk;

constexpr double kPi = 3.14159265358979;

// FREQ knob value for a frequency (inverse of expmap 16 Hz..16 kHz)
inline float freqKnob (double hz) { return (float) (std::log (hz / 16.0) / std::log (1000.0)); }

// A bare engine holding only the NTO: same module id => same moduleSeed as in
// System for a given instance seed; fast enough for 10-minute drift renders.
struct Solo
{
    Engine e;
    NTO* n = nullptr;
    float depth = 0.0f;             // ANALOG DRIFT depth (OUT ctl stand-in)
    std::vector<float> src;         // test-owned voltages, injected per jack

    explicit Solo (uint64_t seed = kDefaultInstanceSeed, bool drift = false)
    {
        n = e.add<NTO> ("nto", "NEW TIMBRAL OSCILLATOR");
        e.setSeed (seed);
        e.prepare (SR);
        src.assign (n->jacks.size(), 0.0f);
        for (int j = NTO::jOctA; j <= NTO::jSync; ++j)
            n->feeds[(size_t) j].src.push_back (&src[(size_t) j]);
        if (drift) n->driftDepth = &depth;
    }
    float& v (int jack) { return src[(size_t) jack]; }
    float out (int jack) const { return n->jacks[(size_t) jack].value; }
    void tick() { e.tick(); }
    void run (int samples) { for (int i = 0; i < samples; ++i) e.tick(); }
    std::vector<float> capture (int jack, int samples)
    {
        std::vector<float> b ((size_t) samples);
        for (int i = 0; i < samples; ++i) { e.tick(); b[(size_t) i] = out (jack); }
        return b;
    }
};

// ---- analysis helpers -------------------------------------------------------
// frequency from upward zero crossings (linear interpolation), Hz
inline double zcFreq (const std::vector<float>& x)
{
    double first = -1, last = -1; int count = 0;
    for (size_t i = 1; i < x.size(); ++i)
        if (x[i - 1] < 0.0f && x[i] >= 0.0f)
        {
            const double t = (double) (i - 1) + x[i - 1] / (double) (x[i - 1] - x[i]);
            if (first < 0) first = t;
            last = t; ++count;
        }
    if (count < 2) return 0.0;
    return (count - 1) * SR / (last - first);
}
inline double cents (double ratio) { return 1200.0 * std::log2 (ratio); }

inline void fft (std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2.0 * kPi / (double) len;
        const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1.0, 0.0);
            for (size_t k = 0; k < len / 2; ++k)
            {
                const auto u = a[i + k], t = w * a[i + k + len / 2];
                a[i + k] = u + t; a[i + k + len / 2] = u - t;
                w *= wl;
            }
        }
    }
}

// Blackman-Harris-windowed magnitude spectrum, scaled so a sinusoid of
// amplitude A peaks at ~A. Bin k <-> k*SR/N Hz.
inline std::vector<double> spectrum (const std::vector<float>& x)
{
    const size_t n = x.size();
    std::vector<std::complex<double>> a (n);
    double wsum = 0.0;
    for (size_t i = 0; i < n; ++i)
    {
        const double p = 2.0 * kPi * (double) i / (double) n;
        const double w = 0.35875 - 0.48829 * std::cos (p) + 0.14128 * std::cos (2 * p) - 0.01168 * std::cos (3 * p);
        a[i] = { x[i] * w, 0.0 };
        wsum += w;
    }
    fft (a);
    std::vector<double> m (n / 2 + 1);
    for (size_t k = 0; k <= n / 2; ++k) m[k] = 2.0 * std::abs (a[k]) / wsum;
    return m;
}

// peak magnitude within +/- tolHz of f
inline double peakNear (const std::vector<double>& m, size_t n, double f, double tolHz = 12.0)
{
    const double bin = SR / (double) n;
    double best = 0.0;
    for (long k = std::max (0L, (long) ((f - tolHz) / bin)); k <= (long) ((f + tolHz) / bin) && k < (long) m.size(); ++k)
        best = std::max (best, m[(size_t) k]);
    return best;
}

// strongest component at least `excl` Hz from every harmonic of f0 (DC
// included), within [0, fmax]; returned relative to the fundamental (dBc)
inline double worstNonHarmonic (const std::vector<double>& m, size_t n, double f0, double fmax,
                                double excl = 20.0, double* atHz = nullptr)
{
    const double bin = SR / (double) n;
    const double fund = peakNear (m, n, f0);
    double worst = 1e-30, worstF = 0;
    for (size_t k = 0; k < m.size(); ++k)
    {
        const double f = (double) k * bin;
        if (f > fmax) break;
        const double h = std::round (f / f0);
        if (std::fabs (f - h * f0) <= excl) continue;
        if (m[k] > worst) { worst = m[k]; worstF = f; }
    }
    if (atHz) *atHz = worstF;
    return db (worst, fund);
}

// THD (harmonics 2..40 / fundamental) via Goertzel on an integer number of periods
inline double thd (const std::vector<float>& x, double f0)
{
    auto amp = [&x] (double f)
    { Goertzel g; g.init (f); for (auto v : x) g.push (v); return g.amp(); };
    const double a1 = amp (f0);
    double s = 0.0;
    for (int h = 2; h <= 40 && h * f0 < SR * 0.5; ++h) { const double a = amp (h * f0); s += a * a; }
    return std::sqrt (s) / std::max (a1, 1e-12);
}

inline double centroid (const std::vector<double>& m, size_t n)
{
    const double bin = SR / (double) n;
    double num = 0, den = 0;
    for (size_t k = 1; k < m.size(); ++k) { num += k * bin * m[k]; den += m[k]; }
    return den > 0 ? num / den : 0.0;
}

inline bool finiteBounded (const std::vector<float>& x, float lim)
{
    for (auto v : x) if (! std::isfinite (v) || std::fabs (v) > lim) return false;
    return true;
}

// ---------------------------------------------------------------------------
// T02
// ---------------------------------------------------------------------------
// interval error over -3..+3 V (each V vs the 0 V reading), cents
inline double intervalError (Solo& s, int jack, double* fRef = nullptr, int win = 48000)
{
    double f[7];
    for (int i = 0; i < 7; ++i)
    {
        s.v (jack) = (float) (i - 3);
        s.run (2400);                               // settle 50 ms
        f[i] = zcFreq (s.capture (NTO::jSine, win));
    }
    s.v (jack) = 0.0f;
    double worst = 0.0;
    for (int i = 0; i < 7; ++i)
        worst = std::max (worst, std::fabs (cents (f[i] / f[3]) - 1200.0 * (i - 3)));
    if (fRef) *fRef = f[3];
    return worst;
}

inline void runT02()
{
    GROUP ("T02 NTO 1V/oct tracking (drift disabled)");
    {
        Solo s; double f0 = 0;
        const double eA = intervalError (s, NTO::jOctA, &f0);
        CHECK (eA <= 3.0, "1V/OCT A interval error <= 3 cents over -3..+3 V", eA);
        CHECK (std::fabs (cents (f0 / 261.63)) < 1.0, "default FREQ knob = C4 261.63 Hz (+/- 1 cent)", f0);
        Solo s2;
        const double eB = intervalError (s2, NTO::jOctB);
        CHECK (eB <= 3.0, "1V/OCT B interval error <= 3 cents over -3..+3 V", eB);
        // A and B sum: +1 V on each = +2 oct
        Solo s3; s3.v (NTO::jOctA) = 1.0f; s3.v (NTO::jOctB) = 1.0f; s3.run (2400);
        const double fab = zcFreq (s3.capture (NTO::jSine, 48000));
        CHECK (std::fabs (cents (fab / f0) - 2400.0) <= 3.0, "1V/OCT A + B sum (+1 V each = +2 oct)", cents (fab / f0));
    }
    // knob extremes: amplitude vs nominal Fourier coefficient (+/- 1 dB) and
    // waveform identity by harmonic structure (16 Hz) / spectral purity (16 kHz)
    const double aSine = kAudio, aTri = kAudio * 8.0 / (kPi * kPi), aSaw = kAudio * 2.0 / kPi;
    const double aPul = kCV * 2.0 / kPi; // 0/5 V square fundamental
    {
        Solo s; s.n->ctls[NTO::cFreq].v = 0.0f; s.run (9600);
        const int n = 48000; // 1 s = 16 periods
        auto sn = s.capture (NTO::jSine, n);
        Solo st; st.n->ctls[NTO::cFreq].v = 0.0f; st.run (9600);
        auto tr = st.capture (NTO::jTri, n);
        Solo sw; sw.n->ctls[NTO::cFreq].v = 0.0f; sw.run (9600);
        auto sa = sw.capture (NTO::jSaw, n);
        Solo sp; sp.n->ctls[NTO::cFreq].v = 0.0f; sp.run (9600);
        auto pu = sp.capture (NTO::jPulse, n);
        const double f = zcFreq (sn);
        CHECK (std::fabs (cents (f / 16.0)) < 2.0, "FREQ knob min = 16 Hz", f);
        auto amp = [] (const std::vector<float>& x, double fr)
        { Goertzel g; g.init (fr); for (auto v : x) g.push (v); return g.amp(); };
        float pk = 0; for (auto v : sn) pk = std::max (pk, std::fabs (v));
        CHECK (std::fabs (db (pk, aSine)) <= 1.0 && std::fabs (db (amp (sn, 16.0), aSine)) <= 1.0,
               "16 Hz SINE peak + fundamental within 1 dB of 2.5 V", db (pk, aSine), db (amp (sn, 16.0), aSine));
        CHECK (thd (sn, 16.0) <= 0.02, "16 Hz SINE is a sine (THD <= 2%)", thd (sn, 16.0));
        const double t3 = db (amp (tr, 48.0), amp (tr, 16.0)), t2 = db (amp (tr, 32.0), amp (tr, 16.0));
        CHECK (std::fabs (db (amp (tr, 16.0), aTri)) <= 1.0 && std::fabs (t3 + 19.085) <= 1.0 && t2 < -40.0,
               "16 Hz TRI: fund within 1 dB, H3 = -19.1 dB, odd-only", db (amp (tr, 16.0), aTri), t3);
        const double s2 = db (amp (sa, 32.0), amp (sa, 16.0)), s3 = db (amp (sa, 48.0), amp (sa, 16.0));
        CHECK (std::fabs (db (amp (sa, 16.0), aSaw)) <= 1.0 && std::fabs (s2 + 6.02) <= 1.0 && std::fabs (s3 + 9.54) <= 1.0,
               "16 Hz SAW: fund within 1 dB, H2 = -6.0, H3 = -9.5 dB", db (amp (sa, 16.0), aSaw), s2);
        float pmin = 1e9f, pmax = -1e9f; for (auto v : pu) { pmin = std::min (pmin, v); pmax = std::max (pmax, v); }
        CHECK (std::fabs (db (amp (pu, 16.0), aPul)) <= 1.0 && pmin > -0.5f && pmax < 5.5f && pmax > 4.75f,
               "16 Hz PULSE: 0/5 V square, fund within 1 dB", pmin, pmax);
    }
    {
        const int n = 16384;
        auto grab = [n] (int jack) // capture n: MSVC requires it (passed by reference)
        { Solo s; s.n->ctls[NTO::cFreq].v = 1.0f; s.run (9600); return s.capture (jack, n); };
        auto sn = grab (NTO::jSine), tr = grab (NTO::jTri), sa = grab (NTO::jSaw), pu = grab (NTO::jPulse);
        double aS = 0, aT = 0, aW = 0;
        { auto m = spectrum (sn); aS = peakNear (m, n, 16000.0); }
        { auto m = spectrum (tr); aT = peakNear (m, n, 16000.0); }
        { auto m = spectrum (sa); aW = peakNear (m, n, 16000.0); }
        CHECK (std::fabs (db (aS, aSine)) <= 1.0 && std::fabs (db (aT, aTri)) <= 1.0
            && std::fabs (db (aW, aSaw)) <= 1.0,
               "16 kHz: SINE/TRI/SAW fundamentals within 1 dB of nominal", db (aS, aSine), db (aW, aSaw));
        std::printf ("  16 kHz fundamentals re nominal: sine %.2f, tri %.2f, saw %.2f dB\n",
                     db (aS, aSine), db (aT, aTri), db (aW, aSaw));
        // PULSE is a 0/5 V logic output (base-rate polyBLEP, no ringing): at
        // 16 kHz it must still stay within its rails and clock once per cycle
        int edges = 0; bool hi = false; float pmin = 1e9f, pmax = -1e9f;
        for (auto v : pu)
        {
            pmin = std::min (pmin, v); pmax = std::max (pmax, v);
            if (! hi && v > kLogicHi) { ++edges; hi = true; } else if (hi && v < kLogicHi) hi = false;
        }
        const double rate = edges * SR / (double) n;
        CHECK (std::fabs (rate / 16000.0 - 1.0) < 0.01 && pmin >= 0.0f && pmax <= kCV,
               "16 kHz PULSE: one logic edge per cycle, within 0/5 V", rate, pmax);
        double worst = -999;
        for (auto* x : { &sn, &tr, &sa })
        { auto m = spectrum (*x); worst = std::max (worst, worstNonHarmonic (m, n, 16000.0, 24000.0)); }
        CHECK (worst <= -30.0, "16 kHz: band-limited (only H1 < Nyquist; others <= -30 dBc)", worst);
        const double f = zcFreq (sn);
        CHECK (std::fabs (cents (f / 16000.0)) < 5.0, "FREQ knob max = 16 kHz", f);
    }
    // >= 32 kHz via CV: real core frequency (cycle count), monotonic, finite
    {
        double prevHz = 0; bool mono = true, ok = true; double hzTop = 0; int64_t cyc = 0;
        for (float v : { 0.0f, 0.5f, 1.0f, 1.05f, 2.0f })
        {
            Solo s; s.n->ctls[NTO::cFreq].v = 1.0f; s.v (NTO::jOctA) = v; s.run (4800);
            const int64_t c0 = s.n->cycles();
            for (int i = 0; i < 48000; ++i)
            {
                s.tick();
                for (int j = NTO::jSine; j <= NTO::jPulse; ++j)
                    if (! std::isfinite (s.out (j)) || std::fabs (s.out (j)) > 6.0f) ok = false;
            }
            const double hz = (double) (s.n->cycles() - c0);
            if (hz < prevHz) mono = false;
            prevHz = hz;
            if (std::fabs (v - 1.05f) < 1e-6f) { hzTop = hz; cyc = s.n->cycles() - c0; }
        }
        CHECK (hzTop >= 32000.0, "via CV the core reaches >= 32 kHz (cycles/s, +1.05 V)", hzTop, (double) cyc);
        CHECK (mono, "core frequency monotonic in CV up to the ceiling (non-wrapped)", prevHz);
        CHECK (ok, "all outputs finite and bounded (<= 6 V) at >= 32 kHz");
    }
}

// ---------------------------------------------------------------------------
// T03
// ---------------------------------------------------------------------------
inline void runT03()
{
    GROUP ("T03 NTO band-limiting + through-zero FM");
    {
        const int n = 16384;
        const double f0 = 1975.0;
        auto grab = [&] (int jack)
        { Solo s; s.n->ctls[NTO::cFreq].v = freqKnob (f0); s.run (9600); return s.capture (jack, n); };
        auto sa = grab (NTO::jSaw);
        auto m = spectrum (sa);
        double fIn = 0, fFull = 0;
        const double inBand = worstNonHarmonic (m, n, f0, 6000.0, 20.0, &fIn);
        const double full   = worstNonHarmonic (m, n, f0, 24000.0, 20.0, &fFull);
        std::printf ("  saw 1975 Hz: worst alias in-band %.1f dBc @ %.0f Hz, full-band %.1f dBc @ %.0f Hz\n",
                     inBand, fIn, full, fFull);
        CHECK (inBand <= -60.0, "SAW 1975 Hz: strongest non-harmonic 0-6 kHz <= -60 dBc", inBand, fIn);
        CHECK (full <= -30.0,   "SAW 1975 Hz: strongest non-harmonic 0-24 kHz <= -30 dBc", full, fFull);
        // the other edge-bearing outputs, reported (not gated)
        for (auto j : { NTO::jTri, NTO::jSine, NTO::jPulse, NTO::jVar })
        {
            auto mm = spectrum (grab (j));
            std::printf ("  out %d @ 1975 Hz: in-band %.1f dBc, full-band %.1f dBc (info)\n", (int) j,
                         worstNonHarmonic (mm, n, f0, 6000.0), worstNonHarmonic (mm, n, f0, 24000.0));
        }
    }
    // through-zero linear FM: carrier 1 kHz, sine modulator 230 Hz, 2.5 V at
    // full depth -> deviation 1.5 kHz, beta 6.5: instantaneous f crosses zero
    {
        const int n = 32768;
        const double fc = 1000.0, fm = 230.0;
        Solo s; s.n->ctls[NTO::cFreq].v = freqKnob (fc); s.n->ctls[NTO::cLinDepth].v = 1.0f;
        long t = 0; float minHz = 1e9f, maxHz = -1e9f;
        double pk[5] = { 0, 0, 0, 0, 0 };
        auto drive = [&] { s.v (NTO::jLinFm) = (float) (2.5 * std::sin (2.0 * kPi * fm * (double) t++ / SR)); };
        for (int i = 0; i < 24000; ++i) { drive(); s.tick(); }
        std::vector<float> sn ((size_t) n);
        bool ok = true;
        for (int i = 0; i < n; ++i)
        {
            drive(); s.tick();
            sn[(size_t) i] = s.out (NTO::jSine);
            minHz = std::min (minHz, s.n->instHz()); maxHz = std::max (maxHz, s.n->instHz());
            for (int j = NTO::jSine; j <= NTO::jPulse; ++j)
            {
                pk[j] = std::max (pk[j], (double) std::fabs (s.out (j)));
                if (! std::isfinite (s.out (j)) || std::fabs (s.out (j)) > 6.0f) ok = false;
            }
        }
        CHECK (minHz < -250.0f && maxHz > 2250.0f, "TZFM: instantaneous frequency crosses zero", minHz, maxHz);
        { std::printf ("  TZFM peaks:"); for (double p : pk) std::printf (" %.2f", p); std::printf ("\n"); }
        CHECK (ok, "TZFM: no NaN/Inf, all outputs bounded (<= 6 V)");
        auto m = spectrum (sn);
        double amps[9]; double mx = 0;
        for (int k = -4; k <= 4; ++k) { amps[k + 4] = peakNear (m, (size_t) n, fc + k * fm, 4.0); mx = std::max (mx, amps[k + 4]); }
        double worst = 0; int used = 0;
        for (int k = 1; k <= 4; ++k)
        {
            const double lo = amps[4 - k], hi = amps[4 + k];
            if (std::max (lo, hi) < mx * 0.0316) continue; // only sidebands within 30 dB of the strongest
            worst = std::max (worst, std::fabs (db (lo, hi))); ++used;
        }
        std::printf ("  TZFM sidebands (dB re strongest), n=-4..4:");
        for (double a : amps) std::printf (" %.1f", db (a, mx));
        std::printf ("\n");
        CHECK (used >= 2 && worst <= 1.0, "TZFM spectrum symmetric about carrier (pairs within 1 dB)", worst, used);
        // abuse: rail-level FM + max index, VC LIN stacked
        Solo x; x.n->ctls[NTO::cLinDepth].v = 1.0f; x.v (NTO::jVcLin) = 5.0f;
        bool ok2 = true;
        for (int i = 0; i < 48000; ++i)
        {
            x.v (NTO::jLinFm) = (i / 37) % 2 ? 12.0f : -12.0f;
            x.tick();
            for (int j = NTO::jSine; j <= NTO::jPulse; ++j)
                if (! std::isfinite (x.out (j)) || std::fabs (x.out (j)) > 8.0f) ok2 = false;
        }
        CHECK (ok2, "TZFM abuse (+/-12 V square FM, full index): finite, bounded");
    }
}

// ---------------------------------------------------------------------------
// T04
// ---------------------------------------------------------------------------
// average slave frequency (core cycles) between t0 and t1 seconds while a
// 0/5 V square master at fm drives SYNC
inline double syncedFreq (double fSlave, double fm, double t0, double t1)
{
    Solo s; s.n->ctls[NTO::cFreq].v = freqKnob (fSlave);
    s.run (4800); // let the knob settle before the master starts
    long t = 0; int64_t c0 = 0;
    const long n0 = (long) (t0 * SR), n1 = (long) (t1 * SR);
    for (long i = 0; i < n1; ++i)
    {
        const double ph = std::fmod (fm * (double) t++ / SR, 1.0);
        s.v (NTO::jSync) = ph < 0.5 ? kCV : 0.0f;
        s.tick();
        if (i == n0 - 1) c0 = s.n->cycles();
    }
    return (double) (s.n->cycles() - c0) * SR / (double) (n1 - n0);
}

inline void runT04()
{
    GROUP ("T04 NTO waveshape + soft sync");
    {
        const int n = 16384;
        double prev = -1; bool mono = true;
        std::printf ("  VAR centroid (Hz) vs VC WAVE 0..5 V:");
        for (int k = 0; k <= 8; ++k)
        {
            Solo s; s.n->ctls[NTO::cWave].v = 0.0f; s.v (NTO::jVcWave) = 5.0f * k / 8.0f;
            s.run (9600);
            const double c = centroid (spectrum (s.capture (NTO::jVar, n)), (size_t) n);
            std::printf (" %.0f", c);
            if (c <= prev) mono = false;
            prev = c;
        }
        std::printf ("\n");
        CHECK (mono, "VAR spectral centroid strictly increasing over 9 shape-CV steps", prev);
    }
    {
        const double f0 = 250.0; // 48000-sample window = exactly 250 periods
        Solo s; s.n->ctls[NTO::cFreq].v = freqKnob (f0); s.n->ctls[NTO::cWave].v = 0.0f; s.run (9600);
        auto var = s.capture (NTO::jVar, 48000);
        Solo s2; s2.n->ctls[NTO::cFreq].v = freqKnob (f0); s2.run (9600);
        auto sn = s2.capture (NTO::jSine, 48000);
        const double tv = thd (var, f0), ts = thd (sn, f0);
        CHECK (tv <= 0.02, "VAR at minimum shape: THD <= 2% (near-pure sine)", tv);
        CHECK (ts <= 0.02, "SINE output: THD <= 2%", ts);
        CHECK (ts > 0.002, "SINE keeps a hardware-like residual (THD > 0.2%, info bound)", ts);
        // round 3 (2026-11-06): VAR starts from the exact sine — no kinked,
        // under-driven shaper (was 1.55 % THD, H3 -37 dBc, slow odd tail)
        CHECK (tv <= 0.002, "VAR at minimum shape as clean as SINE (THD <= 0.2%, no kinked-shaper tail)", tv);
        Solo s3; s3.n->ctls[NTO::cFreq].v = freqKnob (f0); s3.n->ctls[NTO::cWave].v = 1.0f; s3.run (9600);
        const double tmax = thd (s3.capture (NTO::jVar, 48000), f0);
        CHECK (tmax > 0.3, "VAR at maximum shape is strongly non-sinusoidal (THD > 30%)", tmax);
    }
    // soft sync: lock within 100 ms (measured 100-500 ms) at +/-5% of 1:1,
    // 2:1 (slave twice master), 1:2; no lock 20% off
    {
        const double fm = 300.0;
        bool lockAll = true; double worstLock = 0;
        for (double r : { 1.0, 2.0, 0.5 })
            for (double d : { -0.05, 0.05 })
            {
                const double f = syncedFreq (r * fm * (1.0 + d), fm, 0.1, 0.5);
                const double err = std::fabs (f / (r * fm) - 1.0);
                worstLock = std::max (worstLock, err);
                if (err > 0.002) { lockAll = false; std::printf ("  no lock: ratio %.1f detune %+.0f%% -> %.2f Hz\n", r, d * 100, f); }
            }
        CHECK (lockAll, "soft sync locks 1:1, 2:1, 1:2 at +/-5% within 100 ms", worstLock);
        bool freeAll = true; double minMiss = 1e9;
        for (double r : { 1.0, 2.0, 0.5 })
            for (double d : { -0.2, 0.2 })
            {
                const double fFree = r * fm * (1.0 + d);
                const double f = syncedFreq (fFree, fm, 0.1, 0.5);
                double miss = 1e9; // distance to the nearest simple ratio
                for (double q : { 0.5, 1.0, 2.0 }) miss = std::min (miss, std::fabs (f / (q * fm) - 1.0));
                minMiss = std::min (minMiss, miss);
                if (miss < 0.02) { freeAll = false; std::printf ("  locked at 20%%: ratio %.1f detune %+.0f%% -> %.2f Hz\n", r, d * 100, f); }
            }
        CHECK (freeAll, "soft sync does NOT lock 20% off a simple ratio (>= 2% away)", minMiss);
    }
}

// ---------------------------------------------------------------------------
// T17 (NTO half)
// ---------------------------------------------------------------------------
inline void runT17()
{
    GROUP ("T17 NTO analog layer (tolerance + drift)");
    {
        // same seed, drift on: two fresh full systems render bit-identical NTO outs
        auto render = [] (uint64_t seed)
        {
            Rig r; r.sys.engine.setSeed (seed); r.sys.prepare (SR);
            r.sys.nto->driftDepth = &r.sys.out->ctls[OUT::cDrift].v; // default 0.2
            r.sys.out->ctls[OUT::cDrift].v = 1.0f;
            std::vector<float> o;
            for (int i = 0; i < 48000; ++i)
            {
                r.sys.engine.tick();
                for (int j = NTO::jSine; j <= NTO::jPulse; ++j) o.push_back (r.sys.nto->jacks[(size_t) j].value);
            }
            return o;
        };
        const auto a = render (kDefaultInstanceSeed), b = render (kDefaultInstanceSeed);
        bool same = a.size() == b.size();
        for (size_t i = 0; same && i < a.size(); ++i) same = ! differs (a[i], b[i]);
        CHECK (same, "same instance seed, drift on -> bit-identical NTO render");
    }
    {
        // drift disabled: different seeds give identical (ideal) output
        Solo a (1, true), b (2, true);
        bool same = true;
        for (int i = 0; i < 48000 && same; ++i) { a.tick(); b.tick(); same = ! differs (a.out (NTO::jSaw), b.out (NTO::jSaw)); }
        CHECK (same, "ANALOG DRIFT 0 -> ideal: seeds 1 and 2 bit-identical");
    }
    for (float depth : { 0.2f, 1.0f })
    {
        Solo s (kDefaultInstanceSeed, true); s.depth = depth;
        const double e = intervalError (s, NTO::jOctA, nullptr, 24000);
        char name[96];
        std::snprintf (name, sizeof name, "drift %.1f: 1V/oct interval error <= 4 cents", depth);
        CHECK (e <= 4.0, name, e);
    }
    {
        // 10 minutes at full drift depth, two seeds: absolute pitch vs ideal
        // C4 within +/-6 cents in every 1 s window; wander is real (moves)
        double worstAbs = 0, minPP = 1e9;
        for (uint64_t seed : { kDefaultInstanceSeed, (uint64_t) 0x1234567ULL })
        {
            Solo s (seed, true); s.depth = 1.0f; s.run (4800);
            double lo = 1e9, hi = -1e9;
            std::vector<float> buf (48000);
            for (int w = 0; w < 600; ++w)
            {
                for (int i = 0; i < 48000; ++i) { s.tick(); buf[(size_t) i] = s.out (NTO::jSine); }
                const double c = cents (zcFreq (buf) / 261.63);
                lo = std::min (lo, c); hi = std::max (hi, c);
            }
            worstAbs = std::max ({ worstAbs, std::fabs (lo), std::fabs (hi) });
            minPP = std::min (minPP, hi - lo);
            std::printf ("  seed %llx: 10 min pitch range %.2f .. %.2f cents (p-p %.2f)\n",
                         (unsigned long long) seed, lo, hi, hi - lo);
        }
        CHECK (worstAbs <= 6.0, "drift 1.0: absolute pitch within +/-6 cents over 10 min", worstAbs);
        CHECK (minPP >= 0.3, "drift 1.0: pitch actually wanders (p-p >= 0.3 cents)", minPP);
    }
    {
        // different seeds -> measurably different tolerance offsets (2 s mean)
        double c[4]; int i = 0;
        for (uint64_t seed : { (uint64_t) 11, (uint64_t) 22, (uint64_t) 33, (uint64_t) 44 })
        {
            Solo s (seed, true); s.depth = 1.0f; s.run (4800);
            c[i++] = cents (zcFreq (s.capture (NTO::jSine, 96000)) / 261.63);
        }
        double minD = 1e9;
        for (int a = 0; a < 4; ++a) for (int b = a + 1; b < 4; ++b) minD = std::min (minD, std::fabs (c[a] - c[b]));
        std::printf ("  seed offsets (cents): %.2f %.2f %.2f %.2f\n", c[0], c[1], c[2], c[3]);
        CHECK (minD >= 0.05, "different seeds -> measurably different pitch offsets (>= 0.05 c)", minD);
    }
}

// ---------------------------------------------------------------------------
// perf (informational; the gate is T16 full-system)
// ---------------------------------------------------------------------------
inline void runPerf()
{
    Solo s (kDefaultInstanceSeed, true); s.depth = 0.2f;
    s.n->ctls[NTO::cLinDepth].v = 0.3f; s.n->ctls[NTO::cWave].v = 0.6f;
    double best = 1e30;
    for (int run = 0; run < 3; ++run)
    {
        auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < 480000; ++i) { s.v (NTO::jLinFm) = (i & 64) ? 1.0f : -1.0f; s.tick(); }
        best = std::min (best, std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count());
    }
    const double cyc = best * kAssumedPCoreHz / 480000.0;
    std::printf ("  NTO %.1f cyc/smp (spike 414)  [min of 3 x 10 s]\n", cyc);
    CHECK (cyc > 0 && cyc < 3.0 * 414.0, "NTO cost within 3x its perf-spike figure (SPEC §10)", cyc);
}

// ---------------------------------------------------------------------------
// render evidence (SATK_RENDER=1)
// ---------------------------------------------------------------------------
inline void writeWav16 (const std::string& fn, const std::vector<float>& L)
{
    FILE* f = std::fopen (fn.c_str(), "wb");
    if (f == nullptr) { std::printf ("  render: cannot open %s\n", fn.c_str()); return; }
    const uint32_t n = (uint32_t) L.size(), dataSz = n * 2, riff = 36 + dataSz;
    const uint32_t sr = (uint32_t) SR, byps = sr * 2, fmtSz = 16;
    const uint16_t ch = 1, bits = 16, blk = 2, pcm = 1;
    std::fwrite ("RIFF", 1, 4, f); std::fwrite (&riff, 4, 1, f);
    std::fwrite ("WAVEfmt ", 1, 8, f);
    std::fwrite (&fmtSz, 4, 1, f); std::fwrite (&pcm, 2, 1, f); std::fwrite (&ch, 2, 1, f);
    std::fwrite (&sr, 4, 1, f); std::fwrite (&byps, 4, 1, f);
    std::fwrite (&blk, 2, 1, f); std::fwrite (&bits, 2, 1, f);
    std::fwrite ("data", 1, 4, f); std::fwrite (&dataSz, 4, 1, f);
    for (uint32_t i = 0; i < n; ++i)
    {
        const int16_t s = (int16_t) std::lround (std::clamp (L[i], -1.0f, 1.0f) * 32000.0f);
        std::fwrite (&s, 2, 1, f);
    }
    std::fclose (f);
    std::printf ("  render: wrote %s (%u samples)\n", fn.c_str(), n);
}

inline void renderEvidence()
{
    GROUP ("NTO render evidence (SATK_RENDER=1)");
    const std::string dir = "gauntlet/renders/v2work/nto";
    std::error_code ec;
    std::filesystem::create_directories (dir, ec);
    std::vector<float> L;
    const float g = 0.4f / kAudio;
    { // 0-2 s: saw 1975 Hz (T03 condition; v1 equivalent aliased at -27 dBc)
        Solo s; s.n->ctls[NTO::cFreq].v = freqKnob (1975.0);
        for (int i = 0; i < 96000; ++i) { s.tick(); L.push_back (s.out (NTO::jSaw) * g); }
    }
    { // 2-6 s: VAR shape sweep 0 -> 1 at 130.8 Hz
        Solo s; s.n->ctls[NTO::cFreq].v = freqKnob (130.81); s.n->ctls[NTO::cWave].v = 0.0f;
        for (int i = 0; i < 192000; ++i) { s.v (NTO::jVcWave) = 5.0f * i / 192000.0f; s.tick(); L.push_back (s.out (NTO::jVar) * g); }
    }
    { // 6-9 s: TZFM, carrier 440 Hz, modulator 440*1.5 with rising index
        Solo s; s.n->ctls[NTO::cFreq].v = freqKnob (440.0); s.n->ctls[NTO::cLinDepth].v = 1.0f;
        for (int i = 0; i < 144000; ++i)
        {
            s.v (NTO::jLinFm) = (float) (2.5 * (i / 144000.0) * std::sin (2.0 * kPi * 660.0 * i / SR));
            s.tick(); L.push_back (s.out (NTO::jSine) * g);
        }
    }
    { // 9-12 s: soft sync, master 220 Hz, slave swept 180 -> 520 Hz (locks near 1:1, 2:1)
        Solo s;
        for (int i = 0; i < 144000; ++i)
        {
            s.n->ctls[NTO::cFreq].v = freqKnob (180.0 * std::pow (520.0 / 180.0, i / 144000.0));
            s.v (NTO::jSync) = std::fmod (220.0 * i / SR, 1.0) < 0.5 ? kCV : 0.0f;
            s.tick(); L.push_back (s.out (NTO::jTri) * g);
        }
    }
    writeWav16 (dir + "/nto_evidence.wav", L);
}

// ---------------------------------------------------------------------------
inline void run()
{
    runT02();
    runT03();
    runT04();
    runT17();
    GROUP ("NTO per-module perf (informational)");
    runPerf();
    if (std::getenv ("SATK_RENDER") != nullptr)
        renderEvidence();
}

inline const bool registered = gauntlet::registerTest ("t_nto", &run);

} // namespace t_nto
