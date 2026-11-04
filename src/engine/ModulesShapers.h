// Serge Animal TKB v2 — VCM (Wave Multipliers) and RING (Ring Modulator).
// Jack/ctl registration FROZEN per docs/SPEC.md §4.5-§4.6 (indices append-only
// forever). DSP: hero tier per gauntlet/research/dsp-techniques.md §1.5/§1.6,
// gates T08/T09. Every nonlinear island runs at 2x through the low-latency IIR
// halfbands (these modules sit in patchable feedback loops — LOG 2026-11-02).
//   VCM MID : piecewise-linear reflector-cascade transfer (6 turnarounds,
//             decaying + progressively denser folds, then hold) with rounded
//             knees (box-smoothed), the whole composite in ONE closed-form
//             2nd-order ADAA kernel.
//   VCM BOT : three cascaded level-compensated full-wave rectifiers (ADAA1
//             |x| each), depth crossfades stage by stage; SQ out = squared-up
//             (ADAA1 tanh, high gain) version of the cascade output.
//   VCM TOP : LO = linear two-quadrant VCA, HI = VC rounded clip (ADAA1 tanh).
//   RING    : four-quadrant transconductance multiplier tanh(X)·tanh(Y)
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
// final stage saturates against its rail). Every corner is rounded by
// box-smoothing the transfer over ±w in the input domain (parabolic knees,
// like the real diode knees), and the curve is normalised to peak ±1.
//
// Why decaying folds (fitted, logged in LOG.md): a constant-amplitude folder
// (triangle or sine) has an intrinsic ripple in spectral centroid vs gain
// (period 2 in folder units — the sine peak alternately dwells on a flat
// turnaround or a steep flank) that breaks T08's monotonic sweep for VC FOLD
// steps of 2^0.286. Decay 0.5 / shrink 1.15 keep the centroid rising over
// any ×1.215 gain step for every ideal-sine input level up to gain 16.
//
// Why w = 1.0, i.e. the first knee's rounding reaches the origin (2026-11-04,
// LOG.md): with w 0.6 the transfer was exactly linear below |u| = 0.4 and the
// knee came in gently above it. A REAL sine (NTO SINE: under-driven shaper,
// a small kink at each peak, H3 -37 dBc, THD 1.5 %) then lost centroid as FOLD
// rose from minimum (262 -> 251 Hz): the soft knee onset lowered the slope
// under the peaks, smoothing the input's kink series (and cancelling its
// H3/H5, opposite sign) before it added enough harmonics of its own. With
// w = 1 the curve is u - 0.44·u|u| near zero: gently compressive from the
// first millivolt, like a transistor folder, never a clean linear zone. Its
// own harmonics then dominate the input's residue at every fold setting, so
// more FOLD always means more upper harmonics for slightly impure sines too.
// Cost: ~4.6 % THD at minimum FOLD with a 2.5 V sine (v1's sin() folder: ~2.6 %).
//
// ADAA: P1..P3 (P1' = pl, P2' = P1, P3' = P2) are tabulated per segment at
// configure time, so f / F1 / F2 of the smoothed composite (box differences
// of P1 / P2 / P3) are exact closed forms of the WHOLE stage — ADAA2 wraps
// it as one kernel (DspUtils rule). pl odd -> P1 even, P2 odd, P3 even.
struct FoldKernel
{
    static constexpr int kMaxTurns = 10;
    int K = 0;                              // turnarounds (finite segments)
    double b[kMaxTurns + 1] {}, v[kMaxTurns + 1] {};   // breakpoints / values
    double m[kMaxTurns + 2] {};             // slope of segment k (starts at b[k-1])
    double A1[kMaxTurns + 2] {}, A2[kMaxTurns + 2] {}, A3[kMaxTurns + 2] {}; // P at b[k-1]
    double w = 0.6, inv2w = 1.0 / 1.2, n = 1.0;

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
        A1[1] = A2[1] = A3[1] = 0.0;
        for (int k = 1; k <= K; ++k)
        {
            const double t = b[k] - b[k - 1];
            double p1, p2, p3;
            segP (k, t, p1, p2, p3);
            A1[k + 1] = p1; A2[k + 1] = p2; A3[k + 1] = p3;
        }
        w = width; inv2w = 0.5 / w; n = 1.0;
        double pk = 0.0; // normalise the rounded curve to peak ±1
        for (int i = 0; i <= 4000; ++i) pk = std::max (pk, std::fabs (f (b[K] * i / 4000.0)));
        n = 1.0 / pk;
    }

    // ADAA kernel concept
    double f  (double u) const { return n * (P<1> (u + w) - P<1> (u - w)) * inv2w; }
    double F1 (double u) const { return n * (P<2> (u + w) - P<2> (u - w)) * inv2w; }
    double F2 (double u) const { return n * (P<3> (u + w) - P<3> (u - w)) * inv2w; }

private:
    void segP (int k, double t, double& p1, double& p2, double& p3) const
    {
        const double vv = v[k - 1], mm = m[k], t2 = t * t;
        p1 = A1[k] + vv * t + mm * t2 * 0.5;
        p2 = A2[k] + A1[k] * t + vv * t2 * 0.5 + mm * t2 * t * (1.0 / 6.0);
        p3 = A3[k] + A2[k] * t + A1[k] * t2 * 0.5 + vv * t2 * t * (1.0 / 6.0)
             + mm * t2 * t2 * (1.0 / 24.0);
    }
    template <int Order>
    double P (double u) const
    {
        const double a = std::fabs (u);
        int k = 1;
        while (k <= K && a > b[k]) ++k; // k = K + 1: the hold segment
        const double t = a - b[k - 1], vv = v[k - 1], mm = m[k], t2 = t * t;
        if constexpr (Order == 1)
            return A1[k] + vv * t + mm * t2 * 0.5;                       // even
        else if constexpr (Order == 2)
        {
            const double p = A2[k] + A1[k] * t + vv * t2 * 0.5 + mm * t2 * t * (1.0 / 6.0);
            return u < 0.0 ? -p : p;                                      // odd
        }
        else
            return A3[k] + A2[k] * t + A1[k] * t2 * 0.5 + vv * t2 * t * (1.0 / 6.0)
                   + mm * t2 * t2 * (1.0 / 24.0);                         // even
    }
};

// Full-wave rectifier |x| (VCM BOT stages).
struct AbsKernel
{
    double f  (double x) const { return std::fabs (x); }
    double F1 (double x) const { return 0.5 * x * std::fabs (x); }
    double F2 (double x) const { return x * x * std::fabs (x) * (1.0 / 6.0); }
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

    void prepare (double sampleRate) override
    {
        Module::prepare (sampleRate);
        smoothK = 1.0f - std::exp (-1.0f / (0.002f * sr)); // ~2 ms knob lag
        envRel  = std::exp (-1.0f / (kEnvTau * 2.0f * sr)); // runs at the 2x rate
        dcK     = 1.0f - std::exp (-2.0f * 3.14159265f * kSqDcHz / (2.0f * sr));

        foldS.snap  (ctls[cFold].v,     mapFold);
        biasS.snap  (ctls[cBias].v,     mapBias);
        driveS.snap (ctls[cTopDrive].v, mapDrive);
        depthS.snap (ctls[cBotDepth].v, mapUnit);

        foldNL.kernel.configure (kFoldTurns, kFoldDecay, kFoldShrink, kFoldW);
        foldNL.reset();
        upMid.prepare(); dnMid.prepare();
        resetTop();
        resetBot();
        lastFoldExp = 1.0e9f; foldMul = 1.0f;
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
        { lastFoldExp = foldExp; foldMul = std::clamp (0.5f * std::exp2 (foldExp), kFoldMin, kFoldMax); }
        // two inputs mix before processing (hardware); gain applied before
        // upsampling so audio-rate fold CV stays band-limited too
        const float u = foldMul * (in (jMidIn1) + in (jMidIn2)) * (1.0f / kAudio) + bias;
        float ua, ub;
        upMid.process (u, ua, ub);
        const float ya = foldNL.process (ua);
        const float yb = foldNL.process (ub);
        const float mid = flushDenorm (shapers::railLimit (kAudio * dnMid.process (ya, yb)));
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
            const float d3 = 3.0f * depth;
            const float a[3] = { std::clamp (d3,        0.0f, 1.0f),
                                 std::clamp (d3 - 1.0f, 0.0f, 1.0f),
                                 std::clamp (d3 - 2.0f, 0.0f, 1.0f) };
            const float xb = (in (jBotIn1) + in (jBotIn2)) * (1.0f / kAudio);
            float x0, x1;
            upBot.process (xb, x0, x1);
            float b0, s0, b1, s1;
            botStep (x0, a, b0, s0);
            botStep (x1, a, b1, s1);
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
    static constexpr double kFoldW         = 1.0;    // knee rounding half-width (folder units)
    static constexpr float  kTopCvOctPerV  = 0.4f;   // HI drive CV, same law as fold
    static constexpr float  kTopMaxDrive   = 40.0f;
    static constexpr float  kEnvTau        = 0.1f;   // rectifier level-compensation release (s)
    static constexpr float  kEnvFloor      = 0.02f;  // SQ normaliser floor (50 mV): silence -> 0
    static constexpr float  kSqGain        = 8.0f;   // squared-up slope
    static constexpr float  kSqDcHz        = 4.0f;   // SQ threshold tracker (AC-coupling corner)

    static float mapFold  (float v) { return 4.0f * std::clamp (v, 0.0f, 1.0f); }
    static float mapBias  (float v) { return dsp::av (std::clamp (v, 0.0f, 1.0f)); }
    static float mapDrive (float v) { return dsp::expmap (v, 1.0f, 10.0f); }
    static float mapUnit  (float v) { return std::clamp (v, 0.0f, 1.0f); }

    // One 2x-rate step of the rectifier cascade. Stage k (amount a_k):
    //   r_k = 2·|y| − env_k   full-wave rectify + level compensation: a wave
    //                         of peak A becomes a doubled-frequency wave that
    //                         still spans ±A (env_k = peak follower of y)
    //   y   = ym + a_k·(r_k − ym), ym = y delayed half a sample to line up
    //                         with the ADAA1 output.
    void botStep (float x, const float (&a)[3], float& bot, float& sq)
    {
        float y = x;
        for (int k = 0; k < 3; ++k)
        {
            env[k] = std::max (std::fabs (y), flushDenorm (env[k] * envRel));
            const float r  = 2.0f * rect[k].process (y) - env[k];
            const float ym = 0.5f * (y + yPrev[k]);
            yPrev[k] = y;
            y = ym + a[k] * (r - ym);
        }
        bot = y;
        // squared-up 2nd output: high-gain rounded comparator of the cascade
        // output against its own running mean (AC-coupled threshold), level-
        // normalised by the input envelope. The fraction of the rectified wave
        // above its mean changes as the stages fade in, so the duty cycle
        // moves with depth — "resembling enhanced PWM" (hardware-spec §2.5).
        yDc += dcK * (y - yDc);
        yDc = flushDenorm (yDc);
        sq  = sqNL.process (kSqGain * (y - yDc) / (env[0] + kEnvFloor));
    }
    void resetBot()
    {
        upBot.prepare(); dnBot.prepare(); dnSq.prepare(); sqNL.reset();
        yDc = 0.0f;
        for (int k = 0; k < 3; ++k) { rect[k].reset(); env[k] = 0.0f; yPrev[k] = 0.0f; }
    }
    void resetTop()
    {
        upTop.prepare(); dnTop.prepare(); topNL.reset();
        topHiPrimed = false;
    }

    float smoothK = 0.01f, envRel = 0.9999f, dcK = 0.0003f, yDc = 0.0f;
    shapers::Smoothed foldS, biasS, driveS, depthS;
    float lastFoldExp = 1.0e9f, foldMul = 1.0f, lastTopV = 1.0e9f, topMul = 1.0f;
    bool topActive = false, botActive = false, topHiPrimed = false;

    dsp::ADAA2<shapers::FoldKernel> foldNL;
    dsp::ADAA1<dsp::TanhKernel> topNL, sqNL;
    dsp::ADAA1<shapers::AbsKernel> rect[3];
    float env[3] {}, yPrev[3] {};

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
    //   bleed(t) = -90 + 50·sqrt(t) dB  ->  t = 0: -90 (<= -80 calibrated min),
    //   t = 0.15 (default): -70.6 (inside the -60..-80 "subtle" band),
    //   t = 1: -40 (>= -50 audible bleed). Monotonic.
    static float bleedLaw (float t)
    {
        const float dB = kBleedMinDb + kBleedSpanDb * std::sqrt (std::clamp (t, 0.0f, 1.0f));
        return std::pow (10.0f, dB * 0.05f);
    }

private:
    static constexpr float kAx = 0.35f;          // signal-side transconductance drive
    static constexpr float kAy = 0.35f;          // carrier-side drive (rounds hot carriers)
    static constexpr float kBleedMinDb  = -90.0f;
    static constexpr float kBleedSpanDb = 50.0f;

    static float mapGain (float v) { return dsp::expmap (v, 0.5f, 4.0f); }
    static float mapUnit (float v) { return std::clamp (v, 0.0f, 1.0f); }

    // Four-quadrant transconductance core at 2x: tanh(ax·X)·tanh(ay·Y),
    // normalised (small-signal X gain 1; Y = nominal 2.5 V -> factor 1);
    // morph m: X·[(1−m) + m·Y] (0 dry, 0.5 = 100 % AM, 1 = balanced ring);
    // plus the trimmed X/Y feedthrough, aligned to the ADAA half-sample delay.
    float core (float ux, float uy, float m)
    {
        const float xs = nlX.process (kAx * ux) * (1.0f / kAx);
        const float ys = nlY.process (kAy * uy) * yNorm;
        const float uxm = 0.5f * (ux + uxPrev); uxPrev = ux;
        const float uym = 0.5f * (uy + uyPrev); uyPrev = uy;
        return xs * ((1.0f - m) + m * ys) + betaX * uxm + betaY * uym;
    }

    float smoothK = 0.01f;
    const float yNorm = 1.0f / std::tanh (kAy);
    shapers::Smoothed gainS, morphS;
    float trimXCache = -1.0f, trimYCache = -1.0f, betaX = 0.0f, betaY = 0.0f;
    float sgnX = 1.0f, sgnY = 1.0f;
    float uxPrev = 0.0f, uyPrev = 0.0f;
    dsp::ADAA1<dsp::TanhKernel> nlX, nlY;
    dsp::IirUpsampler2x   upX, upY;
    dsp::IirDownsampler2x dn;
};

} // namespace satk
