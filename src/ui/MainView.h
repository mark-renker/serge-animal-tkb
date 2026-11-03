// Serge Animal TKB v2 — main panel view (two Animal rows + full-width TKB
// strip), banana-cable interaction layer, zoom control, shared TooltipWindow,
// and the plugin editor: free-shape window over a scrolling panel, independent
// zoom (0.5x-2.4x); zoom and window size both persist (U01/U02).
//
// FOUNDATION SKELETON: module faces use a generic auto-layout (all jacks and
// ctls placed on an inch grid with labels + tooltips) plus a hand-blocked TKB
// strip (4x16 knob field, 16 touch pads, one-hot stage jacks). The UI fan-out
// replaces buildSkeleton* with per-module layouts transcribed from the
// hardware photos; everything else here (cables, drag-patching, zoom, resize,
// tooltips, editor) is the shipping implementation.
#pragma once
#include "Widgets.h"
#include "../plugin/PluginProcessor.h"

namespace satkui {

constexpr float RAIL_IN  = 0.24f;   // wooden side rails, inches
constexpr int   HEADER_H = 30;
constexpr int   ROW_GAP  = 20;
constexpr float TKB_H_IN = 6.45f;    // TKB strip (still much wider than tall)
constexpr float IO_H_IN  = 1.0f;    // dark KEY/MIDI I/O strip

inline juce::RangedAudioParameter& ModuleComp::ctlParam (int ctlIdx)
{
    auto* p = proc.apvts.getParameter (SatkProcessor::paramID (target(), ctlIdx));
    jassert (p != nullptr);
    return *p;
}

inline ButtonW* ModuleComp::addButton (int ctlIdx, float x, float y, const juce::String& tip, float r)
{
    auto* b = new ButtonW (tip);
    auto* modPtr = &target();
    auto* procPtr = &proc;
    b->onPress = [procPtr, modPtr, ctlIdx] (bool down) { procPtr->setMomentary (modPtr, ctlIdx, down); };
    buttons.add (b);
    addAndMakeVisible (b);
    b->setBounds (pxr (sq (x, y, r)));
    return b;
}

inline TouchPadW* ModuleComp::addPad (int padCtlIdx, int stageIdx, float x, float y,
                                      float wInches, float hInches, int pressureCtlIdx,
                                      const juce::String& tip)
{
    auto* modPtr = &target();
    auto* procPtr = &proc;
    auto* p = new TouchPadW (target(), stageIdx, tip,
        [procPtr, modPtr, padCtlIdx] (bool down) { procPtr->setMomentary (modPtr, padCtlIdx, down); },
        ctlParam (pressureCtlIdx));
    pads.add (p);
    addAndMakeVisible (p);
    p->setBounds (pxr ({ x - wInches / 2, y - hInches / 2, wInches, hInches }));
    return p;
}

// ------------------------------------------------ per-module layouts --------
// Hardware-faithful faces transcribed from the reference photos
// (gauntlet/reference: STS Animal panel, La Bestia II/III, R*S TKB) in the
// PF50 house layout language: inch-grid ModuleComp, section boxes, jack
// labels beneath, waveform glyphs, candy arches over the big knobs.

inline void buildNTO (ModuleComp& c)
{
    using M = satk::NTO;
    auto& m = c.mod;
    // outputs box (STS: SINE/TRIANGLE/SAW row, VARIABLE/PULSE row)
    c.addBox (0.14f, 0.66f, 3.12f, 1.70f);
    c.addLabel ("OUTPUTS", 1.70f, 0.735f, 1.2f, 0.125f);
    c.addJack (M::jSine, 0.62f, 1.22f, "SINE", true, jackTip (m, M::jSine));
    c.addJack (M::jTri,  1.70f, 1.22f, "TRI",  true, jackTip (m, M::jTri));
    c.addJack (M::jSaw,  2.78f, 1.22f, "SAW",  true, jackTip (m, M::jSaw));
    c.addJack (M::jVar,   1.16f, 1.94f, "VAR",   true, jackTip (m, M::jVar));
    c.addJack (M::jPulse, 2.24f, 1.94f, "PULSE", true, jackTip (m, M::jPulse));
    c.addLed (0, 3.06f, 0.86f, "Oscillator LED: pulses at the oscillator rate (visible at LFO speeds).", 0.10f);
    // pitch section (1V/OCT pair, VC FREQ + attenuverter)
    c.addBox (0.14f, 2.52f, 1.60f, 1.92f);
    c.addLabel ("A", 0.55f, 2.60f, 0.3f, 0.115f);
    c.addLabel ("B", 1.23f, 2.60f, 0.3f, 0.115f);
    c.addJack (M::jOctA, 0.55f, 2.94f, {}, true, jackTip (m, M::jOctA));
    c.addJack (M::jOctB, 1.23f, 2.94f, {}, true, jackTip (m, M::jOctB));
    c.addLabel ("1V/OCT", 0.89f, 3.14f, 0.9f, 0.135f);
    c.addJack (M::jVcFreq, 0.55f, 3.82f, "VC FREQ", true, jackTip (m, M::jVcFreq), 0.78f);
    c.addKnob (M::cVcFreqAmt, 1.23f, 3.82f, false, true, {}, ctlTip (m, M::cVcFreqAmt));
    // linear FM section
    c.addBox (1.88f, 2.52f, 1.38f, 1.92f);
    c.addLabel ("LIN FM", 2.57f, 2.60f, 0.8f, 0.125f);
    c.addJack (M::jLinFm, 2.24f, 3.06f, "IN", true, jackTip (m, M::jLinFm), 0.6f);
    c.addJack (M::jVcLin, 2.92f, 3.06f, "VC", true, jackTip (m, M::jVcLin), 0.6f);
    c.addKnob (M::cLinDepth, 2.57f, 3.86f, false, false, "DEPTH", ctlTip (m, M::cLinDepth));
    // waveshape section
    c.addBox (0.14f, 4.58f, 1.60f, 1.36f);
    c.addJack (M::jVcWave, 0.55f, 5.02f, "VC WAVE", true, jackTip (m, M::jVcWave), 0.82f);
    c.addKnob (M::cWave, 1.23f, 5.02f, false, false, {}, ctlTip (m, M::cWave));
    c.addGlyph (Glyph::Sine,    1.02f, 5.48f, 0.32f, 0.16f);
    c.addGlyph (Glyph::PulseUp, 1.46f, 5.48f, 0.30f, 0.16f);
    c.addLine (1.20f, 5.40f, 1.20f, 5.56f);
    // sync / fine / freq
    c.addJack (M::jSync, 0.55f, 6.30f, "SYNC", true, jackTip (m, M::jSync), 0.7f);
    c.addKnob (M::cFine, 1.55f, 6.22f, false, false, "FINE", ctlTip (m, M::cFine));
    c.addKnob (M::cFreq, 2.68f, 6.16f, true, false, {}, ctlTip (m, M::cFreq));
    c.addLabel ("FREQ", 2.68f, 6.60f, 0.8f, 0.14f, true);
}

inline void buildVCM (ModuleComp& c)
{
    using M = satk::VCM;
    auto& m = c.mod;
    // TOP section
    c.addBox (0.14f, 0.66f, 2.72f, 1.86f);
    c.addLabel ("TOP", 0.50f, 0.725f, 0.55f, 0.12f);
    c.addLed (0, 2.62f, 0.80f, "Wave-multiplier LED: shows the middle (folder) output level.", 0.09f);
    c.addJack (M::jTopIn,  0.55f, 1.22f, "IN",  true, jackTip (m, M::jTopIn), 0.6f);
    c.addJack (M::jVcTop,  1.50f, 1.22f, "VC",  true, jackTip (m, M::jVcTop), 0.6f);
    c.addJack (M::jTopOut, 2.45f, 1.22f, "OUT", true, jackTip (m, M::jTopOut), 0.6f);
    c.addToggle (M::cTopMode, 0.85f, 2.06f, 2, ctlTip (m, M::cTopMode));
    c.addLabel ("LO", 1.24f, 1.82f, 0.35f, 0.115f);
    c.addLabel ("HI", 1.24f, 2.18f, 0.35f, 0.115f);
    c.addKnob (M::cTopDrive, 2.05f, 2.06f, false, false, "DRIVE", ctlTip (m, M::cTopDrive));
    // MIDDLE section (the classic folder)
    c.addBox (0.14f, 2.66f, 2.72f, 2.24f);
    c.addLabel ("MIDDLE", 0.62f, 2.725f, 0.8f, 0.12f);
    c.addJack (M::jMidIn1, 0.55f, 3.18f, "IN 1", true, jackTip (m, M::jMidIn1), 0.6f);
    c.addJack (M::jMidIn2, 1.50f, 3.18f, "IN 2", true, jackTip (m, M::jMidIn2), 0.6f);
    c.addJack (M::jMidOut, 2.45f, 3.18f, "OUT",  true, jackTip (m, M::jMidOut), 0.6f);
    c.addJack (M::jVcFold, 0.55f, 4.14f, "VC FOLD", true, jackTip (m, M::jVcFold), 0.8f);
    c.addKnob (M::cFold, 1.50f, 4.10f, true, false, {}, ctlTip (m, M::cFold));
    c.addLabel ("FOLD", 1.50f, 4.53f, 0.7f, 0.13f, true);
    c.addKnob (M::cBias, 2.45f, 4.10f, false, true, {}, ctlTip (m, M::cBias));
    c.addLabel ("BIAS", 2.45f, 4.56f, 0.7f, 0.125f);
    // BOTTOM section (rectifier cascade)
    c.addBox (0.14f, 5.04f, 2.72f, 1.86f);
    c.addLabel ("BOTTOM", 0.62f, 5.105f, 0.8f, 0.12f);
    c.addJack (M::jBotIn1, 0.55f, 5.56f, "IN 1", true, jackTip (m, M::jBotIn1), 0.6f);
    c.addJack (M::jBotIn2, 1.50f, 5.56f, "IN 2", true, jackTip (m, M::jBotIn2), 0.6f);
    c.addJack (M::jBotOut, 2.45f, 5.56f, "OUT",  true, jackTip (m, M::jBotOut), 0.6f);
    c.addJack (M::jVcBot, 0.55f, 6.42f, "VC", true, jackTip (m, M::jVcBot), 0.6f);
    c.addKnob (M::cBotDepth, 1.50f, 6.38f, false, false, "DEPTH", ctlTip (m, M::cBotDepth));
    c.addGlyph (Glyph::PulseUp, 2.14f, 6.16f, 0.28f, 0.15f);
    c.addJack (M::jBotSq, 2.45f, 6.42f, "SQ OUT", true, jackTip (m, M::jBotSq), 0.72f);
}

inline void buildRING (ModuleComp& c)
{
    using M = satk::RING;
    auto& m = c.mod;
    c.addLed (0, 1.38f, 0.84f, "Ring-modulator LED: shows the output level.", 0.09f);
    c.addJack (M::jX, 0.50f, 1.22f, "X IN", true, jackTip (m, M::jX), 0.62f);
    c.addJack (M::jY, 1.20f, 1.22f, "Y IN", true, jackTip (m, M::jY), 0.62f);
    c.addJack (M::jVcMorph, 0.50f, 2.24f, "VC", true, jackTip (m, M::jVcMorph), 0.6f);
    c.addKnob (M::cMorph, 1.20f, 2.24f, false, false, "MORPH", ctlTip (m, M::cMorph));
    c.addKnob (M::cGain, 0.85f, 3.40f, false, false, "GAIN", ctlTip (m, M::cGain));
    // bleed trims (a boxed "feature" pair, per the R*S rejection-trim note)
    c.addBox (0.14f, 4.10f, 1.42f, 1.30f);
    c.addLabel ("BLEED", 0.85f, 4.165f, 0.8f, 0.12f);
    c.addKnob (M::cTrimX, 0.50f, 4.86f, false, false, "X", ctlTip (m, M::cTrimX));
    c.addKnob (M::cTrimY, 1.20f, 4.86f, false, false, "Y", ctlTip (m, M::cTrimY));
    c.addJack (M::jOut, 0.85f, 6.12f, "OUT", true, jackTip (m, M::jOut), 0.7f);
}

inline void buildVCFQ (ModuleComp& c)
{
    using M = satk::VCFQ;
    auto& m = c.mod;
    // outputs box (La Bestia: BAND/HIGH over NOTCH/LOW)
    c.addBox (0.14f, 0.66f, 2.62f, 1.74f);
    c.addLabel ("OUTPUTS", 1.45f, 0.725f, 1.2f, 0.12f);
    c.addJack (M::jBp,    0.75f, 1.20f, "BAND",  true, jackTip (m, M::jBp), 0.7f);
    c.addJack (M::jHp,    2.15f, 1.20f, "HIGH",  true, jackTip (m, M::jHp), 0.7f);
    c.addJack (M::jNotch, 0.75f, 1.94f, "NOTCH", true, jackTip (m, M::jNotch), 0.7f);
    c.addJack (M::jLp,    2.15f, 1.94f, "LOW",   true, jackTip (m, M::jLp), 0.7f);
    c.addLed (0, 2.56f, 0.86f, "Filter LED: shows the band-pass output level.", 0.10f);
    // inputs
    c.addJack (M::jIn,  0.75f, 2.86f, "IN", true, jackTip (m, M::jIn), 0.6f);
    c.addJack (M::jAgc, 2.15f, 2.86f, "AGC IN", true, jackTip (m, M::jAgc), 0.8f);
    c.addKnob (M::cGain, 0.75f, 3.66f, false, false, "GAIN", ctlTip (m, M::cGain));
    c.addLine (0.75f, 3.06f, 0.75f, 3.38f);
    c.addJack (M::jTrig, 2.15f, 3.66f, "TRIG IN", true, jackTip (m, M::jTrig), 0.8f);
    // CV row: VC Q / 1V/OCT / VC F with attenuverters below
    c.addJack (M::jVcQ, 0.60f, 4.48f, "VC Q", true, jackTip (m, M::jVcQ), 0.62f);
    c.addJack (M::jOct, 1.45f, 4.48f, "1V/OCT", true, jackTip (m, M::jOct), 0.75f);
    c.addJack (M::jVcF, 2.30f, 4.48f, "VC F", true, jackTip (m, M::jVcF), 0.62f);
    c.addKnob (M::cVcQAmt, 0.60f, 5.28f, false, true, {}, ctlTip (m, M::cVcQAmt));
    c.addKnob (M::cPing,   1.45f, 5.28f, false, false, "PING", ctlTip (m, M::cPing));
    c.addKnob (M::cVcFAmt, 2.30f, 5.28f, false, true, {}, ctlTip (m, M::cVcFAmt));
    // big Q / FREQ
    c.addKnob (M::cQ, 0.72f, 6.22f, true, false, {}, ctlTip (m, M::cQ));
    c.addLabel ("Q", 0.72f, 6.64f, 0.5f, 0.14f, true);
    c.addKnob (M::cFreq, 2.02f, 6.22f, true, false, {}, ctlTip (m, M::cFreq));
    c.addLabel ("FREQ", 2.02f, 6.64f, 0.8f, 0.14f, true);
}

inline void buildNoiseRvg (ModuleComp& c, satk::Module& noise)
{
    c.titleText = "NOISE & RANDOM VOLTAGES";
    {
        using N = satk::NOISE;
        c.use (&noise);
        c.addBox (0.14f, 0.66f, 1.82f, 1.86f);
        c.addLabel ("NOISE", 1.05f, 0.72f, 1.0f, 0.14f);
        c.addJack (N::jWhite, 0.55f, 1.20f, "WHITE", true, jackTip (noise, N::jWhite), 0.8f);
        c.addJack (N::jPink,  1.55f, 1.20f, "PINK",  true, jackTip (noise, N::jPink), 0.8f);
        c.addJack (N::jShSrc, 0.55f, 2.02f, "S/H SRC", true, jackTip (noise, N::jShSrc), 0.9f);
        c.addGlyph (Glyph::Saw, 1.45f, 2.02f, 0.40f, 0.20f);
        c.use (nullptr);
    }
    using M = satk::RVG;
    auto& m = c.mod;
    c.addBox (0.14f, 2.66f, 1.82f, 1.90f);
    c.addLabel ("RANDOM", 1.05f, 2.72f, 1.0f, 0.14f);
    c.addLed (0, 1.80f, 2.84f, "Random-clock LED: flashes on each internal clock pulse.", 0.09f);
    c.addJack (M::jStepped, 0.55f, 3.18f, "STEPPED", true, jackTip (m, M::jStepped), 0.85f);
    c.addJack (M::jSmooth,  1.55f, 3.18f, "SMOOTH",  true, jackTip (m, M::jSmooth), 0.85f);
    c.addJack (M::jPulse,   0.55f, 4.02f, "PULSE",   true, jackTip (m, M::jPulse), 0.7f);
    c.addJack (M::jTrig,    1.55f, 4.02f, "TRIG",    true, jackTip (m, M::jTrig), 0.7f);
    c.addJack (M::jVcRate, 1.05f, 4.98f, "VC RATE", true, jackTip (m, M::jVcRate), 0.9f);
    c.addLine (1.05f, 5.20f, 1.05f, 5.54f);
    c.addKnob (M::cRate, 1.05f, 5.98f, true, false, {}, ctlTip (m, M::cRate));
    c.addLabel ("RATE", 1.05f, 6.42f, 0.8f, 0.14f, true);
}

// VCA 1 + VCA 2 as one dual-VCA panel (cf. the Animal's dual-VCA UAP).
inline void buildDualVca (ModuleComp& c, satk::Module& vca2)
{
    c.titleText = "DUAL VCA";
    using M = satk::VCA;
    satk::Module* chans[2] = { &c.mod, &vca2 };
    for (int ch = 0; ch < 2; ++ch)
    {
        auto& m = *chans[ch];
        c.use (ch == 0 ? nullptr : &vca2);
        const float x  = ch == 0 ? 0.63f : 1.67f;
        const float bx = ch == 0 ? 0.14f : 1.18f;
        c.addBox (bx, 0.66f, 0.98f, 6.00f);
        c.addLabel (ch == 0 ? "1" : "2", x, 0.72f, 0.3f, 0.16f);
        c.addJack (M::jIn, x, 1.22f, "IN", true, jackTip (m, M::jIn), 0.6f);
        c.addJack (M::jVc, x, 2.20f, {}, true, jackTip (m, M::jVc));
        c.addDiamond (x, 2.20f, 0.30f);
        c.addLabel ("VC", x, 2.54f, 0.6f, 0.145f);
        c.addKnob (M::cLevel, x, 3.30f, false, false, "LEVEL", ctlTip (m, M::cLevel));
        c.addToggle (M::cResponse, x - 0.16f, 4.48f, 2, ctlTip (m, M::cResponse));
        c.addLabel ("LIN", x + 0.20f, 4.30f, 0.4f, 0.14f);
        c.addLabel ("EXP", x + 0.20f, 4.56f, 0.4f, 0.14f);
        c.addLed (0, x, 5.44f, "VCA LED: shows the audio level leaving the VCA.", 0.09f);
        c.addLine (x, 5.58f, x, 5.80f);
        c.addJack (M::jOut, x, 6.02f, "OUT", true, jackTip (m, M::jOut), 0.6f);
    }
    c.use (nullptr);
}

// KEY/MIDI as the dark I/O strip along the bottom (PF50 house layout).
inline void buildKeyStrip (ModuleComp& c)
{
    using M = satk::KEY;
    auto& m = c.mod;
    c.darkFace = true;
    c.addLabel ("MIDI", 0.55f, 0.18f, 0.8f, 0.15f);
    c.addJack (M::jKv,   0.55f, 0.48f, "1V/OCT", true, jackTip (m, M::jKv), 0.8f);
    c.addJack (M::jGate, 1.35f, 0.48f, "GATE",   true, jackTip (m, M::jGate), 0.7f);
    c.addJack (M::jVel,  2.15f, 0.48f, "VEL",    true, jackTip (m, M::jVel), 0.7f);
    c.addJack (M::jClk,  2.95f, 0.48f, "CLK",    true, jackTip (m, M::jClk), 0.7f);
    c.addKnob (M::cGlide, 3.85f, 0.42f, false, false, "GLIDE", ctlTip (m, M::cGlide));
    c.addLed (0, 4.55f, 0.42f, "Gate LED: lit while a MIDI note is held.", 0.09f);
    c.addLabel ("SERGE ANIMAL TKB  -  SOFTWARE RECREATION  -  RENKY DENK", c.wIn - 3.0f, 0.40f, 5.6f, 0.14f);
}

inline void buildDUSG (ModuleComp& c)
{
    using M = satk::DUSG;
    auto& m = c.mod;
    c.addLed (0, 1.00f, 0.86f, "Slope LED: follows the 0-5 V slope output.", 0.09f);
    c.addBox (0.14f, 0.66f, 1.72f, 2.10f);
    c.addJack (M::jOut, 0.55f, 1.30f, "OUT", true, jackTip (m, M::jOut), 0.6f);
    c.addGlyph (Glyph::ArcAR, 1.45f, 1.02f, 0.32f, 0.16f);
    c.addJack (M::jEnd, 1.45f, 1.30f, "END", true, jackTip (m, M::jEnd), 0.6f);
    c.addJack (M::jTrig, 0.55f, 2.24f, "TRIG", true, jackTip (m, M::jTrig), 0.6f);
    c.addJack (M::jIn,   1.45f, 2.24f, "IN", true, jackTip (m, M::jIn), 0.6f);
    // rate section
    c.addBox (0.14f, 2.94f, 1.72f, 2.62f);
    c.addJack (M::jVcRise, 0.55f, 3.42f, "VC RISE", true, jackTip (m, M::jVcRise), 0.8f);
    c.addJack (M::jVcFall, 1.45f, 3.42f, "VC FALL", true, jackTip (m, M::jVcFall), 0.8f);
    c.addLine (0.55f, 3.62f, 0.55f, 4.10f);
    c.addLine (1.45f, 3.62f, 1.45f, 4.10f);
    c.addGlyph (Glyph::RampUp,   0.55f, 4.28f, 0.3f, 0.16f);
    c.addGlyph (Glyph::RampDown, 1.45f, 4.28f, 0.3f, 0.16f);
    c.addKnob (M::cRise, 0.55f, 4.86f, false, false, "RISE", ctlTip (m, M::cRise));
    c.addKnob (M::cFall, 1.45f, 4.86f, false, false, "FALL", ctlTip (m, M::cFall));
    c.addToggle (M::cCycle, 1.00f, 6.10f, 2, ctlTip (m, M::cCycle));
    c.addLabel ("CYCLE", 1.00f, 6.44f, 0.8f, 0.13f);
}

inline void buildSSG (ModuleComp& c)
{
    using M = satk::SSG;
    auto& m = c.mod;
    // smooth section
    c.addBox (0.14f, 0.66f, 2.92f, 2.16f);
    c.addLabel ("SMOOTH", 0.68f, 0.725f, 0.85f, 0.12f);
    c.addJack (M::jSmIn,     0.60f, 1.28f, "IN", true, jackTip (m, M::jSmIn), 0.6f);
    c.addJack (M::jSmVcRate, 1.60f, 1.28f, "VC RATE", true, jackTip (m, M::jSmVcRate), 0.8f);
    c.addJack (M::jSmOut,    2.60f, 1.28f, "OUT", true, jackTip (m, M::jSmOut), 0.6f);
    c.addLed (0, 2.16f, 1.02f, "Smooth LED: follows the smooth-section output voltage.", 0.09f);
    c.addJack (M::jHold,  0.60f, 2.22f, "HOLD", true, jackTip (m, M::jHold), 0.6f);
    c.addJack (M::jCycle, 1.60f, 2.22f, "CYCLE", true, jackTip (m, M::jCycle), 0.66f);
    c.addKnob (M::cSmRate, 2.60f, 2.18f, false, false, "RATE", ctlTip (m, M::cSmRate));
    // stepped section
    c.addBox (0.14f, 3.06f, 2.92f, 2.16f);
    c.addLabel ("STEPPED", 0.70f, 3.125f, 0.9f, 0.12f);
    c.addJack (M::jStIn,   0.60f, 3.68f, "IN", true, jackTip (m, M::jStIn), 0.6f);
    c.addJack (M::jSample, 1.60f, 3.68f, "SAMPLE", true, jackTip (m, M::jSample), 0.76f);
    c.addJack (M::jStOut,  2.60f, 3.68f, "OUT", true, jackTip (m, M::jStOut), 0.6f);
    c.addLed (1, 2.16f, 3.42f, "Stepped LED: follows the stepped-section output voltage.", 0.09f);
    c.addJack (M::jStVcRate, 0.60f, 4.62f, "VC RATE", true, jackTip (m, M::jStVcRate), 0.8f);
    c.addKnob (M::cStRate, 2.60f, 4.58f, false, false, "RATE", ctlTip (m, M::cStRate));
    // coupler
    c.addJack (M::jCoupler,    1.20f, 5.92f, {}, true, jackTip (m, M::jCoupler));
    c.addJack (M::jHotCoupler, 2.00f, 5.92f, "HOT", true, jackTip (m, M::jHotCoupler), 0.6f);
    c.addLine (1.40f, 5.92f, 1.80f, 5.92f);
    c.addLabel ("COUPLER", 1.60f, 6.24f, 1.1f, 0.135f);
}

inline void buildOUT (ModuleComp& c)
{
    using M = satk::OUT;
    auto& m = c.mod;
    c.addLed (0, 2.36f, 0.84f, "Output LED: shows the final stereo output level.", 0.10f);
    c.addJack (M::jIn1, 0.70f, 1.22f, "IN 1", true, jackTip (m, M::jIn1), 0.6f);
    c.addJack (M::jIn2, 2.00f, 1.22f, "IN 2", true, jackTip (m, M::jIn2), 0.6f);
    c.addLine (0.70f, 1.42f, 0.70f, 1.84f);
    c.addLine (2.00f, 1.42f, 2.00f, 1.84f);
    c.addKnob (M::cPan1, 0.70f, 2.22f, false, true, "PAN 1", ctlTip (m, M::cPan1));
    c.addKnob (M::cPan2, 2.00f, 2.22f, false, true, "PAN 2", ctlTip (m, M::cPan2));
    // monitor bus
    c.addBox (0.14f, 3.10f, 2.42f, 1.66f);
    c.addLabel ("MONITOR", 1.35f, 3.19f, 1.1f, 0.14f);
    c.addChoice (M::cMonSrc, 1.35f, 3.66f, 2.05f, ctlTip (m, M::cMonSrc));
    c.addKnob (M::cMonLvl, 1.35f, 4.36f, false, false, "LEVEL", ctlTip (m, M::cMonLvl));
    // drift + master
    c.addKnob (M::cDrift, 2.00f, 5.60f, false, false, "DRIFT", ctlTip (m, M::cDrift));
    c.addKnob (M::cMaster, 0.85f, 6.10f, true, false, {}, ctlTip (m, M::cMaster));
    c.addLabel ("MASTER", 0.85f, 6.54f, 1.0f, 0.14f, true);
}

// TKB strip, modeled on the R*S TKB panel: I/O row on top (outputs boxed at
// left, transport inputs boxed at right), clock/mode controls, 16 stage-pulse
// jacks over the 4x16 knob field with row LEDs, and the black touch-keyboard
// strip with 16 gold pads at the bottom.
inline void buildTKB (ModuleComp& c)
{
    // Modeled on the Random*Source TKB panel: ONE 16-column grid shared by the
    // top I/O jacks (outputs boxed, transport inputs after), the stage-pulse
    // row, the 4x16 knob field and the touch keyboard. The software-only
    // clock/mode controls (no hardware equivalent) live in a boxed block at
    // the left edge so they don't break the hardware grid.
    using T = satk::TKB;
    auto& m = c.mod;
    const float w = c.wIn;
    c.showTitle = false;

    // ---- left control block (software clock / mode) ----
    c.addBox (0.18f, 0.40f, 1.42f, 5.72f);
    c.addLabel ("CLOCK / MODE", 0.89f, 0.46f, 1.3f, 0.14f);
    c.addToggle (T::cRun,    0.55f, 0.98f, 2, ctlTip (m, T::cRun));
    c.addLabel ("RUN", 0.55f, 1.30f, 0.6f, 0.14f);
    c.addToggle (T::cRandom, 1.23f, 0.98f, 2, ctlTip (m, T::cRandom));
    c.addLabel ("RANDOM", 1.23f, 1.30f, 0.7f, 0.14f);
    c.addKnob (T::cRate,    0.55f, 1.95f, false, false, "RATE",  ctlTip (m, T::cRate));
    c.addKnob (T::cGateLen, 1.23f, 1.95f, false, false, "GATE",  ctlTip (m, T::cGateLen));
    c.addKnob (T::cRange,   0.55f, 3.00f, false, false, "RANGE", ctlTip (m, T::cRange));
    c.addKnob (T::cGlide,   1.23f, 3.00f, false, false, "GLIDE", ctlTip (m, T::cGlide));
    c.addChoice (T::cLength, 0.89f, 4.30f, 1.10f, ctlTip (m, T::cLength), {});
    c.addLabel ("LENGTH", 0.89f, 4.52f, 0.9f, 0.14f);

    // ---- the 16-column grid ----
    const float gx0 = 2.30f, gx1 = w - 0.18f;
    const float pitch = (gx1 - gx0) / 16.0f;
    auto colX = [&] (int s) { return gx0 + pitch * ((float) s + 0.5f); };

    // top row: 9 outputs (boxed, hardware order) + 7 transport inputs
    struct TopJ { int j; const char* l1; const char* l2; };
    const TopJ top[16] = {
        { T::jAbcd, "ABCD", nullptr }, { T::jA, "A", nullptr }, { T::jB, "B", nullptr },
        { T::jC, "C", nullptr },       { T::jD, "D", nullptr },
        { T::jKv, "KEY", "VOLT" },     { T::jKp, "KEY", "PULSE" }, { T::jGate, "KEY", "GATE" },
        { T::jPressure, "PRES-", "SURE" },
        { T::jClock, "CLOCK", nullptr }, { T::jVertClock, "VERT", "CLOCK" },
        { T::jReset, "RESET", nullptr }, { T::jVertReset, "VERT", "RESET" },
        { T::jRandom, "RANDOM", nullptr }, { T::jUpDown, "UP/", "DOWN" }, { T::jHold, "HOLD", nullptr },
    };
    const float topY = 0.78f;
    c.addBox (colX (0) - pitch * 0.48f, 0.40f, pitch * 8.96f, 1.02f);
    for (int s = 0; s < 16; ++s)
    {
        c.addJack (top[s].j, colX (s), topY, {}, true, jackTip (m, top[s].j));
        c.addLabel (top[s].l1, colX (s), topY + 0.20f, pitch * 0.98f, 0.14f);
        if (top[s].l2 != nullptr)
            c.addLabel (top[s].l2, colX (s), topY + 0.37f, pitch * 0.98f, 0.14f);
    }

    // stage-pulse jacks 1..16, directly under the top row
    for (int s = 0; s < 16; ++s)
    {
        c.addJack (T::jStage1 + s, colX (s), 1.80f, {}, true, jackTip (m, T::jStage1 + s));
        c.addLabel (juce::String (s + 1), colX (s), 2.00f, 0.4f, 0.15f);
    }

    // knob rows A-D with letters + solid row lamps
    const char rowNames[4] = { 'A', 'B', 'C', 'D' };
    for (int r = 0; r < 4; ++r)
    {
        const float y = 2.62f + (float) r * 0.58f;
        c.addLabel (juce::String::charToString (rowNames[r]), gx0 - 0.20f, y - 0.10f, 0.3f, 0.18f);
        auto* led = c.addLed (16 + r, gx0 - 0.48f, y,
                              "Row " + juce::String::charToString (rowNames[r])
                              + " LED: lit while this row drives the ABCD output.", 0.085f);
        led->solid = true;
        for (int s = 0; s < 16; ++s)
        {
            const int ctl = T::cRowA1 + r * 16 + s;
            c.addKnob (ctl, colX (s), y, false, false, {}, ctlTip (m, ctl));
        }
    }

    // touch keyboard: full-width black strip, tall gold egg pads filling it
    const float stripY = 5.04f, stripH = 1.08f;
    c.addFill (gx0 - 0.62f, stripY, w - 0.18f - (gx0 - 0.62f), stripH, juce::Colour (0xff17150f));
    for (int s = 0; s < 16; ++s)
        c.addPad (T::cPad1 + s, s, colX (s), stripY + stripH * 0.5f,
                  juce::jmin (0.70f, pitch * 0.90f), stripH - 0.08f,
                  T::cPressure, ctlTip (m, T::cPad1 + s));

    // panel title bottom-right, as on the R*S panel
    c.addLabel ("TOUCH ACTIVATED KEYBOARD SEQUENCER", w - 2.75f, 6.18f, 5.2f, 0.17f);
}

inline void buildSkeleton (ModuleComp& c)
{
    const auto& id = c.mod.id;
    if (id == "nto")   { buildNTO (c);   return; }
    if (id == "vcm")   { buildVCM (c);   return; }
    if (id == "ring")  { buildRING (c);  return; }
    if (id == "vcfq")  { buildVCFQ (c);  return; }
    if (id == "rvg")   { buildNoiseRvg (c, *c.proc.sys.noise); return; }
    if (id == "dusg1" || id == "dusg2") { buildDUSG (c); return; }
    if (id == "ssg")   { buildSSG (c);   return; }
    if (id == "key")   { buildKeyStrip (c); return; }
    if (id == "vca1")  { buildDualVca (c, *c.proc.sys.vca2); return; }
    if (id == "out")   { buildOUT (c);   return; }
    if (id == "tkb")   { buildTKB (c);   return; }
}

// --------------------------------------------------------------------------
class PanelView : public juce::Component, public CableHost, private juce::Timer
{
public:
    explicit PanelView (SatkProcessor& p) : proc (p)
    {
        hostPtr = this;

        auto widthOf = [] (satk::Module* m) -> float
        {
            auto id = m->id;
            if (id == "nto")   return 3.4f;
            if (id == "vcm")   return 3.0f;
            if (id == "ring")  return 1.7f;
            if (id == "vcfq")  return 2.9f;
            if (id == "noise") return 1.3f;
            if (id == "rvg")   return 2.1f; // + NOISE
            if (id == "dusg1" || id == "dusg2") return 2.0f;
            if (id == "ssg")   return 3.2f;
            if (id == "key")   return 1.5f;
            if (id == "vca1")  return 2.3f; // + VCA 2
            if (id == "out")   return 2.7f;
            return 2.0f;
        };

        const std::vector<std::vector<satk::Module*>> rows = {
            { proc.sys.nto, proc.sys.vcm, proc.sys.ring, proc.sys.vcfq, proc.sys.rvg },        // rvg panel hosts NOISE
            { proc.sys.dusg1, proc.sys.dusg2, proc.sys.ssg, proc.sys.vca1, proc.sys.out },     // vca1 panel hosts VCA 2
        };
        rowNames = { "ANIMAL - VOICE", "ANIMAL - CONTROL & OUTPUT",
                     "ANIMAL - TKB" };

        float maxRowIn = 0;
        for (auto& row : rows)
        {
            float w = 2 * RAIL_IN;
            for (auto* m : row) w += widthOf (m);
            maxRowIn = std::max (maxRowIn, w);
        }
        logicalW = (int) std::ceil (maxRowIn * PX) + 20;
        int y = 14; // header is pinned by the editor, outside the scrolling panel
        for (auto& row : rows)
        {
            float rowWIn = 2 * RAIL_IN;
            for (auto* m : row) rowWIn += widthOf (m);
            int x = (logicalW - (int) (rowWIn * PX)) / 2;
            juce::Rectangle<int> rr (x, y, (int) (rowWIn * PX), (int) (MOD_H * PX));
            rowRects.push_back (rr);
            x += (int) (RAIL_IN * PX);
            for (auto* m : row)
            {
                auto* mc = new ModuleComp (*m, widthOf (m), hostPtr, proc);
                buildSkeleton (*mc);
                mods.add (mc);
                addAndMakeVisible (mc);
                mc->setTopLeftPosition (x, y);
                x += mc->getWidth();
            }
            y += (int) (MOD_H * PX) + ROW_GAP;
        }
        // ---- TKB strip (full width, wider-than-tall) ----
        {
            const float wIn = maxRowIn - 2 * RAIL_IN;
            const int x = (logicalW - (int) (maxRowIn * PX)) / 2;
            juce::Rectangle<int> rr (x, y, (int) (maxRowIn * PX), (int) (TKB_H_IN * PX));
            rowRects.push_back (rr);
            auto* mc = new ModuleComp (*proc.sys.tkb, wIn, hostPtr, proc);
            mc->setSize ((int) (wIn * PX), (int) (TKB_H_IN * PX));
            buildSkeleton (*mc);
            mods.add (mc);
            addAndMakeVisible (mc);
            mc->setTopLeftPosition (x + (int) (RAIL_IN * PX), y);
            y += mc->getHeight() + ROW_GAP;
        }
        // ---- dark I/O strip (KEY/MIDI), full width, PF50 style ----
        {
            const float wIn = maxRowIn;
            const int x = (logicalW - (int) (maxRowIn * PX)) / 2;
            auto* mc = new ModuleComp (*proc.sys.key, wIn, hostPtr, proc);
            mc->setSize ((int) (wIn * PX), (int) (IO_H_IN * PX));
            buildSkeleton (*mc);
            mods.add (mc);
            addAndMakeVisible (mc);
            mc->setTopLeftPosition (x, y - ROW_GAP + 18);
            y += mc->getHeight() + 10;
        }
        logicalH = y;
        setSize (logicalW, logicalH);

        for (auto* mc : mods)
            for (auto* j : mc->jacks)
                allJacks.push_back (j);

        startTimerHz (30);
        setOpaque (true);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (col::headerBg);
        // boat backgrounds + wood rails + row nameplates
        for (size_t i = 0; i < rowRects.size(); ++i)
        {
            auto rr = rowRects[i].toFloat();
            g.setColour (juce::Colour (0xff0e0d0f));
            g.fillRect (rr.expanded (3.0f));
            g.setGradientFill (juce::ColourGradient (col::wood, rr.getX(), rr.getY(), col::woodDark, rr.getX(), rr.getBottom(), false));
            g.fillRect (juce::Rectangle<float> (rr.getX(), rr.getY(), RAIL_IN * PX, rr.getHeight()));
            g.fillRect (juce::Rectangle<float> (rr.getRight() - RAIL_IN * PX, rr.getY(), RAIL_IN * PX, rr.getHeight()));
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            for (float wx : { rr.getX() + 3.0f, rr.getRight() - RAIL_IN * PX + 3.0f })
                for (int k = 0; k < 3; ++k)
                    g.drawVerticalLine ((int) (wx + (float) k * 3.7f), rr.getY() + 4, rr.getBottom() - 4);
            // boat nameplate on the lower rail edge
            g.setColour (juce::Colour (0xff0e0d0f));
            g.fillRect (juce::Rectangle<float> (rr.getX() - 3, rr.getBottom() - 1, rr.getWidth() + 6, 15.0f));
            g.setColour (col::ioInk.withAlpha (0.8f));
            g.setFont (panelFont (10.0f, true));
            g.drawText (rowNames[(int) i], (int) rr.getX() + 8, (int) rr.getBottom() - 1, 340, 14, juce::Justification::centredLeft);
        }
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        // ---- banana cables ----
        auto cables = proc.cableSnapshot();
        int idx = 0;
        for (auto& c : cables)
        {
            auto* a = findJack (c.modA, c.jackA);
            auto* b = findJack (c.modB, c.jackB);
            if (a && b) drawCable (g, centreOf (a), centreOf (b), cableColour (idx));
            ++idx;
        }
        if (dragSrc != nullptr)
            drawCable (g, centreOf (dragSrc), dragPos, juce::Colours::white.withAlpha (0.75f));
    }

    // ---- CableHost ----
    void jackDragStart (JackW* j) override { dragSrc = j; dragPos = centreOf (j); repaint(); }
    void jackDragMove (juce::Point<float> screen) override
    {
        dragPos = getLocalPoint (nullptr, screen);
        auto* over = jackAt (dragPos);
        for (auto* j : allJacks) j->dropTarget = (j == over && j != dragSrc);
        repaint();
    }
    void jackDragEnd (juce::Point<float> screen) override
    {
        auto pos = getLocalPoint (nullptr, screen);
        auto* dst = jackAt (pos);
        if (dragSrc && dst && dst != dragSrc)
            proc.addCable (proc.sys.engine.indexOf (&dragSrc->mod), dragSrc->jack,
                           proc.sys.engine.indexOf (&dst->mod), dst->jack);
        for (auto* j : allJacks) j->dropTarget = false;
        dragSrc = nullptr;
        repaint();
    }
    void jackRightClick (JackW* j) override
    {
        juce::PopupMenu menu;
        auto cables = proc.cableSnapshot();
        int mIdx = proc.sys.engine.indexOf (&j->mod);
        std::vector<satk::Cable> mine;
        for (auto& c : cables)
            if ((c.modA == mIdx && c.jackA == j->jack) || (c.modB == mIdx && c.jackB == j->jack))
                mine.push_back (c);
        if (mine.empty()) return;
        int id = 1;
        for (auto& c : mine)
        {
            bool aIsThis = (c.modA == mIdx && c.jackA == j->jack);
            auto& other = aIsThis ? *proc.sys.engine.mods[(size_t) c.modB] : *proc.sys.engine.mods[(size_t) c.modA];
            int oj = aIsThis ? c.jackB : c.jackA;
            menu.addItem (id++, "Remove -> " + juce::String (other.id) + " " + other.jacks[(size_t) oj].name);
        }
        menu.addSeparator();
        menu.addItem (999, "Remove all cables here");
        menu.showMenuAsync (juce::PopupMenu::Options(), [this, mine, mIdx, j] (int result)
        {
            if (result == 999) proc.removeCablesAt (mIdx, j->jack);
            else if (result > 0 && result <= (int) mine.size()) proc.removeCable (mine[(size_t) result - 1]);
            repaint();
        });
    }

    // ---- drag on empty panel space pans the viewport (editor installs onPan;
    // deltas are in SCREEN pixels so panning feels 1:1 at any zoom) ----
    void mouseDown (const juce::MouseEvent& e) override
    {
        lastPanPos = e.getScreenPosition();
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const auto p = e.getScreenPosition();
        if (onPan) onPan (p - lastPanPos);
        lastPanPos = p;
    }
    void mouseUp (const juce::MouseEvent&) override { setMouseCursor (juce::MouseCursor::NormalCursor); }

    int logicalW = 980, logicalH = 1200;
    std::function<void (juce::Point<int>)> onPan;

    juce::OwnedArray<ModuleComp> mods;

private:
    void timerCallback() override
    {
        for (auto* mc : mods)
        {
            for (auto* l : mc->leds) l->refresh();
            for (auto* p : mc->pads) p->refresh();
        }
        int cv = proc.cableVersion();
        if (cv != lastCableVer) { lastCableVer = cv; repaint(); }
    }

    JackW* findJack (int modIdx, int jackIdx)
    {
        auto* m = proc.sys.engine.mods[(size_t) modIdx].get();
        for (auto* j : allJacks)
            if (&j->mod == m && j->jack == jackIdx) return j;
        return nullptr;
    }
    juce::Point<float> centreOf (JackW* j)
    {
        return getLocalPoint (j, j->getLocalBounds().getCentre().toFloat());
    }
    JackW* jackAt (juce::Point<float> p)
    {
        for (auto* j : allJacks)
        {
            auto b = getLocalArea (j, j->getLocalBounds().toFloat()).expanded (5.0f);
            if (b.contains (p)) return j;
        }
        return nullptr;
    }
    static juce::Colour cableColour (int i)
    {
        static const juce::Colour cols[] = {
            juce::Colour (0xffc0392b), juce::Colour (0xff23211e), juce::Colour (0xff2e5fb7),
            juce::Colour (0xff2f7d4f), juce::Colour (0xffd9a62e), juce::Colour (0xff77726a) };
        return cols[i % 6];
    }
    static void drawCable (juce::Graphics& g, juce::Point<float> a, juce::Point<float> b, juce::Colour col)
    {
        float dist = a.getDistanceFrom (b);
        float sag = juce::jmin (150.0f, 22.0f + dist * 0.16f);
        juce::Point<float> m ((a.x + b.x) * 0.5f, juce::jmax (a.y, b.y) + sag);
        juce::Path p;
        p.startNewSubPath (a);
        p.quadraticTo (m, b);
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.strokePath (p, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (col.withAlpha (0.88f));
        g.strokePath (p, juce::PathStrokeType (4.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colours::white.withAlpha (0.18f));
        g.strokePath (p, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        for (auto pt : { a, b })
        {
            g.setColour (col.darker (0.4f));
            g.fillEllipse (juce::Rectangle<float> (13, 13).withCentre (pt));
            g.setColour (col);
            g.fillEllipse (juce::Rectangle<float> (8, 8).withCentre (pt));
        }
    }

    SatkProcessor& proc;
    CableHost* hostPtr = nullptr;
    std::vector<JackW*> allJacks;
    std::vector<juce::Rectangle<int>> rowRects;
    juce::StringArray rowNames;
    JackW* dragSrc = nullptr;
    juce::Point<float> dragPos;
    int lastCableVer = -1;
    juce::Point<int> lastPanPos;
};

// --------------------------------------------------------------------------
// Editor: free-shape window + independent zoom (amendment 2026-11-02, LOG.md).
//   - the WINDOW resizes to any shape (corner drag); it is a viewport onto the
//     panel and never changes the zoom;
//   - ZOOM (menu, or Cmd+wheel toward the pointer) scales the panel only;
//     "Fit" picks the zoom that shows the whole panel in the current window;
//   - wheel/trackpad scrolls, dragging empty panel space pans;
//   - header (title, zoom, CLEAR) is pinned above the scrolling panel;
//   - zoom (editorScale) and window size (editorW/H) persist in plugin state.
class ZoomViewport : public juce::Viewport
{
public:
    std::function<void (float factor, juce::Point<int> posInViewport)> onCmdWheel;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (e.mods.isCommandDown() && onCmdWheel)
        {
            const float dy = w.deltaY != 0.0f ? w.deltaY : w.deltaX;
            onCmdWheel (std::pow (2.0f, dy * 0.6f), e.getEventRelativeTo (this).getPosition());
            return;
        }
        juce::Viewport::mouseWheelMove (e, w);
    }
};

class SatkEditor : public juce::AudioProcessorEditor
{
public:
    static constexpr float kMinScale = 0.5f, kMaxScale = 2.4f; // zoom range
    static constexpr int   kMinW = 420, kMinH = 300;           // smallest window
    static constexpr int   kFitId = 1;                         // zoom-menu id of "Fit"

    explicit SatkEditor (SatkProcessor& p) : AudioProcessorEditor (p), proc (p), view (p)
    {
        // capture persisted state BEFORE installing size limits (setResizeLimits
        // fires resized() on the initial 0x0 bounds)
        const float z0 = juce::jlimit (kMinScale, kMaxScale, proc.getEditorScale());
        int w0 = proc.getEditorW(), h0 = proc.getEditorH();

        setLookAndFeel (&lnf);

        holder.addAndMakeVisible (view);
        viewport.setViewedComponent (&holder, false);
        viewport.setScrollBarsShown (true, true);
        viewport.setScrollBarThickness (10);
        viewport.getVerticalScrollBar().setColour (juce::ScrollBar::thumbColourId, col::ioInk.withAlpha (0.45f));
        viewport.getHorizontalScrollBar().setColour (juce::ScrollBar::thumbColourId, col::ioInk.withAlpha (0.45f));
        viewport.onCmdWheel = [this] (float f, juce::Point<int> at) { setZoom (zoom * f, &at); };
        addAndMakeVisible (viewport);
        view.onPan = [this] (juce::Point<int> d) { viewport.setViewPosition (viewport.getViewPosition() - d); };

        zoomBox.addItem ("Fit", kFitId);
        zoomBox.addSeparator();
        for (int pct : { 50, 75, 100, 125, 150, 200, 240 })
            zoomBox.addItem ("ZOOM  " + juce::String (pct) + " %", pct);
        zoomBox.setTooltip ("Zoom: scales the panel (50-240%). Fit shows the whole panel in the window. "
                            "Cmd+scroll zooms toward the pointer; drag the window corner to any shape.");
        zoomBox.setColour (juce::ComboBox::backgroundColourId, col::headerBg.brighter (0.08f));
        zoomBox.setColour (juce::ComboBox::textColourId,       col::ioInk);
        zoomBox.setColour (juce::ComboBox::outlineColourId,    col::ioInk.withAlpha (0.55f));
        zoomBox.setColour (juce::ComboBox::arrowColourId,      col::ioInk);
        zoomBox.onChange = [this]
        {
            const int id = zoomBox.getSelectedId();
            if (id == kFitId) zoomToFit();
            else if (id > 0)  setZoom ((float) id / 100.0f);
        };
        addAndMakeVisible (zoomBox);

        clearBtn.setButtonText ("CLEAR");
        clearBtn.setTooltip ("Remove every patch cable from the panel (knob settings are kept).");
        clearBtn.setColour (juce::TextButton::buttonColourId,  col::headerBg.brighter (0.08f));
        clearBtn.setColour (juce::TextButton::textColourOffId, col::ioInk);
        clearBtn.onClick = [this] { proc.clearAllCables(); };
        addAndMakeVisible (clearBtn);

        tips = std::make_unique<juce::TooltipWindow> (this, 400); // shared TooltipWindow (U03)

        setResizable (true, true);
        setResizeLimits (kMinW, kMinH,
                         (int) std::lround ((float) view.logicalW * kMaxScale),
                         (int) std::lround ((float) view.logicalH * kMaxScale) + HEADER_H);

        applyZoom (z0);
        if (w0 <= 0 || h0 <= 0) // first open: whole panel at the saved zoom, clipped to the screen
        {
            w0 = holder.getWidth();
            h0 = holder.getHeight() + HEADER_H;
            if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
            {
                w0 = juce::jmin (w0, (int) ((float) d->userArea.getWidth()  * 0.92f));
                h0 = juce::jmin (h0, (int) ((float) d->userArea.getHeight() * 0.88f));
            }
        }
        setSize (juce::jmax (kMinW, w0), juce::jmax (kMinH, h0));
    }

    ~SatkEditor() override { setLookAndFeel (nullptr); }

    // ---- zoom (panel scale; window untouched) ------------------------------
    // keeps the panel point under `anchor` (viewport coords; centre if null) fixed
    void setZoom (float z, const juce::Point<int>* anchor = nullptr)
    {
        z = juce::jlimit (kMinScale, kMaxScale, z);
        const auto a = anchor != nullptr ? *anchor
                                         : juce::Point<int> (viewport.getWidth() / 2, viewport.getHeight() / 2);
        const auto panelPt = (viewport.getViewPosition() + a).toFloat() / zoom;
        applyZoom (z);
        viewport.setViewPosition ((panelPt * zoom).roundToInt() - a);
    }
    void zoomToFit()
    {
        // a hair under exact so the rounded-up content never exceeds the
        // viewport by a pixel (which would bring the scrollbars back)
        setZoom (0.999f * juce::jmin ((float) viewport.getWidth()  / (float) view.logicalW,
                                      (float) viewport.getHeight() / (float) view.logicalH));
        viewport.setViewPosition (0, 0);
    }
    float getZoom() const noexcept { return zoom; }

    // legacy entry point (tests/tools): zoom AND size the window to show the
    // whole panel at that zoom
    void setScaleStep (float f)
    {
        applyZoom (juce::jlimit (kMinScale, kMaxScale, f));
        setSize (holder.getWidth(), holder.getHeight() + HEADER_H);
        viewport.setViewPosition (0, 0);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto header = r.removeFromTop (HEADER_H);
        clearBtn.setBounds (header.removeFromRight (104).reduced (6, 4));
        zoomBox.setBounds (header.removeFromRight (150).reduced (6, 4));
        viewport.setBounds (r);
        proc.setEditorSize (getWidth(), getHeight()); // window size persists, zoom unchanged
    }
    void paint (juce::Graphics& g) override
    {
        g.fillAll (col::headerBg);
        g.setColour (col::ioInk);
        g.setFont (panelFont (15.0f, true));
        g.drawText ("SERGE  ANIMAL  TKB", 14, 0, 200, HEADER_H, juce::Justification::centredLeft);
        if (getWidth() > 640)
        {
            g.setColour (col::ioInk.withAlpha (0.55f));
            g.setFont (panelFont (11.0f, false));
            g.drawText ("RENKY DENK  -  SOFTWARE RECREATION", 220, 0, juce::jmax (0, getWidth() - 480),
                        HEADER_H, juce::Justification::centredLeft);
        }
    }

    juce::TooltipWindow* tooltipWindow() const { return tips.get(); }

    SatkProcessor& proc;
    PanelView view;
    ZoomViewport viewport;
    juce::ComboBox zoomBox;   // the visible ZOOM control (U02)
    juce::TextButton clearBtn;

private:
    void applyZoom (float z)
    {
        zoom = z;
        holder.setSize ((int) std::ceil ((float) view.logicalW * z), (int) std::ceil ((float) view.logicalH * z));
        view.setTransform (juce::AffineTransform::scale (z));
        view.setTopLeftPosition (0, 0);
        proc.setEditorScale (z);
        updateZoomDisplay();
    }
    void updateZoomDisplay()
    {
        const int pct = juce::roundToInt (zoom * 100.0f);
        for (int step : { 50, 75, 100, 125, 150, 200, 240 })
            if (std::abs (pct - step) <= 1) { zoomBox.setSelectedId (step, juce::dontSendNotification); return; }
        zoomBox.setSelectedId (0, juce::dontSendNotification);
        zoomBox.setTextWhenNothingSelected ("ZOOM  " + juce::String (pct) + " %");
    }

    SatkLookAndFeel lnf;
    juce::Component holder;   // sized logical*zoom; hosts the transformed panel
    float zoom = 1.0f;
    std::unique_ptr<juce::TooltipWindow> tips;
};

} // namespace satkui

using SatkEditor = satkui::SatkEditor;
