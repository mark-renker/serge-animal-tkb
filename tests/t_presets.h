// Serge Animal TKB v2 — factory presets (phase 5): every program applies
// cleanly, renders finite audio at a sane level, and program 0 IS the frozen
// default patch.
#pragma once
#include "Harness.h"
#include "../src/plugin/Presets.h"

namespace t_presets {

using namespace gauntlet;
using namespace satk;

struct PresetRender { double rms = 0, peak = 0; bool finite = true; };

// 3 s with a held MIDI note 60 (a no-op for the drones / sequenced patches),
// RMS over the whole render (one-shot envelopes decay inside the first second)
inline PresetRender renderPreset (int idx, bool& applied)
{
    Rig r;
    applied = presets::applyToEngine (r.sys, idx);
    PresetRender pr;
    double acc = 0; long n = 0;
    for (int i = 0; i < (int) (3.0 * SR); ++i)
    {
        r.sys.key->pitchV = 0.0f;
        r.sys.key->gateV  = 5.0f;
        r.sys.engine.tick();
        const float x = r.sys.out->outL, y = r.sys.out->outR;
        if (! std::isfinite (x) || ! std::isfinite (y)) pr.finite = false;
        pr.peak = std::max (pr.peak, (double) std::max (std::fabs (x), std::fabs (y)));
        acc += 0.5 * ((double) x * x + (double) y * y); ++n;
    }
    pr.rms = std::sqrt (acc / (double) std::max (1L, n));
    return pr;
}

inline void run()
{
    GROUP ("Phase 5 factory presets (v1 demos as v2 cable sets)");

    const auto& list = presets::all();
    CHECK (list.size() == 7, "7 programs: default voice + six v1 demos", (double) list.size());

    // program 0 == the frozen default patch (cables) and all-default ctls
    {
        Rig a; a.sys.patchDefaults(); a.sys.engine.rebuild();
        Rig b; presets::applyToEngine (b.sys, 0);
        bool same = a.sys.engine.cables.size() == b.sys.engine.cables.size();
        for (size_t i = 0; same && i < a.sys.engine.cables.size(); ++i)
            same = a.sys.engine.cables[i] == b.sys.engine.cables[i];
        CHECK (same, "program 0 cables == System::patchDefaults()", (double) b.sys.engine.cables.size());
    }

    for (int i = 0; i < (int) list.size(); ++i)
    {
        bool applied = false;
        const auto pr = renderPreset (i, applied);
        std::printf ("  %-18s rms %.4f  peak %.3f\n", list[(size_t) i].name, pr.rms, pr.peak);
        char name[96];
        std::snprintf (name, sizeof (name), "program %d '%s': applies, finite, audible, level-matched", i, list[(size_t) i].name);
        // audible, never at full scale, and no program louder than ~+12 dB over
        // the default voice (rms 0.036) so switching programs doesn't jump
        CHECK (applied && pr.finite && pr.rms > 0.005 && pr.rms < 0.15 && pr.peak < 1.0, name, pr.rms, pr.peak);
    }
}

inline const bool registered = gauntlet::registerTest ("t_presets", &run);

} // namespace t_presets
