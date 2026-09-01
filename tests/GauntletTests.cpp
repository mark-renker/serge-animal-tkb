// Serge Animal TKB v2 — Gauntlet hard gate G2: headless DSP verification suite.
// Registry runner. Pure C++17, no JUCE. Exit code = number of failures.
// Frozen with gauntlet/GAUNTLET.md rev B; per-module files below are filled in
// by the stage-2 module builders (each owns exactly one t_*.h).
#include "Harness.h"

#include "t_nto.h"       // T02-T04            (NTO builder)
#include "t_dusg_ssg.h"  // T05-T06            (slopes builder)
#include "t_vcfq.h"      // T07                (VCFQ builder)
#include "t_vcm_ring.h"  // T08-T09            (shapers builder)
#include "t_noise_vca.h" // T10-T11            (noise/VCA builder)
#include "t_tkb_key.h"   // T12                (TKB/KEY builder)
#include "t_system.h"    // T00, T01, T13, T14, T15, T16, T17 (system-level)

int main()
{
    std::printf ("Serge Animal TKB v2 gauntlet — DSP suite (SR=%.0f)\n", gauntlet::SR);
    std::printf ("registered test files: %d\n", (int) gauntlet::registry().size());

    for (auto& t : gauntlet::registry())
        t.fn();

    std::printf ("\n%d passed, %d failed\n", gauntlet::passed, gauntlet::failed);
    return gauntlet::failed;
}
