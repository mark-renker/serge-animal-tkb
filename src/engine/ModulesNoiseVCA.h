// Serge Animal TKB v2 — NOISE, RVG, VCA (x2).
// Jack/ctl registration FROZEN per docs/SPEC.md §4.7-§4.9 (indices append-only
// forever). Stage-2 implementation (gates T10/T11), hero tier per
// gauntlet/research/dsp-techniques.md §1.7 and hardware-spec.md §2.6/§2.8:
//   NOISE — one PCG32 audio stream (white; pink = Kellet-refined filter of the
//           SAME stream, as in hardware where pink is filtered white), and a
//           SEPARATE PCG32 stream for the S/H source: a per-cycle randomly
//           rate-modulated ~400 Hz sawtooth spanning 0..5 V (equi-probable
//           S&H input; ±1 oct/cycle modulation kills autocorrelation past a
//           few periods — measured worst |r| ≈ 0.04 beyond 10 ms).
//   RVG   — SSG-internally-patched-with-noise semantics: internal RANDOM-timing
//           clock (log-uniform ±1 oct around the RATE knob, 1 V/oct VC RATE),
//           STEPPED redraws 0..5 V only on trigger (internal fire OR external
//           TRIG edge), SMOOTH is the linear-glide slewed version whose slew
//           rate tracks the clock rate; PULSE mirrors the internal clock only.
//   VCA   — hardware UAP law: exponential 12 dB/V over the top of the range,
//           max gain +6 dB (x2) at 5 V (unity at 4.5 V), rapid cutoff below
//           0.5 V (about 600 dB/V below the knee, silent under 0.2 V); LEVEL
//           adds 0..5 V to the VC sum; LIN mode = v1 law clamp01(level + V/5).
//           One-pole CV smoothing (tau 1.2 ms) sizes the T11 click bound; soft
//           saturation (8 V fastTanh) at hot levels.
#pragma once
#include "EngineCore.h"

namespace satk {

// Noise Source: white + pink audio outs and the S/H SOURCE — a randomly-
// modulated ~400 Hz sawtooth spanning 0..5 V (equi-probable S&H input).
class NOISE : public Module
{
public:
    enum J { jWhite, jPink, jShSrc };
    // no ctls (frozen: none in v1 either)

    NOISE()
    {
        addJack ("WHITE",      JackType::Out, JackColor::Black);
        addJack ("PINK",       JackType::Out, JackColor::Black);
        addJack ("S/H SOURCE", JackType::Out, JackColor::White); // 0..5 V
    }

    void reseed() override
    {
        rngWhite.seed (moduleSeed, kStreamWhite); // audio stream (white -> pink)
        rngSh.seed    (moduleSeed, kStreamSh);    // separate S/H stream (SPEC §4.7)
        b0 = b1 = b2 = b3 = b4 = b5 = b6 = 0.0f;
        shPhase = rngSh.nextFloat01();            // deterministic per-seed phase
        shFreq  = nextShFreq();
    }

    void prepare (double sampleRate) override
    {
        Module::prepare (sampleRate);
        reseed(); // idempotent for a given moduleSeed (T14 bit-identical)
    }

    void tick() override
    {
        // WHITE: uniform (exactly flat spectrum), +-2.5 V nominal audio.
        const float w = rngWhite.nextBipolar();
        jacks[jWhite].value = kAudio * w;

        // PINK: Paul Kellet "refined" pinking filter (-3 dB/oct within a
        // fraction of a dB across the audio band), fed by the same white
        // stream as the WHITE out (hardware: pink is filtered white).
        b0 = 0.99886f * b0 + w * 0.0555179f;
        b1 = 0.99332f * b1 + w * 0.0750759f;
        b2 = 0.96900f * b2 + w * 0.1538520f;
        b3 = 0.86650f * b3 + w * 0.3104856f;
        b4 = 0.55000f * b4 + w * 0.5329522f;
        b5 = -0.7616f * b5 - w * 0.0168980f;
        const float p = b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362f;
        b6 = w * 0.115926f;
        jacks[jPink].value = kPinkGain * p; // RMS ~1.06 V, rare peaks ~5 V ("hot")

        // S/H SOURCE: sawtooth 0..5 V, base ~400 Hz, frequency redrawn
        // log-uniformly within +-1 octave at every wrap. Uniform-in-time value
        // distribution regardless of period => equi-probable S&H input.
        shPhase += shFreq / sr;
        if (shPhase >= 1.0f)
        {
            shPhase -= 1.0f;
            shFreq = nextShFreq();
        }
        jacks[jShSrc].value = kCV * shPhase;
    }

private:
    float nextShFreq() { return kShBaseHz * std::exp2 (kShModOct * rngSh.nextBipolar()); }

    static constexpr uint64_t kStreamWhite = 0x4E4F495357484954ULL; // "NOISWHIT"
    static constexpr uint64_t kStreamSh    = 0x4E4F495353485352ULL; // "NOISSHSR"
    static constexpr float kPinkGain = 0.6f;    // measured: RMS 1.06 V for uniform white
    static constexpr float kShBaseHz = 400.0f;  // hardware: ~400 Hz saw
    static constexpr float kShModOct = 1.0f;    // per-cycle +-1 oct rate modulation

    dsp::PCG32 rngWhite, rngSh;
    float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
    float shPhase = 0.0f, shFreq = 400.0f;
};

// Random Voltage Generator (hardware: an SSG internally patched with noise).
// Stepped changes only on trigger (internal clock or TRIG in); SMOOTH is the
// slewed version; PULSE mirrors the internal random-timing clock.
class RVG : public Module
{
public:
    enum J { jStepped, jSmooth, jPulse, jTrig, jVcRate };
    enum C { cRate };

    RVG()
    {
        addJack ("STEPPED", JackType::Out, JackColor::White);
        addJack ("SMOOTH",  JackType::Out, JackColor::White);
        addJack ("PULSE",   JackType::Out, JackColor::Red);
        addJack ("TRIG",    JackType::In,  JackColor::Red);   // external trigger, ORs with clock
        addJack ("VC RATE", JackType::In,  JackColor::White); // 1 V/oct on internal clock

        addCtl ("RATE", 0.6344f); // expo 0.05..50 Hz -> 4 Hz
        leds.resize (1);
    }

    void reseed() override
    {
        rngVal.seed (moduleSeed, kStreamVal); // stepped voltage draws
        rngClk.seed (moduleSeed, kStreamClk); // clock-interval timing draws
        phase   = 0.0f;
        thresh  = nextThresh();
        stepped = kCV * rngVal.nextFloat01(); // deterministic initial voltage
        smooth  = stepped;                    // start settled
        pulse.reset();
        trigEdge.reset();
    }

    void prepare (double sampleRate) override
    {
        Module::prepare (sampleRate);
        reseed(); // idempotent for a given moduleSeed (T14 bit-identical)
    }

    void tick() override
    {
        // Internal random-timing clock: knob expo 0.05..50 Hz, VC RATE 1 V/oct;
        // each interval is log-uniform within +-1 octave of the nominal period.
        const float knobHz = dsp::expmap (ctls[cRate].v, 0.05f, 50.0f);
        const float oct    = std::clamp (in (jVcRate), -6.0f, 6.0f);
        const float rate   = std::clamp (knobHz * std::exp2 (oct), 0.01f, 200.0f);

        bool fire = false;
        phase += rate / sr;
        if (phase >= thresh)
        {
            phase -= thresh;
            thresh = nextThresh();
            fire = true;
            pulse.fire (sr, 1.0f); // PULSE mirrors the INTERNAL clock only
        }
        if (trigEdge.rising (in (jTrig))) // external trigger ORs in (no pulse)
            fire = true;

        if (fire)
            stepped = kCV * rngVal.nextFloat01(); // redraw ONLY on trigger (T10)

        // SMOOTH: linear glide toward the stepped target; slew rate tracks the
        // clock rate ("rate of change of all outputs is voltage controllable").
        const float slew = 2.0f * kCV * rate / sr; // full span in half a nominal period
        smooth += std::clamp (stepped - smooth, -slew, slew);

        jacks[jStepped].value = stepped;
        jacks[jSmooth].value  = smooth;
        jacks[jPulse].value   = pulse.tick();
        leds[0] = smooth * (1.0f / kCV);
    }

private:
    float nextThresh() { return std::exp2 (rngClk.nextBipolar()); } // 0.5..2 periods

    static constexpr uint64_t kStreamVal = 0x525647535445504CULL; // "RVGSTEPL"
    static constexpr uint64_t kStreamClk = 0x525647434C4F434BULL; // "RVGCLOCK"

    dsp::PCG32 rngVal, rngClk;
    EdgeDetector trigEdge;
    PulseGen pulse;
    float phase = 0.0f, thresh = 1.0f, stepped = 0.0f, smooth = 0.0f;
};

// VCA (one channel of the UAP/dual-VCA family). Exponential response
// ~12 dB/V over the top of the range, sharp cutoff below 0.5 V CV, max gain
// +6 dB (gain factor 2); LIN mode = linear in CV.
class VCA : public Module
{
public:
    enum J { jOut, jIn, jVc };
    enum C { cLevel, cResponse };

    VCA()
    {
        addJack ("OUT", JackType::Out, JackColor::Black);
        addJack ("IN",  JackType::In,  JackColor::Black);
        addJack ("VC",  JackType::In,  JackColor::White);

        addCtl ("LEVEL",    0.0f);            // adds 0..5 V to the VC input sum
        addCtl ("RESPONSE", 1.0f, 2, kLinExp); // 0 = LIN, 1 = EXP (hardware law)
        leds.resize (1);
    }

    void reseed() override { vcSm = 0.0f; } // no RNG; deterministic state reset

    void prepare (double sampleRate) override
    {
        Module::prepare (sampleRate);
        cvCoeff = 1.0f - std::exp (-1.0f / (kCvTau * sr));
        vcSm = 0.0f;
    }

    void tick() override
    {
        // Summed control voltage: VC jack + LEVEL knob (0..5 V), one-pole
        // smoothed (tau 1.2 ms) — sized to pass the T11 click bound while
        // keeping the default 1 ms DUSG pluck snappy.
        const float vc = in (jVc) + ctls[cLevel].v * kCV;
        vcSm = flushDenorm (vcSm + cvCoeff * (vc - vcSm));

        float g;
        if (ctls[cResponse].v >= 0.5f) // EXP (hardware UAP law)
        {
            // 12 dB/V: g = 2 * 2^(2*(V-5)) => +6 dB at 5 V, unity at 4.5 V,
            // -48 dB at the 0.5 V knee; below the knee ~600 dB/V plunge
            // ("drops rapidly to full attenuation"), exactly 0 under 0.2 V.
            if (vcSm >= 0.5f)
                g = std::min (2.0f, 2.0f * std::exp2 (2.0f * (vcSm - 5.0f)));
            else if (vcSm > 0.2f)
                g = kKneeGain * std::exp2 (kKneeOctPerV * (vcSm - 0.5f));
            else
                g = 0.0f;
        }
        else // LIN (v1 compatibility): gain = clamp01(level + V/5)
        {
            g = std::clamp (vcSm * (1.0f / kCV), 0.0f, 1.0f);
        }

        // Soft saturation at hot levels (nominal +-2.5 V x max gain 2 passes
        // nearly clean; beyond that it rounds off analog-style).
        const float y = in (jIn) * g;
        jacks[jOut].value = flushDenorm (kSatV * dsp::fastTanh (y * (1.0f / kSatV)));
        leds[0] = std::min (1.0f, 0.5f * g);
    }

private:
    static constexpr const char* kLinExp[2] = { "LIN", "EXP" };
    static constexpr float kCvTau      = 0.0012f;      // CV smoothing tau (s)
    static constexpr float kKneeGain   = 0.00390625f;  // 2^-8 = gain at the 0.5 V knee
    static constexpr float kKneeOctPerV = 99.658f;     // ~600 dB/V below the knee
    static constexpr float kSatV       = 8.0f;         // soft-sat scale (volts)

    float cvCoeff = 0.03f, vcSm = 0.0f;
};

} // namespace satk
