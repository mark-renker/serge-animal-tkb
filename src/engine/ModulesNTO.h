// Serge Animal TKB v2 — NTO (New Timbral Oscillator).
// FOUNDATION STUB: jack/ctl registration is FROZEN per docs/SPEC.md §4.1
// (indices are append-only forever). tick() is silence; the stage-2 NTO
// builder implements the DSP against gates T02-T04, T17 using DspUtils only.
#pragma once
#include "EngineCore.h"

namespace satk {

// Triangle-core VCO: sine/tri/saw/variable + pulse outs, two calibrated 1V/oct
// inputs, attenuverted exponential FM, AC-coupled through-zero linear FM with
// VC index, VC waveshape (sine -> 'M'/pulse-like), SOFT sync.
class NTO : public Module
{
public:
    enum J { jSine, jTri, jSaw, jVar, jPulse,
             jOctA, jOctB, jVcFreq, jLinFm, jVcLin, jVcWave, jSync };
    enum C { cFreq, cFine, cWave, cVcFreqAmt, cLinDepth };

    NTO()
    {
        addJack ("SINE",    JackType::Out, JackColor::Black);
        addJack ("TRI",     JackType::Out, JackColor::Black);
        addJack ("SAW",     JackType::Out, JackColor::Black);
        addJack ("VAR",     JackType::Out, JackColor::Black);
        addJack ("PULSE",   JackType::Out, JackColor::Red);
        addJack ("1V/OCT A",JackType::In,  JackColor::White);
        addJack ("1V/OCT B",JackType::In,  JackColor::White);
        addJack ("VC FREQ", JackType::In,  JackColor::White);
        addJack ("LIN FM",  JackType::In,  JackColor::Black);
        addJack ("VC LIN",  JackType::In,  JackColor::White);
        addJack ("VC WAVE", JackType::In,  JackColor::White);
        addJack ("SYNC",    JackType::In,  JackColor::Purple);

        addCtl ("FREQ",        0.4045f); // expo 16 Hz..16 kHz -> 261.63 Hz
        addCtl ("FINE",        0.5f);    // ±100 cents, center = 0
        addCtl ("WAVESHAPE",   0.15f);   // 0..1, sums with VC WAVE
        addCtl ("VC FREQ AMT", 0.5f);    // attenuverter, full = ±0.4 oct/V
        addCtl ("LIN FM DEPTH",0.0f);    // 0..1, full = ±3·f0 per 5 V
        leds.resize (1);
    }

    void prepare (double sampleRate) override
    {
        Module::prepare (sampleRate);
        // STUB: stage-2 builder resets phase/drift state here, derived from moduleSeed.
    }

    void tick() override
    {
        // STUB: silence. Stage-2 NTO builder: polyBLEP/BLAMP core, ADAA shaper,
        // TZFM via signed increment, soft sync as bounded phase nudge.
    }
};

} // namespace satk
