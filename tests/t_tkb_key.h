// Serge Animal TKB v2 — TKB/KEY gauntlet tests: T12 (stage advance/wrap,
// last-touched reset, random select, vertical clock/ABCD, HOLD, UP/DOWN,
// pad gate+pressure, semitone KV + GLIDE, one-hot stage pulses) plus the KEY
// MIDI-contract pieces of T15. Registration hook only at foundation; the
// stage-2 TKB/KEY builder fills it.
#pragma once
#include "Harness.h"

namespace t_tkb_key {

inline void run()
{
    gauntlet::GROUP ("T12 TKB/KEY (pending stage-2 TKB/KEY builder)");
    std::printf ("  (no assertions yet — registration hook only)\n");
}

inline const bool registered = gauntlet::registerTest ("t_tkb_key", &run);

} // namespace t_tkb_key
