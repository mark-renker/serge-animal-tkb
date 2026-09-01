// Serge Animal TKB v2 — NTO gauntlet tests: T02 (1V/oct tracking), T03
// (band-limiting/TZFM), T04 (shape morph + soft sync) and the NTO half of
// T17 (drift bounds). Registration hook only at foundation; the stage-2 NTO
// builder fills the assertions per gauntlet/GAUNTLET.md.
#pragma once
#include "Harness.h"

namespace t_nto {

inline void run()
{
    gauntlet::GROUP ("T02-T04 NTO (pending stage-2 NTO builder)");
    std::printf ("  (no assertions yet — registration hook only)\n");
}

inline const bool registered = gauntlet::registerTest ("t_nto", &run);

} // namespace t_nto
