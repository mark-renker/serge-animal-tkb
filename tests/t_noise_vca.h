// Serge Animal TKB v2 — NOISE/RVG/VCA gauntlet tests: T10 (spectral trends,
// S/H source bounds/autocorrelation, RVG trigger semantics) and T11 (expo
// 12 dB/V law, cutoff, +6 dB max gain, click bound). Registration hook only
// at foundation; the stage-2 noise/VCA builder fills it.
#pragma once
#include "Harness.h"

namespace t_noise_vca {

inline void run()
{
    gauntlet::GROUP ("T10-T11 NOISE/RVG/VCA (pending stage-2 noise/VCA builder)");
    std::printf ("  (no assertions yet — registration hook only)\n");
}

inline const bool registered = gauntlet::registerTest ("t_noise_vca", &run);

} // namespace t_noise_vca
