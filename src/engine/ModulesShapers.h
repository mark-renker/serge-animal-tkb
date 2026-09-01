// Serge Animal TKB v2 — VCM (Wave Multipliers) and RING (Ring Modulator).
// FOUNDATION STUBS: jack/ctl registration FROZEN per docs/SPEC.md §4.5-§4.6
// (indices append-only forever). tick() stubbed; the stage-2 shapers builder
// implements against gates T08/T09 using DspUtils only.
#pragma once
#include "EngineCore.h"

namespace satk {

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

    void prepare (double sampleRate) override { Module::prepare (sampleRate); }
    void tick() override
    {
        // STUB: silence. Stage-2 shapers builder: piecewise folding cascade
        // with 1st/2nd-order ADAA + 2x OS, rail clip inside the ADAA kernel.
    }

private:
    static constexpr const char* kLoHi[2] = { "LO (VCA)", "HI (SQUARE-UP)" };
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

    void prepare (double sampleRate) override { Module::prepare (sampleRate); }
    void tick() override
    {
        // STUB: silence. Stage-2 shapers builder: tanh·tanh transconductance
        // model, ADAA + 2x OS, carrier suppression ≥ 55 dB at nominal trims.
    }
};

} // namespace satk
