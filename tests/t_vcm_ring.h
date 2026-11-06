// Serge Animal TKB v2 — VCM/RING gauntlet tests, gates T08 and T09
// (GAUNTLET.md rev B). Measurement conditions per T08: bias centred, analog
// tolerance layer not involved (VCM has none; RING's only per-unit tolerance
// is the bleed polarity), sine input at the nominal Serge audio level
// (±2.5 V peak = SPEC §1 kAudio) unless a test states otherwise.
//   T08 MID : at max FOLD -> >= 5 odd harmonics each >= -40 dBc (checked re the
//             fundamental AND re the strongest component), odd:even energy
//             >= 12 dB; spectral centroid strictly increasing over 8 FOLD-knob
//             steps AND over 8 VC FOLD steps; DC-ramp static transfer shows
//             >= 2 slope-sign reversals; folded ~1 kHz sine (997 Hz, so
//             aliases do not land on harmonic bins of 48 kHz) -> every
//             non-harmonic component <= -50 dBc. Regression: the FOLD-knob
//             centroid also rises with the REAL NTO SINE as the input
//             (via VCA1 LIN 0.8, the sound-bar patch, and direct).
//   T08 BOT : full depth -> even harmonics dominate by >= 12 dB; BOT SQ OUT
//             present, squared-up (crest), distinct from BOT OUT, PWM-like
//             (duty moves with depth).
//   T08 TOP : LO -> gain linear in VC TOP (r^2 > 0.99); HI -> monotonic
//             static transfer and clipping that increases monotonically with
//             VC TOP (rounded: bounded, no hard flat top).
//   T09     : carrier suppression >= 55 dB at nominal trims with the
//             sidebands dominant; X/Y feedthrough trims: 0 <= -80 dB,
//             default in -60..-80 dB, max >= -50 dB, monotonic; morph
//             dry -> AM -> ring monotonic; DC carrier = clean VCA.
// Plus a default-voice loudness SURROGATE for T15 (NTO and DUSG are still
// stubs at this commit, so their outputs are emulated at the jack).
// Render evidence (SATK_RENDER=1): gauntlet/renders/v2work/shapers/*.wav.
#pragma once
#include "Harness.h"
#include "t_nto.h" // freqKnob, zcFreq (real-NTO-sine fold sweep)
#include <complex>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace t_vcm_ring {

using namespace gauntlet;
using namespace satk;

constexpr double kPi = 3.14159265358979323846;

inline void injectSrc (Module* m, int jack, const float* src)
{
    m->feeds[(size_t) jack].src.push_back (src);
}

// ---- analysis helpers ------------------------------------------------------
// Goertzel bank over harmonics 1..nH of f0 (window = whole periods of f0
// at 48 kHz -> leakage-free for integer-Hz f0 and a 1 s window).
inline std::vector<double> harmonics (const std::vector<float>& x, double f0, int nH)
{
    std::vector<double> a ((size_t) nH + 1, 0.0);
    for (int h = 1; h <= nH; ++h)
    {
        if (h * f0 >= SR * 0.5) break;
        Goertzel g; g.init (h * f0);
        for (float v : x) g.push (v);
        a[(size_t) h] = g.amp();
    }
    return a;
}

inline double centroid (const std::vector<double>& a, double f0)
{
    double num = 0.0, den = 0.0;
    for (size_t h = 1; h < a.size(); ++h) { num += (double) h * f0 * a[h]; den += a[h]; }
    return den > 0.0 ? num / den : 0.0;
}

inline void oddEvenEnergy (const std::vector<double>& a, double& odd, double& even)
{
    odd = even = 0.0;
    for (size_t h = 1; h < a.size(); ++h) ((h & 1u) ? odd : even) += a[h] * a[h];
}

inline double rmsOf (const std::vector<float>& x)
{
    double s = 0.0;
    for (float v : x) s += (double) v * v;
    return std::sqrt (s / (double) std::max<size_t> (1, x.size()));
}
inline double peakOf (const std::vector<float>& x)
{
    double p = 0.0;
    for (float v : x) p = std::max (p, (double) std::fabs (v));
    return p;
}
inline double meanOf (const std::vector<float>& x)
{
    double s = 0.0;
    for (float v : x) s += v;
    return s / (double) std::max<size_t> (1, x.size());
}

// in-place iterative radix-2 FFT
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
                const auto u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v; a[i + k + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

// 4-term Blackman-Harris amplitude spectrum (sidelobes ~ -92 dB), amplitude-
// calibrated for sinusoids. Returns |X| for bins 0..N/2.
inline std::vector<double> ampSpectrum (const std::vector<float>& x)
{
    const size_t n = x.size();
    std::vector<std::complex<double>> a (n);
    double wsum = 0.0;
    for (size_t i = 0; i < n; ++i)
    {
        const double t = 2.0 * kPi * (double) i / (double) n;
        const double w = 0.35875 - 0.48829 * std::cos (t) + 0.14128 * std::cos (2 * t)
                       - 0.01168 * std::cos (3 * t);
        a[i] = x[i] * w; wsum += w;
    }
    fft (a);
    std::vector<double> m (n / 2 + 1);
    for (size_t k = 0; k <= n / 2; ++k) m[k] = 2.0 * std::abs (a[k]) / wsum;
    return m;
}

// ---- rigs ------------------------------------------------------------------
struct VcmRun
{
    float fold = 1.0f, vcFold = 0.0f, bias = 0.5f, depth = 0.3f;
    float drive = 0.301f, topMode = 1.0f, vcTop = 0.0f, vcBot = 0.0f;
    bool patchVcTop = false;
};

// Render `n` samples of one VCM output with a sine (amp V, f Hz) on the
// section's input after `settle` samples. Inputs are injected at the jack.
inline std::vector<float> vcmSine (const VcmRun& p, int inJack, int outJack,
                                   double f, double amp, int n, int settle = 9600)
{
    Rig r; auto* v = r.sys.vcm;
    v->ctls[VCM::cFold].v = p.fold;      v->ctls[VCM::cBias].v = p.bias;
    v->ctls[VCM::cBotDepth].v = p.depth; v->ctls[VCM::cTopDrive].v = p.drive;
    v->ctls[VCM::cTopMode].v = p.topMode;
    r.sys.prepare (SR); // snap the knob smoothers to the test settings
    float src = 0.0f, vf = p.vcFold, vt = p.vcTop, vb = p.vcBot;
    injectSrc (v, inJack, &src);
    injectSrc (v, VCM::jVcFold, &vf);
    injectSrc (v, VCM::jVcBot, &vb);
    if (p.patchVcTop) injectSrc (v, VCM::jVcTop, &vt);
    long k = 0;
    auto pre = [&] (int) { src = (float) (amp * std::sin (2.0 * kPi * f * (double) k++ / SR)); };
    r.step (settle, pre);
    std::vector<float> y ((size_t) n);
    for (int i = 0; i < n; ++i)
    { pre (0); r.sys.engine.tick(); y[(size_t) i] = v->jacks[(size_t) outJack].value; }
    return y;
}

// Static transfer: slow DC ramp lo..hi volts over n samples (input band
// << fold features, so the read-out is the memoryless curve).
inline std::vector<float> vcmRamp (const VcmRun& p, int inJack, int outJack,
                                   double lo, double hi, int n, float* midMakeupOut = nullptr)
{
    Rig r; auto* v = r.sys.vcm;
    v->ctls[VCM::cFold].v = p.fold; v->ctls[VCM::cBias].v = p.bias;
    v->ctls[VCM::cTopDrive].v = p.drive; v->ctls[VCM::cTopMode].v = p.topMode;
    r.sys.prepare (SR);
    float src = (float) lo, vt = p.vcTop;
    injectSrc (v, inJack, &src);
    if (p.patchVcTop) injectSrc (v, VCM::jVcTop, &vt);
    r.step (4800);
    std::vector<float> y ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        src = (float) (lo + (hi - lo) * (double) i / (double) (n - 1));
        r.sys.engine.tick();
        y[(size_t) i] = v->jacks[(size_t) outJack].value;
    }
    if (midMakeupOut) *midMakeupOut = v->midMakeup();
    return y;
}

// direction changes of a curve, with hysteresis h (V) against numeric fuzz
inline int slopeReversals (const std::vector<float>& y, double h)
{
    int dir = 0, rev = 0;
    double ext = y.empty() ? 0.0 : y[0];
    for (float s : y)
    {
        if (dir >= 0 && s < ext - h)      { if (dir > 0) ++rev; dir = -1; ext = s; }
        else if (dir <= 0 && s > ext + h) { if (dir < 0) ++rev; dir = 1;  ext = s; }
        else if ((dir > 0 && s > ext) || (dir < 0 && s < ext)) ext = s;
    }
    return rev;
}

inline double rSquared (const std::vector<double>& x, const std::vector<double>& y)
{
    const double n = (double) x.size();
    double sx = 0, sy = 0, sxx = 0, syy = 0, sxy = 0;
    for (size_t i = 0; i < x.size(); ++i)
    { sx += x[i]; sy += y[i]; sxx += x[i] * x[i]; syy += y[i] * y[i]; sxy += x[i] * y[i]; }
    const double cov = sxy - sx * sy / n, vx = sxx - sx * sx / n, vy = syy - sy * sy / n;
    return (vx > 0 && vy > 0) ? cov * cov / (vx * vy) : 0.0;
}

// ---------------------------------------------------------------------------
// T08 MID
// ---------------------------------------------------------------------------
inline void testMid()
{
    constexpr double f0 = 220.0; // v1 manifest reference pitch
    constexpr int nH = 100;
    VcmRun p; p.fold = 1.0f;     // max FOLD (gain 8x), bias centred
    const auto y = vcmSine (p, VCM::jMidIn1, VCM::jMidOut, f0, kAudio, 48000);
    const auto a = harmonics (y, f0, nH);

    double strongest = 0.0;
    for (size_t h = 1; h < a.size(); ++h) strongest = std::max (strongest, a[h]);
    int oddReFund = 0, oddReMax = 0;
    for (size_t h = 1; h < a.size(); h += 2)
    {
        if (a[h] >= 0.01 * a[1])      ++oddReFund;
        if (a[h] >= 0.01 * strongest) ++oddReMax;
    }
    std::printf ("  MID @ max fold, 220 Hz 2.5 V: H1..H15 (dB re strongest):");
    for (int h = 1; h <= 15; ++h) std::printf (" %.1f", db (a[(size_t) h], strongest));
    std::printf ("\n");
    CHECK (oddReFund >= 5 && oddReMax >= 5,
           "T08 MID max fold: >= 5 odd harmonics >= -40 dBc (re H1 / re max)",
           oddReFund, oddReMax);
    double odd, even; oddEvenEnergy (a, odd, even);
    const double oe = 10.0 * std::log10 (odd / std::max (even, 1e-30));
    CHECK (oe >= 12.0, "T08 MID max fold: odd:even energy >= 12 dB", oe, 12);

    // centroid vs FOLD knob (8 steps k/7: gain 0.5x -> 8x) and vs VC FOLD
    // (8 steps 0..5 V at knob 0.5: gain 2x -> 8x)
    std::vector<double> cK, cV;
    for (int k = 0; k < 8; ++k)
    {
        VcmRun q; q.fold = (float) k / 7.0f;
        cK.push_back (centroid (harmonics (vcmSine (q, VCM::jMidIn1, VCM::jMidOut, f0, kAudio, 48000), f0, nH), f0));
        VcmRun c; c.fold = 0.5f; c.vcFold = 5.0f * (float) k / 7.0f;
        cV.push_back (centroid (harmonics (vcmSine (c, VCM::jMidIn1, VCM::jMidOut, f0, kAudio, 48000), f0, nH), f0));
    }
    bool monoK = true, monoV = true;
    for (size_t i = 1; i < cK.size(); ++i) { monoK &= cK[i] > cK[i - 1]; monoV &= cV[i] > cV[i - 1]; }

    // round 2 (2026-11-05): minimum FOLD passes a sine nearly clean, and the
    // section holds its level as it folds (hardware folders do; v2 r1 fell
    // 6.6 dB from FOLD step 4 to 8)
    {
        VcmRun q; q.fold = 0.0f;
        const auto y0 = vcmSine (q, VCM::jMidIn1, VCM::jMidOut, f0, kAudio, 48000);
        const double thd0 = t_nto::thd (y0, f0);
        std::printf ("  MID minimum FOLD, 2.5 V sine: THD %.2f %%\n", 100.0 * thd0);
        CHECK (thd0 <= 0.015, "T08 MID minimum fold passes a 2.5 V sine nearly clean (THD <= 1.5%)", thd0, 0.015);
        std::vector<double> lv;
        for (int k = 0; k < 8; ++k)
        {
            VcmRun r; r.fold = (float) k / 7.0f;
            lv.push_back (db (rmsOf (vcmSine (r, VCM::jMidIn1, VCM::jMidOut, f0, kAudio, 48000)), 1.0));
        }
        const double span = *std::max_element (lv.begin(), lv.end()) - *std::min_element (lv.begin(), lv.end());
        std::printf ("  MID rms vs FOLD knob (dBV):");
        for (double v : lv) std::printf (" %.1f", v);
        std::printf ("\n");
        CHECK (span <= 4.0, "MID level held over the 8 FOLD steps (2.5 V sine, within +-2 dB)", span, 4.0);
    }
    std::printf ("  centroid vs FOLD knob (Hz):");
    for (double c : cK) std::printf (" %.0f", c);
    std::printf ("\n  centroid vs VC FOLD 0..5 V (Hz):");
    for (double c : cV) std::printf (" %.0f", c);
    std::printf ("\n");
    CHECK (monoK, "T08 MID centroid strictly increasing over 8 FOLD steps", cK.front(), cK.back());
    CHECK (monoV, "T08 MID centroid strictly increasing over 8 VC FOLD steps", cV.front(), cV.back());

    // static transfer at max fold: it must FOLD (turnarounds), not saturate
    // The fold peaks are +-2.5 V times the section's level-compensation
    // makeup (2026-11-05: output level held across the FOLD sweep, makeup
    // capped at 2x), so the bound is checked on the makeup-scaled peak and
    // the absolute peak may never exceed 2x nominal.
    float mk = 0.0f;
    const auto tr = vcmRamp (p, VCM::jMidIn1, VCM::jMidOut, -kAudio, kAudio, 96000, &mk);
    const int rev = slopeReversals (tr, 0.05);
    CHECK (rev >= 2, "T08 MID static transfer: >= 2 slope-sign reversals", rev, 2);
    CHECK (peakOf (tr) <= 1.02 * kAudio * mk && peakOf (tr) >= 0.9 * kAudio * mk && mk <= 2.0f,
           "T08 MID transfer bounded at the fold peaks (+-2.5 V x makeup <= 2)", peakOf (tr), kAudio * mk);

    // symmetry sanity: bias off-centre must bring in even harmonics
    VcmRun b = p; b.bias = 0.7f;
    const auto ab = harmonics (vcmSine (b, VCM::jMidIn1, VCM::jMidOut, f0, kAudio, 48000), f0, nH);
    double ob, eb; oddEvenEnergy (ab, ob, eb);
    CHECK (10.0 * std::log10 (ob / std::max (eb, 1e-30)) < oe - 20.0,
           "T08 MID BIAS off-centre adds even harmonics", 10.0 * std::log10 (ob / std::max (eb, 1e-30)), oe);
}

// Regression (sound bar, vcm_mid_foldsweep.wav): the centroid must also rise
// for a REAL, slightly impure sine — the NTO SINE jack (under-driven tri->sine
// shaper: H3 ~ -37 dBc, THD ~1.5 %, inside T04), not the ideal sine above.
// Feeds: (a) the sound-bar manifest patch, NTO SINE -> VCA1 (LIN, LEVEL 0.8)
// -> MID IN 1; (b) NTO SINE straight into MID IN 1. NTO 220 Hz, BIAS centred,
// drift 0 (T08 conditions); FOLD knob 8 steps k/7, 1 s each after a 0.2 s
// settle; centroid on the refined (zero-crossing) f0. Before the 2026-11-04
// fix the manifest feed went 262 -> 251 -> 270 Hz: the knee onset smoothed
// away the NTO's peak kink faster than it added harmonics of its own (LOG.md).
inline std::vector<double> realSineFoldSweep (bool viaVca, std::vector<double>* levels = nullptr)
{
    constexpr int nH = 100;
    std::vector<double> c;
    for (int k = 0; k < 8; ++k)
    {
        Rig r; auto& s = r.sys;
        s.out->ctls[OUT::cDrift].v = 0.0f;
        s.nto->ctls[NTO::cFreq].v = t_nto::freqKnob (220.0);
        s.vca1->ctls[VCA::cResponse].v = 0.0f; // LIN
        s.vca1->ctls[VCA::cLevel].v = 0.8f;
        if (viaVca)
        {
            s.patch (s.nto, NTO::jSine, s.vca1, VCA::jIn);
            s.patch (s.vca1, VCA::jOut, s.vcm, VCM::jMidIn1);
        }
        else
            s.patch (s.nto, NTO::jSine, s.vcm, VCM::jMidIn1);
        s.vcm->ctls[VCM::cBias].v = 0.5f;
        s.vcm->ctls[VCM::cFold].v = (float) k / 7.0f;
        s.prepare (SR); // snap the knob smoothers to the test settings
        r.step (9600);
        std::vector<float> y (48000), ref (48000);
        for (size_t i = 0; i < y.size(); ++i)
        {
            s.engine.tick();
            y[i] = s.vcm->jacks[VCM::jMidOut].value;
            ref[i] = s.nto->jacks[NTO::jSine].value;
        }
        const double f0 = t_nto::zcFreq (ref);
        c.push_back (centroid (harmonics (y, f0, nH), f0));
        if (levels) levels->push_back (db (rmsOf (y), 1.0));
    }
    return c;
}

inline void testMidRealSine()
{
    for (int pass = 0; pass < 2; ++pass)
    {
        const bool viaVca = pass == 0;
        std::vector<double> lv;
        const auto c = realSineFoldSweep (viaVca, &lv);
        bool mono = true;
        double worst = 1.0e9;
        for (size_t i = 1; i < c.size(); ++i) { mono &= c[i] > c[i - 1]; worst = std::min (worst, c[i] / c[i - 1]); }
        std::printf ("  real NTO SINE %s -> MID, centroid vs FOLD knob (Hz):",
                     viaVca ? "-> VCA1 LIN 0.8" : "direct");
        for (double v : c) std::printf (" %.1f", v);
        std::printf ("  (worst step ratio %.4f)\n", worst);
        const double span = *std::max_element (lv.begin(), lv.end()) - *std::min_element (lv.begin(), lv.end());
        std::printf ("    rms per FOLD step (dBV):");
        for (double v : lv) std::printf (" %.1f", v);
        std::printf ("  (span %.1f dB)\n", span);
        if (viaVca)
            CHECK (span <= 4.0, "MID level held over the sound-bar FOLD sweep (real sine via VCA 0.8, +-2 dB)", span, 4.0);
        CHECK (mono, viaVca ? "T08 MID real NTO sine via VCA 0.8: centroid rises (8 FOLD)"
                            : "T08 MID real NTO sine direct: centroid rises (8 FOLD)",
               c.front(), c.back());
    }
}

// Anti-aliasing: max fold, ~1 kHz sine (997 Hz) at 2.5 V; every component
// more than 20 Hz from a harmonic n·f0 < Nyquist counts as an alias.
inline double aliasDbc (float fold, float vcFold, double& reMaxOut)
{
    constexpr double f0 = 997.0;
    constexpr int N = 65536;
    VcmRun p; p.fold = fold; p.vcFold = vcFold;
    const auto y = vcmSine (p, VCM::jMidIn1, VCM::jMidOut, f0, kAudio, N);
    const auto m = ampSpectrum (y);
    const double binHz = SR / N;
    double worst = 0.0, fund = 0.0, strongest = 0.0;
    for (size_t k = 1; k < m.size(); ++k)
    {
        const double f = (double) k * binHz;
        const double h = std::round (f / f0);
        const bool harm = h >= 1.0 && std::fabs (f - h * f0) <= 20.0 && h * f0 < SR * 0.5;
        if (harm) { strongest = std::max (strongest, m[k]); if (h < 1.5) fund = std::max (fund, m[k]); }
        else if (f > 20.0) worst = std::max (worst, m[k]);
    }
    reMaxOut = db (worst, strongest);
    return db (worst, fund);
}

inline void testMidAlias()
{
    double reMax = 0.0;
    const double reFund = aliasDbc (1.0f, 0.0f, reMax);
    std::printf ("  MID alias @ max fold (8x), 997 Hz: worst %.1f dB re H1, %.1f dB re strongest\n", reFund, reMax);
    CHECK (reFund <= -50.0 && reMax <= -50.0,
           "T08 MID anti-aliasing: folded 1 kHz, aliases <= -50 dBc", reFund, reMax);
    double reMax16 = 0.0;
    const double reFund16 = aliasDbc (1.0f, 5.0f, reMax16); // CV to the 16x cap (beyond the gate)
    std::printf ("  (info) MID alias @ 16x cap: %.1f dB re H1, %.1f dB re strongest\n", reFund16, reMax16);
}

// round 3 (2026-11-06): stepped ctls must not click. A 2.0 V 220 Hz sine
// through MID (or RING X, Y = 2.5 V DC), the ctl stepped at t = 0.5 s; HF
// energy (4-12 kHz) in 512-sample Hann windows around the step vs the
// steady state before/after: excess <= +6 dB. (The 1-pole 2 ms smoother's
// slope corner splattered +31 dB at FOLD step 1 -> 2 of the sound-bar sweep.)
inline double hfBand (const std::vector<float>& x, size_t a, double lo, double hi)
{
    constexpr size_t n = 512;
    std::vector<std::complex<double>> c (n);
    for (size_t i = 0; i < n; ++i)
        c[i] = (double) x[a + i] * (0.5 - 0.5 * std::cos (2.0 * kPi * (double) i / (double) n));
    fft (c);
    double e = 0.0;
    for (size_t k = 1; k < n / 2; ++k)
    {
        const double f = (double) k * SR / (double) n;
        if (f >= lo && f < hi) e += std::norm (c[k]);
    }
    return e;
}
inline double stepExcessDb (const std::vector<float>& y, size_t t)
{
    const double before = hfBand (y, t - 14400, 4000.0, 12000.0);
    const double after  = hfBand (y, t + 14400, 4000.0, 12000.0);
    double tr = 0.0;
    for (int o = -256; o <= 256; o += 32) tr = std::max (tr, hfBand (y, (size_t) ((long) t + o - 256), 4000.0, 12000.0));
    return 10.0 * std::log10 ((tr + 1e-30) / (std::max (before, after) + 1e-30));
}
inline void testCtlSteps()
{
    // MID: FOLD 0 -> 1/7 and 1/7 -> 2/7 (the cleanest, most exposed steps)
    double worst = -1e9;
    for (int k = 0; k < 2; ++k)
    {
        Rig r; auto* v = r.sys.vcm;
        v->ctls[VCM::cFold].v = (float) k / 7.0f; v->ctls[VCM::cBias].v = 0.5f;
        r.sys.prepare (SR);
        float src = 0.0f; injectSrc (v, VCM::jMidIn1, &src);
        std::vector<float> y (48000);
        for (int i = 0; i < 48000; ++i)
        {
            if (i == 24000) v->ctls[VCM::cFold].v = (float) (k + 1) / 7.0f;
            src = (float) (2.0 * std::sin (2.0 * kPi * 220.0 * i / SR));
            r.sys.engine.tick();
            y[(size_t) i] = v->jacks[VCM::jMidOut].value;
        }
        worst = std::max (worst, stepExcessDb (y, 24000));
    }
    std::printf ("  MID FOLD step 0 -> 1/7 -> 2/7: worst 4-12 kHz excess at the step %+.1f dB\n", worst);
    CHECK (worst <= 6.0, "VCM MID: stepped FOLD does not click (4-12 kHz excess <= +6 dB)", worst, 6.0);
    // RING: MORPH 0.5 -> 0.75 with Y at 0 V (output = X·(1 - m): a pure
    // -6 dB gain step on X)
    {
        Rig r; auto* g = r.sys.ring;
        g->ctls[RING::cMorph].v = 0.5f;
        r.sys.prepare (SR);
        float x = 0.0f, yc = 0.0f;
        injectSrc (g, RING::jX, &x); injectSrc (g, RING::jY, &yc);
        std::vector<float> y (48000);
        for (int i = 0; i < 48000; ++i)
        {
            if (i == 24000) g->ctls[RING::cMorph].v = 0.75f;
            x = (float) (2.0 * std::sin (2.0 * kPi * 220.0 * i / SR));
            r.sys.engine.tick();
            y[(size_t) i] = g->jacks[RING::jOut].value;
        }
        const double ex = stepExcessDb (y, 24000);
        std::printf ("  RING MORPH step 0.5 -> 0.75 (Y = 0 V): 4-12 kHz excess %+.1f dB\n", ex);
        CHECK (ex <= 6.0, "RING: stepped MORPH does not click (4-12 kHz excess <= +6 dB)", ex, 6.0);
    }
}

// ---------------------------------------------------------------------------
// T08 BOT
// ---------------------------------------------------------------------------
inline double dutyOf (const std::vector<float>& x)
{
    const double m = meanOf (x);
    size_t hi = 0;
    for (float v : x) hi += v > m ? 1u : 0u;
    return (double) hi / (double) x.size();
}

inline void testBot()
{
    constexpr double f0 = 220.0;
    VcmRun p; p.depth = 1.0f;
    const auto bot = vcmSine (p, VCM::jBotIn1, VCM::jBotOut, f0, kAudio, 48000);
    const auto sq  = vcmSine (p, VCM::jBotIn1, VCM::jBotSq,  f0, kAudio, 48000);
    const auto a = harmonics (bot, f0, 100);
    double odd, even; oddEvenEnergy (a, odd, even);
    const double eo = 10.0 * std::log10 (even / std::max (odd, 1e-30));
    std::printf ("  BOT @ full depth: H1..H8 (dB re strongest):");
    double strongest = 0.0;
    for (size_t h = 1; h < a.size(); ++h) strongest = std::max (strongest, a[h]);
    for (int h = 1; h <= 8; ++h) std::printf (" %.1f", db (a[(size_t) h], strongest));
    std::printf ("\n");
    CHECK (eo >= 12.0, "T08 BOT full depth: even:odd energy >= 12 dB", eo, 12);

    // round 2 (2026-11-05): a rectifier CASCADE — each of the 3 stages doubles
    // the frequency, so a sine's energy moves H1 -> H2 -> H4 -> H8 as the
    // depth passes 1/3, 2/3, 1 (hardware-spec §2.5 "2nd, 4th, 8th"), at a
    // held level, from the first cycle (no fade-in)
    {
        const float dd[3] = { 1.0f / 3.0f, 2.0f / 3.0f, 1.0f };
        const int want[3] = { 2, 4, 8 };
        bool order = true; double worstMargin = 1e9, lmin = 1e9, lmax = -1e9;
        for (int i = 0; i < 3; ++i)
        {
            VcmRun q; q.depth = dd[i];
            const auto y = vcmSine (q, VCM::jBotIn1, VCM::jBotOut, f0, kAudio, 48000);
            const auto h = harmonics (y, f0, 100);
            size_t hs = 1; double second = 0.0;
            for (size_t k = 1; k < h.size(); ++k) if (h[k] > h[hs]) hs = k;
            for (size_t k = 1; k < h.size(); ++k) if (k != hs) second = std::max (second, h[k]);
            order &= (int) hs == want[i];
            worstMargin = std::min (worstMargin, db (h[hs], second));
            lmin = std::min (lmin, rmsOf (y)); lmax = std::max (lmax, rmsOf (y));
            std::printf ("  BOT depth %.2f: strongest H%d, next %.1f dB below\n", dd[i], (int) hs, db (h[hs], second));
        }
        CHECK (order && worstMargin >= 20.0, "T08 BOT octave cascade: depth 1/3, 2/3, 1 -> H2, H4, H8 dominant (>= 20 dB)",
               worstMargin, 20);
        std::vector<double> lv;
        for (int k = 0; k <= 12; ++k)
        {
            VcmRun q; q.depth = (float) k / 12.0f;
            lv.push_back (rmsOf (vcmSine (q, VCM::jBotIn1, VCM::jBotOut, f0, kAudio, 48000)));
        }
        const double span = db (*std::max_element (lv.begin(), lv.end()), *std::min_element (lv.begin(), lv.end()));
        CHECK (span <= 1.5, "T08 BOT level held over the depth sweep (13 steps, sine, within 1.5 dB)", span, 1.5);
        VcmRun q; q.depth = 1.0f;
        const auto on = vcmSine (q, VCM::jBotIn1, VCM::jBotOut, f0, kAudio, 4800, 0);
        double pk0 = 0.0, pk1 = 0.0;
        for (size_t i = 0; i < 480; ++i) pk0 = std::max (pk0, (double) std::fabs (on[i]));
        for (size_t i = 2400; i < 4800; ++i) pk1 = std::max (pk1, (double) std::fabs (on[i]));
        CHECK (pk0 >= 0.9 * pk1, "T08 BOT no fade-in: first 10 ms peak >= 90% of steady", pk0, pk1);
    }

    // round 3 (2026-11-06): RECTIFIER character, not a smooth polynomial.
    // Full-wave rectifiers make cusps -> a dense even series falling slowly:
    // at full depth H16, H24, H32 and H48 each >= -45 dB re the dominant H8
    // (the round-2 square-law cascade: H16 -112 dB, nothing above ~6 kHz),
    // with an alias floor <= -90 dB re strongest over 0-20 kHz (220 Hz).
    {
        const auto h = harmonics (bot, f0, 100);
        double strongest = 0.0;
        for (size_t k = 1; k < h.size(); ++k) strongest = std::max (strongest, h[k]);
        const double s16 = db (h[16], strongest), s24 = db (h[24], strongest),
                     s32 = db (h[32], strongest), s48 = db (h[48], strongest);
        std::printf ("  BOT full depth cusp series (dB re strongest): H16 %.1f H24 %.1f H32 %.1f H48 %.1f H64 %.1f H96 %.1f\n",
                     s16, s24, s32, s48, db (h[64], strongest), db (h[96], strongest));
        CHECK (std::min ({ s16, s24, s32, s48 }) >= -45.0,
               "T08 BOT rectifier cusps: dense even series (H16/H24/H32/H48 >= -45 dB re max)",
               std::min ({ s16, s24, s32, s48 }), -45.0);
        std::vector<float> w (bot.begin(), bot.begin() + 32768);
        const auto m = t_nto::spectrum (w);
        const double reH1 = t_nto::worstNonHarmonic (m, w.size(), f0, 20000.0, 20.0);
        const double reMax = reH1 + db (t_nto::peakNear (m, w.size(), f0, 3.0), t_nto::peakNear (m, w.size(), 8.0 * f0, 3.0));
        std::printf ("  BOT full depth alias floor 0-20 kHz: %.1f dB re strongest\n", reMax);
        CHECK (reMax <= -90.0, "T08 BOT anti-aliasing: non-harmonic 0-20 kHz <= -90 dB re max", reMax, -90.0);
    }

    // level compensation: rectification must not shrink the output
    VcmRun p0; p0.depth = 0.0f;
    const auto dry = vcmSine (p0, VCM::jBotIn1, VCM::jBotOut, f0, kAudio, 48000);
    const double acFull = rmsOf (bot) * rmsOf (bot) - meanOf (bot) * meanOf (bot);
    const double lvl = db (std::sqrt (std::max (0.0, acFull)), rmsOf (dry));
    CHECK (std::fabs (lvl) <= 3.0, "T08 BOT level-compensated (AC rms depth 1 vs 0 within 3 dB)", lvl, 3);

    // squared-up second output: present, square-ish, distinct, PWM-like.
    // "Squared-up" = dwells at its rails: fraction of samples within 20 % of
    // the ±2.5 V plateau (rms/peak crest is reported only — the halfband's
    // Gibbs overshoot on band-limited edges inflates the peak).
    auto railDwell = [] (const std::vector<float>& x)
    {
        const double m = meanOf (x);
        size_t k = 0;
        for (float v : x) k += std::fabs (v - m) >= 0.8 * kAudio ? 1u : 0u;
        return (double) k / (double) x.size();
    };
    const double dwSq = railDwell (sq), dwBot = railDwell (bot);
    const double crestSq = rmsOf (sq) / std::max (1e-9, peakOf (sq));
    const double crestBot = rmsOf (bot) / std::max (1e-9, peakOf (bot));
    double sxy = 0, sxx = 0, syy = 0;
    const double mb = meanOf (bot), ms = meanOf (sq);
    for (size_t i = 0; i < bot.size(); ++i)
    { const double u = bot[i] - mb, w = sq[i] - ms; sxy += u * w; sxx += u * u; syy += w * w; }
    const double corr = sxy / std::sqrt (std::max (1e-30, sxx * syy));
    std::printf ("  BOT SQ: rms %.3f V peak %.3f V crest %.3f (BOT %.3f), rail dwell %.2f (BOT %.2f),"
                 " corr(BOT,SQ) %.3f\n", rmsOf (sq), peakOf (sq), crestSq, crestBot, dwSq, dwBot, corr);
    CHECK (rmsOf (sq) >= 1.0, "T08 BOT SQ OUT present (rms >= 1 V)", rmsOf (sq), 1);
    CHECK (dwSq >= 0.7 && dwSq >= dwBot + 0.3,
           "T08 BOT SQ OUT squared-up (>= 70% at its rails, BOT +0.3)", dwSq, dwBot);
    CHECK (corr < 0.95, "T08 BOT SQ OUT distinct from BOT OUT (corr < 0.95)", corr, 0.95);
    std::vector<double> duty;
    const float depths[] = { 0.0f, 0.15f, 0.3f, 0.5f, 0.7f, 1.0f };
    for (float d : depths)
    {
        VcmRun q; q.depth = d;
        duty.push_back (dutyOf (vcmSine (q, VCM::jBotIn1, VCM::jBotSq, f0, kAudio, 48000)));
    }
    const double dutySpan = *std::max_element (duty.begin(), duty.end())
                          - *std::min_element (duty.begin(), duty.end());
    std::printf ("  BOT SQ duty vs depth 0/.15/.3/.5/.7/1:");
    for (double d : duty) std::printf (" %.3f", d);
    std::printf ("\n");
    CHECK (dutySpan >= 0.05, "T08 BOT SQ OUT PWM-like: duty moves with depth", dutySpan, 0.05);
}

// ---------------------------------------------------------------------------
// T08 TOP
// ---------------------------------------------------------------------------
inline void testTop()
{
    constexpr double f0 = 1000.0;
    // LO: linear VCA — amplitude vs VC TOP over 0..5 V in 11 steps
    std::vector<double> cv, amp;
    for (int k = 0; k <= 10; ++k)
    {
        VcmRun p; p.topMode = 0.0f; p.patchVcTop = true; p.vcTop = 0.5f * (float) k;
        const auto y = vcmSine (p, VCM::jTopIn, VCM::jTopOut, f0, 1.0, 48000);
        Goertzel g; g.init (f0);
        for (float s : y) g.push (s);
        cv.push_back (p.vcTop); amp.push_back (g.amp());
    }
    const double r2 = rSquared (cv, amp);
    std::printf ("  TOP LO: amp @ 0/2.5/5 V = %.4f / %.4f / %.4f (1 V in)\n", amp[0], amp[5], amp[10]);
    CHECK (r2 > 0.99, "T08 TOP LO: gain linear in CV (r^2 > 0.99)", r2, 0.99);
    CHECK (amp[0] < 1e-4 && std::fabs (amp[10] - 1.0) < 0.02,
           "T08 TOP LO: closed at 0 V, unity at 5 V (default drive)", amp[0], amp[10]);

    // HI: monotonic static transfer (DC ramp) at min and max CV
    bool mono = true; double worstDrop = 0.0, pk = 0.0;
    for (float vt : { 0.0f, 5.0f })
    {
        VcmRun p; p.topMode = 1.0f; p.patchVcTop = true; p.vcTop = vt;
        const auto tr = vcmRamp (p, VCM::jTopIn, VCM::jTopOut, -kAudio, kAudio, 48000);
        for (size_t i = 1; i < tr.size(); ++i)
        { worstDrop = std::min (worstDrop, (double) (tr[i] - tr[i - 1])); }
        pk = std::max (pk, peakOf (tr));
    }
    mono = worstDrop > -1e-4;
    CHECK (mono && pk <= kAudio * 1.001, "T08 TOP HI: static transfer monotonic, bounded",
           worstDrop, pk);

    // HI: clipping increases monotonically with VC TOP (crest -> square),
    // rounded (odd-harmonic series decays: H3 > H5 > H7)
    std::vector<double> crest;
    bool rounded = true;
    for (int k = 0; k < 8; ++k)
    {
        VcmRun p; p.topMode = 1.0f; p.patchVcTop = true; p.vcTop = 5.0f * (float) k / 7.0f;
        const auto y = vcmSine (p, VCM::jTopIn, VCM::jTopOut, 220.0, kAudio, 48000);
        crest.push_back (rmsOf (y) / std::max (1e-9, peakOf (y)));
        const auto a = harmonics (y, 220.0, 9);
        rounded &= a[3] > a[5] && a[5] > a[7];
    }
    bool incr = true;
    for (size_t i = 1; i < crest.size(); ++i) incr &= crest[i] > crest[i - 1];
    std::printf ("  TOP HI crest (rms/peak) vs VC TOP 0..5 V:");
    for (double c : crest) std::printf (" %.3f", c);
    std::printf ("\n");
    CHECK (incr, "T08 TOP HI: VC clipping monotonic (crest rises over 8 CV steps)", crest.front(), crest.back());
    CHECK (rounded, "T08 TOP HI: rounded clip (H3 > H5 > H7 at every step)");
}

// ---------------------------------------------------------------------------
// T09 RING
// ---------------------------------------------------------------------------
struct RingRun
{
    float morph = 1.0f, trimX = 0.15f, trimY = 0.15f, gain = 0.3333f;
    double fx = 300.0, ax = kAudio, dcX = 0.0;   // signal X
    double fy = 80.0,  ay = kAudio, dcY = 0.0;   // carrier Y (fy 0 -> DC only)
};

inline std::vector<float> ringRun (const RingRun& p, int n = 48000, int settle = 4800,
                                   std::vector<float>* xOut = nullptr)
{
    Rig r; auto* g = r.sys.ring;
    g->ctls[RING::cMorph].v = p.morph; g->ctls[RING::cGain].v = p.gain;
    g->ctls[RING::cTrimX].v = p.trimX; g->ctls[RING::cTrimY].v = p.trimY;
    r.sys.prepare (SR);
    float x = 0.0f, y = 0.0f;
    injectSrc (g, RING::jX, &x);
    injectSrc (g, RING::jY, &y);
    long k = 0;
    auto pre = [&] (int)
    {
        const double t = (double) k++ / SR;
        x = (float) (p.dcX + p.ax * std::sin (2.0 * kPi * p.fx * t));
        y = (float) (p.dcY + (p.fy > 0.0 ? p.ay * std::sin (2.0 * kPi * p.fy * t) : 0.0));
    };
    r.step (settle, pre);
    std::vector<float> o ((size_t) n);
    if (xOut) xOut->assign ((size_t) n, 0.0f);
    for (int i = 0; i < n; ++i)
    {
        pre (0); r.sys.engine.tick(); o[(size_t) i] = g->jacks[RING::jOut].value;
        if (xOut) (*xOut)[(size_t) i] = x;
    }
    return o;
}

inline double toneOf (const std::vector<float>& x, double f)
{
    Goertzel g; g.init (f);
    for (float v : x) g.push (v);
    return g.amp();
}

inline void testRing()
{
    // carrier suppression at nominal trims: X 300 Hz, Y (carrier) 80 Hz,
    // both 2.5 V; window = 1 s (integer cycles -> leakage-free Goertzel)
    {
        RingRun p;
        const auto o = ringRun (p);
        const double lsb = toneOf (o, 220.0), usb = toneOf (o, 380.0);
        const double car = toneOf (o, 80.0), sig = toneOf (o, 300.0);
        const double supp = db (std::min (lsb, usb), car);
        const double suppX = db (std::min (lsb, usb), sig);
        std::printf ("  RING nominal: sidebands %.3f/%.3f V, carrier %.2e V, signal %.2e V\n",
                     lsb, usb, car, sig);
        CHECK (supp >= 55.0, "T09 carrier suppression >= 55 dB at nominal trims", supp, 55);
        CHECK (suppX >= 55.0 && lsb > 10.0 * car && usb > 10.0 * sig,
               "T09 sidebands dominant (signal also suppressed >= 55 dB)", suppX, 55);
        CHECK (supp <= 85.0, "T09 suppression inside the 55..85 dB sound-bar band", supp, 85);
    }

    // round 2 (2026-11-05): a pure four-quadrant multiplier fed a symmetric
    // unipolar (0..5 V) carrier makes X and X ± fY only. The tanh(0.35·Y)
    // core bent the offset carrier into a 2nd harmonic: X ± 2·fY at -32 dBc.
    {
        RingRun p; p.dcY = kAudio; // 0..5 V sine carrier
        const auto o = ringRun (p);
        const double strongest = std::max ({ toneOf (o, 300.0), toneOf (o, 220.0), toneOf (o, 380.0) });
        const double p2 = std::max (toneOf (o, 140.0), toneOf (o, 460.0));
        const double x3 = toneOf (o, 900.0);
        std::printf ("  RING unipolar carrier: X±2fY %.1f dB, 3fX %.1f dB re strongest\n",
                     db (p2, strongest), db (x3, strongest));
        CHECK (db (p2, strongest) <= -80.0, "T09 offset (unipolar) carrier: no X +- 2fY products (<= -80 dB)",
               db (p2, strongest), -80);
        CHECK (db (x3, strongest) <= -80.0, "T09 nominal signal passes linearly (3fX <= -80 dB)", db (x3, strongest), -80);
    }

    // feedthrough trims: each input alone, morph = full ring
    auto feed = [] (bool yside, float trim)
    {
        RingRun p; (yside ? p.trimY : p.trimX) = trim;
        if (yside) p.ax = 0.0; else p.ay = 0.0;
        const auto o = ringRun (p);
        return db (toneOf (o, yside ? p.fy : p.fx), kAudio);
    };
    const float trims[] = { 0.0f, 0.15f, 0.3f, 0.5f, 0.75f, 1.0f };
    for (bool yside : { false, true })
    {
        std::vector<double> f;
        for (float t : trims) f.push_back (feed (yside, t));
        bool mono = true;
        for (size_t i = 1; i < f.size(); ++i) mono &= f[i] > f[i - 1];
        std::printf ("  RING %s feedthrough (dB) @ trim 0/.15/.3/.5/.75/1:", yside ? "Y" : "X");
        for (double v : f) std::printf (" %.1f", v);
        std::printf ("\n");
        CHECK (f[0] <= -80.0, yside ? "T09 Y trim 0 = calibrated min (<= -80 dB)"
                                    : "T09 X trim 0 = calibrated min (<= -80 dB)", f[0], -80);
        CHECK (f[1] <= -60.0 && f[1] >= -80.0,
               yside ? "T09 Y trim default in the -60..-80 dB band"
                     : "T09 X trim default in the -60..-80 dB band", f[1], -70);
        CHECK (f.back() >= -50.0 && mono,
               yside ? "T09 Y trim max >= -50 dB audible bleed, monotonic"
                     : "T09 X trim max >= -50 dB audible bleed, monotonic", f.back(), -50);
    }

    // morph dry -> AM -> ring: signal term falls, sidebands rise, monotonic
    {
        std::vector<double> sig, sb;
        for (int k = 0; k <= 8; ++k)
        {
            RingRun p; p.morph = (float) k / 8.0f;
            const auto o = ringRun (p);
            sig.push_back (toneOf (o, 300.0));
            sb.push_back (toneOf (o, 220.0) + toneOf (o, 380.0));
        }
        bool mono = true;
        for (size_t i = 1; i < sig.size(); ++i) mono &= sig[i] < sig[i - 1] && sb[i] > sb[i - 1];
        std::printf ("  RING morph 0/.5/1: signal %.3f/%.3f/%.1e V, sidebands %.3f/%.3f/%.3f V\n",
                     sig[0], sig[4], sig[8], sb[0], sb[4], sb[8]);
        CHECK (mono, "T09 morph dry -> AM -> ring monotonic (9 steps)", sig[0], sb[8]);
        CHECK (std::fabs (db (sig[0], kAudio)) < 0.5 && sb[0] < 1e-4 * sig[0],
               "T09 morph 0 = dry signal (unity, no sidebands)", sig[0], sb[0]);
        CHECK (std::fabs (db (sb[4], sig[4])) < 1.0,
               "T09 morph 0.5 = 100% AM (sideband pair = carrier level)", sb[4], sig[4]);
    }

    // DC carrier -> clean VCA: amplitude vs DC Y linear-ish and monotonic,
    // closed at 0 V, low THD, four-quadrant (negative DC inverts)
    {
        std::vector<double> dc, amp;
        for (int k = 0; k <= 10; ++k)
        {
            RingRun p; p.fy = 0.0; p.dcY = 0.5 * k;
            dc.push_back (p.dcY); amp.push_back (toneOf (ringRun (p), 300.0));
        }
        bool mono = true;
        for (size_t i = 1; i < amp.size(); ++i) mono &= amp[i] > amp[i - 1];
        const double r2 = rSquared (dc, amp);
        std::printf ("  RING DC carrier: amp @ 0/2.5/5 V = %.2e / %.3f / %.3f V, r^2 %.5f\n",
                     amp[0], amp[5], amp[10], r2);
        CHECK (mono && r2 > 0.99, "T09 DC carrier = VCA: monotonic, linear (r^2 > 0.99)", r2, 0.99);
        CHECK (db (amp[10], amp[0]) >= 55.0, "T09 DC carrier 0 V closes the VCA (>= 55 dB)",
               db (amp[10], amp[0]), 55);

        RingRun p; p.fy = 0.0; p.dcY = kAudio;
        std::vector<float> xin;
        const auto o = ringRun (p, 48000, 4800, &xin);
        const auto a = harmonics (o, 300.0, 15);
        double hs = 0.0;
        for (size_t h = 2; h < a.size(); ++h) hs += a[h] * a[h];
        const double thd = std::sqrt (hs) / a[1];
        CHECK (thd <= 0.03 && std::fabs (db (a[1], kAudio)) < 1.0,
               "T09 DC carrier +2.5 V: clean unity VCA (THD <= 3%)", thd, a[1]);
        // spectral purity: no components off the X harmonics
        double spur = toneOf (o, 80.0) + toneOf (o, 150.0) + toneOf (o, 450.0);
        CHECK (spur < 1e-4 * a[1], "T09 DC carrier: no spurious products", spur, a[1]);

        RingRun n = p; n.dcY = -kAudio;
        std::vector<float> xn;
        const auto on = ringRun (n, 48000, 4800, &xn);
        double cp = 0, cn = 0;
        for (size_t i = 0; i < o.size(); ++i) { cp += o[i] * xin[i]; cn += on[i] * xn[i]; }
        CHECK (cp > 0.0 && cn < 0.0, "T09 four-quadrant: negative DC carrier inverts", cp, cn);
    }
}

// ---------------------------------------------------------------------------
// Module-level feedback stress (T13 contribution): every VCM section and the
// RING self-patched (out -> own in, out -> own CV), hot 10 V kick, max fold /
// depth / drive / ring gain, 10 s: finite, within rails, denormal-free.
// ---------------------------------------------------------------------------
inline void testFeedbackStress()
{
    Rig r; auto* v = r.sys.vcm; auto* g = r.sys.ring;
    v->ctls[VCM::cFold].v = 1.0f; v->ctls[VCM::cBotDepth].v = 1.0f;
    v->ctls[VCM::cTopDrive].v = 1.0f; g->ctls[RING::cGain].v = 1.0f;
    g->ctls[RING::cTrimX].v = 1.0f; g->ctls[RING::cTrimY].v = 1.0f;
    r.sys.prepare (SR);
    r.sys.patch (v, VCM::jMidOut, v, VCM::jMidIn2);
    r.sys.patch (v, VCM::jMidOut, v, VCM::jVcFold);
    r.sys.patch (v, VCM::jBotSq,  v, VCM::jBotIn2);
    r.sys.patch (v, VCM::jBotOut, v, VCM::jVcBot);
    r.sys.patch (v, VCM::jTopOut, v, VCM::jVcTop);
    r.sys.patch (v, VCM::jTopOut, g, RING::jX);
    r.sys.patch (g, RING::jOut,   g, RING::jY);
    r.sys.patch (g, RING::jOut,   v, VCM::jTopIn);
    r.sys.patch (g, RING::jOut,   g, RING::jVcMorph);
    float kick = 0.0f;
    for (Module* m : { (Module*) v, (Module*) g })
        for (int j : { m == v ? VCM::jMidIn1 : RING::jX, m == v ? VCM::jBotIn1 : RING::jY })
            injectSrc (m, j, &kick);
    bool finite = true, inRails = true, denorm = false;
    float worst = 0.0f;
    const int n = (int) (SR * 10.0);
    for (int i = 0; i < n; ++i)
    {
        kick = i < 2400 ? 10.0f * (float) std::sin (0.07 * i) : 0.0f;
        if (i == 240000) v->ctls[VCM::cTopMode].v = 0.0f; // flip TOP to LO mid-run
        r.sys.engine.tick();
        for (Module* m : { (Module*) v, (Module*) g })
            for (auto& jk : m->jacks)
            {
                const float x = jk.value;
                finite &= std::isfinite (x);
                inRails &= std::fabs (x) <= kRails;
                denorm |= x != 0.0f && std::fabs (x) < 1.0e-30f;
                worst = std::max (worst, std::fabs (x));
            }
    }
    CHECK (finite && inRails && ! denorm,
           "T13 (module): VCM/RING self-feedback 10 s finite, in rails, no denormals", worst, kRails);
}

// ---------------------------------------------------------------------------
// Default-voice SURROGATE for T15 loudness (NTO/DUSG are stubs at this commit):
// default cables; NTO VAR emulated as a 2.5 V sine at 261.63 Hz, DUSG1 OUT as
// its default 1 ms / 250 ms linear AD to 5 V on note-on; velocity 100.
// Checks the folder path keeps the voice audible; reports rms/peak vs the
// SPEC §1 target (rms ~ 0.07, peak ~ 0.37).
// ---------------------------------------------------------------------------
inline void testDefaultVoiceSurrogate (double& rmsOut, double& peakOut)
{
    Rig r;
    r.sys.patchDefaults();
    r.sys.key->velV = 100.0f / 127.0f * kCV;
    r.sys.key->gateV = kCV;
    const int n = (int) SR;
    std::vector<float> o ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        const double t = (double) i / SR;
        r.sys.nto->jacks[NTO::jVar].value = (float) (kAudio * std::sin (2.0 * kPi * 261.63 * t));
        const double env = t < 0.001 ? t / 0.001 : std::max (0.0, 1.0 - (t - 0.001) / 0.25);
        r.sys.dusg1->jacks[DUSG::jOut].value = (float) (kCV * env);
        r.sys.engine.tick();
        o[(size_t) i] = r.sys.out->outL;
    }
    rmsOut = rmsOf (o); peakOut = peakOf (o);
    double mx = 0.0;
    for (int i = 0; i < 12000; ++i) mx += (double) o[(size_t) i] * o[(size_t) i];
    const double rms250 = std::sqrt (mx / 12000.0);
    std::printf ("  default voice (surrogate NTO/DUSG): rms(1 s) %.4f, rms(first 250 ms) %.4f, peak %.3f"
                 " [SPEC target ~0.07 / 0.37]\n", rmsOut, rms250, peakOut);
    CHECK (rmsOut >= 0.02, "T15 surrogate: VCM-mid default voice audible (rms >= 0.02 in 1 s)", rmsOut, 0.02);
}

// ---------------------------------------------------------------------------
// perf (T16 protocol, bare module, VCM with all three sections live)
// ---------------------------------------------------------------------------
inline void testPerf()
{
    GROUP ("T08-T09 perf (T16 protocol, bare module; binding 6x gate lives in t_system)");
    auto bench = [] (bool ring) -> double
    {
        Rig r;
        float src = 0.0f;
        long k = 0;
        if (ring) { injectSrc (r.sys.ring, RING::jX, &src); injectSrc (r.sys.ring, RING::jY, &src); }
        else
            for (int j : { VCM::jTopIn, VCM::jMidIn1, VCM::jBotIn1 })
                injectSrc (r.sys.vcm, j, &src);
        r.sys.vcm->ctls[VCM::cFold].v = 1.0f;
        Module* m = ring ? (Module*) r.sys.ring : (Module*) r.sys.vcm;
        const long nS = (long) (SR * 10.0);
        double best = 1e30;
        for (int run = 0; run < 3; ++run)
        {
            auto t0 = std::chrono::steady_clock::now();
            for (long i = 0; i < nS; ++i)
            {
                src = (float) (kAudio * std::sin (0.0342 * (double) (k++ & 0xffff)));
                m->tick();
            }
            best = std::min (best, std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count());
        }
        return best * kAssumedPCoreHz / (double) nS;
    };
    const double cVcm = bench (false), cRing = bench (true);
    std::printf ("  perf (bare module, min of 3 x 10 s, incl. test sine): VCM %.0f cyc/smp (spike 552), RING %.0f (spike 188)\n",
                 cVcm, cRing);
    CHECK (cVcm <= 3.0 * 552.0, "perf: VCM within 3x its SPEC.md S10 spike figure", cVcm, 3.0 * 552.0);
    CHECK (cRing <= 3.0 * 188.0, "perf: RING within 3x its SPEC.md S10 spike figure", cRing, 3.0 * 188.0);
}

// ---------------------------------------------------------------------------
// Render evidence (SATK_RENDER=1) — v2 counterparts of v1 manifest files 3/4/8
// ---------------------------------------------------------------------------
inline void writeWav16 (const std::string& fn, const std::vector<float>& L)
{
    FILE* f = std::fopen (fn.c_str(), "wb");
    if (f == nullptr) { std::printf ("  RENDER: cannot open %s\n", fn.c_str()); return; }
    const uint32_t n = (uint32_t) L.size(), dataSz = n * 4u, riff = 36u + dataSz;
    const uint16_t ch = 2, bits = 16, blk = 4, pcm = 1;
    const uint32_t fmtSz = 16, sr = (uint32_t) SR, byps = sr * blk;
    std::fwrite ("RIFF", 1, 4, f); std::fwrite (&riff, 4, 1, f);
    std::fwrite ("WAVEfmt ", 1, 8, f);
    std::fwrite (&fmtSz, 4, 1, f); std::fwrite (&pcm, 2, 1, f); std::fwrite (&ch, 2, 1, f);
    std::fwrite (&sr, 4, 1, f); std::fwrite (&byps, 4, 1, f);
    std::fwrite (&blk, 2, 1, f); std::fwrite (&bits, 2, 1, f);
    std::fwrite ("data", 1, 4, f); std::fwrite (&dataSz, 4, 1, f);
    for (uint32_t i = 0; i < n; ++i)
    {
        const int16_t s = (int16_t) std::lround (std::clamp (L[i], -1.0f, 1.0f) * 32000.0f);
        const int16_t st[2] = { s, s };
        std::fwrite (st, 2, 2, f);
    }
    std::fclose (f);
    std::printf ("  RENDER %s  %.2fs  peak %.3f  rms %.4f\n", fn.c_str(), n / SR, peakOf (L), rmsOf (L));
}

inline void renders()
{
    namespace fs = std::filesystem;
    fs::path dir = fs::path (__FILE__).parent_path().parent_path()
                 / "gauntlet" / "renders" / "v2work" / "shapers";
    if (const char* ov = std::getenv ("SATK_RENDER_DIR")) dir = ov;
    std::error_code ec; fs::create_directories (dir, ec);
    // Section outputs straight to file at kAudio volts -> ±1.0, scaled 0.5
    // (comparable to the v1 renders' monitor level 0.5).
    auto scale = [] (std::vector<float>& v) { for (auto& s : v) s *= 0.5f / kAudio; };

    { // mid fold sweep: 220 Hz 0.8 x 2.5 V, FOLD stepped k/7 each second
        std::vector<float> all;
        for (int k = 0; k < 8; ++k)
        {
            VcmRun p; p.fold = (float) k / 7.0f;
            auto y = vcmSine (p, VCM::jMidIn1, VCM::jMidOut, 220.0, 0.8 * kAudio, 48000);
            all.insert (all.end(), y.begin(), y.end());
        }
        scale (all); writeWav16 ((dir / "vcm_mid_foldsweep.wav").string(), all);
    }
    { // bottom at full depth (+ SQ out)
        VcmRun p; p.depth = 1.0f;
        auto y = vcmSine (p, VCM::jBotIn1, VCM::jBotOut, 220.0, 0.8 * kAudio, 5 * 48000);
        scale (y); writeWav16 ((dir / "vcm_bot_full.wav").string(), y);
        auto s = vcmSine (p, VCM::jBotIn1, VCM::jBotSq, 220.0, 0.8 * kAudio, 5 * 48000);
        scale (s); writeWav16 ((dir / "vcm_bot_sq_full.wav").string(), s);
    }
    { // ring: 300 Hz x 80 Hz bipolar, full ring
        RingRun p;
        auto o = ringRun (p, 5 * 48000);
        scale (o); writeWav16 ((dir / "ring_sines.wav").string(), o);
    }
}

// ---------------------------------------------------------------------------
inline void run()
{
    GROUP ("T08 VCM (mid folder, bottom rectifiers + SQ, top LO/HI, alias floor)");
    testMid();
    testMidRealSine();
    testMidAlias();
    testBot();
    testCtlSteps();
    testTop();
    GROUP ("T09 RING (suppression, trims, morph, DC carrier)");
    testRing();
    GROUP ("T13 contribution: VCM/RING self-feedback stress");
    testFeedbackStress();
    GROUP ("T15 surrogate: default voice through the real VCM mid folder");
    double rms = 0, pk = 0;
    testDefaultVoiceSurrogate (rms, pk);
    testPerf();
    if (const char* env = std::getenv ("SATK_RENDER"))
        if (env[0] == '1') renders();
}

inline const bool registered = gauntlet::registerTest ("t_vcm_ring", &run);

} // namespace t_vcm_ring
