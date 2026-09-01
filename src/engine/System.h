// Serge Animal TKB v2 — full-system graph builder.
// REGISTRATION ORDER IS FROZEN AT FOUNDATION (docs/SPEC.md §3): it defines the
// implicit one-sample feedback delay points (a module reading a LATER-
// registered module's output sees the previous sample). Mirrors v1's source
// ordering; OUT registers last so the master taps everything same-sample.
// NEVER insert or reorder — append only.
#pragma once
#include "ModulesNTO.h"
#include "ModulesSlopes.h"
#include "ModulesVCFQ.h"
#include "ModulesShapers.h"
#include "ModulesNoiseVCA.h"
#include "ModulesTKB.h"
#include "ModulesOut.h"

namespace satk {

struct System
{
    Engine engine;

    NTO*   nto   = nullptr;
    DUSG*  dusg1 = nullptr;
    DUSG*  dusg2 = nullptr;
    SSG*   ssg   = nullptr;
    VCFQ*  vcfq  = nullptr;
    VCM*   vcm   = nullptr;
    RING*  ring  = nullptr;
    NOISE* noise = nullptr;
    RVG*   rvg   = nullptr;
    TKB*   tkb   = nullptr;
    KEY*   key   = nullptr;
    VCA*   vca1  = nullptr;
    VCA*   vca2  = nullptr;
    OUT*   out   = nullptr;

    System()
    {
        auto& e = engine;
        nto   = e.add<NTO>   ("nto",   "NEW TIMBRAL OSCILLATOR");
        dusg1 = e.add<DUSG>  ("dusg1", "SLOPE GENERATOR 1", 0.2454f, 0.6978f); // 1 ms / 250 ms
        dusg2 = e.add<DUSG>  ("dusg2", "SLOPE GENERATOR 2", 0.5659f, 0.5659f); // 50 ms / 50 ms
        ssg   = e.add<SSG>   ("ssg",   "SMOOTH & STEPPED GENERATOR");
        vcfq  = e.add<VCFQ>  ("vcfq",  "VARIABLE Q VCF");
        vcm   = e.add<VCM>   ("vcm",   "WAVE MULTIPLIERS");
        ring  = e.add<RING>  ("ring",  "RING MODULATOR");
        noise = e.add<NOISE> ("noise", "NOISE SOURCE");
        rvg   = e.add<RVG>   ("rvg",   "RANDOM VOLTAGE GENERATOR");
        tkb   = e.add<TKB>   ("tkb",   "TOUCH ACTIVATED KEYBOARD SEQUENCER");
        key   = e.add<KEY>   ("key",   "KEY / MIDI");
        vca1  = e.add<VCA>   ("vca1",  "VCA 1");
        vca2  = e.add<VCA>   ("vca2",  "VCA 2");
        out   = e.add<OUT>   ("out",   "OUTPUT MIXER");

        wireMonitor();
        out->velSrc = &key->jacks[KEY::jVel].value; // velocity scales final mix (v1)
    }

    void prepare (double sr) { engine.prepare (sr); }

    // convenience for tests/presets
    bool patch (Module* a, int ja, Module* b, int jb)
    {
        return engine.connect (engine.indexOf (a), ja, engine.indexOf (b), jb);
    }

    // The FROZEN default cable set (docs/SPEC.md §6). Cables 1-5 are v1's five
    // default matrix connections; 6-7 replace v1's hardwired VCA->pan mixer.
    void patchDefaults()
    {
        patch (key,   KEY::jGate, dusg1, DUSG::jTrig);   // KEY gate -> DUSG1 trig
        patch (nto,   NTO::jVar,  vcm,   VCM::jMidIn1);  // NTO var -> VCM (mid) in
        patch (vcm,   VCM::jMidOut, vcfq, VCFQ::jIn);    // VCM mid -> VCFQ in
        patch (vcfq,  VCFQ::jLp,  vca1,  VCA::jIn);      // VCFQ LP -> VCA1 in
        patch (dusg1, DUSG::jOut, vca1,  VCA::jVc);      // DUSG1 out -> VCA1 CV
        patch (vca1,  VCA::jOut,  out,   OUT::jIn1);     // VCA1 -> mixer 1
        patch (vca2,  VCA::jOut,  out,   OUT::jIn2);     // VCA2 -> mixer 2
    }

private:
    // Monitor bus wiring, index-aligned with OUT::kMonitorSources (frozen).
    void wireMonitor()
    {
        auto& m = out->monSrc;
        m.assign ((size_t) OUT::kNumMonitorSources, nullptr);
        auto set = [&m] (int idx, Module* mod, int jack)
        { m[(size_t) idx] = &mod->jacks[(size_t) jack].value; };

        set (1,  nto,   NTO::jSine);   set (2,  nto,   NTO::jSaw);
        set (3,  nto,   NTO::jVar);    set (4,  dusg1, DUSG::jOut);
        set (5,  dusg1, DUSG::jEnd);   set (6,  dusg2, DUSG::jOut);
        set (7,  dusg2, DUSG::jEnd);   set (8,  ssg,   SSG::jSmOut);
        set (9,  ssg,   SSG::jStOut);  set (10, ssg,   SSG::jCoupler);
        set (11, vcfq,  VCFQ::jLp);    set (12, vcfq,  VCFQ::jBp);
        set (13, vcfq,  VCFQ::jHp);    set (14, vcm,   VCM::jTopOut);
        set (15, vcm,   VCM::jMidOut); set (16, vcm,   VCM::jBotOut);
        set (17, ring,  RING::jOut);   set (18, noise, NOISE::jWhite);
        set (19, noise, NOISE::jPink); set (20, rvg,   RVG::jStepped);
        set (21, rvg,   RVG::jSmooth); set (22, tkb,   TKB::jA);
        set (23, tkb,   TKB::jB);      set (24, tkb,   TKB::jC);
        set (25, tkb,   TKB::jD);      set (26, tkb,   TKB::jGate);
        set (27, key,   KEY::jKv);     set (28, key,   KEY::jGate);
        set (29, vca1,  VCA::jOut);    set (30, vca2,  VCA::jOut);
        set (31, nto,   NTO::jTri);    set (32, nto,   NTO::jPulse);
        set (33, ssg,   SSG::jCycle);  set (34, ssg,   SSG::jHotCoupler);
        set (35, vcfq,  VCFQ::jNotch); set (36, vcm,   VCM::jBotSq);
        set (37, noise, NOISE::jShSrc);set (38, rvg,   RVG::jPulse);
        set (39, tkb,   TKB::jAbcd);   set (40, tkb,   TKB::jKv);
        set (41, tkb,   TKB::jPressure); set (42, tkb, TKB::jKp);
    }
};

} // namespace satk
