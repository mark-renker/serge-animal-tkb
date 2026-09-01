// Serge Animal TKB v2 — VCFQ (Variable Q VCF).
// FOUNDATION STUB: jack/ctl registration FROZEN per docs/SPEC.md §4.4
// (indices append-only forever). tick() stubbed; the stage-2 VCFQ builder
// implements against gate T07 using DspUtils only.
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

    void prepare (double sampleRate) override { Module::prepare (sampleRate); }
    void tick() override
    {
        // STUB: silence. Stage-2 VCFQ builder: TPT/ZDF SVF at 2x OS with
        // per-integrator tanh, AGC 1/Q law (logged), 0.5-1 ms shaped ping.
    }
};

} // namespace satk
