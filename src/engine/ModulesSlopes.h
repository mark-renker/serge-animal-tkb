// Serge Animal TKB v2 — DUSG (x2) and SSG (slew-core family).
// FOUNDATION STUBS: jack/ctl registration FROZEN per docs/SPEC.md §4.2-§4.3
// (indices append-only forever). tick() stubs; the stage-2 slopes builder
// implements the shared slew core against gates T05/T06 using DspUtils only.
#pragma once
#include "EngineCore.h"

namespace satk {

// Universal Slope Generator (one channel of the hardware DUSG). Linear-by-
// design slopes with tanh current-limit soft landing; END/GATE is a LEVEL
// (high during rise and while OUT sits at bottom, low during fall — T05e);
// non-retriggerable during a transient; gate into IN = ASR; CYCLE ctl
// self-patches END->TRIG conceptually.
class DUSG : public Module
{
public:
    enum J { jOut, jEnd, jTrig, jIn, jVcRise, jVcFall };
    enum C { cRise, cFall, cCycle };

    // riseDef/fallDef are normalized positions on the expo 50 µs..10 s map:
    // dusg1: rise 1 ms (0.2454), fall 250 ms (0.6978)
    // dusg2: rise 50 ms (0.5659), fall 50 ms (0.5659)
    DUSG (float riseDef, float fallDef)
    {
        addJack ("OUT",      JackType::Out, JackColor::White); // 0..5 V
        addJack ("END/GATE", JackType::Out, JackColor::Red);   // level, 0/5 V
        addJack ("TRIG",     JackType::In,  JackColor::Red);
        addJack ("IN",       JackType::In,  JackColor::Black); // DC follower / ASR gate
        addJack ("VC RISE",  JackType::In,  JackColor::White); // 1 V/oct, + = faster
        addJack ("VC FALL",  JackType::In,  JackColor::White); // 1 V/oct, + = faster

        addCtl ("RISE",  riseDef); // expo 50 µs..10 s
        addCtl ("FALL",  fallDef); // expo 50 µs..10 s
        addCtl ("CYCLE", 0.0f, 2, kOffOn);
        leds.resize (1);
    }

    void prepare (double sampleRate) override { Module::prepare (sampleRate); }
    void tick() override
    {
        // STUB: silence. Stage-2 slopes builder: linear-drive slew core with
        // tanh-limited approach, BLEP'd END/GATE edges, 2x OS at audio rates.
    }

private:
    static constexpr const char* kOffOn[2] = { "OFF", "ON" };
};

// Smooth & Stepped Generator. Smooth = VC lag with CYCLE pulse out + HOLD
// track-and-hold; Stepped = S&H with VC step-size limiting (static without
// SAMPLE pulses); Coupler comparator 0/5 V plus HOT bipolar out (≥ ±10 V).
class SSG : public Module
{
public:
    enum J { jSmOut, jCycle, jSmIn, jSmVcRate, jHold,
             jStOut, jStIn, jSample, jStVcRate,
             jCoupler, jHotCoupler };
    enum C { cSmRate, cStRate };

    SSG()
    {
        addJack ("SMOOTH OUT",  JackType::Out, JackColor::White); // 0..4-5 V
        addJack ("CYCLE",       JackType::Out, JackColor::Red);   // pulse each cycle; patch to SMOOTH IN to cycle
        addJack ("SMOOTH IN",   JackType::In,  JackColor::Black);
        addJack ("VC RATE",     JackType::In,  JackColor::White); // 1 V/oct, + = faster
        addJack ("HOLD",        JackType::In,  JackColor::Red);   // high = freeze (track & hold)
        addJack ("STEPPED OUT", JackType::Out, JackColor::White);
        addJack ("STEPPED IN",  JackType::In,  JackColor::Black);
        addJack ("SAMPLE",      JackType::In,  JackColor::Red);
        addJack ("ST VC RATE",  JackType::In,  JackColor::White); // VC step-size limit
        addJack ("COUPLER",     JackType::Out, JackColor::Red);   // 0/5 V comparator
        addJack ("HOT COUPLER", JackType::Out, JackColor::Grey);  // swings ≥ ±10 V

        addCtl ("SMOOTH RATE",  0.5659f); // expo 50 µs..10 s -> 50 ms
        addCtl ("STEPPED RATE", 0.2454f); // expo 50 µs..10 s -> 1 ms
        leds.resize (2);
    }

    void prepare (double sampleRate) override { Module::prepare (sampleRate); }
    void tick() override
    {
        // STUB: silence. Stage-2 slopes builder: shared slew core, BLEP'd
        // comparator edges, exact hold, VC step-size limited S&H (T06).
    }
};

} // namespace satk
