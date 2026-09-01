// Serge Animal TKB v2 — VCM/RING gauntlet tests: T08 (odd-harmonic folding,
// transfer turnarounds, even-dominant bottom + squared-up 2nd out, top LO
// linear VCA / HI rounded clip, alias floor) and T09 (carrier suppression,
// trims, morph monotonicity, DC-carrier VCA behavior). Registration hook only
// at foundation; the stage-2 shapers builder fills it.
#pragma once
#include "Harness.h"

namespace t_vcm_ring {

inline void run()
{
    gauntlet::GROUP ("T08-T09 VCM/RING (pending stage-2 shapers builder)");
    std::printf ("  (no assertions yet — registration hook only)\n");
}

inline const bool registered = gauntlet::registerTest ("t_vcm_ring", &run);

} // namespace t_vcm_ring
