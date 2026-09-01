// Serge Animal TKB v2 — TKB (Touch Activated Keyboard Sequencer) and KEY/MIDI.
// FOUNDATION STUBS: jack/ctl registration FROZEN per docs/SPEC.md §4.10-§4.11
// (indices append-only forever). The stage-2 TKB/KEY builder implements
// against gate T12 (+ T15 MIDI contract) using DspUtils only.
#pragma once
#include "EngineCore.h"

namespace satk {

// 16 stages x 4 knob rows + 16 touch pads (engine-visible momentary ctls) +
// pressure. Sequential clock 1..LENGTH with wrap; RESET -> LAST-TOUCHED stage;
// RANDOM armed + pulse -> random stage within LENGTH; vertical clock cycles
// rows A->B->C->D on the ABCD output (64-step sequences); HOLD freezes;
// UP/DOWN reverses. KV = last-touched pad, semitone-calibrated, GLIDE slewed.
class TKB : public Module
{
public:
    enum J { jClock, jVertClock, jReset, jVertReset, jRandom, jUpDown, jHold,
             jA, jB, jC, jD, jAbcd, jKv, jPressure, jGate, jKp,
             jStage1 /* ..jStage1+15 = ST 1..ST 16 (one-hot stage levels) */ };
    enum C { cRun, cRate, cLength, cRandom, cGateLen, cRange, cGlide,
             cRowA1 = 7   /* ..cRowA1+15  */,
             cRowB1 = 23  /* ..cRowB1+15  */,
             cRowC1 = 39  /* ..cRowC1+15  */,
             cRowD1 = 55  /* ..cRowD1+15  */,
             cPad1  = 71  /* ..cPad1+15   */,
             cPressure = 87 };

    TKB()
    {
        addJack ("CLOCK",      JackType::In,  JackColor::Red);
        addJack ("VERT CLOCK", JackType::In,  JackColor::Red);
        addJack ("RESET",      JackType::In,  JackColor::Red);   // -> last-touched stage
        addJack ("VERT RESET", JackType::In,  JackColor::Red);
        addJack ("RANDOM",     JackType::In,  JackColor::Red);   // ORs with RANDOM ctl
        addJack ("UP/DOWN",    JackType::In,  JackColor::Red);   // high = step down
        addJack ("HOLD",       JackType::In,  JackColor::Red);   // high = freeze
        addJack ("A",          JackType::Out, JackColor::White); // active stage, row A (x RANGE)
        addJack ("B",          JackType::Out, JackColor::White);
        addJack ("C",          JackType::Out, JackColor::White);
        addJack ("D",          JackType::Out, JackColor::White);
        addJack ("ABCD",       JackType::Out, JackColor::White); // active row x active stage
        addJack ("KV",         JackType::Out, JackColor::White); // last-touched pad, semitones, GLIDE slew
        addJack ("PRESSURE",   JackType::Out, JackColor::White); // 0..5 V
        addJack ("GATE",       JackType::Out, JackColor::Red);   // GATE LEN gate per step (v1 semantics)
        addJack ("KP",         JackType::Out, JackColor::Red);   // key pulse: high while a pad is touched
        for (int i = 0; i < 16; ++i)
            addJack (kStageNames[i], JackType::Out, JackColor::Red); // one-hot stage levels

        addCtl ("RUN",      0.0f, 2, kOffOn);
        addCtl ("RATE",     0.5252f);           // expo 0.1..30 Hz -> 2 Hz
        addCtl ("LENGTH",   7.0f, 16, kLengths); // index 7 = "8" (v1-ism, logged)
        addCtl ("RANDOM",   0.0f, 2, kOffOn);
        addCtl ("GATE LEN", 0.5654f);           // expo 5 ms..1 s -> 100 ms
        addCtl ("RANGE",    0.3333f);           // linear 0.5..5 oct -> 2 oct (row scaling, v1-ism)
        addCtl ("GLIDE",    0.0f);              // 0..1 s slew on KV
        for (int i = 0; i < 64; ++i) addCtl (kRowNames[i], 0.5f);          // 0..1 -> 0..5 V
        for (int i = 0; i < 16; ++i) addCtl (kPadNames[i], 0.0f, 2, kOffOn); // momentary pads
        addCtl ("PRESSURE", 0.0f);              // 0..1 -> 0..5 V, written by UI / aftertouch
        leds.resize (20); // 16 stage LEDs + 4 row LEDs
    }

    void prepare (double sampleRate) override { Module::prepare (sampleRate); }
    void tick() override
    {
        // STUB: silence. Stage-2 TKB builder implements the full sequencer
        // per SPEC.md §4.10 (RESET -> last-touched, vertical clock, semitone
        // KV distinct from row-A CV, GLIDE slew, one-hot stage outs).
    }

private:
    static constexpr const char* kOffOn[2] = { "OFF", "ON" };
    static constexpr const char* kLengths[16] =
        { "1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16" };
    static constexpr const char* kStageNames[16] =
        { "ST 1","ST 2","ST 3","ST 4","ST 5","ST 6","ST 7","ST 8",
          "ST 9","ST 10","ST 11","ST 12","ST 13","ST 14","ST 15","ST 16" };
    static constexpr const char* kPadNames[16] =
        { "PAD 1","PAD 2","PAD 3","PAD 4","PAD 5","PAD 6","PAD 7","PAD 8",
          "PAD 9","PAD 10","PAD 11","PAD 12","PAD 13","PAD 14","PAD 15","PAD 16" };
    static constexpr const char* kRowNames[64] =
        { "A1","A2","A3","A4","A5","A6","A7","A8","A9","A10","A11","A12","A13","A14","A15","A16",
          "B1","B2","B3","B4","B5","B6","B7","B8","B9","B10","B11","B12","B13","B14","B15","B16",
          "C1","C2","C3","C4","C5","C6","C7","C8","C9","C10","C11","C12","C13","C14","C15","C16",
          "D1","D2","D3","D4","D5","D6","D7","D8","D9","D10","D11","D12","D13","D14","D15","D16" };
};

// KEY: the MIDI bridge (mono, last-note priority — plugin layer writes the
// fields each sample). KV is 1 V/oct with note 60 (C4) = 0 V exactly; VEL
// holds the last note-on velocity (init 4.0 V = v1's default gain 0.8) and
// hard-scales the OUTPUT MIXER's final mix (v1 contract, wired in System).
class KEY : public Module
{
public:
    enum J { jKv, jGate, jVel, jClk };
    enum C { cGlide };

    KEY()
    {
        addJack ("KV",   JackType::Out, JackColor::White); // 1 V/oct, C4 = 0 V (bipolar)
        addJack ("GATE", JackType::Out, JackColor::Red);
        addJack ("VEL",  JackType::Out, JackColor::White); // 0..5 V, holds last velocity
        addJack ("CLK",  JackType::Out, JackColor::Red);   // host-transport 16th pulses

        addCtl ("GLIDE", 0.0f); // 0..0.5 s portamento on KV
        leds.resize (1);
    }

    void prepare (double sampleRate) override { Module::prepare (sampleRate); }
    void tick() override
    {
        // Field pass-through is final; stage-2 builder adds the GLIDE slew.
        jacks[jKv].value   = pitchV;
        jacks[jGate].value = gateV;
        jacks[jVel].value  = velV;
        jacks[jClk].value  = clkV;
        leds[0] = gateV > kLogicHi ? 1.0f : 0.0f;
    }

    // written by the plugin layer (or tests) each sample:
    float pitchV = 0.0f, gateV = 0.0f, velV = 4.0f, clkV = 0.0f;
};

} // namespace satk
