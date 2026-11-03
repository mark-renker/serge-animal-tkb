// Serge Animal TKB v2 — main panel view (two Animal rows + full-width TKB
// strip), banana-cable interaction layer, zoom control, shared TooltipWindow,
// and the plugin editor with aspect-locked resize (0.5x-2.4x) whose scale is
// the SINGLE persisted source of truth (U01/U02).
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
constexpr float TKB_H_IN = 7.0f;    // TKB strip (still much wider than tall)

inline juce::RangedAudioParameter& ModuleComp::ctlParam (int ctlIdx)
{
    auto* p = proc.apvts.getParameter (SatkProcessor::paramID (mod, ctlIdx));
    jassert (p != nullptr);
    return *p;
}

inline ButtonW* ModuleComp::addButton (int ctlIdx, float x, float y, const juce::String& tip, float r)
{
    auto* b = new ButtonW (tip);
    auto* modPtr = &mod;
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
    auto* modPtr = &mod;
    auto* procPtr = &proc;
    auto* p = new TouchPadW (mod, stageIdx, tip,
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
    c.addLed (0, 1.70f, 1.94f, "Oscillator LED: pulses at the oscillator rate (visible at LFO speeds).", 0.10f);
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
    c.addLed (0, 1.45f, 1.58f, "Filter LED: shows the band-pass output level.", 0.10f);
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

inline void buildNOISE (ModuleComp& c)
{
    using M = satk::NOISE;
    auto& m = c.mod;
    c.addBox (0.14f, 0.66f, 1.02f, 3.62f);
    c.addJack (M::jWhite, 0.65f, 1.16f, "WHITE", true, jackTip (m, M::jWhite), 0.9f);
    c.addJack (M::jPink,  0.65f, 2.24f, "PINK",  true, jackTip (m, M::jPink), 0.9f);
    c.addJack (M::jShSrc, 0.65f, 3.32f, "S/H SRC", true, jackTip (m, M::jShSrc), 0.95f);
    c.addGlyph (Glyph::Saw, 0.65f, 4.02f, 0.34f, 0.18f);
}

inline void buildRVG (ModuleComp& c)
{
    using M = satk::RVG;
    auto& m = c.mod;
    c.addLed (0, 1.42f, 0.84f, "Random-clock LED: flashes on each internal clock pulse.", 0.09f);
    c.addBox (0.14f, 0.66f, 1.42f, 2.30f);
    c.addJack (M::jStepped, 0.50f, 1.22f, "STEPD", true, jackTip (m, M::jStepped), 0.62f);
    c.addJack (M::jSmooth,  1.20f, 1.22f, "SMOOTH", true, jackTip (m, M::jSmooth), 0.68f);
    c.addJack (M::jPulse, 0.50f, 2.24f, "PULSE", true, jackTip (m, M::jPulse), 0.62f);
    c.addJack (M::jTrig,  1.20f, 2.24f, "TRIG", true, jackTip (m, M::jTrig), 0.6f);
    c.addJack (M::jVcRate, 0.85f, 3.62f, "VC RATE", true, jackTip (m, M::jVcRate), 0.9f);
    c.addLine (0.85f, 3.82f, 0.85f, 4.36f);
    c.addKnob (M::cRate, 0.85f, 4.78f, true, false, {}, ctlTip (m, M::cRate));
    c.addLabel ("RATE", 0.85f, 5.22f, 0.8f, 0.14f, true);
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

inline void buildKEY (ModuleComp& c)
{
    using M = satk::KEY;
    auto& m = c.mod;
    c.addLed (0, 1.16f, 0.86f, "Gate LED: lit while a MIDI note is held.", 0.09f);
    c.addBox (0.14f, 0.66f, 1.22f, 3.86f);
    c.addJack (M::jKv, 0.75f, 1.22f, "KV", true, jackTip (m, M::jKv), 0.6f);
    c.addLabel ("~1V/OCT", 0.75f, 1.60f, 0.8f, 0.115f);
    c.addJack (M::jGate, 0.75f, 2.14f, "GATE", true, jackTip (m, M::jGate), 0.6f);
    c.addJack (M::jVel,  0.75f, 3.04f, "VEL", true, jackTip (m, M::jVel), 0.6f);
    c.addJack (M::jClk,  0.75f, 3.94f, "CLK", true, jackTip (m, M::jClk), 0.6f);
    c.addKnob (M::cGlide, 0.75f, 5.10f, false, false, "GLIDE", ctlTip (m, M::cGlide));
}

inline void buildVCA (ModuleComp& c)
{
    using M = satk::VCA;
    auto& m = c.mod;
    c.addJack (M::jIn, 0.65f, 1.22f, "IN", true, jackTip (m, M::jIn), 0.6f);
    c.addJack (M::jVc, 0.65f, 2.20f, "VC", true, jackTip (m, M::jVc), 0.6f);
    c.addDiamond (0.65f, 2.20f, 0.30f);
    c.addKnob (M::cLevel, 0.65f, 3.30f, false, false, "LEVEL", ctlTip (m, M::cLevel));
    c.addToggle (M::cResponse, 0.52f, 4.52f, 2, ctlTip (m, M::cResponse));
    c.addLabel ("LIN", 0.98f, 4.26f, 0.4f, 0.115f);
    c.addLabel ("EXP", 0.98f, 4.66f, 0.4f, 0.115f);
    c.addLed (0, 0.65f, 5.44f, "VCA LED: shows the audio level leaving the VCA.", 0.09f);
    c.addLine (0.65f, 5.58f, 0.65f, 5.80f);
    c.addJack (M::jOut, 0.65f, 6.02f, "OUT", true, jackTip (m, M::jOut), 0.6f);
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
    c.addLabel ("MONITOR", 1.35f, 3.165f, 1.1f, 0.125f);
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
    using T = satk::TKB;
    auto& m = c.mod;
    const float w = c.wIn;

    // ---- I/O row ----
    c.addBox (0.20f, 0.62f, 4.15f, 1.00f);
    const char* rowJ[5] = { "A", "B", "C", "D", "ABCD" };
    for (int i = 0; i < 5; ++i)
        c.addJack (T::jA + i, 0.60f + 0.80f * (float) i + (i == 4 ? 0.10f : 0.0f), 1.06f,
                   rowJ[i], true, jackTip (m, T::jA + i), 0.7f);
    c.addBox (4.55f, 0.62f, 3.30f, 1.00f);
    c.addJack (T::jKv,       4.95f, 1.06f, "KV",    true, jackTip (m, T::jKv), 0.6f);
    c.addJack (T::jPressure, 5.85f, 1.06f, "PRESS", true, jackTip (m, T::jPressure), 0.72f);
    c.addJack (T::jGate,     6.65f, 1.06f, "GATE",  true, jackTip (m, T::jGate), 0.62f);
    c.addJack (T::jKp,       7.40f, 1.06f, "KP",    true, jackTip (m, T::jKp), 0.5f);
    c.addBox (8.10f, 0.62f, 5.70f, 1.00f);
    struct InJ { int j; float x; const char* l1; const char* l2; };
    const InJ ins[7] = {
        { T::jClock,     8.50f,  "CLOCK", nullptr },
        { T::jVertClock, 9.32f,  "VERT",  "CLOCK" },
        { T::jReset,     10.14f, "RESET", nullptr },
        { T::jVertReset, 10.96f, "VERT",  "RESET" },
        { T::jRandom,    11.78f, "RANDOM", nullptr },
        { T::jUpDown,    12.60f, "UP/DN", nullptr },
        { T::jHold,      13.42f, "HOLD",  nullptr },
    };
    for (auto& ij : ins)
    {
        c.addJack (ij.j, ij.x, 1.02f, {}, true, jackTip (m, ij.j));
        c.addLabel (ij.l1, ij.x, 1.22f, 0.78f, 0.115f);
        if (ij.l2 != nullptr)
            c.addLabel (ij.l2, ij.x, 1.355f, 0.78f, 0.115f);
    }

    // ---- clock / mode controls ----
    c.addToggle (T::cRun, 1.30f, 2.16f, 2, ctlTip (m, T::cRun));
    c.addLabel ("RUN", 1.30f, 2.50f, 0.6f, 0.13f);
    c.addKnob (T::cRate, 3.20f, 2.16f, false, false, "RATE", ctlTip (m, T::cRate));
    c.addChoice (T::cLength, 5.10f, 2.16f, 1.0f, ctlTip (m, T::cLength), "LENGTH");
    c.addToggle (T::cRandom, 7.00f, 2.16f, 2, ctlTip (m, T::cRandom));
    c.addLabel ("RANDOM", 7.00f, 2.50f, 0.9f, 0.13f);
    c.addKnob (T::cGateLen, 8.90f, 2.16f, false, false, "GATE", ctlTip (m, T::cGateLen));
    c.addKnob (T::cRange, 10.80f, 2.16f, false, false, "RANGE", ctlTip (m, T::cRange));
    c.addKnob (T::cGlide, 12.70f, 2.16f, false, false, "GLIDE", ctlTip (m, T::cGlide));

    // ---- 16-column field ----
    const float gx0 = 1.05f, gx1 = w - 0.15f;
    const float pitch = (gx1 - gx0) / 16.0f;
    auto colX = [&] (int s) { return gx0 + pitch * ((float) s + 0.5f); };
    // stage-pulse jacks with numbers (gold panel: red jack row 1..16)
    for (int s = 0; s < 16; ++s)
    {
        c.addJack (T::jStage1 + s, colX (s), 3.10f, {}, true, jackTip (m, T::jStage1 + s));
        c.addLabel (juce::String (s + 1), colX (s), 3.30f, 0.4f, 0.125f);
    }
    c.addLabel ("STAGE PULSES", 0.52f, 2.98f, 0.72f, 0.105f);
    // knob rows with letters + row LEDs
    const char rowNames[4] = { 'A', 'B', 'C', 'D' };
    for (int r = 0; r < 4; ++r)
    {
        const float y = 3.86f + (float) r * 0.60f;
        c.addLabel (juce::String::charToString (rowNames[r]), 0.42f, y - 0.09f, 0.3f, 0.17f);
        c.addLed (16 + r, 0.76f, y,
                  "Row " + juce::String::charToString (rowNames[r])
                  + " LED: lit while this row drives the ABCD output.", 0.09f);
        for (int s = 0; s < 16; ++s)
        {
            const int ctl = T::cRowA1 + r * 16 + s;
            c.addKnob (ctl, colX (s), y, false, false, {}, ctlTip (m, ctl));
        }
    }
    // touch keyboard: black strip + gold pads (pressure = click height)
    c.addFill (0.24f, 6.00f, w - 0.48f, 0.92f, juce::Colour (0xff17150f));
    for (int s = 0; s < 16; ++s)
        c.addPad (T::cPad1 + s, s, colX (s), 6.46f, juce::jmin (0.70f, pitch * 0.88f), 0.80f,
                  T::cPressure, ctlTip (m, T::cPad1 + s));
}

inline void buildSkeleton (ModuleComp& c)
{
    const auto& id = c.mod.id;
    if (id == "nto")   { buildNTO (c);   return; }
    if (id == "vcm")   { buildVCM (c);   return; }
    if (id == "ring")  { buildRING (c);  return; }
    if (id == "vcfq")  { buildVCFQ (c);  return; }
    if (id == "noise") { buildNOISE (c); return; }
    if (id == "rvg")   { buildRVG (c);   return; }
    if (id == "dusg1" || id == "dusg2") { buildDUSG (c); return; }
    if (id == "ssg")   { buildSSG (c);   return; }
    if (id == "key")   { buildKEY (c);   return; }
    if (id == "vca1" || id == "vca2")   { buildVCA (c); return; }
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
            if (id == "rvg")   return 1.7f;
            if (id == "dusg1" || id == "dusg2") return 2.0f;
            if (id == "ssg")   return 3.2f;
            if (id == "key")   return 1.5f;
            if (id == "vca1" || id == "vca2")   return 1.3f;
            if (id == "out")   return 2.7f;
            return 2.0f;
        };

        const std::vector<std::vector<satk::Module*>> rows = {
            { proc.sys.nto, proc.sys.vcm, proc.sys.ring, proc.sys.vcfq, proc.sys.noise, proc.sys.rvg },
            { proc.sys.dusg1, proc.sys.dusg2, proc.sys.ssg, proc.sys.key, proc.sys.vca1, proc.sys.vca2, proc.sys.out },
        };
        rowNames = { "ANIMAL - VOICE", "ANIMAL - CONTROL & OUTPUT",
                     "TOUCH ACTIVATED KEYBOARD SEQUENCER" };

        float maxRowIn = 0;
        for (auto& row : rows)
        {
            float w = 2 * RAIL_IN;
            for (auto* m : row) w += widthOf (m);
            maxRowIn = std::max (maxRowIn, w);
        }
        logicalW = (int) std::ceil (maxRowIn * PX) + 20;
        int y = HEADER_H + 14;
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
        logicalH = y;
        setSize (logicalW, logicalH);

        // ---- header widgets: zoom control + clear ----
        for (int pct : { 50, 75, 100, 125, 150, 200 })
            zoomBox.addItem (juce::String (pct) + " %", pct);
        zoomBox.setTextWhenNothingSelected ("ZOOM");
        zoomBox.setTooltip ("Zoom: sets the editor to a fixed scale step (50-200%). "
                            "Dragging the window corner resizes continuously; both write the same saved scale.");
        // dark-header variants of the house widget style
        zoomBox.setColour (juce::ComboBox::backgroundColourId, col::headerBg.brighter (0.08f));
        zoomBox.setColour (juce::ComboBox::textColourId,       col::ioInk);
        zoomBox.setColour (juce::ComboBox::outlineColourId,    col::ioInk.withAlpha (0.55f));
        zoomBox.setColour (juce::ComboBox::arrowColourId,      col::ioInk);
        zoomBox.onChange = [this]
        {
            const int id = zoomBox.getSelectedId();
            if (id > 0 && onZoom) onZoom ((float) id / 100.0f);
        };
        addAndMakeVisible (zoomBox);
        zoomBox.setBounds (logicalW - 236, 4, 112, HEADER_H - 8);

        clearBtn.setButtonText ("CLEAR");
        clearBtn.setTooltip ("Remove every patch cable from the panel (knob settings are kept).");
        clearBtn.setColour (juce::TextButton::buttonColourId,  col::headerBg.brighter (0.08f));
        clearBtn.setColour (juce::TextButton::textColourOffId, col::ioInk);
        clearBtn.onClick = [this] { proc.clearAllCables(); };
        addAndMakeVisible (clearBtn);
        clearBtn.setBounds (logicalW - 112, 4, 92, HEADER_H - 8);

        for (auto* mc : mods)
            for (auto* j : mc->jacks)
                allJacks.push_back (j);

        startTimerHz (30);
        setOpaque (true);
    }

    // reflect the current scale in the zoom control (called by the editor)
    void setZoomDisplay (float scale)
    {
        const int pct = juce::roundToInt (scale * 100.0f);
        bool onStep = false;
        for (int step : { 50, 75, 100, 125, 150, 200 })
            if (std::abs (pct - step) <= 1) { zoomBox.setSelectedId (step, juce::dontSendNotification); onStep = true; }
        if (! onStep)
        {
            zoomBox.setSelectedId (0, juce::dontSendNotification);
            zoomBox.setTextWhenNothingSelected ("ZOOM " + juce::String (pct) + "%");
        }
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (col::headerBg);
        g.setColour (col::ioInk);
        g.setFont (panelFont (15.0f, true));
        g.drawText ("SERGE  ANIMAL  TKB", 14, 0, 500, HEADER_H, juce::Justification::centredLeft);
        g.setColour (col::ioInk.withAlpha (0.55f));
        g.setFont (panelFont (11.0f, false));
        g.drawText ("RENKY DENK  -  SOFTWARE RECREATION", 220, 0, 420, HEADER_H, juce::Justification::centredLeft);
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

    int logicalW = 980, logicalH = 1200;
    std::function<void (float)> onZoom;   // set by the editor (writes the shared scale)
    juce::ComboBox zoomBox;               // the visible ZOOM control (U02)
    juce::TextButton clearBtn;

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
};

// --------------------------------------------------------------------------
// Editor. The persisted editor scale (SatkProcessor::get/setEditorScale) is
// the single source of truth: the ZOOM control's discrete steps and corner
// drag-resize both funnel through resized() -> setEditorScale (U01/U02).
class SatkEditor : public juce::AudioProcessorEditor
{
public:
    static constexpr float kMinScale = 0.5f, kMaxScale = 2.4f;

    explicit SatkEditor (SatkProcessor& p) : AudioProcessorEditor (p), proc (p), view (p)
    {
        // capture the persisted scale BEFORE installing size limits:
        // setResizeLimits() constrains the initial 0x0 bounds up to the
        // minimum, which fires resized() and would overwrite the restored
        // value with kMinScale.
        const float s = juce::jlimit (kMinScale, kMaxScale, proc.getEditorScale());

        // house style for the remaining stock JUCE widgets (menus, CLEAR,
        // popups, tooltip window) — children inherit the editor's LookAndFeel
        setLookAndFeel (&lnf);

        addAndMakeVisible (view);
        tips = std::make_unique<juce::TooltipWindow> (this, 400); // shared TooltipWindow (U03)

        const float aspect = (float) view.logicalW / (float) view.logicalH;
        setResizable (true, true);
        setResizeLimits ((int) std::lround ((float) view.logicalW * kMinScale),
                         (int) std::lround ((float) view.logicalH * kMinScale),
                         (int) std::lround ((float) view.logicalW * kMaxScale),
                         (int) std::lround ((float) view.logicalH * kMaxScale));
        getConstrainer()->setFixedAspectRatio (aspect);

        view.onZoom = [this] (float f) { setScaleStep (f); };

        setSize ((int) std::lround ((float) view.logicalW * s),
                 (int) std::lround ((float) view.logicalH * s));
    }

    ~SatkEditor() override { setLookAndFeel (nullptr); }

    // discrete zoom entry point (also used by the UX tests)
    void setScaleStep (float f)
    {
        f = juce::jlimit (kMinScale, kMaxScale, f);
        setSize ((int) std::lround ((float) view.logicalW * f),
                 (int) std::lround ((float) view.logicalH * f));
    }

    void resized() override
    {
        const float s = juce::jmin ((float) getWidth() / (float) view.logicalW,
                                    (float) getHeight() / (float) view.logicalH);
        view.setTransform (juce::AffineTransform::scale (s));
        view.setTopLeftPosition (0, 0);
        proc.setEditorScale (s);        // drag-resize and zoom share this value
        view.setZoomDisplay (s);
    }
    void paint (juce::Graphics& g) override { g.fillAll (satkui::col::headerBg); }

    juce::TooltipWindow* tooltipWindow() const { return tips.get(); }

    SatkProcessor& proc;
    PanelView view;

private:
    SatkLookAndFeel lnf;
    std::unique_ptr<juce::TooltipWindow> tips;
};

} // namespace satkui

using SatkEditor = satkui::SatkEditor;
