// Serge Animal TKB v2 — factory presets (phase 5): program 0 is the default
// voice, programs 1..6 are v1's six demo patches recreated as v2 cable sets.
//
// JUCE-free data + an engine-level apply(), so satk_gauntlet can verify every
// preset renders (tests/t_presets.h); the plugin applies the same data through
// its APVTS parameters (SatkProcessor::setCurrentProgram).
//
// Translation notes (v1 matrix -> v2 patch cords), logged in gauntlet/LOG.md:
//  - v1 matrix slots carried a per-connection amount; v2 cords are unity, so an
//    amount becomes the destination's attenuverter where one exists
//    (av = 0.5 + amount / 2), or is absorbed into a knob that sets the same
//    musical depth (e.g. TKB RANGE for row-driven CV).
//  - v1's "KEY held at f" drones become the NTO FREQ knob at f with VCA LEVEL
//    holding the VCAs open (no MIDI note needed).
//  - v1's monitor-bus output (demo 2) is kept as the monitor bus.
#pragma once
#include "../engine/System.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace satk::presets {

// inverse of dsp::expmap: the knob position (0..1) that maps to `v` on lo..hi
inline float expPos (float v, float lo, float hi)
{
    return std::clamp (std::log (v / lo) / std::log (hi / lo), 0.0f, 1.0f);
}
// v1 attenuator amount (0..1) -> v2 attenuverter position (centre = 0)
inline float av (float amount) { return std::clamp (0.5f + 0.5f * amount, 0.0f, 1.0f); }
// DUSG / SSG time knobs: expo 50 us .. 10 s
inline float timePos (float seconds) { return expPos (seconds, 50.0e-6f, 10.0f); }

struct Ctl   { const char* mod; int ctl; float value; }; // continuous: 0..1; stepped: index
struct Cable { const char* modA; int jackA; const char* modB; int jackB; };

struct Preset
{
    const char* name;
    std::vector<Ctl> ctls;    // on top of every ctl's default
    std::vector<Cable> cables; // the complete cable set
};

// the frozen default cable set (SPEC §6), as data
inline std::vector<Cable> defaultCables()
{
    return {
        { "key",   KEY::jGate,  "dusg1", DUSG::jTrig },
        { "nto",   NTO::jVar,   "vcm",   VCM::jMidIn1 },
        { "vcm",   VCM::jMidOut,"vcfq",  VCFQ::jIn },
        { "vcfq",  VCFQ::jLp,   "vca1",  VCA::jIn },
        { "dusg1", DUSG::jOut,  "vca1",  VCA::jVc },
        { "vca1",  VCA::jOut,   "out",   OUT::jIn1 },
        { "vca2",  VCA::jOut,   "out",   OUT::jIn2 },
        { "key",   KEY::jKv,    "nto",   NTO::jOctA },
    };
}

inline void addRow (std::vector<Ctl>& c, int rowBase, const float (&v)[16])
{
    for (int i = 0; i < 16; ++i) c.push_back ({ "tkb", rowBase + i, v[i] });
}

inline const std::vector<Preset>& all()
{
    static const std::vector<Preset> list = []
    {
        std::vector<Preset> p;

        // 0 — the out-of-the-box voice
        p.push_back ({ "Default Voice", {}, defaultCables() });

        // 1 — v1 demo 1: default voice, NTO shape + mid fold swept by a slow cycling DUSG2
        {
            Preset x { "Fold Sweep Voice", {}, defaultCables() };
            x.ctls = {
                { "vcm",   VCM::cFold, 0.15f },
                { "dusg2", DUSG::cCycle, 1 }, { "dusg2", DUSG::cRise, timePos (0.9f) }, { "dusg2", DUSG::cFall, timePos (0.9f) },
                { "vcfq",  VCFQ::cFreq, expPos (2200.0f, 20.0f, 12000.0f) }, { "vcfq", VCFQ::cQ, expPos (3.0f, 0.5f, 40.0f) },
                { "dusg1", DUSG::cFall, timePos (0.5f) },
                { "nto",   NTO::cWave, 0.35f },
            };
            x.cables.push_back ({ "dusg2", DUSG::jOut, "nto", NTO::jVcWave });
            x.cables.push_back ({ "dusg2", DUSG::jOut, "vcm", VCM::jVcFold });
            p.push_back (x);
        }

        // 2 — v1 demo 2: no oscillator; TKB gates ping the VCFQ, row B tunes it per step,
        //     RVG randomises Q; heard on the monitor bus (VCFQ LP)
        {
            Preset x { "Filter Pings", {}, {} };
            x.ctls = {
                { "tkb",  TKB::cRun, 1 }, { "tkb", TKB::cRate, expPos (7.0f, 0.1f, 30.0f) }, { "tkb", TKB::cLength, 7 },
                { "vcfq", VCFQ::cFreq, expPos (300.0f, 20.0f, 12000.0f) }, { "vcfq", VCFQ::cQ, expPos (18.0f, 0.5f, 40.0f) },
                { "vcfq", VCFQ::cPing, 0.9f }, { "vcfq", VCFQ::cVcFAmt, av (0.5f) }, { "vcfq", VCFQ::cVcQAmt, av (0.25f) },
                { "rvg",  RVG::cRate, expPos (3.0f, 0.05f, 50.0f) },
                { "out",  OUT::cMonSrc, 11 }, { "out", OUT::cMonLvl, 0.9f },
            };
            const float rowB[16] = { 0.05f, 0.5f, 0.25f, 0.8f, 0.12f, 0.6f, 0.35f, 0.95f, 0, 0, 0, 0, 0, 0, 0, 0 };
            addRow (x.ctls, TKB::cRowB1, rowB);
            x.cables = {
                { "tkb", TKB::jGate, "vcfq", VCFQ::jTrig },
                { "tkb", TKB::jB,    "vcfq", VCFQ::jVcF },
                { "rvg", RVG::jStepped, "vcfq", VCFQ::jVcQ },
                { "vca1", VCA::jOut, "out", OUT::jIn1 }, { "vca2", VCA::jOut, "out", OUT::jIn2 },
            };
            p.push_back (x);
        }

        // 3 — v1 demo 3 "Serge bongos": DUSG1 cycling at audio rate, TKB row A bends its
        //     rise/fall (pitch), DUSG2 transients strike it through VCA2
        {
            Preset x { "Slope Bongos", {}, {} };
            x.ctls = {
                { "tkb", TKB::cRun, 1 }, { "tkb", TKB::cRate, expPos (6.0f, 0.1f, 30.0f) }, { "tkb", TKB::cLength, 15 },
                { "tkb", TKB::cGateLen, expPos (0.02f, 0.005f, 1.0f) },
                { "tkb", TKB::cRange, (1.75f - 0.5f) / 4.5f },    // v1 amount 0.35 x 5 V
                { "dusg1", DUSG::cCycle, 1 }, { "dusg1", DUSG::cRise, timePos (0.004f) }, { "dusg1", DUSG::cFall, timePos (0.004f) },
                { "dusg2", DUSG::cRise, timePos (0.001f) }, { "dusg2", DUSG::cFall, timePos (0.09f) },
                { "vca2", VCA::cResponse, 1 },
            };
            const float rowA[16] = { 0.2f, 0.2f, 0.55f, 0.2f, 0.75f, 0.35f, 0.2f, 0.9f,
                                     0.2f, 0.55f, 0.2f, 0.35f, 0.65f, 0.2f, 0.85f, 0.45f };
            addRow (x.ctls, TKB::cRowA1, rowA);
            x.cables = {
                { "tkb", TKB::jA, "dusg1", DUSG::jVcRise },
                { "tkb", TKB::jA, "dusg1", DUSG::jVcFall },
                { "tkb", TKB::jGate, "dusg2", DUSG::jTrig },
                { "dusg1", DUSG::jOut, "vca2", VCA::jIn },
                { "dusg2", DUSG::jOut, "vca2", VCA::jVc },
                { "vca1", VCA::jOut, "out", OUT::jIn1 }, { "vca2", VCA::jOut, "out", OUT::jIn2 },
            };
            p.push_back (x);
        }

        // 4 — v1 demo 4: NTO x DUSG2 (slope oscillator) ring mod through the bottom
        //     folder; fold depth, filter and a hint of pitch drift swept by RVG smooth
        {
            Preset x { "Ring Fold Drone", {}, {} };
            x.ctls = {
                { "nto",  NTO::cFreq, expPos (65.41f, 16.0f, 16000.0f) }, { "nto", NTO::cVcFreqAmt, av (0.04f) },
                { "dusg2", DUSG::cCycle, 1 }, { "dusg2", DUSG::cRise, timePos (0.0051f) }, { "dusg2", DUSG::cFall, timePos (0.0051f) },
                { "rvg",  RVG::cRate, expPos (0.3f, 0.05f, 50.0f) },
                { "vcm",  VCM::cBotDepth, 0.45f },
                { "vcfq", VCFQ::cFreq, expPos (500.0f, 20.0f, 12000.0f) }, { "vcfq", VCFQ::cQ, expPos (4.0f, 0.5f, 40.0f) },
                { "vcfq", VCFQ::cVcFAmt, av (0.35f) },
                { "vca1", VCA::cLevel, 0.17f }, { "vca1", VCA::cResponse, 0 }, // levels trimmed so programs
                { "vca2", VCA::cLevel, 0.14f }, { "vca2", VCA::cResponse, 0 }, // sit near the default voice (re-trimmed for sound-r2)
            };
            x.cables = {
                { "nto",  NTO::jVar, "ring", RING::jX },
                { "dusg2", DUSG::jOut, "ring", RING::jY },
                { "ring", RING::jOut, "vcm", VCM::jBotIn1 },
                { "rvg",  RVG::jSmooth, "vcm", VCM::jVcBot },
                { "rvg",  RVG::jSmooth, "nto", NTO::jVcFreq },
                { "rvg",  RVG::jSmooth, "vcfq", VCFQ::jVcF },
                { "vcm",  VCM::jBotOut, "vca1", VCA::jIn },
                { "vcm",  VCM::jBotOut, "vcfq", VCFQ::jIn },
                { "vcfq", VCFQ::jLp, "vca2", VCA::jIn },
                { "vca1", VCA::jOut, "out", OUT::jIn1 }, { "vca2", VCA::jOut, "out", OUT::jIn2 },
            };
            p.push_back (x);
        }

        // 5 — v1 demo 5: noise -> stepped (clocked by the TKB) -> NTO pitch; the smooth
        //     section slews a copy into the filter cutoff; TKB gates the envelope
        {
            Preset x { "Random Melody", {}, defaultCables() };
            x.ctls = {
                { "tkb", TKB::cRun, 1 }, { "tkb", TKB::cRate, expPos (4.0f, 0.1f, 30.0f) },
                { "nto", NTO::cFreq, expPos (130.81f, 16.0f, 16000.0f) }, { "nto", NTO::cWave, 0.55f },
                { "nto", NTO::cVcFreqAmt, av (0.45f) },
                { "ssg", SSG::cSmRate, timePos (0.12f) },
                { "dusg1", DUSG::cFall, timePos (0.35f) },
                { "vcfq", VCFQ::cFreq, expPos (900.0f, 20.0f, 12000.0f) }, { "vcfq", VCFQ::cQ, expPos (6.0f, 0.5f, 40.0f) },
                { "vcfq", VCFQ::cVcFAmt, av (0.4f) },
                { "vcm", VCM::cFold, 0.2f },
            };
            // the TKB clock, not the MIDI gate, fires the envelope
            x.cables.erase (x.cables.begin()); // drop key.jGate -> dusg1.jTrig
            x.cables.push_back ({ "tkb", TKB::jGate, "dusg1", DUSG::jTrig });
            x.cables.push_back ({ "noise", NOISE::jWhite, "ssg", SSG::jStIn });
            x.cables.push_back ({ "tkb", TKB::jGate, "ssg", SSG::jSample });
            x.cables.push_back ({ "ssg", SSG::jStOut, "ssg", SSG::jSmIn });
            x.cables.push_back ({ "ssg", SSG::jStOut, "nto", NTO::jVcFreq });
            x.cables.push_back ({ "ssg", SSG::jSmOut, "vcfq", VCFQ::jVcF });
            p.push_back (x);
        }

        // 6 — v1 demo 6 "wild animal": VCFQ BP -> folder -> ring mod against the NTO ->
        //     back into the filter; SSG-slewed random voltage steers the scream
        {
            Preset x { "Wild Animal", {}, {} };
            x.ctls = {
                { "nto",  NTO::cFreq, expPos (98.0f, 16.0f, 16000.0f) },
                { "vcfq", VCFQ::cFreq, expPos (400.0f, 20.0f, 12000.0f) }, { "vcfq", VCFQ::cQ, expPos (9.0f, 0.5f, 40.0f) },
                { "vcfq", VCFQ::cGain, 0.85f * 0.5f },           // v1 input amount 0.85 (GAIN 0..2)
                { "vcfq", VCFQ::cVcFAmt, av (0.55f) },
                { "ssg",  SSG::cSmRate, timePos (0.3f) },
                { "dusg1", DUSG::cCycle, 1 }, { "dusg1", DUSG::cRise, timePos (0.4f) }, { "dusg1", DUSG::cFall, timePos (0.4f) },
                { "vcm",  VCM::cFold, 0.5f },
                { "vca1", VCA::cLevel, 0.55f }, { "vca1", VCA::cResponse, 0 },
                { "vca2", VCA::cLevel, 0.40f }, { "vca2", VCA::cResponse, 0 },
            };
            x.cables = {
                { "vcfq", VCFQ::jBp, "vcm", VCM::jMidIn1 },
                { "nto",  NTO::jVar, "vcm", VCM::jMidIn2 },
                { "vcm",  VCM::jMidOut, "ring", RING::jX },
                { "nto",  NTO::jSine, "ring", RING::jY },
                { "ring", RING::jOut, "vcfq", VCFQ::jIn },
                { "noise", NOISE::jWhite, "ssg", SSG::jStIn },
                { "dusg1", DUSG::jEnd, "ssg", SSG::jSample },
                { "ssg",  SSG::jStOut, "ssg", SSG::jSmIn },
                { "ssg",  SSG::jSmOut, "vcfq", VCFQ::jVcF },
                { "vcfq", VCFQ::jBp, "vca1", VCA::jIn },
                { "ring", RING::jOut, "vca2", VCA::jIn },
                { "vca1", VCA::jOut, "out", OUT::jIn1 }, { "vca2", VCA::jOut, "out", OUT::jIn2 },
            };
            p.push_back (x);
        }
        return p;
    }();
    return list;
}

inline Module* findModule (Engine& e, const char* id)
{
    for (auto& m : e.mods)
        if (m->id == id) return m.get();
    return nullptr;
}

// Engine-level apply (tests / offline tools). Every ctl returns to its
// default first, then the preset's ctls and its complete cable set go in.
// Returns false if the preset references an unknown module.
inline bool applyToEngine (System& sys, int index)
{
    const auto& list = all();
    if (index < 0 || index >= (int) list.size()) return false;
    const auto& p = list[(size_t) index];
    for (auto& m : sys.engine.mods)
        for (auto& c : m->ctls) c.v = c.def;
    for (const auto& c : p.ctls)
    {
        auto* m = findModule (sys.engine, c.mod);
        if (m == nullptr || c.ctl < 0 || c.ctl >= (int) m->ctls.size()) return false;
        m->ctls[(size_t) c.ctl].v = c.value;
    }
    sys.engine.cables.clear();
    for (const auto& c : p.cables)
    {
        auto* a = findModule (sys.engine, c.modA);
        auto* b = findModule (sys.engine, c.modB);
        if (a == nullptr || b == nullptr) return false;
        sys.patch (a, c.jackA, b, c.jackB);
    }
    sys.engine.rebuild();
    return true;
}

} // namespace satk::presets
