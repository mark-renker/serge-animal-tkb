// Serge Animal TKB v2 — main panel view (two Animal rows + full-width TKB
// strip), banana-cable interaction layer, zoom control, shared TooltipWindow,
// and the plugin editor: proportional window whose corner drag zooms the panel
// (0.5x-2.4x); the zoom persists (U01/U02).
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
constexpr float TKB_H_IN = 6.40f;    // TKB strip (still much wider than tall)
constexpr float IO_H_IN  = 1.0f;    // dark KEY/MIDI I/O strip
// row-module faces are authored on a 7-inch grid (content 0.62..6.86 in) and
// packed into MOD_H (content 0.62..MOD_H-0.14) by the vertical pack factor
constexpr float kRowPack = (MOD_H - 0.14f - 0.62f) / (6.86f - 0.62f);

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
    b->setBounds (pxr (sq (tx (x), ty (y), r)));
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
    p->setBounds (pxr ({ tx (x) - wInches / 2, ty (y) - hInches / 2, wInches, hInches }));
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
    c.addLed (0, 0.94f, 0.98f, "Oscillator LED: pulses at the oscillator rate (visible at LFO speeds).", 0.10f);
    // pitch section (1V/OCT pair, VC FREQ + attenuverter)
    c.addBox (0.14f, 2.52f, 1.60f, 1.92f);
    c.addLabel ("A", 0.30f, 2.86f, 0.2f, 0.14f);
    c.addLabel ("B", 0.98f, 2.86f, 0.2f, 0.14f);
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
    // sync
    c.addBox (0.14f, 6.06f, 1.60f, 0.80f);
    c.addJack (M::jSync, 0.94f, 6.30f, "SYNC", true, jackTip (m, M::jSync), 0.7f);
    // frequency section: FINE above the big FREQ knob
    c.addBox (1.88f, 4.58f, 1.38f, 2.28f);
    c.addKnob (M::cFine, 2.57f, 4.93f, false, false, "FINE", ctlTip (m, M::cFine));
    c.addKnob (M::cFreq, 2.57f, 6.08f, true, false, {}, ctlTip (m, M::cFreq));
    c.addLabel ("FREQ", 2.57f, 6.53f, 0.8f, 0.14f, true);
}

inline void buildVCM (ModuleComp& c)
{
    using M = satk::VCM;
    auto& m = c.mod;
    // TOP section
    c.addBox (0.14f, 0.66f, 2.72f, 1.86f);
    c.addLabel ("TOP", 1.50f, 0.725f, 0.8f, 0.14f);
    c.addLed (0, 2.74f, 2.96f, "Wave-multiplier LED: shows the middle (folder) output level.", 0.09f);
    c.addJack (M::jTopIn,  0.55f, 1.22f, "IN",  true, jackTip (m, M::jTopIn), 0.6f);
    c.addJack (M::jVcTop,  1.50f, 1.22f, "VC",  true, jackTip (m, M::jVcTop), 0.6f);
    c.addJack (M::jTopOut, 2.45f, 1.22f, "OUT", true, jackTip (m, M::jTopOut), 0.6f);
    c.addToggle (M::cTopMode, 0.85f, 2.06f, 2, ctlTip (m, M::cTopMode));
    c.addLabel ("LO", 1.10f, 1.80f, 0.35f, 0.14f);
    c.addLabel ("HI", 1.10f, 2.16f, 0.35f, 0.14f);
    c.addKnob (M::cTopDrive, 2.05f, 1.96f, false, false, "DRIVE", ctlTip (m, M::cTopDrive));
    // MIDDLE section (the classic folder)
    c.addBox (0.14f, 2.66f, 2.72f, 2.24f);
    c.addLabel ("MIDDLE", 1.50f, 2.725f, 1.0f, 0.14f);
    c.addJack (M::jMidIn1, 0.55f, 3.18f, "IN 1", true, jackTip (m, M::jMidIn1), 0.6f);
    c.addJack (M::jMidIn2, 1.50f, 3.18f, "IN 2", true, jackTip (m, M::jMidIn2), 0.6f);
    c.addJack (M::jMidOut, 2.45f, 3.18f, "OUT",  true, jackTip (m, M::jMidOut), 0.6f);
    c.addJack (M::jVcFold, 0.55f, 4.22f, "VC FOLD", true, jackTip (m, M::jVcFold), 0.8f);
    c.addKnob (M::cFold, 1.50f, 4.20f, true, false, {}, ctlTip (m, M::cFold));
    c.addLabel ("FOLD", 1.50f, 4.62f, 0.7f, 0.14f, true);
    c.addKnob (M::cBias, 2.45f, 4.18f, false, true, {}, ctlTip (m, M::cBias));
    c.addLabel ("BIAS", 2.45f, 4.64f, 0.7f, 0.14f);
    // BOTTOM section (rectifier cascade)
    c.addBox (0.14f, 5.04f, 2.72f, 1.86f);
    c.addLabel ("BOTTOM", 1.50f, 5.105f, 1.0f, 0.14f);
    c.addJack (M::jBotIn1, 0.55f, 5.50f, "IN 1", true, jackTip (m, M::jBotIn1), 0.6f);
    c.addJack (M::jBotIn2, 1.50f, 5.50f, "IN 2", true, jackTip (m, M::jBotIn2), 0.6f);
    c.addJack (M::jBotOut, 2.45f, 5.50f, "OUT",  true, jackTip (m, M::jBotOut), 0.6f);
    c.addJack (M::jVcBot, 0.55f, 6.36f, "VC", true, jackTip (m, M::jVcBot), 0.6f);
    c.addKnob (M::cBotDepth, 1.50f, 6.26f, false, false, "DEPTH", ctlTip (m, M::cBotDepth));
    c.addGlyph (Glyph::PulseUp, 2.14f, 6.12f, 0.28f, 0.15f);
    c.addJack (M::jBotSq, 2.45f, 6.36f, "SQ OUT", true, jackTip (m, M::jBotSq), 0.72f);
}

inline void buildRING (ModuleComp& c)
{
    using M = satk::RING;
    auto& m = c.mod;
    c.addBox (0.14f, 0.66f, 1.42f, 1.16f);
    c.addLabel ("INPUTS", 0.85f, 0.72f, 1.0f, 0.14f);
    c.addJack (M::jX, 0.50f, 1.22f, "X IN", true, jackTip (m, M::jX), 0.62f);
    c.addJack (M::jY, 1.20f, 1.22f, "Y IN", true, jackTip (m, M::jY), 0.62f);
    c.addBox (0.14f, 1.96f, 1.42f, 1.30f);
    c.addLabel ("MORPH", 0.85f, 2.02f, 1.0f, 0.14f);
    c.addDiamondJack (M::jVcMorph, 0.50f, 2.62f, "VC", jackTip (m, M::jVcMorph));
    c.addKnob (M::cMorph, 1.20f, 2.62f, false, false, {}, ctlTip (m, M::cMorph));
    c.addBox (0.14f, 3.40f, 1.42f, 1.20f);
    c.addKnob (M::cGain, 0.85f, 4.00f, false, false, "GAIN", ctlTip (m, M::cGain));
    // bleed trims (a boxed "feature" pair, per the R*S rejection-trim note)
    c.addBox (0.14f, 4.74f, 1.42f, 1.18f);
    c.addLabel ("BLEED", 0.85f, 4.80f, 0.8f, 0.14f);
    c.addKnob (M::cTrimX, 0.50f, 5.28f, false, false, "X", ctlTip (m, M::cTrimX));
    c.addKnob (M::cTrimY, 1.20f, 5.28f, false, false, "Y", ctlTip (m, M::cTrimY));
    c.addBox (0.14f, 6.06f, 1.42f, 0.80f);
    c.addJack (M::jOut, 0.85f, 6.34f, "OUT", true, jackTip (m, M::jOut), 0.7f);
    c.addLed (0, 1.20f, 6.34f, "Ring-modulator LED: shows the output level.", 0.09f);
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
    c.addLed (0, 1.06f, 0.98f, "Filter LED: shows the band-pass output level.", 0.10f);
    // inputs
    c.addDiamondJack (M::jIn, 0.75f, 2.86f, "IN", jackTip (m, M::jIn));
    c.addJack (M::jAgc, 2.15f, 2.86f, "AGC IN", true, jackTip (m, M::jAgc), 0.8f);
    c.addKnob (M::cGain, 0.60f, 3.88f, false, false, "GAIN", ctlTip (m, M::cGain));
    c.addKnob (M::cPing, 1.45f, 3.88f, false, false, "PING", ctlTip (m, M::cPing));
    c.addJack (M::jTrig, 2.30f, 3.88f, "TRIG IN", true, jackTip (m, M::jTrig), 0.8f);
    // CV row: VC Q jack -> attenuverter | 1V/OCT | attenuverter <- VC F jack
    c.addJack (M::jVcQ, 0.38f, 4.80f, "VC Q", true, jackTip (m, M::jVcQ), 0.62f);
    c.addLine (0.56f, 4.80f, 0.68f, 4.80f);
    c.addKnob (M::cVcQAmt, 0.94f, 4.80f, false, false, {}, ctlTip (m, M::cVcQAmt));
    c.addLabel (juce::CharPointer_UTF8 ("\xc2\xb1"), 0.94f, 5.10f, 0.3f, 0.15f); // attenuverter
    c.addJack (M::jOct, 1.45f, 4.80f, "1V/OCT", true, jackTip (m, M::jOct), 0.62f);
    c.addKnob (M::cVcFAmt, 1.96f, 4.80f, false, false, {}, ctlTip (m, M::cVcFAmt));
    c.addLabel (juce::CharPointer_UTF8 ("\xc2\xb1"), 1.96f, 5.10f, 0.3f, 0.15f);
    c.addLine (2.22f, 4.80f, 2.34f, 4.80f);
    c.addJack (M::jVcF, 2.52f, 4.80f, "VC F", true, jackTip (m, M::jVcF), 0.62f);
    // big Q / FREQ
    c.addKnob (M::cQ, 0.85f, 6.02f, true, false, {}, ctlTip (m, M::cQ));
    c.addLabel ("Q", 0.85f, 6.44f, 0.5f, 0.14f, true);
    c.addKnob (M::cFreq, 2.05f, 6.02f, true, false, {}, ctlTip (m, M::cFreq));
    c.addLabel ("FREQ", 2.05f, 6.44f, 0.8f, 0.14f, true);
}

inline void buildNoiseRvg (ModuleComp& c, satk::Module& noise)
{
    c.titleText = "NOISE & RANDOM";
    {
        using N = satk::NOISE;
        c.use (&noise);
        c.addBox (0.14f, 0.66f, 1.82f, 1.86f);
        c.addLabel ("NOISE", 1.05f, 0.72f, 1.0f, 0.14f);
        c.addJack (N::jWhite, 0.55f, 1.20f, "WHITE", true, jackTip (noise, N::jWhite), 0.8f);
        c.addJack (N::jPink,  1.55f, 1.20f, "PINK",  true, jackTip (noise, N::jPink), 0.8f);
        c.addJack (N::jShSrc, 0.55f, 2.02f, "S/H SRC", true, jackTip (noise, N::jShSrc), 0.9f);
        c.use (nullptr);
    }
    using M = satk::RVG;
    auto& m = c.mod;
    c.addBox (0.14f, 2.66f, 1.82f, 1.90f);
    c.addLabel ("RANDOM", 1.05f, 2.72f, 1.0f, 0.14f);
    c.addLed (0, 0.86f, 3.80f, "Random-clock LED: flashes on each internal clock pulse.", 0.09f);
    c.addJack (M::jStepped, 0.55f, 3.18f, "STEPPED", true, jackTip (m, M::jStepped), 0.85f);
    c.addJack (M::jSmooth,  1.55f, 3.18f, "SMOOTH",  true, jackTip (m, M::jSmooth), 0.85f);
    c.addJack (M::jPulse,   0.55f, 4.02f, "PULSE",   true, jackTip (m, M::jPulse), 0.7f);
    c.addJack (M::jTrig,    1.55f, 4.02f, "TRIG",    true, jackTip (m, M::jTrig), 0.7f);
    c.addBox (0.14f, 4.70f, 1.82f, 2.16f);
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
// Dark I/O strip along the bottom (PF50 house layout): MIDI keyboard I/O,
// plus the TKB's software-only clock/mode controls (no hardware equivalent,
// so they stay off the hardware-faithful TKB panel).
inline void buildKeyStrip (ModuleComp& c, satk::Module& tkb)
{
    using M = satk::KEY;
    auto& m = c.mod;
    c.darkFace = true;
    c.addLabel ("MIDI KEYBOARD", 1.95f, 0.03f, 2.0f, 0.14f);
    c.addJack (M::jKv,   0.55f, 0.46f, "1V/OCT", true, jackTip (m, M::jKv), 0.8f);
    c.addJack (M::jGate, 1.25f, 0.46f, "GATE",   true, jackTip (m, M::jGate), 0.7f);
    c.addLed (0, 1.50f, 0.30f, "Gate LED: lit while a MIDI note is held.", 0.07f);
    c.addJack (M::jVel,  1.95f, 0.46f, "VEL",    true, jackTip (m, M::jVel), 0.7f);
    c.addJack (M::jClk,  2.65f, 0.46f, "CLK",    true, jackTip (m, M::jClk), 0.7f);
    c.addKnob (M::cGlide, 3.35f, 0.42f, false, false, "PORTA", ctlTip (m, M::cGlide), 0.21f);
    c.addLine (3.85f, 0.10f, 3.85f, 0.90f);

    using T = satk::TKB;
    c.use (&tkb);
    const float sx[7] = { 4.35f, 5.10f, 5.85f, 6.60f, 7.35f, 8.10f, 9.00f };
    c.addLabel ("SEQUENCER CLOCK / MODE", (sx[0] + sx[6]) * 0.5f, 0.03f, 4.0f, 0.14f);
    c.addToggle (T::cRun,    sx[0], 0.42f, 2, ctlTip (tkb, T::cRun));
    c.addLabel ("RUN", sx[0], 0.67f, 0.6f, 0.14f);
    c.addToggle (T::cRandom, sx[1], 0.42f, 2, ctlTip (tkb, T::cRandom));
    c.addLabel ("RANDOM", sx[1], 0.67f, 0.7f, 0.14f);
    c.addKnob (T::cRate,    sx[2], 0.42f, false, false, "RATE",  ctlTip (tkb, T::cRate), 0.21f);
    c.addKnob (T::cGateLen, sx[3], 0.42f, false, false, "GATE",  ctlTip (tkb, T::cGateLen), 0.21f);
    c.addKnob (T::cRange,   sx[4], 0.42f, false, false, "RANGE", ctlTip (tkb, T::cRange), 0.21f);
    c.addKnob (T::cGlide,   sx[5], 0.42f, false, false, "GLIDE", ctlTip (tkb, T::cGlide), 0.21f);
    c.addChoice (T::cLength, sx[6], 0.42f, 0.85f, ctlTip (tkb, T::cLength), {});
    c.addLabel ("LENGTH", sx[6], 0.67f, 0.9f, 0.14f);
    c.use (nullptr);
    c.addLine (9.75f, 0.10f, 9.75f, 0.90f);

    c.addLabel ("SERGE ANIMAL TKB", c.wIn - 1.55f, 0.30f, 2.6f, 0.15f);
    c.addLabel ("SOFTWARE RECREATION  -  RENKY DENK", c.wIn - 1.55f, 0.52f, 2.6f, 0.14f);
}

inline void buildDUSG (ModuleComp& c)
{
    using M = satk::DUSG;
    auto& m = c.mod;
    c.addBox (0.14f, 0.66f, 1.72f, 2.32f);
    c.addJack (M::jOut, 0.55f, 1.22f, "OUT", true, jackTip (m, M::jOut), 0.6f);
    c.addLed (0, 0.86f, 0.98f, "Slope LED: follows the 0-5 V slope output.", 0.09f);
    c.addGlyph (Glyph::ArcAR, 1.45f, 0.94f, 0.32f, 0.16f);
    c.addJack (M::jEnd, 1.45f, 1.22f, "END", true, jackTip (m, M::jEnd), 0.6f);
    c.addJack (M::jTrig, 0.55f, 2.20f, "TRIG", true, jackTip (m, M::jTrig), 0.6f);
    c.addDiamondJack (M::jIn, 1.45f, 2.20f, "IN", jackTip (m, M::jIn));
    // rate section (rise / fall / cycle)
    c.addBox (0.14f, 3.10f, 1.72f, 3.76f);
    c.addJack (M::jVcRise, 0.55f, 3.56f, "VC RISE", true, jackTip (m, M::jVcRise), 0.8f);
    c.addJack (M::jVcFall, 1.45f, 3.56f, "VC FALL", true, jackTip (m, M::jVcFall), 0.8f);
    c.addLine (0.55f, 4.02f, 0.55f, 4.22f);
    c.addLine (1.45f, 4.02f, 1.45f, 4.22f);
    c.addGlyph (Glyph::RampUp,   0.55f, 4.38f, 0.3f, 0.16f);
    c.addGlyph (Glyph::RampDown, 1.45f, 4.38f, 0.3f, 0.16f);
    c.addKnob (M::cRise, 0.55f, 4.98f, false, false, "RISE", ctlTip (m, M::cRise));
    c.addKnob (M::cFall, 1.45f, 4.98f, false, false, "FALL", ctlTip (m, M::cFall));
    c.addToggle (M::cCycle, 1.00f, 6.02f, 2, ctlTip (m, M::cCycle));
    c.addLabel ("CYCLE", 1.00f, 6.40f, 0.8f, 0.14f);
}

inline void buildSSG (ModuleComp& c)
{
    using M = satk::SSG;
    auto& m = c.mod;
    // smooth section
    c.addBox (0.14f, 0.66f, 2.92f, 2.30f);
    c.addLabel ("SMOOTH", 1.60f, 0.72f, 1.0f, 0.14f);
    c.addDiamondJack (M::jSmIn, 0.60f, 1.32f, "IN", jackTip (m, M::jSmIn));
    c.addJack (M::jSmVcRate, 1.60f, 1.32f, "VC RATE", true, jackTip (m, M::jSmVcRate), 0.8f);
    c.addJack (M::jSmOut,    2.60f, 1.32f, "OUT", true, jackTip (m, M::jSmOut), 0.6f);
    c.addLed (0, 2.90f, 1.08f, "Smooth LED: follows the smooth-section output voltage.", 0.09f);
    c.addJack (M::jHold,  0.60f, 2.30f, "HOLD", true, jackTip (m, M::jHold), 0.6f);
    c.addJack (M::jCycle, 1.60f, 2.30f, "CYCLE", true, jackTip (m, M::jCycle), 0.66f);
    c.addKnob (M::cSmRate, 2.60f, 2.22f, false, false, "RATE", ctlTip (m, M::cSmRate));
    // stepped section
    c.addBox (0.14f, 3.06f, 2.92f, 2.12f);
    c.addLabel ("STEPPED", 1.60f, 3.12f, 1.0f, 0.14f);
    c.addDiamondJack (M::jStIn, 0.60f, 3.72f, "IN", jackTip (m, M::jStIn));
    c.addJack (M::jSample, 1.60f, 3.72f, "SAMPLE", true, jackTip (m, M::jSample), 0.76f);
    c.addJack (M::jStOut,  2.60f, 3.72f, "OUT", true, jackTip (m, M::jStOut), 0.6f);
    c.addLed (1, 2.90f, 3.48f, "Stepped LED: follows the stepped-section output voltage.", 0.09f);
    c.addJack (M::jStVcRate, 0.60f, 4.66f, "VC RATE", true, jackTip (m, M::jStVcRate), 0.8f);
    c.addKnob (M::cStRate, 2.60f, 4.56f, false, false, "RATE", ctlTip (m, M::cStRate));
    // coupler
    c.addBox (0.14f, 5.30f, 2.92f, 1.56f);
    c.addLabel ("COUPLER", 1.60f, 5.36f, 1.1f, 0.14f);
    c.addJack (M::jCoupler,    1.10f, 5.92f, "OUT", true, jackTip (m, M::jCoupler), 0.6f);
    c.addJack (M::jHotCoupler, 2.10f, 5.92f, "HOT", true, jackTip (m, M::jHotCoupler), 0.6f);
    c.addLine (1.30f, 5.92f, 1.90f, 5.92f);
}

inline void buildOUT (ModuleComp& c)
{
    using M = satk::OUT;
    auto& m = c.mod;
    c.addBox (0.14f, 0.66f, 2.42f, 2.40f);
    c.addLabel ("INPUTS", 1.35f, 0.72f, 1.0f, 0.14f);
    c.addJack (M::jIn1, 0.70f, 1.22f, "IN 1", true, jackTip (m, M::jIn1), 0.6f);
    c.addJack (M::jIn2, 2.00f, 1.22f, "IN 2", true, jackTip (m, M::jIn2), 0.6f);
    c.addLine (0.70f, 1.58f, 0.70f, 1.80f);
    c.addLine (2.00f, 1.58f, 2.00f, 1.80f);
    c.addKnob (M::cPan1, 0.70f, 2.18f, false, true, "PAN 1", ctlTip (m, M::cPan1));
    c.addKnob (M::cPan2, 2.00f, 2.18f, false, true, "PAN 2", ctlTip (m, M::cPan2));
    // monitor bus
    c.addBox (0.14f, 3.20f, 2.42f, 1.96f);
    c.addLabel ("MONITOR", 1.35f, 3.26f, 1.1f, 0.14f);
    c.addSelector (M::cMonSrc, 1.35f, 3.80f, 2.15f, ctlTip (m, M::cMonSrc));
    c.addKnob (M::cMonLvl, 1.35f, 4.50f, false, false, "LEVEL", ctlTip (m, M::cMonLvl));
    // drift + master
    c.addBox (0.14f, 5.30f, 2.42f, 1.56f);
    c.addKnob (M::cMaster, 0.85f, 6.00f, true, false, {}, ctlTip (m, M::cMaster));
    c.addLabel ("MASTER", 0.85f, 6.38f, 1.0f, 0.14f, true);
    c.addLed (0, 1.40f, 5.62f, "Output LED: shows the final stereo output level.", 0.10f);
    c.addKnob (M::cDrift, 2.00f, 5.90f, false, false, "DRIFT", ctlTip (m, M::cDrift));
}

// TKB strip, modeled on the R*S TKB panel: I/O row on top (outputs boxed at
// left, transport inputs boxed at right), clock/mode controls, 16 stage-pulse
// jacks over the 4x16 knob field with row LEDs, and the black touch-keyboard
// strip with 16 gold pads at the bottom.
inline void buildTKB (ModuleComp& c)
{
    // Modeled on the Random*Source TKB panel: ONE full-width 16-column grid
    // shared by the top I/O row (VERT CLOCK first, outputs boxed, CLOCK last),
    // the stage-pulse row, the 4x16 knob field and the touch keyboard. The
    // software-only clock/mode controls live in the dark I/O strip below.
    using T = satk::TKB;
    auto& m = c.mod;
    const float w = c.wIn;
    c.showTitle = false;

    const float gx0 = 0.95f, gx1 = w - 0.18f;
    const float pitch = (gx1 - gx0) / 16.0f;
    auto colX = [&] (int s) { return gx0 + pitch * ((float) s + 0.5f); };

    struct TopJ { int j; const char* label; };
    const TopJ top[16] = {
        { T::jVertClock, "VERT CLOCK" },
        { T::jAbcd, "ABCD" }, { T::jA, "A" }, { T::jB, "B" }, { T::jC, "C" }, { T::jD, "D" },
        { T::jKv, "KEY CV" }, { T::jKp, "KEY PULSE" }, { T::jGate, "KEY GATE" }, { T::jPressure, "PRESSURE" },
        { T::jReset, "RESET" }, { T::jVertReset, "VERT RESET" }, { T::jRandom, "RANDOM" },
        { T::jUpDown, "UP/DOWN" }, { T::jHold, "HOLD" }, { T::jClock, "CLOCK" },
    };
    const float topY = 0.80f;
    c.addBox (colX (1) - pitch * 0.48f, 0.42f, pitch * 8.96f, 0.86f); // outputs ABCD..PRESSURE
    for (int s = 0; s < 16; ++s)
    {
        c.addJack (top[s].j, colX (s), topY, {}, true, jackTip (m, top[s].j));
        const juce::String lab (top[s].label);
        if (lab.startsWith ("VERT "))   // two-word labels stack, as space is one column wide
        {
            c.addLabel ("VERT", colX (s), topY + 0.20f, pitch * 0.98f, 0.14f);
            c.addLabel (lab.fromFirstOccurrenceOf (" ", false, false), colX (s), topY + 0.36f, pitch * 0.98f, 0.14f);
        }
        else
            c.addLabel (lab, colX (s), topY + 0.20f, pitch * 0.98f, 0.14f);
    }

    for (int s = 0; s < 16; ++s)
    {
        c.addJack (T::jStage1 + s, colX (s), 1.62f, {}, true, jackTip (m, T::jStage1 + s));
        c.addLabel (juce::String (s + 1), colX (s), 1.82f, 0.4f, 0.15f);
    }

    c.addLine (gx0 - 0.50f, 2.48f, gx0 - 0.50f, 2.48f + 3.0f * 0.62f); // lamp bus (as on the panel)
    const char rowNames[4] = { 'A', 'B', 'C', 'D' };
    for (int r = 0; r < 4; ++r)
    {
        const float y = 2.48f + (float) r * 0.62f;
        c.addLabel (juce::String::charToString (rowNames[r]), gx0 - 0.20f, y - 0.10f, 0.3f, 0.18f);
        auto* led = c.addLed (16 + r, gx0 - 0.50f, y,
                              "Row " + juce::String::charToString (rowNames[r])
                              + " LED: lit while this row drives the ABCD output.", 0.085f);
        led->solid = true;
        for (int s = 0; s < 16; ++s)
        {
            const int ctl = T::cRowA1 + r * 16 + s;
            c.addKnob (ctl, colX (s), y, false, false, {}, ctlTip (m, ctl), 0.21f);
        }
    }

    // touch keyboard: black strip, pads cut flat by its lower edge
    const float stripY = 4.92f, stripH = 1.12f;
    c.addFill (colX (0) - pitch * 0.5f - 0.04f, stripY, pitch * 16.0f + 0.08f, stripH, juce::Colour (0xff17150f));
    for (int s = 0; s < 16; ++s)
        c.addPad (T::cPad1 + s, s, colX (s), stripY + 0.06f + (stripH - 0.06f) * 0.5f,
                  pitch * 0.94f, stripH - 0.06f,
                  T::cPressure, ctlTip (m, T::cPad1 + s));

    c.addLabel ("TOUCH ACTIVATED KEYBOARD SEQUENCER", w - 2.75f, 6.12f, 5.2f, 0.17f);
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
    if (id == "key")   { buildKeyStrip (c, *c.proc.sys.tkb); return; }
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
            if (id == "vcfq")  return 3.2f; // authored at 2.9 (see authoredWidthOf)
            if (id == "noise") return 1.3f;
            if (id == "rvg")   return 2.1f; // + NOISE
            if (id == "dusg1" || id == "dusg2") return 2.0f;
            if (id == "ssg")   return 3.2f;
            if (id == "key")   return 1.5f;
            if (id == "vca1")  return 2.3f; // + VCA 2
            if (id == "out")   return 2.7f;
            return 2.0f;
        };

        // panels authored narrower than they are shown (layout spreads to fit)
        auto authoredWidthOf = [&] (satk::Module* m) -> float
        {
            if (m->id == "vcfq") return 2.9f;
            return widthOf (m);
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
        // narrower rows spread horizontally so every boat is the same width
        auto rowSpread = [&] (const std::vector<satk::Module*>& row)
        {
            float inner = 0;
            for (auto* m : row) inner += widthOf (m);
            return (maxRowIn - 2 * RAIL_IN) / inner;
        };
        logicalW = (int) std::ceil (maxRowIn * PX) + 20;
        int y = 14; // header is pinned by the editor, outside the scrolling panel
        for (auto& row : rows)
        {
            const float spread = rowSpread (row);
            float rowWIn = 2 * RAIL_IN;
            for (auto* m : row) rowWIn += widthOf (m) * spread;
            int x = (logicalW - (int) (rowWIn * PX)) / 2;
            juce::Rectangle<int> rr (x, y, (int) (rowWIn * PX), (int) (MOD_H * PX));
            rowRects.push_back (rr);
            x += (int) (RAIL_IN * PX);
            for (auto* m : row)
            {
                auto* mc = new ModuleComp (*m, widthOf (m) * spread, hostPtr, proc);
                mc->yK = kRowPack;
                mc->xK = spread * widthOf (m) / authoredWidthOf (m);
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
            g.setColour (col::ioInk.withAlpha (0.5f));
            g.setFont (panelFont (10.0f, false));
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
// Editor: proportional window (amendment 2026-11-03, LOG.md — supersedes the
// free-shape viewport of 2026-11-02 at Mark's request: dragging the corner
// should zoom, never leave blank space).
//   - corner drag scales the whole panel; the window keeps the panel's
//     proportions (height = panel height x zoom + the fixed header);
//   - the ZOOM menu sets the window to a preset scale (50-240 %);
//   - zoom (editorScale) persists in plugin state; the window size follows.
class PanelConstrainer : public juce::ComponentBoundsConstrainer
{
public:
    int logicalW = 1, logicalH = 1, headerH = 0;
    float minZ = 0.5f, maxZ = 2.4f;
    int heightFor (int w) const { return (int) std::lround ((float) w * (float) logicalH / (float) logicalW) + headerH; }
    void checkBounds (juce::Rectangle<int>& bounds, const juce::Rectangle<int>& previous,
                      const juce::Rectangle<int>& /*limits*/, bool isStretchingTop, bool isStretchingLeft,
                      bool isStretchingBottom, bool isStretchingRight) override
    {
        // width drives the zoom; a purely vertical drag converts to a width
        const bool vertOnly = (isStretchingTop || isStretchingBottom) && ! (isStretchingLeft || isStretchingRight);
        int w = vertOnly ? (int) std::lround ((float) (bounds.getHeight() - headerH) * (float) logicalW / (float) logicalH)
                         : bounds.getWidth();
        w = juce::jlimit ((int) std::lround ((float) logicalW * minZ), (int) std::lround ((float) logicalW * maxZ), w);
        const int h = heightFor (w);
        // keep the edge opposite the one being dragged fixed
        const int x = isStretchingLeft ? previous.getRight() - w : bounds.getX();
        const int y = isStretchingTop  ? previous.getBottom() - h : bounds.getY();
        bounds = { x, y, w, h };
    }
};

class SatkEditor : public juce::AudioProcessorEditor
{
public:
    static constexpr float kMinScale = 0.5f, kMaxScale = 2.4f; // zoom range

    explicit SatkEditor (SatkProcessor& p) : AudioProcessorEditor (p), proc (p), view (p)
    {
        // capture the persisted zoom BEFORE installing the constrainer (setting
        // limits fires resized() on the initial 0x0 bounds)
        float z0 = juce::jlimit (kMinScale, kMaxScale, proc.getEditorScale());
        if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()) // never open taller than the screen
            z0 = juce::jmax (kMinScale, juce::jmin (z0, ((float) d->userArea.getHeight() * 0.92f - (float) HEADER_H)
                                                         / (float) view.logicalH));

        setLookAndFeel (&lnf);
        addAndMakeVisible (view);

        for (int pct : { 50, 75, 100, 125, 150, 200, 240 })
            zoomBox.addItem ("ZOOM  " + juce::String (pct) + " %", pct);
        zoomBox.setTooltip ("Zoom: sets the window to a preset scale (50-240%). "
                            "Dragging the window corner zooms continuously.");
        zoomBox.setColour (juce::ComboBox::backgroundColourId, col::headerBg.brighter (0.08f));
        zoomBox.setColour (juce::ComboBox::textColourId,       col::ioInk);
        zoomBox.setColour (juce::ComboBox::outlineColourId,    col::ioInk.withAlpha (0.55f));
        zoomBox.setColour (juce::ComboBox::arrowColourId,      col::ioInk);
        zoomBox.onChange = [this]
        {
            const int id = zoomBox.getSelectedId();
            if (id > 0) setScaleStep ((float) id / 100.0f);
        };
        addAndMakeVisible (zoomBox);

        clearBtn.setButtonText ("CLEAR");
        clearBtn.setTooltip ("Remove every patch cable from the panel (knob settings are kept).");
        clearBtn.setColour (juce::TextButton::buttonColourId,  col::headerBg.brighter (0.08f));
        clearBtn.setColour (juce::TextButton::textColourOffId, col::ioInk);
        clearBtn.onClick = [this] { proc.clearAllCables(); };
        addAndMakeVisible (clearBtn);

        tips = std::make_unique<juce::TooltipWindow> (this, 400); // shared TooltipWindow (U03)

        constrainer.logicalW = view.logicalW;
        constrainer.logicalH = view.logicalH;
        constrainer.headerH  = HEADER_H;
        constrainer.minZ = kMinScale; constrainer.maxZ = kMaxScale;
        constrainer.setSizeLimits ((int) std::lround ((float) view.logicalW * kMinScale),
                                   constrainer.heightFor ((int) std::lround ((float) view.logicalW * kMinScale)),
                                   (int) std::lround ((float) view.logicalW * kMaxScale),
                                   constrainer.heightFor ((int) std::lround ((float) view.logicalW * kMaxScale)));
        setConstrainer (&constrainer);
        setResizable (true, true);

        setScaleStep (z0);
    }

    ~SatkEditor() override { setLookAndFeel (nullptr); }

    // zoom step: size the window to the panel at scale f
    void setScaleStep (float f)
    {
        const int w = (int) std::lround ((float) view.logicalW * juce::jlimit (kMinScale, kMaxScale, f));
        setSize (w, constrainer.heightFor (w));
    }
    float getZoom() const noexcept { return zoom; }

    void resized() override
    {
        auto r = getLocalBounds();
        auto header = r.removeFromTop (HEADER_H);
        clearBtn.setBounds (header.removeFromRight (104).reduced (6, 4));
        zoomBox.setBounds (header.removeFromRight (150).reduced (6, 4));
        zoom = juce::jmin ((float) r.getWidth() / (float) view.logicalW,
                           (float) r.getHeight() / (float) view.logicalH);
        view.setTransform (juce::AffineTransform::scale (zoom).translated (0.0f, (float) HEADER_H));
        view.setTopLeftPosition (0, 0);
        proc.setEditorScale (zoom);   // corner drag and zoom menu share this value
        updateZoomDisplay();
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
    juce::ComboBox zoomBox;   // the visible ZOOM control (U02)
    juce::TextButton clearBtn;
    PanelConstrainer constrainer;

private:
    void updateZoomDisplay()
    {
        const int pct = juce::roundToInt (zoom * 100.0f);
        for (int step : { 50, 75, 100, 125, 150, 200, 240 })
            if (std::abs (pct - step) <= 1) { zoomBox.setSelectedId (step, juce::dontSendNotification); return; }
        zoomBox.setSelectedId (0, juce::dontSendNotification);
        zoomBox.setTextWhenNothingSelected ("ZOOM  " + juce::String (pct) + " %");
    }

    SatkLookAndFeel lnf;
    float zoom = 1.0f;
    std::unique_ptr<juce::TooltipWindow> tips;
};

} // namespace satkui

using SatkEditor = satkui::SatkEditor;
