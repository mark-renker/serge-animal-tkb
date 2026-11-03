// Serge Animal TKB v2 — OUTPUT MIXER (with monitor bus).
// FOUNDATION: jack/ctl registration and the 43-entry monitor source table are
// FROZEN per docs/SPEC.md §4.12/§5 (indices append-only forever). The mix path
// below is a working baseline; integration stage may refine the limiter only.
#pragma once
#include "EngineCore.h"

namespace satk {

// Stereo output mixer: two panned inputs (equal-power sqrt law, v1 contract),
// monitor bus that can audition ANY engine source, master level, and the
// KEY velocity hard-scaling the final mix (v1 contract; pointer wired by
// System). Also owns the global ANALOG DRIFT depth ctl (0 = ideal digital).
class OUT : public Module
{
public:
    enum J { jIn1, jIn2 };
    enum C { cPan1, cPan2, cMonSrc, cMonLvl, cMaster, cDrift };

    static constexpr int kNumMonitorSources = 43;
    // FROZEN monitor source list. 0..30 are v1's source indices verbatim
    // (automation/muscle memory); 31+ are v2 additions, append-only.
    static constexpr const char* kMonitorSources[kNumMonitorSources] =
    {
        "OFF",          // 0
        "NTO SINE",     // 1
        "NTO SAW",      // 2
        "NTO VAR",      // 3
        "DUSG1 OUT",    // 4
        "DUSG1 END",    // 5
        "DUSG2 OUT",    // 6
        "DUSG2 END",    // 7
        "SSG SMOOTH",   // 8
        "SSG STEPPED",  // 9
        "SSG COUPLER",  // 10
        "VCFQ LP",      // 11
        "VCFQ BP",      // 12
        "VCFQ HP",      // 13
        "VCM TOP",      // 14
        "VCM MID",      // 15
        "VCM BOT",      // 16
        "RING",         // 17
        "NOISE WHITE",  // 18
        "NOISE PINK",   // 19
        "RVG STEPPED",  // 20
        "RVG SMOOTH",   // 21
        "TKB A",        // 22
        "TKB B",        // 23
        "TKB C",        // 24
        "TKB D",        // 25
        "TKB GATE",     // 26
        "KEY KV",       // 27
        "KEY GATE",     // 28
        "VCA1",         // 29
        "VCA2",         // 30
        "NTO TRI",      // 31
        "NTO PULSE",    // 32
        "SSG CYCLE",    // 33
        "SSG HOT COUPLER", // 34
        "VCFQ NOTCH",   // 35
        "VCM BOT SQ",   // 36
        "NOISE S/H",    // 37
        "RVG PULSE",    // 38
        "TKB ABCD",     // 39
        "TKB KV",       // 40
        "TKB PRESSURE", // 41
        "TKB KP",       // 42
    };

    OUT()
    {
        addJack ("IN 1", JackType::In, JackColor::Black);
        addJack ("IN 2", JackType::In, JackColor::Black);

        addCtl ("PAN 1",         0.35f);
        addCtl ("PAN 2",         0.65f);
        addCtl ("MONITOR",       0.0f, kNumMonitorSources, kMonitorSources);
        addCtl ("MONITOR LEVEL", 0.4f);
        addCtl ("MASTER",        0.53f); // default trimmed 0.8 -> 0.53 (2026-11-03): note-60 rms ~0.07 = v1 target
        addCtl ("ANALOG DRIFT",  0.2f); // global tolerance+drift depth; 0 disables (T02/T08/T17)
        leds.resize (1);
    }

    void prepare (double sampleRate) override
    {
        Module::prepare (sampleRate);
        dcL.prepare (sr); dcR.prepare (sr);
    }

    void tick() override
    {
        const float pan1 = std::clamp (ctls[cPan1].v, 0.0f, 1.0f);
        const float pan2 = std::clamp (ctls[cPan2].v, 0.0f, 1.0f);
        const float in1 = in (jIn1), in2 = in (jIn2);

        float mon = 0.0f;
        const int sel = (int) std::lround (ctls[cMonSrc].v);
        if (sel > 0 && sel < (int) monSrc.size() && monSrc[(size_t) sel] != nullptr)
            mon = *monSrc[(size_t) sel] * ctls[cMonLvl].v;

        const float master = ctls[cMaster].v;
        const float vel = velSrc != nullptr ? std::clamp (*velSrc / kCV, 0.0f, 1.0f) : 1.0f;

        // v1 mix law: equal-power sqrt pan, monitor at 0.7, velocity scales all.
        float mL = (in1 * std::sqrt (1.0f - pan1) + in2 * std::sqrt (1.0f - pan2) + mon * 0.7f) * master * vel;
        float mR = (in1 * std::sqrt (pan1)        + in2 * std::sqrt (pan2)        + mon * 0.7f) * master * vel;

        // DC block + soft limiter, ±1 nominal into the DAW (kAudio V = 1.0).
        outL = dsp::fastTanh (dcL.tick (mL) / kAudio);
        outR = dsp::fastTanh (dcR.tick (mR) / kAudio);
        leds[0] = std::clamp (std::fabs (outL), 0.0f, 1.0f);
    }

    // monitor source pointers, index-aligned with kMonitorSources ([0] unused);
    // velocity source pointer (KEY VEL jack) — both wired by System's ctor.
    std::vector<const float*> monSrc;
    const float* velSrc = nullptr;

    // read by the plugin layer after each tick:
    float outL = 0.0f, outR = 0.0f;

private:
    DCBlock dcL, dcR;
};

} // namespace satk
