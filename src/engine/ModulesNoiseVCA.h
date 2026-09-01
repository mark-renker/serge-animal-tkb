// Serge Animal TKB v2 — NOISE, RVG, VCA (x2).
// FOUNDATION STUBS: jack/ctl registration FROZEN per docs/SPEC.md §4.7-§4.9
// (indices append-only forever). tick() stubbed; the stage-2 noise/VCA builder
// implements against gates T10/T11 using DspUtils only (PCG32 streams).
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

    void prepare (double sampleRate) override { Module::prepare (sampleRate); }
    void tick() override
    {
        // STUB: silence. Stage-2 builder: per-instance PCG32 streams (seeded
        // from moduleSeed; separate stream ids for white / S&H), proper pink filter.
    }
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

    void prepare (double sampleRate) override { Module::prepare (sampleRate); }
    void tick() override
    {
        // STUB: silence. Stage-2 builder: PCG32 stream distinct from NOISE,
        // stepped + linear-slew smooth (T10).
    }
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

    void prepare (double sampleRate) override { Module::prepare (sampleRate); }
    void tick() override
    {
        // STUB: silence. Stage-2 builder: fitted expo law with cutoff knee,
        // click bound per T11, soft saturation at hot levels.
    }

private:
    static constexpr const char* kLinExp[2] = { "LIN", "EXP" };
};

} // namespace satk
