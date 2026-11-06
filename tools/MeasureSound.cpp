// Serge Animal TKB v2 — sound-bar measurement tool (satk_measure). Pure C++17.
//
// Measures the SAME pre-registered quantities (GAUNTLET.md "Sound bar" + the
// hard-gate band definitions T03/T04/T05/T07/T08/T09) on the v1 reference
// WAVs and the v2 renders, with one code path per quantity. The analysis
// primitives are the gauntlet's own (tests/t_nto.h: Blackman-Harris spectrum,
// worstNonHarmonic, peakNear, zcFreq; tests/t_vcm_ring.h: Goertzel harmonic
// bank, centroid, odd/even energy, slopeReversals, ampSpectrum;
// tests/t_vcfq.h: decayTime) — applied to file samples instead of jack
// captures.
//
// Usage:
//   satk_measure <renders dir>                 -> markdown table (v1 | v2 | control)
//   satk_measure <renders dir> --key <KEY.md>  -> same rows relabelled A/B/C per
//                                                 the key's "key:" lines (packet table)
// All analysis uses the LEFT channel (every file but default_note60 is
// dual-mono). Samples are int16 / 32000 (the writer convention of both
// renderers).
#include "../tests/t_nto.h"
#include "../tests/t_vcm_ring.h"
#include "../tests/t_vcfq.h"
#include "WavIO.h"
#include <array>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>

namespace {

constexpr double SR = 48000.0;
using Buf = std::vector<float>;

// ---- file access ------------------------------------------------------------
std::string gDir;
std::optional<Buf> loadL (const std::string& rel)
{
    wavio::Stereo s;
    if (! wavio::read16 (gDir + "/" + rel, s) || s.sr != 48000) return std::nullopt;
    return s.L;
}
Buf slice (const Buf& x, double t0, double len)
{
    const size_t a = std::min (x.size(), (size_t) std::lround (t0 * SR));
    const size_t b = std::min (x.size(), a + (size_t) std::lround (len * SR));
    return Buf (x.begin() + (long) a, x.begin() + (long) b);
}
Buf sliceN (const Buf& x, double t0, size_t n)
{
    const size_t a = std::min (x.size(), (size_t) std::lround (t0 * SR));
    return Buf (x.begin() + (long) a, x.begin() + (long) std::min (x.size(), a + n));
}

// ---- analysis built on the gauntlet primitives ------------------------------
// f0 refinement: BH spectrum peak near the nominal, log-parabolic interpolation
double refineF0 (const Buf& x, double nominal)
{
    const auto m = t_nto::spectrum (x);
    const double bin = SR / (double) x.size();
    size_t best = 0;
    for (size_t k = (size_t) (0.97 * nominal / bin); k <= (size_t) (1.03 * nominal / bin) && k + 1 < m.size(); ++k)
        if (best == 0 || m[k] > m[best]) best = k;
    if (best == 0) return nominal;
    const double a = std::log (m[best - 1] + 1e-30), b = std::log (m[best] + 1e-30), c = std::log (m[best + 1] + 1e-30);
    const double d = 0.5 * (a - c) / (a - 2 * b + c);
    return ((double) best + d) * bin;
}

// f0 for a signal whose fundamental may be absent (VCM BOT at full depth is
// ~all H8): the strongest interpolated peak among harmonics 1..hMax of the
// nominal, divided by its order. (Round-2 measurement fix: refining at the
// nominal alone locked onto noise when H1 is -110 dB, and the harmonic bank
// then ran off-frequency -> leakage read as H1 -38 dB.)
double refineF0Harm (const Buf& x, double nominal, int hMax)
{
    const auto m = t_nto::spectrum (x);
    const double bin = SR / (double) x.size();
    double bestAmp = -1.0, bestF = nominal;
    for (int h = 1; h <= hMax; ++h)
    {
        const double fn = nominal * h;
        size_t best = 0;
        for (size_t k = (size_t) (0.99 * fn / bin); k <= (size_t) (1.01 * fn / bin) && k + 1 < m.size(); ++k)
            if (best == 0 || m[k] > m[best]) best = k;
        if (best == 0 || m[best] <= bestAmp) continue;
        bestAmp = m[best];
        const double a = std::log (m[best - 1] + 1e-30), b = std::log (m[best] + 1e-30), c = std::log (m[best + 1] + 1e-30);
        const double d = 0.5 * (a - c) / (a - 2 * b + c);
        bestF = ((double) best + d) * bin / h;
    }
    return bestF;
}

// harmonic amplitudes 1..nH over an integer number of periods of f0 (Goertzel
// bank from t_vcm_ring: leakage-free on whole periods)
std::vector<double> harmonicsWin (const Buf& x, double f0, int nH)
{
    const double periods = std::floor ((double) x.size() * f0 / SR);
    const size_t n = std::min (x.size(), (size_t) std::lround (periods * SR / f0));
    return t_vcm_ring::harmonics (Buf (x.begin(), x.begin() + (long) n), f0, nH);
}

double strongestOf (const std::vector<double>& a)
{ double s = 0; for (size_t h = 1; h < a.size(); ++h) s = std::max (s, a[h]); return s; }

bool strictlyIncreasing (const std::vector<double>& v)
{ for (size_t i = 1; i < v.size(); ++i) if (! (v[i] > v[i - 1])) return false; return true; }

std::string fmt (const char* f, double v) { char b[64]; std::snprintf (b, sizeof b, f, v); return b; }
std::string list (const std::vector<double>& v, const char* f = "%.0f")
{
    std::string s;
    for (size_t i = 0; i < v.size(); ++i) s += (i ? ", " : "") + fmt (f, v[i]);
    return s;
}

// ---- result rows ------------------------------------------------------------
enum class Verdict { In, Out, NA, Info };
struct Cell { std::string text = "—"; Verdict v = Verdict::NA; };
struct Row
{
    std::string piece, gate, quantity, band;
    std::map<std::string, Cell> c; // "v1", "v2", "control"
};
std::vector<Row> gRows;

Cell cell (std::string text, bool inBand) { return { std::move (text), inBand ? Verdict::In : Verdict::Out }; }
Cell info (std::string text) { return { std::move (text), Verdict::Info }; }

using Measure = std::function<std::vector<Cell> (const Buf&)>;
using RowSpec = std::array<std::string, 3>; // gate, quantity, band

// Runs `fn` on the v1 and v2 copies of `file` (and the control when given)
// and appends one table row per returned cell index.
void measure (const std::string& piece, const std::string& file, const std::vector<RowSpec>& rows,
              const Measure& fn, const std::string& controlFile = {})
{
    std::map<std::string, std::vector<Cell>> res;
    for (const char* who : { "v1", "v2" })
        if (auto b = loadL (std::string (who) + "/" + file)) res[who] = fn (*b);
    if (! controlFile.empty())
        if (auto b = loadL (controlFile)) res["control"] = fn (*b);
    for (size_t i = 0; i < rows.size(); ++i)
    {
        Row r { piece, rows[i][0], rows[i][1], rows[i][2], {} };
        for (auto& [who, cells] : res) if (i < cells.size()) r.c[who] = cells[i];
        gRows.push_back (r);
    }
}

// =============================================================================
// per-piece measurements
// =============================================================================
std::vector<Cell> mNtoSaw (const Buf& x)
{
    const auto w = sliceN (x, 0.5, 65536);
    const double f0 = refineF0 (w, 1975.0);
    const auto m = t_nto::spectrum (w);
    double fi = 0, ff = 0;
    const double in = t_nto::worstNonHarmonic (m, w.size(), f0, 6000.0, 20.0, &fi);
    const double full = t_nto::worstNonHarmonic (m, w.size(), f0, 24000.0, 20.0, &ff);
    return { cell (fmt ("%.1f dBc", in) + fmt (" @ %.0f Hz", fi), in <= -60.0),
             cell (fmt ("%.1f dBc", full) + fmt (" @ %.0f Hz", ff), full <= -30.0),
             info (fmt ("%.3f Hz", f0)) };
}

std::vector<Cell> mNtoShape (const Buf& x)
{
    std::vector<double> c;
    double thd1 = 0.0;
    for (int k = 0; k < 8; ++k)
    {
        const double f0 = refineF0 (sliceN (x, k + 0.2, 32768), 220.0);
        const auto a = harmonicsWin (slice (x, k + 0.2, 0.75), f0, 100);
        c.push_back (t_vcm_ring::centroid (a, f0));
        if (k == 0)
        {
            double s2 = 0; for (int h = 2; h <= 40; ++h) s2 += a[(size_t) h] * a[(size_t) h];
            thd1 = 100.0 * std::sqrt (s2) / std::max (a[1], 1e-12);
        }
    }
    return { cell (list (c), strictlyIncreasing (c)), info (fmt ("%.2f %%", thd1)) };
}

// waveform turnarounds per period -> static-transfer slope reversals over one
// input sweep: a sine through a memoryless curve with r reversals shows
// 2 + 2r extrema per period. Hysteresis 2 % of the window peak (the gate
// test uses 0.05 V of 2.5 V).
double transferReversals (const Buf& w, double f0)
{
    const int ext = t_vcm_ring::slopeReversals (w, 0.02 * wavio::peak (w));
    const double periods = (double) w.size() * f0 / SR;
    return ((double) ext / periods - 2.0) / 2.0;
}

std::vector<Cell> mVcmMid (const Buf& x)
{
    std::vector<double> c, aMax, lvl, pkl;
    double f0max = 220.0, thd1 = 0.0;
    for (int k = 0; k < 8; ++k)
    {
        const double f0 = refineF0 (sliceN (x, k + 0.2, 32768), 220.0);
        const auto w = slice (x, k + 0.2, 0.75);
        const auto a = harmonicsWin (w, f0, 100);
        c.push_back (t_vcm_ring::centroid (a, f0));
        lvl.push_back (gauntlet::db (wavio::rms (w), 1.0));
        pkl.push_back (gauntlet::db (wavio::peak (w), 1.0));
        if (k == 0)
        {
            double s2 = 0; for (int h = 2; h <= 40; ++h) s2 += a[(size_t) h] * a[(size_t) h];
            thd1 = 100.0 * std::sqrt (s2) / std::max (a[1], 1e-12);
        }
        if (k == 7) { aMax = a; f0max = f0; }
    }
    const double lvlSpan = *std::max_element (lvl.begin(), lvl.end()) - *std::min_element (lvl.begin(), lvl.end());
    const double pkSpan = *std::max_element (pkl.begin(), pkl.end()) - *std::min_element (pkl.begin(), pkl.end());
    // round 3 (critic: clicks at every FOLD step): 4-12 kHz energy in
    // 512-sample Hann windows around each 1 s step vs the steady state
    // 0.3 s before/after (t_vcm_ring::stepExcessDb, the gate test's measure)
    double clickWorst = -1e9;
    for (int k = 1; k < 8; ++k) clickWorst = std::max (clickWorst, t_vcm_ring::stepExcessDb (x, (size_t) k * 48000));
    const double s = strongestOf (aMax);
    int oddFund = 0, oddMax = 0;
    for (size_t h = 1; h < aMax.size(); h += 2)
    {
        if (aMax[h] >= 0.01 * aMax[1]) ++oddFund;
        if (aMax[h] >= 0.01 * s) ++oddMax;
    }
    double odd, even; t_vcm_ring::oddEvenEnergy (aMax, odd, even);
    const double oe = 10.0 * std::log10 (odd / std::max (even, 1e-30));
    const double rev = transferReversals (slice (x, 7.2, 0.75), f0max);
    const auto sp = sliceN (x, 7.2, 32768);
    const double alias = t_nto::worstNonHarmonic (t_nto::spectrum (sp), sp.size(), f0max, 24000.0, 20.0);
    return { cell (fmt ("%.0f", oddFund) + fmt (" re H1 / %.0f re max", oddMax), oddFund >= 5 && oddMax >= 5),
             cell (fmt ("%.1f dB", oe), oe >= 12.0),
             cell (list (c), strictlyIncreasing (c)),
             cell (fmt ("%.1f", rev), rev >= 2.0),
             info (fmt ("%.1f dBc", alias)),
             info (fmt ("%.2f %%", thd1)),
             info (list (lvl, "%.1f") + fmt (" (span %.1f dB)", lvlSpan)),
             info (list (pkl, "%.1f") + fmt (" (span %.1f dB)", pkSpan)),
             info (fmt ("%+.1f dB", clickWorst)) };
}

std::vector<Cell> mVcmBot (const Buf& x)
{
    // steady state 1.0-4.5 s (round-2 fix: past any onset/fade-in and the
    // output DC blocker's settling); f0 from the strongest harmonic
    const double f0 = refineF0Harm (sliceN (x, 1.0, 65536), 220.0, 16);
    const auto a = harmonicsWin (slice (x, 1.0, 3.5), f0, 100);
    double odd, even; t_vcm_ring::oddEvenEnergy (a, odd, even);
    const double eo = 10.0 * std::log10 (even / std::max (odd, 1e-30));
    const double s = strongestOf (a);
    std::vector<double> h116;
    size_t hs = 1;
    for (size_t h = 1; h < a.size(); ++h) if (a[h] > a[hs]) hs = h;
    for (int h = 1; h <= 16; ++h) h116.push_back (gauntlet::db (a[(size_t) h], s));
    const auto sp = sliceN (x, 1.0, 65536);
    const auto m = t_nto::spectrum (sp);
    double fi = 0;
    // worst non-harmonic re the STRONGEST harmonic (worstNonHarmonic is re H1)
    const double reH1 = t_nto::worstNonHarmonic (m, sp.size(), f0, 24000.0, 20.0, &fi);
    const double reMax = reH1 + gauntlet::db (t_nto::peakNear (m, sp.size(), f0, 3.0), t_nto::peakNear (m, sp.size(), f0 * (double) hs, 3.0));
    // round 4: the same re-strongest figure restricted to 0-20 kHz (the
    // full-band worst sits in the output halfband's transition band)
    double fi20 = 0;
    const double reMax20 = t_nto::worstNonHarmonic (m, sp.size(), f0, 20000.0, 20.0, &fi20)
                         + (reMax - reH1);
    return { cell (fmt ("%+.1f dB", eo), eo >= 12.0), info (list (h116, "%.0f")),
             info (fmt ("H%.0f", (double) hs)),
             info (fmt ("%.1f dB", reMax) + fmt (" @ %.0f Hz", fi)),
             info (fmt ("%.1f dB", reMax20) + fmt (" @ %.0f Hz", fi20)) };
}

std::vector<Cell> mPing (const Buf& x)
{
    // per 2 s Q step: detect pings (onset = first sample above 10 % of the
    // step's peak, then a 0.45 s window), peak -> -40 dB (t_vcfq::decayTime);
    // median over the step's pings
    std::vector<double> med, amp;
    double ringHz = 0.0;
    for (int k = 0; k < 4; ++k)
    {
        const auto st = slice (x, 2.0 * k, 2.0);
        const double thr = 0.1 * wavio::peak (st);
        std::vector<double> d; double pkSum = 0;
        for (size_t i = 0; i < st.size(); ++i)
        {
            if (std::fabs (st[i]) <= thr) continue;
            const size_t a = i > 48 ? i - 48 : 0, b = std::min (st.size(), i + (size_t) (0.45 * SR));
            const Buf seg (st.begin() + (long) a, st.begin() + (long) b);
            float p = 0;
            d.push_back (1000.0 * t_vcfq::decayTime (seg, p));
            pkSum += p;
            // round 3: ring frequency from the free ring only — 5 ms past the
            // onset (seg starts 1 ms before it), 40 ms long; the r1/r2 window
            // started at the pulse and included its forced part
            if (k == 3 && ringHz == 0.0 && seg.size() > 288 + 1920)
                ringHz = t_nto::zcFreq (Buf (seg.begin() + 288, seg.begin() + 288 + 1920));
            i = b;
        }
        std::sort (d.begin(), d.end());
        med.push_back (d.empty() ? 0.0 : d[d.size() / 2]);
        amp.push_back (d.empty() ? 0.0 : pkSum / (double) d.size());
    }
    return { cell (list (med, "%.1f") + " ms", strictlyIncreasing (med) && med.front() > 0.0),
             info (list (amp, "%.3f")), info (fmt ("%.1f Hz", ringHz)) };
}

// T05d line fit (shared by dusg_env and dusg_cycle): least-squares line
// through the points of [a, b) whose value lies in 10..90 % of the swing
// (lo + 0.1·span .. lo + 0.9·span), max deviation in % of the swing.
// Returns 100 when fewer than 3 points qualify. t1090 = point span (index units).
double lineDevPct (const std::vector<double>& y, size_t a, size_t b, double lo, double span, double* t1090 = nullptr)
{
    std::vector<std::pair<double, double>> pts;
    for (size_t k = a; k < b; ++k)
        if (y[k] >= lo + 0.1 * span && y[k] <= lo + 0.9 * span) pts.push_back ({ (double) k, y[k] });
    if (pts.size() < 3) return 100.0;
    if (t1090 != nullptr) *t1090 = pts.back().first - pts.front().first;
    double sx = 0, sy = 0, sxx = 0, sxy = 0; const double N = (double) pts.size();
    for (auto& p : pts) { sx += p.first; sy += p.second; sxx += p.first * p.first; sxy += p.first * p.second; }
    const double bb = (N * sxy - sx * sy) / (N * sxx - sx * sx), aa = (sy - bb * sx) / N;
    double dev = 0;
    for (auto& p : pts) dev = std::max (dev, std::fabs (p.second - (aa + bb * p.first)));
    return 100.0 * dev / span;
}

std::vector<Cell> mDusgCycle (const Buf& x)
{
    const auto w = slice (x, 1.0, 4.0);
    size_t up = 0, n = 0;
    for (size_t i = 1; i < w.size(); ++i) if (w[i] != w[i - 1]) { ++n; if (w[i] > w[i - 1]) ++up; }
    const double frac = (double) up / (double) std::max<size_t> (1, n);
    // round 3: T05d linearity on the cycle ramps too (critic request). Each
    // rise and each fall between consecutive turning points (local extrema
    // of the 200 Hz triangle, 1-5 s), the same 10..90 % least-squares line
    // as dusg_env, max deviation in % of that segment's swing; worst over
    // all segments. (The output bus's 10 Hz DC blocker bends a 200 Hz ramp
    // by a small, identical amount in both versions.)
    std::vector<double> y (w.begin(), w.end());
    std::vector<size_t> turn;
    for (size_t i = 1; i + 1 < y.size(); ++i)
    {
        const bool mx = y[i] >= y[i - 1] && y[i] > y[i + 1];
        const bool mn = y[i] <= y[i - 1] && y[i] < y[i + 1];
        if ((mx || mn) && (turn.empty() || i - turn.back() > 40)) turn.push_back (i);
    }
    double worstR = 0, worstF = 0; int segs = 0;
    for (size_t t = 1; t < turn.size(); ++t)
    {
        const size_t a = turn[t - 1], b = turn[t] + 1;
        const double lo = std::min (y[a], y[turn[t]]), span = std::fabs (y[turn[t]] - y[a]);
        if (span <= 0.0) continue;
        const double d = lineDevPct (y, a, b, lo, span);
        if (y[turn[t]] > y[a]) worstR = std::max (worstR, d); else worstF = std::max (worstF, d);
        ++segs;
    }
    return { cell (fmt ("%.3f", frac), std::fabs (frac - 0.5) <= 0.01), info (fmt ("%.2f Hz", t_nto::zcFreq (w))),
             cell (fmt ("rise %.2f", worstR) + fmt (" / fall %.2f %% FS", worstF) + fmt (" (%.0f ramps)", (double) segs),
                   segs > 100 && std::max (worstR, worstF) <= 3.0) };
}

std::vector<Cell> mDusgEnv (const Buf& x)
{
    // envelope of the 1 kHz carrier: max |x| per 1 ms (48-sample) block
    std::vector<double> env;
    for (size_t i = 0; i + 48 <= x.size(); i += 48)
    {
        double p = 0;
        for (size_t j = i; j < i + 48; ++j) p = std::max (p, (double) std::fabs (x[j]));
        env.push_back (p);
    }
    double worstRise = 0, worstFall = 0, riseT = 0, fallT = 0, worstH3 = -300.0;
    const double starts[3] = { 0.3, 2.4, 4.5 };
    for (double st : starts)
    {
        const size_t k0 = (size_t) (st * 1000.0), k1 = std::min (env.size(), k0 + 2000);
        size_t kp = k0;
        for (size_t k = k0; k < k1; ++k) if (env[k] > env[kp]) kp = k;
        const double fs = env[kp];
        // T05d: mid 80 % of the swing (10..90 % FS), least-squares line, max deviation
        auto lineDev = [&] (size_t a, size_t b, double& t1090)
        {
            double n1090 = 0.0;
            const double d = lineDevPct (env, a, b, 0.0, fs, &n1090);
            t1090 = n1090 / 1000.0;
            return d;
        };
        double rt = 0, ft = 0;
        // round 3: carrier purity at the top of the swing (critic: a 3 kHz
        // partial) — H3 re H1 of the 1 kHz carrier, 50 ms ending at the peak
        if (kp >= 50)
        {
            const auto a = harmonicsWin (slice (x, (double) (kp - 50) / 1000.0, 0.05), 1000.0, 5);
            worstH3 = std::max (worstH3, gauntlet::db (a[3], a[1]));
        }
        worstRise = std::max (worstRise, lineDev (k0, kp + 1, rt));
        worstFall = std::max (worstFall, lineDev (kp, k1, ft));
        riseT += rt / 3.0; fallT += ft / 3.0;
    }
    return { cell (fmt ("%.2f %% FS", worstRise), worstRise <= 3.0),
             info (fmt ("%.2f %% FS", worstFall)),
             info (fmt ("%.3f s", riseT / 0.8) + fmt (" / %.3f s", fallT / 0.8)),
             info (fmt ("%.1f dBc", worstH3)) };
}

std::vector<Cell> mDusgDirect (const Buf& x)
{
    return { info (fmt ("peak %.4f", wavio::peak (x)) + fmt (", rms %.5f", wavio::rms (x))) };
}

std::vector<Cell> mRing (const Buf& x)
{
    const auto w = sliceN (x, 0.5, 131072);
    const auto m = t_vcm_ring::ampSpectrum (w);
    const size_t n = w.size();
    const double bin = SR / (double) n;
    // modulator frequency from the strongest lower sideband in 200..270 Hz
    size_t kb = (size_t) (200.0 / bin);
    for (size_t k = kb; k <= (size_t) (270.0 / bin); ++k) if (m[k] > m[kb]) kb = k;
    const double fy = 300.0 - (double) kb * bin;
    const double lsb = t_nto::peakNear (m, n, 300.0 - fy, 3.0), usb = t_nto::peakNear (m, n, 300.0 + fy, 3.0);
    const double car = t_nto::peakNear (m, n, fy, 3.0), sig = t_nto::peakNear (m, n, 300.0, 3.0);
    const double sY = gauntlet::db (std::min (lsb, usb), car), sX = gauntlet::db (std::min (lsb, usb), sig);
    // strongest component 30 Hz .. 2 kHz, the modulator leak and the X±2·fY
    // products (a pure multiplier fed a symmetric triangle has none) re it
    double strongest = 0;
    for (size_t k = (size_t) (30.0 / bin); k < (size_t) (2000.0 / bin); ++k) strongest = std::max (strongest, m[k]);
    const double p2l = t_nto::peakNear (m, n, 300.0 - 2.0 * fy, 3.0), p2u = t_nto::peakNear (m, n, 300.0 + 2.0 * fy, 3.0);
    return { cell (fmt ("%.1f dB", sY), sY >= 55.0 && sY <= 85.0),
             cell (fmt ("%.1f dB", sX), sX >= 55.0 && sX <= 85.0),
             info (fmt ("%.1f Hz", fy)),
             info (fmt ("%.1f dB", gauntlet::db (car, strongest))),
             info (fmt ("%.1f", gauntlet::db (p2l, strongest)) + fmt (" / %.1f dB", gauntlet::db (p2u, strongest))) };
}
// listening-only patch (unipolar modulator): same numbers, informational
std::vector<Cell> mRingInfo (const Buf& x)
{
    auto c = mRing (x);
    for (auto& e : c) e.v = Verdict::Info;
    return c;
}

std::vector<Cell> mRingMorph (const Buf& x)
{
    std::vector<double> sig, sb;
    for (int k = 0; k <= 8; ++k)
    {
        const auto w = sliceN (x, k + 0.25, 32768);
        const auto m = t_vcm_ring::ampSpectrum (w);
        sig.push_back (t_nto::peakNear (m, w.size(), 300.0, 4.0));
        sb.push_back (t_nto::peakNear (m, w.size(), 220.0, 4.0) + t_nto::peakNear (m, w.size(), 380.0, 4.0));
    }
    bool mono = true;
    for (size_t i = 1; i < sig.size(); ++i) mono &= sig[i] < sig[i - 1] && sb[i] > sb[i - 1];
    std::vector<double> sd, bd;
    for (size_t i = 0; i < sig.size(); i += 2)
    { sd.push_back (gauntlet::db (sig[i], 1.0)); bd.push_back (gauntlet::db (sb[i], 1.0)); }
    return { cell ("X " + list (sd, "%.0f") + " / SB " + list (bd, "%.0f") + " dBFS (steps 0,2,4,6,8)", mono) };
}

std::vector<Cell> mEndDuty (const Buf& x)
{
    // END through the DC-blocked monitor: its edges are the big sample steps
    const double fr[5] = { 0.05, 0.25, 0.50, 0.75, 0.95 };
    std::vector<double> duty; double worst = 0;
    for (int k = 0; k < 5; ++k)
    {
        const auto w = slice (x, 1.5 * k + 0.3, 1.1);
        std::vector<double> up, dn;
        for (size_t i = 1; i < w.size(); ++i)
        {
            const double d = (double) w[i] - (double) w[i - 1];
            if (d > 0.05 && (up.empty() || (double) i - up.back() > 4)) up.push_back ((double) i);
            if (d < -0.05 && (dn.empty() || (double) i - dn.back() > 4)) dn.push_back ((double) i);
        }
        double acc = 0; int cnt = 0;
        for (size_t u = 0; u + 1 < up.size(); ++u)
        {
            auto it = std::upper_bound (dn.begin(), dn.end(), up[u]);
            if (it == dn.end() || *it > up[u + 1]) continue;
            acc += (*it - up[u]) / (up[u + 1] - up[u]); ++cnt;
        }
        duty.push_back (cnt ? acc / cnt : 0.0);
        worst = std::max (worst, std::fabs (duty.back() - fr[k]));
    }
    return { cell (list (duty, "%.3f") + fmt (" (worst err %.3f)", worst), worst <= 0.01) };
}

std::vector<Cell> mDefault (const Buf& x)
{
    // round-2 fix: RMS over the SOUNDING span (first..last sample above 1 %
    // of the file peak) instead of a fixed 1 s window that was ~75 % silence
    // for a ~250 ms pluck
    const double thr = 0.01 * wavio::peak (x);
    size_t a = 0, b = x.size();
    while (a < x.size() && std::fabs (x[a]) <= thr) ++a;
    while (b > a && std::fabs (x[b - 1]) <= thr) --b;
    const Buf span (x.begin() + (long) a, x.begin() + (long) b);
    return { info (fmt ("%.4f", wavio::rms (span)) + fmt (" over %.3f s", (double) span.size() / SR)),
             info (fmt ("%.4f", wavio::peak (x))),
             info (fmt ("%.5f", wavio::peak (slice (x, 3.0, 1.5)))) };
}

void runAll()
{
    measure ("nto_saw", "nto_saw_1975.wav",
             { { "T03", "worst non-harmonic, 0–6 kHz (harmonics ±20 Hz excluded)", "≤ −60 dBc" },
               { "T03", "worst non-harmonic, 0–24 kHz", "≤ −30 dBc" },
               { "info", "f0 (BH-FFT peak, interpolated)", "1975 Hz" } },
             mNtoSaw, "control/nto_saw_1975_naive_zoh12k.wav");
    measure ("nto_shape", "nto_shape_sweep.wav",
             { { "T04 (extra)", "harmonic centroid per shape step 1..8 (Hz)", "strictly increasing" },
               { "T04 (info)", "THD at minimum shape (step 1, H2..H40)", "≤ 2 %" } },
             mNtoShape);
    measure ("vcm_mid_fold", "vcm_mid_foldsweep.wav",
             { { "T08", "odd harmonics ≥ −40 dBc at max fold (count)", "≥ 5 (re H1 and re max)" },
               { "T08", "odd:even energy at max fold", "≥ 12 dB" },
               { "T08", "harmonic centroid per fold step 1..8 (Hz)", "strictly increasing" },
               { "T08 (proxy)", "transfer slope reversals at max fold (from waveform extrema)", "≥ 2" },
               { "info", "worst non-harmonic at max fold, 220 Hz (T08's AA band is specified at 1 kHz)", "(−50 dBc @ 1 kHz)" },
               { "info", "THD at minimum fold (step 1, H2..H40)", "—" },
               { "info", "RMS per fold step 1..8 (dBFS)", "—" },
               { "info", "peak per fold step 1..8 (dBFS; round 4)", "—" },
               { "info", "click at the FOLD steps: worst 4–12 kHz excess over steady state (7 steps)", "—" } },
             mVcmMid);
    measure ("vcm_bot", "vcm_bot_full.wav",
             { { "T08", "even:odd energy at full depth (steady state 1.0–4.5 s)", "≥ +12 dB" },
               { "info", "H1..H16 dB re strongest", "—" },
               { "info", "strongest harmonic", "H8 (3 octave stages)" },
               { "info", "worst non-harmonic re strongest harmonic, 0–24 kHz", "—" },
               { "info", "worst non-harmonic re strongest harmonic, 0–20 kHz (round 4)", "—" } },
             mVcmBot, "control/vcm_bot_single_abs.wav");
    measure ("vcfq_ping", "vcfq_ping.wav",
             { { "T07", "ping decay peak→−40 dB, median per Q step (Q 2/6/15/35)", "strictly increasing" },
               { "info", "mean ping peak per Q step (FS)", "—" },
               { "info", "ring frequency, first Q=35 ping (zero crossings, 5–45 ms after onset)", "fc = 800 Hz" } },
             mPing);
    measure ("dusg_cycle", "dusg_cycle.wav",
             { { "T05c (info)", "rising fraction of the MAIN OUT cycle, rise = fall dialed (slope symmetry; not END)", "0.50 ± 0.01" },
               { "info", "cycle frequency", "200 Hz" },
               { "T05d", "cycle ramps: mid-80 % deviation from a line, worst rise / fall over all ramps 1–5 s", "≤ 3 % FS" } },
             mDusgCycle);
    // round-2 fix: T05e is measured on END itself (pair renders, both versions)
    measure ("dusg_end", "supplementary/dusg_end_duty.wav",
             { { "T05e", "END duty at rise/(rise+fall) = .05/.25/.50/.75/.95", "tracks ratio ±0.01" } },
             mEndDuty);
    measure ("dusg_env", "dusg_env_shape.wav",
             { { "T05d", "mid-80 % rise deviation from a line (worst of 3)", "≤ 3 % FS" },
               { "info", "mid-80 % fall deviation from a line (worst of 3)", "—" },
               { "T05a (info)", "rise / fall time (10–90 % ÷ 0.8), dialed 0.5 / 1.0 s", "±10 %" },
               { "info", "1 kHz carrier H3 re H1, 50 ms up to each rise peak (worst of 3)", "—" } },
             mDusgEnv, "control/dusg_env_rc.wav");
    measure ("dusg_env_direct", "dusg_env_shape_monitor_direct.wav",
             { { "info", "slow envelope straight through the monitor bus", "—" } },
             mDusgDirect);
    // T09 band patch (amended 2026-11-05 with Mark's approval): the BIPOLAR
    // (AC-coupled) modulator render; the unipolar ring_sines patch is
    // listening material only (informational rows)
    measure ("ring", "supplementary/ring_bipolar.wav",
             { { "T09", "suppression of Y (modulator, ~80 Hz) re weaker sideband (bipolar modulator)", "55–85 dB" },
               { "T09", "suppression of X (300 Hz sine) re weaker sideband (bipolar modulator)", "55–85 dB" },
               { "info", "modulator frequency (from sideband spacing)", "~80 Hz" },
               { "info", "modulator (~80 Hz) leak re strongest component", "—" },
               { "info", "X ± 2·fY products (≈140 / 460 Hz) re strongest", "—" } },
             mRing, "control/ring_bipolar_leaky.wav");
    measure ("ring_unipolar", "ring_sines.wav",
             { { "info", "Y suppression re weaker sideband (unipolar 0..5 V modulator: AM by construction)", "(listening only)" },
               { "info", "X suppression re weaker sideband (unipolar modulator)", "(listening only)" },
               { "info", "modulator frequency", "~80 Hz" },
               { "info", "modulator (~80 Hz) leak re strongest component", "—" },
               { "info", "X ± 2·fY products (≈140 / 460 Hz) re strongest", "—" } },
             mRingInfo);
    measure ("default_voice", "default_note60.wav",
             { { "info", "RMS over the sounding span (|x| > 1 % of peak), L", "—" },
               { "info", "peak (L)", "—" },
               { "info", "peak 3.0–4.5 s (tail)", "—" } },
             mDefault);
}

// v2-only supplementary rows (no v1 counterpart file)
std::vector<Row> runSupplementary()
{
    std::vector<Row> out;
    auto add = [&] (const std::string& file, const std::vector<RowSpec>& rows, const Measure& fn)
    {
        auto b = loadL ("v2/supplementary/" + file);
        if (! b) return;
        auto cells = fn (*b);
        for (size_t i = 0; i < rows.size() && i < cells.size(); ++i)
            out.push_back ({ file, rows[i][0], rows[i][1], rows[i][2], { { "v2", cells[i] } } });
    };
    add ("ring_morph_sweep.wav",
         { { "T09", "morph 9 steps: X falls, sidebands rise", "strictly monotonic" } }, mRingMorph);
    return out;
}

const char* vtext (Verdict v)
{
    switch (v)
    {
        case Verdict::In:  return "IN";
        case Verdict::Out: return "**OUT**";
        case Verdict::NA:
        case Verdict::Info: break;
    }
    return "—";
}

void printCells (const Row& r, const std::vector<std::string>& who)
{
    std::printf ("| %s | %s | %s | %s |", r.piece.c_str(), r.gate.c_str(), r.quantity.c_str(), r.band.c_str());
    for (auto& w : who)
    {
        auto it = r.c.find (w);
        if (it == r.c.end()) std::printf (" | |");
        else std::printf (" %s | %s |", it->second.text.c_str(), vtext (it->second.v));
    }
    std::printf ("\n");
}

void printVersionTable()
{
    std::printf ("| piece | gate | quantity | band | v1 | v1 | v2 | v2 | control | control |\n"
                 "|---|---|---|---|---|---|---|---|---|---|\n");
    for (auto& r : gRows) printCells (r, { "v1", "v2", "control" });
    std::printf ("\n| file (v2 only) | gate | quantity | band | v2 | v2 |\n|---|---|---|---|---|---|\n");
    for (auto& r : runSupplementary()) printCells (r, { "v2" });
}

// key lines: "key: <piece> A=v1 B=v2 [C=control]"
void printKeyedTable (const std::string& keyFile)
{
    std::ifstream in (keyFile);
    std::map<std::string, std::map<std::string, std::string>> key; // piece -> letter -> source
    std::string line;
    while (std::getline (in, line))
    {
        const auto p = line.find ("key:");
        if (p == std::string::npos) continue;
        std::istringstream ss (line.substr (p + 4));
        std::string piece, tok;
        ss >> piece;
        while (ss >> tok)
        {
            const auto eq = tok.find ('=');
            if (eq != std::string::npos) key[piece][tok.substr (0, eq)] = tok.substr (eq + 1);
        }
    }
    std::printf ("| piece | gate | quantity | band | A | A | B | B | C | C |\n"
                 "|---|---|---|---|---|---|---|---|---|---|\n");
    for (auto& r : gRows)
    {
        Row rr = r; rr.c.clear();
        for (auto& [letter, src] : key[r.piece])
            if (auto it = r.c.find (src); it != r.c.end()) rr.c[letter] = it->second;
        printCells (rr, { "A", "B", "C" });
    }
}

} // namespace

int main (int argc, char** argv)
{
    if (argc < 2) { std::fprintf (stderr, "usage: satk_measure <renders dir> [--key KEY.md]\n"); return 2; }
    gDir = argv[1];
    runAll();
    if (argc >= 4 && std::string (argv[2]) == "--key") printKeyedTable (argv[3]);
    else printVersionTable();
    return 0;
}
