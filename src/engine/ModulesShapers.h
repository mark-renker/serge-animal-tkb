// Serge Animal TKB v2 — VCM (Wave Multipliers) and RING (Ring Modulator).
// Jack/ctl registration FROZEN per docs/SPEC.md §4.5-§4.6 (indices append-only
// forever). DSP: hero tier per gauntlet/research/dsp-techniques.md §1.5/§1.6,
// gates T08/T09. Every nonlinear island runs at 2x through the low-latency IIR
// halfbands (these modules sit in patchable feedback loops — LOG 2026-11-02).
//   VCM MID : piecewise-linear reflector-cascade transfer (6 turnarounds,
//             decaying + progressively denser folds, then hold) with smooth
//             knees (triangle-kernel smoothing), the whole composite in ONE
//             closed-form 2nd-order ADAA kernel; output level compensated
//             across the FOLD range (makeup table).
//   VCM BOT : three cascaded level-compensated square-law full-wave
//             rectifiers, depth crossfades stage by stage (sine: H1 -> H2 ->
//             H4 -> H8), expanded into one degree-8 polynomial and ADAA1'd as
//             one kernel; SQ out = squared-up (ADAA1 tanh, high gain) version
//             of the cascade output.
//   VCM TOP : LO = linear two-quadrant VCA, HI = VC rounded clip (ADAA1 tanh).
//   RING    : four-quadrant multiplier X·Y with linear-then-soft input stages
//             (NOT a diode ring), DC-coupled, morph dry -> AM -> ring, and
//             explicit X/Y feedthrough trims (dB law logged in LOG.md).
#pragma once
#include "EngineCore.h"

namespace satk {

namespace shapers {

// ---- Folding transfer (VCM MID) ------------------------------------------
// Serge-style reflector cascade as a piecewise-linear odd transfer: the input
// rises to the first turnaround at u = 1 (peak +1), and each further stage
// folds the residue back with LESS than unity swing and over a shorter input
// span — successive peaks alternate in sign with amplitude decay^k, and the
// fold spacing shrinks by 1/shrink per turnaround (uneven, progressively
// denser folds). After the last stage the transfer holds (the cascade's
// final stage saturates against its rail). Every corner is smoothed by
// convolving the transfer with a triangle kernel of half-support W in the
// input domain (cubic, C2 knees, like the soft knee of a real diode/transistor
// stage), and the curve is normalised to peak ±1.
//
// Why decaying folds (fitted, logged in LOG.md): a constant-amplitude folder
// (triangle or sine) has an intrinsic ripple in spectral centroid vs gain
// (period 2 in folder units — the sine peak alternately dwells on a flat
// turnaround or a steep flank) that breaks T08's monotonic sweep for VC FOLD
// steps of 2^0.286. Decay 0.5 / shrink 1.15 keep the centroid rising over
// any ×1.215 gain step for every ideal-sine input level up to gain 16.
//
// Knee history (LOG.md): 2026-11-02 box-smoothing w 0.6 (exactly linear below
// |u| 0.4); 2026-11-04 w 1.0 (no linear zone) because the NTO SINE's peak kink
// (an under-driven shaper, H3 -37 ... H51 -88 dBc) was cancelled by the
// knee's onset and the real-sine centroid dipped at FOLD step 2 — at the cost
// of a dirty minimum fold (4.6 % THD at 2.5 V). 2026-11-05 (round 2): the
// root cause is fixed at the source (NTO SINE is now an exact sine with a
// smooth H3-only, compressive residual, ~0.5 % THD), so the folder goes back
// to a clean linear zone (|u| < 1 - W = 0.2) with C2 triangle-kernel knees
// (W 0.8): 0.85 % THD at minimum FOLD for a 2.5 V sine, centroid rising for
// ideal and real sines, and smoother outer knees (lower alias floor at the
// fold peaks than the box knees).
//
// ADAA: P1..P4 (P1' = pl, Pj' = Pj-1) are tabulated per segment at configure
// time, so f / F1 / F2 of the smoothed composite (second differences of
// P2 / P3 / P4 over W^2) are exact closed forms of the WHOLE stage — ADAA2
// wraps it as one kernel (DspUtils rule).
struct FoldKernel
{
    static constexpr int kMaxTurns = 10;
    int K = 0;                              // turnarounds (finite segments)
    double b[kMaxTurns + 1] {}, v[kMaxTurns + 1] {};   // breakpoints / values
    double m[kMaxTurns + 2] {};             // slope of segment k (starts at b[k-1])
    double A[5][kMaxTurns + 2] {};          // A[j][k] = P_j at b[k-1], j = 1..4
    double W = 0.6, invW2 = 1.0 / 0.36, n = 1.0;

    void configure (int turns, double decay, double shrink, double width)
    {
        K = std::clamp (turns, 1, kMaxTurns);
        double u = 1.0, span = 2.0, amp = 1.0, sgn = 1.0;
        b[0] = 0.0; v[0] = 0.0;
        for (int k = 1; k <= K; ++k)
        {
            b[k] = u; v[k] = sgn * amp;
            sgn = -sgn; amp *= decay; u += span;
            span = std::max (1.0, span / shrink);
        }
        for (int k = 1; k <= K; ++k) m[k] = (v[k] - v[k - 1]) / (b[k] - b[k - 1]);
        m[K + 1] = 0.0; // hold after the last stage
        for (int j = 1; j <= 4; ++j) A[j][1] = 0.0;
        for (int k = 1; k <= K; ++k)
        {
            double p[5];
            segP (k, b[k] - b[k - 1], p);
            for (int j = 1; j <= 4; ++j) A[j][k + 1] = p[j];
        }
        W = width; invW2 = 1.0 / (W * W); n = 1.0;
        double pk = 0.0; // normalise the rounded curve to peak ±1
        for (int i = 0; i <= 4000; ++i) pk = std::max (pk, std::fabs (f ((b[K] + W) * i / 4000.0)));
        n = 1.0 / pk;
    }

    // ADAA kernel concept: triangle-kernel smoothing of half-support W is the
    // second difference of the next-higher antiderivative over W^2.
    double f  (double u) const { return n * (P<2> (u + W) - 2.0 * P<2> (u) + P<2> (u - W)) * invW2; }
    double F1 (double u) const { return n * (P<3> (u + W) - 2.0 * P<3> (u) + P<3> (u - W)) * invW2; }
    double F2 (double u) const { return n * (P<4> (u + W) - 2.0 * P<4> (u) + P<4> (u - W)) * invW2; }

private:
    // P_j over segment k at offset t from b[k-1] (Taylor of a linear piece)
    void segP (int k, double t, double (&p)[5]) const
    {
        const double vv = v[k - 1], mm = m[k], t2 = t * t, t3 = t2 * t;
        p[1] = A[1][k] + vv * t + mm * t2 * 0.5;
        p[2] = A[2][k] + A[1][k] * t + vv * t2 * 0.5 + mm * t3 * (1.0 / 6.0);
        p[3] = A[3][k] + A[2][k] * t + A[1][k] * t2 * 0.5 + vv * t3 * (1.0 / 6.0)
               + mm * t2 * t2 * (1.0 / 24.0);
        p[4] = A[4][k] + A[3][k] * t + A[2][k] * t2 * 0.5 + A[1][k] * t3 * (1.0 / 6.0)
               + vv * t2 * t2 * (1.0 / 24.0) + mm * t3 * t2 * (1.0 / 120.0);
    }
    // pl odd -> P1 even, P2 odd, P3 even, P4 odd (evaluated on |u|)
    template <int Order>
    double P (double u) const
    {
        const double a = std::fabs (u);
        int k = 1;
        while (k <= K && a > b[k]) ++k; // k = K + 1: the hold segment
        double p[5];
        segP (k, a - b[k - 1], p);
        if constexpr (Order % 2 == 0) return u < 0.0 ? -p[Order] : p[Order];
        else return p[Order];
    }
};

// VCM BOT composite: the whole three-stage rectifier cascade at the current
// depth, expanded into ONE polynomial of the normalised input u (degree <= 8),
// ADAA1-wrapped as a single kernel (DspUtils rule: one kernel per island).
// Each stage is the level-compensated square-law full-wave rectifier
//   r(y) = 2·y² − 1      (|y| ≤ 1: rectified, doubled, re-centred to ±1)
// crossfaded by its amount a:  y' = y + a·(r(y) − y)  =  2a·y² + (1−a)·y − a.
// Coefficients are rebuilt only when the depth changes.
struct RectCascadeKernel
{
    static constexpr int kDeg = 8;
    double c[kDeg + 1] {}, C[kDeg + 2] {}; // P(u) and its antiderivative

    void build (const float (&a)[3])
    {
        double p[kDeg + 1] {}; p[1] = 1.0; // y0 = u
        int deg = 1;
        for (int k = 0; k < 3; ++k)
        {
            const double ak = a[k];
            if (ak <= 0.0) continue;
            double sq[kDeg + 1] {};
            for (int i = 0; i <= deg; ++i)
                for (int j = 0; j <= deg && i + j <= kDeg; ++j) sq[i + j] += p[i] * p[j];
            deg = std::min (kDeg, 2 * deg);
            for (int i = 0; i <= deg; ++i) p[i] = 2.0 * ak * sq[i] + (1.0 - ak) * p[i];
            p[0] -= ak;
        }
        for (int i = 0; i <= kDeg; ++i) c[i] = p[i];
        C[0] = 0.0;
        for (int i = 0; i <= kDeg; ++i) C[i + 1] = c[i] / (double) (i + 1);
    }
    double f (double u) const
    {
        double y = c[kDeg];
        for (int i = kDeg - 1; i >= 0; --i) y = y * u + c[i];
        return y;
    }
    double F1 (double u) const
    {
        double y = C[kDeg + 1];
        for (int i = kDeg; i >= 0; --i) y = y * u + C[i];
        return y;
    }
};

// RING input stage: exactly linear up to |u| = c (c = 2 -> 5 V at the
// 2.5 V-normalised input), then a tanh shoulder of width k (a hot ±10 V input
// rounds off near c + k). Linear-then-soft so an offset (unipolar 0..5 V)
// carrier is never bent: a bent offset carrier grows its own 2nd harmonic,
// i.e. X ± 2·Y products a four-quadrant multiplier does not make.
struct LinSoftKernel
{
    static constexpr double c = 2.0, k = 1.0;
    double f (double u) const
    {
        const double a = std::fabs (u);
        if (a <= c) return u;
        const double y = c + k * std::tanh ((a - c) / k);
        return u < 0.0 ? -y : y;
    }
    double F1 (double u) const
    {
        const double a = std::fabs (u);
        if (a <= c) return 0.5 * u * u;
        const double t = (a - c) / k; // log cosh, overflow-safe
        return 0.5 * c * c + c * (a - c) + k * k * (t + std::log1p (std::exp (-2.0 * t)) - 0.6931471805599453);
    }
};

// Rail safety: exactly linear below 10 V, soft to 12 V (VCFQ pattern).
inline float railLimit (float v)
{
    constexpr float kKnee = 10.0f, kOver = kRails - 10.0f;
    const float a = std::fabs (v);
    if (a <= kKnee) return v;
    const float y = kKnee + kOver * dsp::fastTanh ((a - kKnee) * (1.0f / kOver));
    return v < 0.0f ? -y : y;
}

// One-pole knob smoother with a change-cached target.
struct Smoothed
{
    float cache = -1.0f, target = 0.0f, v = 0.0f;
    template <typename Map>
    float tick (float raw, float k, Map&& map)
    {
        if (differs (raw, cache)) { cache = raw; target = map (raw); }
        v += k * (target - v);
        return v;
    }
    template <typename Map>
    void snap (float raw, Map&& map) { cache = raw; target = map (raw); v = target; }
};

} // namespace shapers

// Three independent wave-multiplier sections:
//  TOP: LO = linear VCA (gain linear in CV), HI = VC "square-up" rounded clip.
//  MID: classic odd-harmonic folder (real transfer turnarounds; T08).
//  BOT: 3 cascaded full-wave rectifiers (even harmonics) with level
//       compensation + second "squared-up" PWM-like output.
class VCM : public Module
{
public:
    enum J { jTopOut, jTopIn, jVcTop,
             jMidOut, jMidIn1, jMidIn2, jVcFold,
             jBotOut, jBotSq, jBotIn1, jBotIn2, jVcBot };
    enum C { cTopMode, cTopDrive, cFold, cBias, cBotDepth };

    VCM()
    {
        addJack ("TOP OUT",    JackType::Out, JackColor::Black);
        addJack ("TOP IN",     JackType::In,  JackColor::Black);
        addJack ("VC TOP",     JackType::In,  JackColor::White);
        addJack ("MID OUT",    JackType::Out, JackColor::Black);
        addJack ("MID IN 1",   JackType::In,  JackColor::Black);
        addJack ("MID IN 2",   JackType::In,  JackColor::Black);
        addJack ("VC FOLD",    JackType::In,  JackColor::White); // 0.4 oct/V of fold gain (v1)
        addJack ("BOT OUT",    JackType::Out, JackColor::Black);
        addJack ("BOT SQ OUT", JackType::Out, JackColor::Black); // squared-up 2nd output
        addJack ("BOT IN 1",   JackType::In,  JackColor::Black);
        addJack ("BOT IN 2",   JackType::In,  JackColor::Black);
        addJack ("VC BOT",     JackType::In,  JackColor::White);

        addCtl ("TOP MODE",  1.0f, 2, kLoHi); // 0 = LO (linear VCA), 1 = HI (square-up)
        addCtl ("TOP DRIVE", 0.301f);         // expo 1x..10x -> 2x
        addCtl ("FOLD",      0.3f);           // 0..1 mid fold depth
        addCtl ("BIAS",      0.5f);           // ±, center = 0 V DC bias into the folder
        addCtl ("BOT DEPTH", 0.3f);           // 0..1 rectifier depth
        leds.resize (1);
    }

    // Read-only probe (tests): MID output makeup gain at the current fold.
    float midMakeup() const { return makeup; }

    void prepare (double sampleRate) override
    {
        Module::prepare (sampleRate);
        smoothK = 1.0f - std::exp (-1.0f / (0.002f * sr)); // ~2 ms knob lag
        envHoldN = std::max (1, (int) std::lround (kEnvHold * 2.0f * sr)); // 2x rate
        envRelK  = 1.0f - std::exp (-1.0f / (kEnvRelTau * 2.0f * sr));
        dcK     = 1.0f - std::exp (-2.0f * 3.14159265f * kSqDcHz / (2.0f * sr));

        foldS.snap  (ctls[cFold].v,     mapFold);
        biasS.snap  (ctls[cBias].v,     mapBias);
        driveS.snap (ctls[cTopDrive].v, mapDrive);
        depthS.snap (ctls[cBotDepth].v, mapUnit);

        foldNL.kernel.configure (kFoldTurns, kFoldDecay, kFoldShrink, kFoldW);
        buildMakeup();
        foldNL.reset();
        upMid.prepare(); dnMid.prepare();
        resetTop();
        resetBot();
        lastFoldExp = 1.0e9f; foldMul = 1.0f; makeup = 1.0f;
        lastTopV    = 1.0e9f; topMul  = 1.0f;
        topActive = botActive = false;
        for (auto& j : jacks) j.value = 0.0f;
    }

    void tick() override
    {
        // ---- MID: rounded folding transfer, ADAA2 in a 2x island ----------
        // gain law (SPEC §4.5): 0.5x·2^(4·FOLD + 0.4·V), capped 16x (v1)
        const float fold = foldS.tick (ctls[cFold].v, smoothK, mapFold);
        const float bias = biasS.tick (ctls[cBias].v, smoothK, mapBias); // ±1 folder units
        const float foldExp = fold + kFoldCvOctPerV * in (jVcFold);
        if (differs (foldExp, lastFoldExp))
        {
            lastFoldExp = foldExp;
            foldMul = std::clamp (0.5f * std::exp2 (foldExp), kFoldMin, kFoldMax);
            makeup  = makeupFor (foldMul);
        }
        // two inputs mix before processing (hardware); gain applied before
        // upsampling so audio-rate fold CV stays band-limited too
        const float u = foldMul * (in (jMidIn1) + in (jMidIn2)) * (1.0f / kAudio) + bias;
        float ua, ub;
        upMid.process (u, ua, ub);
        const float ya = foldNL.process (ua);
        const float yb = foldNL.process (ub);
        const float mid = flushDenorm (shapers::railLimit (kAudio * makeup * dnMid.process (ya, yb)));
        jacks[jMidOut].value = mid;

        // ---- TOP: LO linear VCA / HI VC rounded clip -----------------------
        const float drive = driveS.tick (ctls[cTopDrive].v, smoothK, mapDrive); // 1..10x
        if (connected (jTopIn))
        {
            topActive = true;
            const float x = in (jTopIn);
            const float vTop = in (jVcTop);
            if (ctls[cTopMode].v < 0.5f)
            {
                // LO: two-quadrant LINEAR VCA, gain = (DRIVE/2)·CV/5 — unity at
                // 5 V with the default 2x knob. Unpatched VC TOP normals to
                // +5 V so the knob alone sets a static gain.
                const float cv = connected (jVcTop) ? std::max (0.0f, vTop) : kCV;
                jacks[jTopOut].value = shapers::railLimit (x * (0.5f * drive) * cv * (1.0f / kCV));
                if (topHiPrimed) resetTop();
            }
            else
            {
                // HI: "square-up" — tanh saturation, drive = DRIVE·2^(0.4·CV)
                topHiPrimed = true;
                if (differs (vTop, lastTopV))
                { lastTopV = vTop; topMul = std::exp2 (kTopCvOctPerV * vTop); }
                const float d = std::clamp (drive * topMul, 0.0f, kTopMaxDrive);
                float va, vb;
                upTop.process (d * x * (1.0f / kAudio), va, vb);
                const float ta = topNL.process (va);
                const float tb = topNL.process (vb);
                jacks[jTopOut].value = flushDenorm (shapers::railLimit (kAudio * dnTop.process (ta, tb)));
            }
        }
        else if (topActive)
        {
            topActive = false;
            resetTop();
            jacks[jTopOut].value = 0.0f;
        }

        // ---- BOT: 3 cascaded level-compensated full-wave rectifiers --------
        const float depthK = depthS.tick (ctls[cBotDepth].v, smoothK, mapUnit);
        if (connected (jBotIn1) || connected (jBotIn2))
        {
            botActive = true;
            const float depth = std::clamp (depthK + in (jVcBot) * (1.0f / kCV), 0.0f, 1.0f);
            if (differs (depth, lastDepth))
            {
                lastDepth = depth;
                const float d3 = 3.0f * depth;
                const float a[3] = { std::clamp (d3,        0.0f, 1.0f),
                                     std::clamp (d3 - 1.0f, 0.0f, 1.0f),
                                     std::clamp (d3 - 2.0f, 0.0f, 1.0f) };
                rectNL.kernel.build (a);
                // power compensation of the one partially-faded stage: the dry
                // and rectified parts are orthogonal octaves for a sine, so a
                // linear crossfade would dip 3 dB mid-way
                botComp = 1.0f;
                for (float ak : a) botComp /= std::sqrt ((1.0f - ak) * (1.0f - ak) + ak * ak);
            }
            const float xb = (in (jBotIn1) + in (jBotIn2)) * (1.0f / kAudio);
            float x0, x1;
            upBot.process (xb, x0, x1);
            float b0, s0, b1, s1;
            botStep (x0, b0, s0);
            botStep (x1, b1, s1);
            jacks[jBotOut].value = flushDenorm (shapers::railLimit (kAudio * dnBot.process (b0, b1)));
            jacks[jBotSq].value  = flushDenorm (shapers::railLimit (kAudio * dnSq.process (s0, s1)));
        }
        else if (botActive)
        {
            botActive = false;
            resetBot();
            jacks[jBotOut].value = jacks[jBotSq].value = 0.0f;
        }

        leds[0] = std::clamp (std::fabs (mid) * (1.0f / kAudio), 0.0f, 1.0f);
    }

private:
    static constexpr const char* kLoHi[2] = { "LO (VCA)", "HI (SQUARE-UP)" };

    // ---- constants (laws logged in gauntlet/LOG.md) -----------------------
    static constexpr float  kFoldCvOctPerV = 0.4f;   // VC FOLD (SPEC §4.5; v1 ±2 oct / 5 V)
    static constexpr float  kFoldMin       = 0.05f;
    static constexpr float  kFoldMax       = 16.0f;  // v1 cap
    static constexpr int    kFoldTurns     = 6;      // reflector-cascade turnarounds, then hold
    static constexpr double kFoldDecay     = 0.5;    // successive fold-peak amplitude ratio
    static constexpr double kFoldShrink    = 1.15;   // successive fold-spacing ratio (denser outward)
    static constexpr double kFoldW         = 0.8;    // knee smoothing half-support (triangle kernel, folder units)
    static constexpr float  kTopCvOctPerV  = 0.4f;   // HI drive CV, same law as fold
    static constexpr float  kTopMaxDrive   = 40.0f;
    static constexpr float  kEnvHold       = 0.05f;  // BOT peak window (s): steady >= 20 Hz -> exact peak
    static constexpr float  kEnvRelTau     = 0.005f; // BOT peak release smoothing toward the window max (s)
    static constexpr float  kEnvMin        = 1.0e-3f;// BOT normaliser floor (2.5 mV)
    static constexpr float  kEnvFloor      = 0.02f;  // SQ normaliser floor (50 mV): silence -> 0
    static constexpr float  kSqGain        = 8.0f;   // squared-up slope
    static constexpr float  kSqDcHz        = 4.0f;   // SQ threshold tracker (AC-coupling corner)

    static float mapFold  (float v) { return 4.0f * std::clamp (v, 0.0f, 1.0f); }
    static float mapBias  (float v) { return dsp::av (std::clamp (v, 0.0f, 1.0f)); }
    static float mapDrive (float v) { return dsp::expmap (v, 1.0f, 10.0f); }
    static float mapUnit  (float v) { return std::clamp (v, 0.0f, 1.0f); }

    // ---- MID level compensation ------------------------------------------
    // A folder holds its level as it folds: the output stage's makeup gain
    // tracks the fold gain so a nominal (2.5 V) sine comes out at the same
    // RMS at every FOLD setting, referenced to the default FOLD (makeup 1
    // there, so the default voice is level-unchanged). Tabulated at
    // configure time from the kernel's own static curve: rms of
    // f(g·sin) over 256 phases, 32 points/octave over the FOLD-knob gain
    // range 0.5x..16x. Below 0.5x (negative VC FOLD) the makeup is held, so
    // the section fades out as a plain attenuator. Capped at 2x so the
    // deepest folds peak at <= 5 V (they sit ~2 dB under the default level
    // instead of the uncompensated ~8 dB).
    static constexpr int   kMkPerOct = 32;
    static constexpr float kMkLog2Lo = -1.0f;                  // 0.5x
    static constexpr float kMkLog2Hi = 4.0f;                   // 16x
    static constexpr int   kMkN = (int) ((kMkLog2Hi - kMkLog2Lo) * kMkPerOct) + 1;
    static constexpr float kMkRefFold = 0.3f;                  // FOLD ctl default
    static constexpr double kMkMax = 2.0;                      // fold peaks stay <= 2x nominal (5 V)
    double foldRms (double g) const
    {
        double acc = 0.0;
        for (int i = 0; i < 256; ++i)
        {
            const double y = foldNL.kernel.f (g * std::sin (6.283185307179586 * (i + 0.5) / 256.0));
            acc += y * y;
        }
        return std::sqrt (acc / 256.0);
    }
    void buildMakeup()
    {
        const double ref = foldRms (0.5 * std::exp2 (4.0 * kMkRefFold));
        for (int i = 0; i < kMkN; ++i)
            mkTab[i] = (float) std::min (kMkMax, ref / foldRms (std::exp2 (kMkLog2Lo + (double) i / kMkPerOct)));
    }
    float makeupFor (float g) const
    {
        const float x = std::clamp ((std::log2 (std::max (g, 1.0e-6f)) - kMkLog2Lo) * (float) kMkPerOct,
                                    0.0f, (float) (kMkN - 1));
        const int i = std::min ((int) x, kMkN - 2);
        const float t = x - (float) i;
        return mkTab[i] + t * (mkTab[i + 1] - mkTab[i]);
    }

    // One 2x-rate step of the rectifier cascade (RectCascadeKernel). Level
    // compensation = the input's own peak: env is the max |x| over a sliding
    // 50..100 ms window (instant attack, 5 ms release toward the window max),
    // so for any steady input >= 20 Hz env is exactly the peak, with no
    // per-cycle ripple. u = x/env spans ±1; every stage maps ±1 onto ±1, so
    // the doubled wave keeps the input's amplitude ("not accompanied by a
    // reduction in output amplitude") and stays centred, which is what lets
    // the next stage double it again: a sine goes H1 -> H2 -> H4 -> H8 as
    // the depth sweeps through the three stages.
    void botStep (float x, float& bot, float& sq)
    {
        const float ax = std::fabs (x);
        if (ax > curMax) curMax = ax;
        if (++holdCount >= envHoldN) { prevMax = curMax; curMax = 0.0f; holdCount = 0; }
        const float target = std::max (curMax, prevMax);
        if (target >= botEnv) botEnv = target;
        else botEnv = flushDenorm (botEnv + envRelK * (target - botEnv));
        const float e = std::max (botEnv, kEnvMin);
        const float y = botEnv * botComp * rectNL.process (x / e);
        bot = y;
        // squared-up 2nd output: high-gain rounded comparator of the cascade
        // output against its own running mean (AC-coupled threshold), level-
        // normalised by the input envelope. The fraction of the rectified wave
        // above its mean changes as the stages fade in, so the duty cycle
        // moves with depth — "resembling enhanced PWM" (hardware-spec §2.5).
        yDc += dcK * (y - yDc);
        yDc = flushDenorm (yDc);
        sq  = sqNL.process (kSqGain * (y - yDc) / (botEnv + kEnvFloor));
    }
    void resetBot()
    {
        upBot.prepare(); dnBot.prepare(); dnSq.prepare(); sqNL.reset(); rectNL.reset();
        yDc = 0.0f; botEnv = curMax = prevMax = 0.0f; holdCount = 0;
        lastDepth = -1.0f; botComp = 1.0f;
    }
    void resetTop()
    {
        upTop.prepare(); dnTop.prepare(); topNL.reset();
        topHiPrimed = false;
    }

    float smoothK = 0.01f, envRelK = 0.002f, dcK = 0.0003f, yDc = 0.0f;
    int envHoldN = 4800, holdCount = 0;
    float botEnv = 0.0f, curMax = 0.0f, prevMax = 0.0f, lastDepth = -1.0f, botComp = 1.0f;
    shapers::Smoothed foldS, biasS, driveS, depthS;
    float lastFoldExp = 1.0e9f, foldMul = 1.0f, lastTopV = 1.0e9f, topMul = 1.0f;
    bool topActive = false, botActive = false, topHiPrimed = false;

    dsp::ADAA2<shapers::FoldKernel> foldNL;
    dsp::ADAA1<dsp::TanhKernel> topNL, sqNL;
    dsp::ADAA1<shapers::RectCascadeKernel> rectNL;
    float makeup = 1.0f;
    float mkTab[kMkN] {};

    dsp::IirUpsampler2x   upMid, upTop, upBot;
    dsp::IirDownsampler2x dnMid, dnTop, dnBot, dnSq;
};

// Four-quadrant transconductance multiplier (R6/CGS/R9 — NOT a diode ring).
// DC-coupled (DC carrier = clean VCA/AM); VC morph dry -> AM -> ring;
// explicit feedthrough trims, default bleed subtle (-60..-80 dB band).
class RING : public Module
{
public:
    enum J { jOut, jX, jY, jVcMorph };
    enum C { cGain, cMorph, cTrimX, cTrimY };

    RING()
    {
        addJack ("OUT",      JackType::Out, JackColor::Black);
        addJack ("X IN",     JackType::In,  JackColor::Black); // signal
        addJack ("Y IN",     JackType::In,  JackColor::Black); // carrier (DC-coupled)
        addJack ("VC MORPH", JackType::In,  JackColor::White); // sums with MORPH knob

        addCtl ("GAIN",         0.3333f); // expo 0.5x..4x -> 1x
        addCtl ("MORPH",        1.0f);    // 0 dry -> 0.5 AM -> 1 full ring (v1 = ring)
        addCtl ("X BLEED TRIM", 0.15f);   // signal feedthrough, 0 = calibrated minimum
        addCtl ("Y BLEED TRIM", 0.15f);   // carrier feedthrough, 0 = calibrated minimum
        leds.resize (1);
    }

    // Unit-to-unit tolerance: only the POLARITY of the residual feedthrough
    // (which side of the null each trim pot sits on) — magnitudes follow the
    // calibrated law, so T09 holds for every instance seed.
    void reseed() override
    {
        dsp::SplitMix64 rng (moduleSeed);
        sgnX = (rng.next() & 1u) ? 1.0f : -1.0f;
        sgnY = (rng.next() & 1u) ? 1.0f : -1.0f;
    }

    void prepare (double sampleRate) override
    {
        Module::prepare (sampleRate);
        smoothK = 1.0f - std::exp (-1.0f / (0.002f * sr));
        gainS.snap  (ctls[cGain].v,  mapGain);
        morphS.snap (ctls[cMorph].v, mapUnit);
        trimXCache = trimYCache = -1.0f;
        upX.prepare(); upY.prepare(); dn.prepare();
        nlX.reset(); nlY.reset();
        uxPrev = uyPrev = 0.0f;
        jacks[jOut].value = 0.0f;
    }

    void tick() override
    {
        const float gain  = gainS.tick (ctls[cGain].v, smoothK, mapGain);
        const float morph = std::clamp (morphS.tick (ctls[cMorph].v, smoothK, mapUnit)
                                        + in (jVcMorph) * (1.0f / kCV), 0.0f, 1.0f);
        if (differs (ctls[cTrimX].v, trimXCache))
        { trimXCache = ctls[cTrimX].v; betaX = sgnX * bleedLaw (trimXCache); }
        if (differs (ctls[cTrimY].v, trimYCache))
        { trimYCache = ctls[cTrimY].v; betaY = sgnY * bleedLaw (trimYCache); }

        // DC-coupled, normalised to the ±2.5 V nominal audio level
        const float ux = in (jX) * (1.0f / kAudio);
        const float uy = in (jY) * (1.0f / kAudio);
        float xa, xb, ya, yb;
        upX.process (ux, xa, xb);
        upY.process (uy, ya, yb);
        const float ca = core (xa, ya, morph);
        const float cb = core (xb, yb, morph);
        const float o  = kAudio * gain * dn.process (ca, cb);
        jacks[jOut].value = flushDenorm (shapers::railLimit (o));
        leds[0] = std::clamp (std::fabs (o) * (1.0f / kAudio), 0.0f, 1.0f);
    }

    // Feedthrough law (amplitude re unity, per trim t in 0..1):
    //   bleed(t) = -102 + 62·sqrt(t) dB  ->  t = 0: -102 (<= -80 calibrated
    //   min), t = 0.15 (default): -78.0 (inside the -60..-80 "subtle" band,
    //   ~80 dB carrier rejection as documented for the R*S RING), t = 1: -40
    //   (>= -50 audible bleed). Monotonic. (Was -90 + 50·sqrt(t), default
    //   -70.6 dB; retuned 2026-11-05, LOG.md.)
    static float bleedLaw (float t)
    {
        const float dB = kBleedMinDb + kBleedSpanDb * std::sqrt (std::clamp (t, 0.0f, 1.0f));
        return std::pow (10.0f, dB * 0.05f);
    }

private:
    static constexpr float kBleedMinDb  = -102.0f;
    static constexpr float kBleedSpanDb = 62.0f;

    static float mapGain (float v) { return dsp::expmap (v, 0.5f, 4.0f); }
    static float mapUnit (float v) { return std::clamp (v, 0.0f, 1.0f); }

    // Four-quadrant multiplier core at 2x: X·Y with each input through the
    // linear-then-soft input stage (exact product up to ±5 V on both sides;
    // hot inputs round off), normalised so Y = 2.5 V is a factor of 1;
    // morph m: X·[(1−m) + m·Y] (0 dry, 0.5 = 100 % AM, 1 = balanced ring);
    // plus the trimmed X/Y feedthrough, aligned to the ADAA half-sample delay.
    // (Was tanh(0.35·X)·tanh(0.35·Y): its curvature bent a unipolar 0..5 V
    // carrier, adding Y's 2nd harmonic -> X ± 2·fY products at -32 dBc, and
    // gave X a 1 % H3. LOG.md 2026-11-05.)
    float core (float ux, float uy, float m)
    {
        const float xs = nlX.process (ux);
        const float ys = nlY.process (uy);
        const float uxm = 0.5f * (ux + uxPrev); uxPrev = ux;
        const float uym = 0.5f * (uy + uyPrev); uyPrev = uy;
        return xs * ((1.0f - m) + m * ys) + betaX * uxm + betaY * uym;
    }

    float smoothK = 0.01f;
    shapers::Smoothed gainS, morphS;
    float trimXCache = -1.0f, trimYCache = -1.0f, betaX = 0.0f, betaY = 0.0f;
    float sgnX = 1.0f, sgnY = 1.0f;
    float uxPrev = 0.0f, uyPrev = 0.0f;
    dsp::ADAA1<shapers::LinSoftKernel> nlX, nlY;
    dsp::IirUpsampler2x   upX, upY;
    dsp::IirDownsampler2x dn;
};

} // namespace satk
