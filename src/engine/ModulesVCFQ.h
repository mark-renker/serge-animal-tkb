// Serge Animal TKB v2 — VCFQ (Variable Q VCF).
// Jack/ctl registration FROZEN per docs/SPEC.md §4.4 (indices append-only
// forever). DSP: hero tier per gauntlet/research/dsp-techniques.md §1.4/§2 —
// Zavalishin TPT/ZDF SVF running in a 2x-oversampled island with
// per-integrator tanh saturation ("predict with linear solve, correct once"),
// input-stage soft clip, AGC input with a resistive-divider compensation law,
// and a 0.5-1 ms diode-clipped shaped ping injected at the filter core
// (independent of the GAIN/AGC input stages). Gate: T07.
#pragma once
#include "EngineCore.h"

namespace satk {

// 2-pole state-variable filter, simultaneous LP/BP/HP/NOTCH. Two audio ins:
// IN (via GAIN, deliberately uncompensated — overdrive at high Q) and AGC IN
// (resonance-compensated, monotonic in Q, bounded). TRIG pings the filter
// (shaped pulse into the BP integrator node -> damped sine at fc).
class VCFQ : public Module
{
public:
    enum J { jLp, jBp, jHp, jNotch,
             jIn, jAgc, jOct, jVcF, jVcQ, jTrig };
    enum C { cFreq, cQ, cGain, cVcFAmt, cVcQAmt, cPing };

    VCFQ()
    {
        addJack ("LP",     JackType::Out, JackColor::Black);
        addJack ("BP",     JackType::Out, JackColor::Black);
        addJack ("HP",     JackType::Out, JackColor::Black);
        addJack ("NOTCH",  JackType::Out, JackColor::Black);
        addJack ("IN",     JackType::In,  JackColor::Black);  // via GAIN, uncompensated
        addJack ("AGC IN", JackType::In,  JackColor::Black);  // Q-compensated input
        addJack ("1V/OCT", JackType::In,  JackColor::White);  // calibrated 1 V/oct
        addJack ("VC F",   JackType::In,  JackColor::White);  // attenuverted, full = 1 V/oct
        addJack ("VC Q",   JackType::In,  JackColor::White);  // attenuverted, full = 0.6 oct/V
        addJack ("TRIG",   JackType::In,  JackColor::Red);    // ping

        addCtl ("FREQ",       0.5767f); // expo 20 Hz..12 kHz -> 800 Hz
        addCtl ("Q",          0.3164f); // expo 0.5..40 -> 2
        addCtl ("GAIN",       0.5f);    // linear 0..2 -> 1.0
        addCtl ("VC F AMT",   0.5f);    // attenuverter, center = 0
        addCtl ("VC Q AMT",   0.5f);    // attenuverter, center = 0
        addCtl ("PING LEVEL", 0.6f);    // 0..1
        leds.resize (1);
    }

    void prepare (double sampleRate) override
    {
        Module::prepare (sampleRate);
        fs2 = 2.0f * sr;
        up.prepare();
        dnLp.prepare(); dnBp.prepare(); dnHp.prepare();
        s1 = s2 = 0.0f;
        trigEdge.reset();
        pingEnv   = 0.0f;
        pingDecay = std::exp (-1.0f / (kPingTau * fs2));
        smoothK   = 1.0f - std::exp (-1.0f / (0.002f * sr)); // ~2 ms knob lag (v1 smoothing)

        // snap knob smoothers to their targets; invalidate coefficient caches
        fcCtlCache = qCtlCache = gainCtlCache = -1.0f;
        refreshKnobTargets();
        fcK = fcTarget; qK = qTarget; gainS = gainTarget;
        lastPo = 1.0e9f; poMul = 1.0f;
        lastQo = 1.0e9f; qoMul = 1.0f;
        lastFc = -1.0f;  g = 0.05f;
    }

    void tick() override
    {
        // ---- control path (knobs smoothed; CV applied per sample — audio-rate
        // FM of fc works, v1 contract) --------------------------------------
        refreshKnobTargets();
        fcK   += smoothK * (fcTarget   - fcK);
        qK    += smoothK * (qTarget    - qK);
        gainS += smoothK * (gainTarget - gainS);

        const float po = in (jOct) + dsp::av (ctls[cVcFAmt].v) * in (jVcF); // octaves
        if (differs (po, lastPo)) { lastPo = po; poMul = std::exp2 (po); }
        const float fc = std::clamp (fcK * poMul, 5.0f, 0.45f * sr);

        const float qo = dsp::av (ctls[cVcQAmt].v) * 0.6f * in (jVcQ); // 0.6 oct/V (v1: 3 oct/5 V)
        if (differs (qo, lastQo)) { lastQo = qo; qoMul = std::exp2 (qo); }
        const float Q = std::clamp (qK * qoMul, 0.5f, 60.0f); // CV extends knob's 40 to 60

        if (differs (fc, lastFc)) { lastFc = fc; g = std::tan (kPiF * fc / fs2); } // prewarped @ 2x rate
        const float twoR = 1.0f / Q;
        const float dInv = 1.0f / (1.0f + twoR * g + g * g); // ZDF solve

        // ---- ping trigger (shaped pulse into the core, NOT the audio input) --
        if (trigEdge.rising (in (jTrig)))
            pingEnv = kPingAmp * std::clamp (ctls[cPing].v, 0.0f, 1.0f);

        // ---- input stages -----------------------------------------------------
        // GAIN path: deliberately uncompensated (overdrive at high Q is the
        // instrument). AGC path: gain falls as Q rises — resistive-divider law
        // agc(Q) = (1 + Q0)/(Q0 + Q), Q0 = 2 (logged; ~1/Q asymptote, bounded
        // compensated resonance gain <= (1+Q0) = +9.5 dB), monotonic in Q.
        const float agcGain = (1.0f + kAgcQ0) / (kAgcQ0 + Q);
        const float xin = in (jIn) * gainS + in (jAgc) * agcGain;

        // ---- 2x oversampled nonlinear ZDF core --------------------------------
        float xa, xb;
        up.process (xin, xa, xb);
        float lpA, bpA, hpA, lpB, bpB, hpB;
        coreStep (xa, twoR, dInv, lpA, bpA, hpA);
        coreStep (xb, twoR, dInv, lpB, bpB, hpB);

        const float lpO = flushDenorm (dnLp.process (lpA, lpB));
        const float bpO = flushDenorm (dnBp.process (bpA, bpB));
        const float hpO = flushDenorm (dnHp.process (hpA, hpB));
        jacks[jLp].value    = lpO;
        jacks[jBp].value    = bpO;
        jacks[jHp].value    = hpO;
        jacks[jNotch].value = flushDenorm (clipOut (lpO + hpO)); // notch = LP + HP (nulls at fc)
        leds[0] = std::clamp (std::fabs (bpO) * (1.0f / kCV), 0.0f, 1.0f);
    }

private:
    // ---- constants (hero VCFQ, dsp-techniques.md §2) ---------------------
    static constexpr float kPiF      = 3.14159265f;
    static constexpr float kInClip   = 10.0f;    // input-stage soft clip (V)
    static constexpr float kIntClip  = 10.0f;    // integrator clip ~ ±10 V rails
    static constexpr float kOutClip  = 11.0f;
    static constexpr float kKnee     = 8.0f;     // state limiter: linear below (V)
    static constexpr float kRail     = 10.0f;    // state limiter ceiling (V)    // output stage limit (< ±12 V rails)
    static constexpr float kAgcQ0    = 2.0f;     // AGC divider constant (law above)
    static constexpr float kPingAmp  = 12.0f;    // pre-clip ping peak at PING LEVEL 1 (V)
    static constexpr float kPingClip = 3.5f;     // diode-clipped ping top (V)
    static constexpr float kPingTau  = 0.25e-3f; // ping decay tau -> ~0.5-1 ms pulse

    static float satInt  (float v) { return kIntClip * dsp::fastTanh (v * (1.0f / kIntClip)); }
    static float railLimit (float v)
    {
        const float a = std::fabs (v);
        if (a <= kKnee) return v;
        const float over = kRail - kKnee;
        const float y = kKnee + over * dsp::fastTanh ((a - kKnee) * (1.0f / over));
        return v < 0.0f ? -y : y;
    }
    static float clipOut (float v) { return kOutClip * dsp::fastTanh (v * (1.0f / kOutClip)); }

    void refreshKnobTargets()
    {
        if (differs (ctls[cFreq].v, fcCtlCache))
        { fcCtlCache = ctls[cFreq].v; fcTarget = dsp::expmap (fcCtlCache, 20.0f, 12000.0f); }
        if (differs (ctls[cQ].v, qCtlCache))
        { qCtlCache = ctls[cQ].v; qTarget = dsp::expmap (qCtlCache, 0.5f, 40.0f); }
        if (differs (ctls[cGain].v, gainCtlCache))
        { gainCtlCache = ctls[cGain].v; gainTarget = dsp::linmap (gainCtlCache, 0.0f, 2.0f); }
    }

    // One internal (2x-rate) step: TPT SVF, per-integrator tanh saturation.
    // Linear ZDF solve predicts hp; one correction pass makes the outputs
    // consistent with the saturated integrator inputs (exact in the linear
    // regime; compresses resonance and limits self-oscillation when hot).
    void coreStep (float x, float twoR, float dInv,
                   float& lpO, float& bpO, float& hpO)
    {
        // ping: exponentially-decaying, diode-clipped top, added at the core
        // node so its level is independent of the GAIN/AGC input stages
        float p = 0.0f;
        if (pingEnv > 1.0e-4f)
        {
            p = kPingClip * dsp::fastTanh (pingEnv * (1.0f / kPingClip));
            pingEnv *= pingDecay;
        }
        else pingEnv = 0.0f;

        const float v0 = kInClip * dsp::fastTanh (x * (1.0f / kInClip)) + p;

        const float hpLin = (v0 - (twoR + g) * s1 - s2) * dInv;
        const float u1 = satInt (hpLin);
        const float bp = s1 + g * u1;
        const float u2 = satInt (bp);
        const float lp = s2 + g * u2;
        const float hp = v0 - twoR * bp - lp; // corrected (== hpLin when linear)

        // trapezoidal state update; the states (integrator capacitor
        // voltages) are knee-limited at the op-amp rails: exactly linear
        // below kKnee (so Q / notch depth are untouched), soft above. This is
        // what bounds a BP->IN self-oscillation loop in hardware.
        s1 = flushDenorm (railLimit (bp + g * satInt (hp)));
        s2 = flushDenorm (railLimit (lp + g * u2));

        lpO = clipOut (lp);
        bpO = clipOut (bp);
        hpO = clipOut (hp);
    }

    // state
    float fs2 = 96000.0f;
    float s1 = 0.0f, s2 = 0.0f;
    float g = 0.05f, lastFc = -1.0f;
    float lastPo = 1.0e9f, poMul = 1.0f, lastQo = 1.0e9f, qoMul = 1.0f;
    float fcCtlCache = -1.0f, qCtlCache = -1.0f, gainCtlCache = -1.0f;
    float fcTarget = 800.0f, qTarget = 2.0f, gainTarget = 1.0f;
    float fcK = 800.0f, qK = 2.0f, gainS = 1.0f, smoothK = 0.01f;
    float pingEnv = 0.0f, pingDecay = 0.96f;
    EdgeDetector trigEdge;
    dsp::Upsampler2x   up;
    dsp::Downsampler2x dnLp, dnBp, dnHp;
};

} // namespace satk
