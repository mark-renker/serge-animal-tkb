// Serge Animal TKB v2 — NOISE/RVG/VCA gauntlet tests.
// T10: white ~ flat (+-6 dB trend 100 Hz-6.4 kHz); pink falls 3 +- 1.5 dB/oct;
//      S/H source bounded 0..5 V with no autocorrelation peak > 0.5 beyond
//      10 ms lag over 5 s; RVG stepped changes only on trigger, smooth output
//      is the slewed version.
// T11: exponential law ~ 12 dB/V over the top of the range within +-1.5 dB;
//      <= -60 dB below 0.5 V CV; max gain +6 dB +- 1 dB; click bound: unity
//      1 kHz sine, CV stepped 0->5 V -> energy in 5-20 kHz within the
//      following 50 ms <= -35 dBFS.
// Also: per-module perf (T16 protocol) print, and SATK_RENDER=1 evidence WAV.
#pragma once
#include "Harness.h"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace t_noise_vca {

using namespace gauntlet;
using namespace satk;

// ---------------------------------------------------------------------------
// harness helpers (local to this file)
// ---------------------------------------------------------------------------

// Feed a test-driven float into an In jack (bypasses the cable graph; the
// pointer must outlive the rig and nothing may call rebuild() afterwards).
inline void inject (Module* m, int jack, const float* src)
{
    m->feeds[(size_t) jack].src.push_back (src);
}

struct Tap { Module* m; int j; std::vector<float>* buf; };

inline void renderTaps (Rig& r, int n, const std::vector<Tap>& taps,
                        const std::function<void (int)>& pre = {})
{
    for (int i = 0; i < n; ++i)
    {
        if (pre) pre (i);
        r.sys.engine.tick();
        for (auto& t : taps) t.buf->push_back (t.m->jacks[(size_t) t.j].value);
    }
}

// Average power (dB) across K log-spaced Goertzel bins inside [fLo, 2*fLo).
inline double octaveBandDb (const std::vector<float>& buf, double fLo, int K)
{
    std::vector<Goertzel> g ((size_t) K);
    for (int k = 0; k < K; ++k)
        g[(size_t) k].init (fLo * std::pow (2.0, (k + 0.5) / (double) K));
    for (float v : buf)
        for (auto& b : g) b.push (v);
    double acc = 0;
    for (auto& b : g) acc += b.amp() * b.amp();
    return 10.0 * std::log10 (std::max (acc / K, 1e-30));
}

// Max |normalized autocovariance| over the given lag list.
inline double maxAbsAutocorr (const std::vector<float>& x, const std::vector<int>& lags)
{
    const int n = (int) x.size();
    double mean = 0;
    for (float v : x) mean += v;
    mean /= n;
    std::vector<float> d ((size_t) n);
    double r0 = 0;
    for (int i = 0; i < n; ++i) { d[(size_t) i] = (float) (x[(size_t) i] - mean); r0 += (double) d[(size_t) i] * d[(size_t) i]; }
    r0 /= n;
    double worst = 0;
    for (int l : lags)
    {
        double r = 0;
        for (int i = 0; i + l < n; ++i) r += (double) d[(size_t) i] * d[(size_t) (i + l)];
        r /= (double) (n - l) * r0;
        worst = std::max (worst, std::fabs (r));
    }
    return worst;
}

// ---------------------------------------------------------------------------
// T10 — NOISE spectra + S/H source statistics
// ---------------------------------------------------------------------------
inline void runT10Noise()
{
    GROUP ("T10 NOISE: white/pink spectra, S/H source");

    Rig r;
    r.step (9600); // 200 ms warmup (pink filter settle)
    std::vector<float> white, pink, sh;
    const int n = 480000; // 10 s
    white.reserve ((size_t) n); pink.reserve ((size_t) n); sh.reserve ((size_t) n);
    renderTaps (r, n, { { r.sys.noise, NOISE::jWhite, &white },
                        { r.sys.noise, NOISE::jPink,  &pink  },
                        { r.sys.noise, NOISE::jShSrc, &sh    } });

    // amplitude sanity: white bounded +-2.5 V, pink within rails
    {
        float wPeak = 0, pPeak = 0;
        for (float v : white) wPeak = std::max (wPeak, std::fabs (v));
        for (float v : pink)  pPeak = std::max (pPeak, std::fabs (v));
        CHECK (wPeak <= kAudio + 1e-4f && pPeak < kRails,
               "white bounded +-2.5 V; pink within rails", wPeak, pPeak);
    }

    // octave band levels, 100..6400 Hz (6 octaves, 96 bins each)
    double wBand[6], pBand[6];
    for (int k = 0; k < 6; ++k)
    {
        const double fLo = 100.0 * std::pow (2.0, k);
        wBand[k] = octaveBandDb (white, fLo, 96);
        pBand[k] = octaveBandDb (pink,  fLo, 96);
    }

    // white: flat within +-6 dB trend over 100 Hz-6.4 kHz
    {
        double mean = 0;
        for (double v : wBand) mean += v;
        mean /= 6.0;
        double dev = 0;
        for (double v : wBand) dev = std::max (dev, std::fabs (v - mean));
        CHECK (dev <= 6.0, "white ~ flat: +-6 dB trend 100 Hz-6.4 kHz", dev);
    }

    // pink: falls 3 +- 1.5 dB/oct (regression slope AND every adjacent octave)
    {
        double xm = 2.5, ym = 0;
        for (double v : pBand) ym += v;
        ym /= 6.0;
        double num = 0, den = 0;
        for (int k = 0; k < 6; ++k) { num += (k - xm) * (pBand[k] - ym); den += (k - xm) * (k - xm); }
        const double slope = num / den;
        bool adj = true;
        double worstAdj = -3.0;
        for (int k = 0; k < 5; ++k)
        {
            const double dd = pBand[k + 1] - pBand[k];
            if (dd < -4.5 || dd > -1.5) adj = false;
            if (std::fabs (dd + 3.0) > std::fabs (worstAdj + 3.0)) worstAdj = dd;
        }
        CHECK (slope >= -4.5 && slope <= -1.5 && adj,
               "pink falls 3 +- 1.5 dB/oct (slope, all octaves)", slope, worstAdj);
    }

    // S/H source: bounded 0..5 V
    {
        float lo = 99, hi = -99;
        for (float v : sh) { lo = std::min (lo, v); hi = std::max (hi, v); }
        CHECK (lo >= 0.0f && hi <= 5.0f, "S/H source bounded 0..5 V", lo, hi);
    }

    // S/H source: no autocorrelation peak > 0.5 beyond 10 ms lag over 5 s
    {
        std::vector<float> x (sh.begin(), sh.begin() + 240000); // 5 s
        std::vector<int> lags;
        for (int l = 480; l <= 9600; l += 4) lags.push_back (l);      // 10..200 ms dense
        for (int l = 10080; l <= 96000; l += 480) lags.push_back (l); // ..2 s sparse
        const double worst = maxAbsAutocorr (x, lags);
        CHECK (worst <= 0.5,
               "S/H source: no autocorr peak > 0.5 beyond 10 ms over 5 s", worst);
    }

    // extra (hardware "equi-probable"): 10-bin histogram ~ uniform
    {
        int h[10] = {};
        for (float v : sh) ++h[std::min (9, (int) (v * 2.0f))];
        double lo = 1, hi = 0;
        for (int b : h)
        {
            const double f = b / (double) sh.size();
            lo = std::min (lo, f); hi = std::max (hi, f);
        }
        CHECK (lo > 0.08 && hi < 0.12, "S/H source equi-probable (10-bin hist ~10%)", lo, hi);
    }
}

// ---------------------------------------------------------------------------
// T10 — RVG trigger semantics + slewed smooth
// ---------------------------------------------------------------------------
inline void runT10Rvg()
{
    GROUP ("T10 RVG: stepped on trigger only, smooth slewed, random clock");

    // (a) external TRIG with internal clock parked at 0.05 Hz (first internal
    //     fire cannot land before 10 s): stepped changes ONLY on trigger.
    {
        Rig r;
        auto* rvg = r.sys.rvg;
        rvg->ctls[RVG::cRate].v = 0.0f; // expo 0.05..50 Hz -> 0.05 Hz
        float trig = 0.0f;
        inject (rvg, RVG::jTrig, &trig);

        const int period = 4800, nPulse = 20, n = period * nPulse;
        int changes = 0;
        bool changesOnEdgeOnly = true, pulseSilent = true, bounded = true;
        float prevStep = rvg->jacks[RVG::jStepped].value;
        for (int i = 0; i < n; ++i)
        {
            trig = (i % period) < 96 ? kCV : 0.0f;
            r.sys.engine.tick();
            const float st = rvg->jacks[RVG::jStepped].value;
            if (st != prevStep)
            {
                ++changes;
                if (i % period != 0) changesOnEdgeOnly = false; // edge samples only
            }
            prevStep = st;
            if (st < 0.0f || st > 5.0f) bounded = false;
            if (rvg->jacks[RVG::jPulse].value > kLogicHi) pulseSilent = false;
        }
        CHECK (changes == nPulse && changesOnEdgeOnly && bounded,
               "stepped changes ONLY on trigger (20 ext trigs -> 20 changes)",
               changes, nPulse);
        CHECK (pulseSilent, "PULSE mirrors internal clock only (silent on ext trig)");
    }

    // (b) internal random-timing clock at default 4 Hz over 5 s: pulse edges
    //     match stepped changes, plausible count, randomized intervals;
    //     smooth = slewed version (bounded slope, monotonic approach).
    {
        Rig r;
        auto* rvg = r.sys.rvg;
        const int n = 240000; // 5 s
        // module's law @ 4 Hz; the RATE default (0.6344) maps to ~4.001 Hz,
        // hence the 0.1 % tolerance below
        const float slewBound = 2.0f * kCV * 4.0f / (float) SR;
        r.sys.engine.tick(); // warm-up: jacks read 0 until the first tick
        int changes = 0, pulses = 0, minIv = 1 << 30, maxIv = 0, lastChange = -1;
        bool prevPulse = false, slewOk = true, monoOk = true, smBounded = true;
        float prevStep = rvg->jacks[RVG::jStepped].value;
        float prevSm = rvg->jacks[RVG::jSmooth].value;
        float prevDist = std::fabs (prevSm - prevStep);
        float maxJump = 0;
        for (int i = 0; i < n; ++i)
        {
            r.sys.engine.tick();
            const float st = rvg->jacks[RVG::jStepped].value;
            const float sm = rvg->jacks[RVG::jSmooth].value;
            const bool pl = rvg->jacks[RVG::jPulse].value > kLogicHi;
            if (pl && ! prevPulse) ++pulses;
            prevPulse = pl;
            if (std::fabs (sm - prevSm) > slewBound * 1.001f + 1e-7f) slewOk = false;
            if (sm < 0.0f || sm > 5.0f) smBounded = false;
            const float dist = std::fabs (sm - st);
            if (st != prevStep)
            {
                ++changes;
                maxJump = std::max (maxJump, std::fabs (st - prevStep));
                if (lastChange >= 0)
                {
                    const int iv = i - lastChange;
                    minIv = std::min (minIv, iv); maxIv = std::max (maxIv, iv);
                }
                lastChange = i;
            }
            else if (dist > prevDist + 1e-5f) monoOk = false; // between triggers
            prevStep = st; prevSm = sm; prevDist = dist;
        }
        CHECK (changes >= 8 && changes <= 40 && pulses == changes,
               "internal clock: stepped changes track pulses (4 Hz nominal, 5 s)",
               changes, pulses);
        CHECK (changes > 2 && maxIv > (int) (1.2 * minIv),
               "clock timing is randomized (interval spread > 1.2x)",
               minIv / SR, maxIv / SR);
        CHECK (slewOk && monoOk && smBounded && maxJump > 1.0f,
               "smooth = slewed version (linear glide, bounded slope)",
               slewBound, maxJump);
    }

    // extra (SPEC 4.8): VC RATE is 1 V/oct on the internal clock
    {
        auto fires = [] (float vcRateVolts)
        {
            Rig r;
            float cv = vcRateVolts;
            inject (r.sys.rvg, RVG::jVcRate, &cv);
            int c = 0;
            float prev = r.sys.rvg->jacks[RVG::jStepped].value;
            for (int i = 0; i < 480000; ++i)
            {
                r.sys.engine.tick();
                const float st = r.sys.rvg->jacks[RVG::jStepped].value;
                if (st != prev) ++c;
                prev = st;
            }
            return c;
        };
        const int f0 = fires (0.0f), f1 = fires (1.0f);
        const double ratio = f1 / std::max (1.0, (double) f0);
        CHECK (ratio > 1.4 && ratio < 2.8, "VC RATE ~ 1 V/oct (rate doubles per volt)",
               (double) f0, (double) f1);
    }
}

// ---------------------------------------------------------------------------
// T11 — VCA law + click bound
// ---------------------------------------------------------------------------
struct VcaProbe
{
    Rig r;
    VCA* vca = nullptr;
    float cv = 0.0f, audio = 0.0f;
    long n = 0;

    explicit VcaProbe (bool second = false)
    {
        vca = second ? r.sys.vca2 : r.sys.vca1;
        inject (vca, VCA::jVc, &cv);
        inject (vca, VCA::jIn, &audio);
    }

    void tickSine (float amp) // 1 kHz = exactly 48 samples/period
    {
        audio = amp * (float) std::sin (2.0 * 3.14159265358979 * (double) (n % 48) / 48.0);
        ++n;
        r.sys.engine.tick();
    }

    double gainDb (float vcVolts, float amp = 0.5f)
    {
        cv = vcVolts;
        for (int i = 0; i < 9600; ++i) tickSine (amp); // 200 ms settle
        Goertzel g; g.init (1000.0);
        for (int i = 0; i < 24000; ++i)                // 500 ms, 500 exact periods
        {
            tickSine (amp);
            g.push (vca->jacks[VCA::jOut].value);
        }
        return db (g.amp(), amp);
    }
};

inline void runT11()
{
    GROUP ("T11 VCA: expo 12 dB/V law, cutoff, max gain, click bound");

    VcaProbe p; // vca1, RESPONSE default EXP
    const double g2 = p.gainDb (2.0f), g3 = p.gainDb (3.0f),
                 g4 = p.gainDb (4.0f), g5 = p.gainDb (5.0f);

    const double s32 = g3 - g2, s43 = g4 - g3, s54 = g5 - g4;
    CHECK (s32 >= 10.5 && s32 <= 13.5 && s43 >= 10.5 && s43 <= 13.5
        && s54 >= 10.5 && s54 <= 13.5,
           "expo ~ 12 dB/V within +-1.5 dB over the top of the range", s32, s54);

    CHECK (g5 >= 5.0 && g5 <= 7.0, "max gain +6 dB +- 1 dB at 5 V", g5);

    const double gKnee = p.gainDb (0.48f), gLow = p.gainDb (0.25f), gZero = p.gainDb (0.0f);
    CHECK (gKnee <= -60.0 && gLow <= -60.0 && gZero <= -60.0,
           "<= -60 dB below 0.5 V CV", gKnee, gLow);

    // second instance (vca2) obeys the same law
    {
        VcaProbe q (true);
        const double h4 = q.gainDb (4.0f), h5 = q.gainDb (5.0f);
        CHECK (h5 >= 5.0 && h5 <= 7.0 && (h5 - h4) >= 10.5 && (h5 - h4) <= 13.5,
               "VCA2: same expo law (+6 dB max, 12 dB/V)", h5, h5 - h4);
    }

    // extra (SPEC 4.9 v1 law): LIN mode gain = clamp01(level + V/5)
    {
        VcaProbe q;
        q.vca->ctls[VCA::cResponse].v = 0.0f; // LIN
        const double gHalf = q.gainDb (2.5f); // expect -6.02 dB
        const double gFull = q.gainDb (5.0f); // expect 0 dB (clamped at 1)
        CHECK (std::fabs (gHalf + 6.02) < 0.6 && std::fabs (gFull) < 0.6,
               "LIN mode: gain = clamp01(level + V/5)", gHalf, gFull);
    }

    // click bound: unity (full-scale, 2.5 V) 1 kHz sine, CV stepped 0 -> 5 V;
    // 5-20 kHz energy in the following 50 ms <= -35 dBFS (FS = 2.5 V).
    {
        VcaProbe q;
        q.cv = 0.0f;
        for (int i = 0; i < 9600; ++i) q.tickSine (kAudio); // settled closed
        q.cv = kCV;                                          // the step
        std::vector<Goertzel> bins;
        for (int f = 5000; f <= 20000; f += 100)
        { Goertzel g; g.init ((double) f); bins.push_back (g); }
        for (int i = 0; i < 2400; ++i)                       // following 50 ms
        {
            q.tickSine (kAudio);
            const float v = q.vca->jacks[VCA::jOut].value;
            for (auto& b : bins) b.push (v);
        }
        double e = 0;
        for (auto& b : bins) e += b.amp() * b.amp();
        const double dbfs = 20.0 * std::log10 (std::max (std::sqrt (e), 1e-12) / (double) kAudio);
        CHECK (dbfs <= -35.0,
               "click bound: CV step 0->5 V -> 5-20 kHz in following 50 ms <= -35 dBFS",
               dbfs);
    }
}

// ---------------------------------------------------------------------------
// perf — T16 protocol applied per module (informational; the binding 6x gate
// is system-level in t_system). Spike figures (SPEC 10): NOISE 17, RVG 33,
// VCA 18 cycles/sample.
// ---------------------------------------------------------------------------
template <typename M>
inline double moduleCycles (bool feedVca = false, double seconds = 10.0, int runs = 5)
{
    M m;
    m.id = "perf"; m.title = "perf";
    m.feeds.assign (m.jacks.size(), {});
    m.moduleSeed = 0xC0FFEE5EEDULL;
    float cvV = 3.0f, inV = 0.0f;
    if (feedVca)
    {
        m.feeds[(size_t) VCA::jVc].src.push_back (&cvV);
        m.feeds[(size_t) VCA::jIn].src.push_back (&inV);
    }
    m.reseed();
    m.prepare (SR);
    Module* mod = &m;
    const long n = (long) (SR * seconds);
    double best = 1e30;
    for (int run = 0; run < runs; ++run)
    {
        auto t0 = std::chrono::steady_clock::now();
        for (long i = 0; i < n; ++i)
        {
            if (feedVca) inV = (float) (i & 1023) * 0.004f - 2.0f;
            mod->tick();
        }
        const double dt = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        best = std::min (best, dt);
    }
    volatile float sink = mod->jacks[0].value;
    (void) sink;
    return best * kAssumedPCoreHz / (double) n;
}

inline void runPerf()
{
    GROUP ("perf (T16 protocol per module; binding 6x gate lives in t_system)");
    const double cn = moduleCycles<NOISE>();
    const double cr = moduleCycles<RVG>();
    const double cv = moduleCycles<VCA> (true);
    std::printf ("  NOISE %.1f cyc/smp (spike 17) | RVG %.1f (spike 33) | "
                 "VCA %.1f (spike 18)  [min of 5 x 10 s]\n", cn, cr, cv);
    CHECK (cn > 0 && cr > 0 && cv > 0, "per-module perf measured (informational)");
}

// ---------------------------------------------------------------------------
// render evidence (SATK_RENDER=1): 5 s noise+RVG WAV, deterministic seed.
// Not part of the v1 manifest sound bar (noise is not a sound-bar dimension).
// ---------------------------------------------------------------------------
inline void writeWav16 (const std::string& fn, const std::vector<float>& L,
                        const std::vector<float>& R)
{
    FILE* f = std::fopen (fn.c_str(), "wb");
    if (f == nullptr) { std::printf ("  render: cannot open %s\n", fn.c_str()); return; }
    const uint32_t n = (uint32_t) L.size(), dataSz = n * 4, riff = 36 + dataSz;
    const uint32_t sr = (uint32_t) SR, byps = sr * 4, fmtSz = 16;
    const uint16_t ch = 2, bits = 16, blk = 4, pcm = 1;
    std::fwrite ("RIFF", 1, 4, f); std::fwrite (&riff, 4, 1, f);
    std::fwrite ("WAVEfmt ", 1, 8, f);
    std::fwrite (&fmtSz, 4, 1, f); std::fwrite (&pcm, 2, 1, f); std::fwrite (&ch, 2, 1, f);
    std::fwrite (&sr, 4, 1, f); std::fwrite (&byps, 4, 1, f);
    std::fwrite (&blk, 2, 1, f); std::fwrite (&bits, 2, 1, f);
    std::fwrite ("data", 1, 4, f); std::fwrite (&dataSz, 4, 1, f);
    for (uint32_t i = 0; i < n; ++i)
    {
        const int16_t s[2] = {
            (int16_t) std::lround (std::clamp (L[i], -1.0f, 1.0f) * 32000.0f),
            (int16_t) std::lround (std::clamp (R[i], -1.0f, 1.0f) * 32000.0f) };
        std::fwrite (s, 2, 2, f);
    }
    std::fclose (f);
    std::printf ("  render: wrote %s (%u samples)\n", fn.c_str(), n);
}

inline void renderEvidence()
{
    const std::string dir = "gauntlet/renders/v2work/noise-vca";
    std::error_code ec;
    std::filesystem::create_directories (dir, ec);

    Rig r; // default instance seed -> deterministic
    r.step (9600);
    const int n = 240000; // 5 s
    std::vector<float> L, R;
    L.reserve ((size_t) n); R.reserve ((size_t) n);
    auto* noise = r.sys.noise; auto* rvg = r.sys.rvg;
    for (int i = 0; i < n; ++i)
    {
        r.sys.engine.tick();
        float l, rr;
        if (i < 60000)       // 0.00-1.25 s: white
            l = rr = noise->jacks[NOISE::jWhite].value * (0.5f / kAudio);
        else if (i < 120000) // 1.25-2.50 s: pink
            l = rr = noise->jacks[NOISE::jPink].value * (0.5f / kAudio);
        else if (i < 180000) // 2.50-3.75 s: S/H source (recentred)
            l = rr = (noise->jacks[NOISE::jShSrc].value - 2.5f) * (0.5f / 2.5f);
        else                 // 3.75-5.00 s: RVG stepped (L) / smooth (R)
        {
            l  = (rvg->jacks[RVG::jStepped].value - 2.5f) * (0.8f / 2.5f);
            rr = (rvg->jacks[RVG::jSmooth].value  - 2.5f) * (0.8f / 2.5f);
        }
        L.push_back (l); R.push_back (rr);
    }
    writeWav16 (dir + "/noise_rvg_evidence.wav", L, R);
}

// ---------------------------------------------------------------------------
inline void run()
{
    runT10Noise();
    runT10Rvg();
    runT11();
    runPerf();
    if (std::getenv ("SATK_RENDER") != nullptr)
        renderEvidence();
}

inline const bool registered = gauntlet::registerTest ("t_noise_vca", &run);

} // namespace t_noise_vca
