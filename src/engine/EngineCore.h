// Serge Animal TKB v2 — patchable engine core (no JUCE dependency; headless-testable).
// Ported near-verbatim from Paperface 50 (~/dev/paperface50-emulation,
// src/engine/EngineCore.h). Two deliberate, frozen changes:
//   1. Bi-directional jacks are dropped (this instrument has no router module);
//      JackType is In/Out only and Engine::connect requires exactly one of each.
//   2. Engine-level INSTANCE SEED (gates T14/T17): a uint64 owned by the Engine,
//      set at construction / setSeed(), serialized with the plugin state. Every
//      module receives a per-module seed derived from (instanceSeed, module id)
//      and must derive ALL of its random state (component tolerance, drift phase,
//      noise streams) deterministically from it in reseed()/prepare().
//
// Voltage conventions (docs/SPEC.md §1, frozen):
//   BLACK  = bipolar audio, ±2.5 V nominal        WHITE = 0..5 V control voltage
//   RED    = 0/5 V pulses, logic threshold 2.5 V  GREY  = "hot" outs (beyond ±5 V)
//   PURPLE = sync. System rails ±12 V; every summed input is rail-clamped.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "DspUtils.h"

namespace satk {

constexpr float kCV      = 5.0f;   // full-scale DC control voltage
constexpr float kAudio   = 2.5f;   // nominal audio amplitude (±)
constexpr float kRails   = 12.0f;  // system voltage rails (clamp)
constexpr float kLogicHi = 2.5f;   // threshold: a signal above this reads "high"

// Default instance seed. Fresh instances share it (deterministic renders and
// snapshots); the UI may later offer a "re-roll unit serial" that changes it.
constexpr uint64_t kDefaultInstanceSeed = 0x53415432303236ULL; // "SAT2026"

enum class JackType  : uint8_t { In, Out };
enum class JackColor : uint8_t { White, Red, Black, Purple, Grey };

inline float clampRails (float v) { return v >  kRails ? kRails : (v < -kRails ? -kRails : v); }
inline bool  isHigh     (float v) { return v > kLogicHi; }
inline float flushDenorm(float v) { return std::fabs (v) < 1.0e-15f ? 0.0f : v; }
// Exact change test for coefficient caches (deliberately bit-level; spelled
// without == so -Wfloat-equal stays clean in our sources).
inline bool  differs    (float a, float b) { return a < b || a > b; }

struct Jack
{
    const char* name = "";
    JackType    type = JackType::In;
    JackColor   color = JackColor::White;
    float       value = 0.0f;      // last output value (Out jacks)
};

// A knob/switch/button owned by a module. The plugin layer binds these to host
// parameters; tests poke .v directly. Continuous ctls are normalized 0..1 and
// mapped inside the module (mapping frozen in docs/SPEC.md). Stepped ctls hold
// the step INDEX in .v (0..steps-1). `labels` (steps entries, may be nullptr)
// feeds the host's choice-parameter labels.
struct Ctl
{
    const char* name = "";
    float v = 0.0f;
    float def = 0.0f;
    int   steps = 0;     // 0 = continuous; 2 = toggle/momentary; n = n-way switch
    const char* const* labels = nullptr;
};

class Module
{
public:
    virtual ~Module() = default;

    std::string id;      // stable id, e.g. "nto" — used in state + param IDs
    std::string title;   // panel title, e.g. "NEW TIMBRAL OSCILLATOR"
    std::vector<Jack> jacks;
    std::vector<Ctl>  ctls;
    std::vector<float> leds; // 0..1 brightness, written by tick, read by UI

    // Per-module seed, derived by the Engine from (instanceSeed, id). Valid
    // before prepare(). reseed() is called whenever it changes; modules derive
    // tolerance offsets / RNG stream seeds here (and may re-derive in prepare).
    uint64_t moduleSeed = 0;
    virtual void reseed() {}

    virtual void prepare (double sampleRate) { sr = (float) sampleRate; }
    virtual void tick() = 0; // one sample: read inputs, write jack values

    // -- helpers wired by Engine::rebuild() --------------------------------
    float in (int jackIdx) const;        // summed, rail-clamped input
    bool  connected (int jackIdx) const; // any cable on this jack?

    int addJack (const char* n, JackType t, JackColor c)
    { jacks.push_back ({ n, t, c, 0.0f }); return (int) jacks.size() - 1; }
    int addCtl (const char* n, float def, int steps = 0, const char* const* labels = nullptr)
    { ctls.push_back ({ n, def, def, steps, labels }); return (int) ctls.size() - 1; }

    float sr = 48000.0f;

    // per-jack source pointers (outputs feeding this jack), rebuilt on patch change
    struct Feed { std::vector<const float*> src; };
    std::vector<Feed> feeds;
};

inline float Module::in (int j) const
{
    const auto& f = feeds[(size_t) j];
    float s = 0.0f;
    for (auto* p : f.src) s += *p;
    return clampRails (s);
}
inline bool Module::connected (int j) const { return ! feeds[(size_t) j].src.empty(); }

struct Cable
{
    int modA = 0, jackA = 0, modB = 0, jackB = 0;
    bool operator== (const Cable& o) const
    { return (modA==o.modA && jackA==o.jackA && modB==o.modB && jackB==o.jackB)
          || (modA==o.modB && jackA==o.jackB && modB==o.modA && jackB==o.jackA); }
};

class Engine
{
public:
    std::vector<std::unique_ptr<Module>> mods;
    std::vector<Cable> cables;

    template <typename T, typename... A>
    T* add (const char* id, const char* title, A&&... a)
    {
        auto m = std::make_unique<T> (std::forward<A> (a)...);
        m->id = id; m->title = title;
        auto* raw = m.get();
        mods.push_back (std::move (m));
        return raw;
    }

    Module* byId (const std::string& id)
    {
        for (auto& m : mods) if (m->id == id) return m.get();
        return nullptr;
    }
    int indexOf (const Module* m) const
    {
        for (size_t i = 0; i < mods.size(); ++i) if (mods[i].get() == m) return (int) i;
        return -1;
    }

    // ---- instance seed (frozen contract, gates T14/T17) -------------------
    uint64_t seed() const noexcept { return instanceSeed; }
    void setSeed (uint64_t s) { instanceSeed = s; applySeeds(); }
    void applySeeds()
    {
        for (auto& m : mods)
        {
            m->moduleSeed = dsp::seedMix (instanceSeed, dsp::fnv1a (m->id.c_str()));
            m->reseed();
        }
    }

    void prepare (double sampleRate)
    {
        applySeeds();
        for (auto& m : mods) m->prepare (sampleRate);
        rebuild();
    }

    // Exactly one end must be an Out and the other an In.
    bool connect (int mA, int jA, int mB, int jB)
    {
        if (mA < 0 || mB < 0 || mA >= (int) mods.size() || mB >= (int) mods.size()) return false;
        if (jA < 0 || jB < 0 || jA >= (int) mods[(size_t) mA]->jacks.size()
                             || jB >= (int) mods[(size_t) mB]->jacks.size()) return false;
        if (mA == mB && jA == jB) return false;
        const auto ta = mods[(size_t) mA]->jacks[(size_t) jA].type;
        const auto tb = mods[(size_t) mB]->jacks[(size_t) jB].type;
        if (ta == tb) return false; // In-In / Out-Out refused
        Cable c { mA, jA, mB, jB };
        for (auto& e : cables) if (e == c) return false;
        cables.push_back (c);
        rebuild();
        return true;
    }
    void disconnectAt (int m, int j)
    {
        cables.erase (std::remove_if (cables.begin(), cables.end(),
            [&] (const Cable& c) { return (c.modA==m && c.jackA==j) || (c.modB==m && c.jackB==j); }),
            cables.end());
        rebuild();
    }
    void removeCable (const Cable& c)
    {
        cables.erase (std::remove (cables.begin(), cables.end(), c), cables.end());
        rebuild();
    }
    void clearCables() { cables.clear(); rebuild(); }

    // Resolve feeds. Direction: Out -> In (only). Multiple outs into one In
    // sum (banana output-stacking); the sum is rail-clamped in Module::in().
    void rebuild()
    {
        for (auto& m : mods)
            m->feeds.assign (m->jacks.size(), {});
        for (auto& c : cables)
        {
            auto& a = mods[(size_t) c.modA]->jacks[(size_t) c.jackA];
            auto& b = mods[(size_t) c.modB]->jacks[(size_t) c.jackB];
            if (a.type == JackType::Out && b.type == JackType::In)
                mods[(size_t) c.modB]->feeds[(size_t) c.jackB].src.push_back (&a.value);
            else if (b.type == JackType::Out && a.type == JackType::In)
                mods[(size_t) c.modA]->feeds[(size_t) c.jackA].src.push_back (&b.value);
        }
    }

    void tick() { for (auto& m : mods) m->tick(); }

private:
    uint64_t instanceSeed = kDefaultInstanceSeed;
};

// ---- small engine-level helpers (ported from PF50 ModulesVoice.h) ----------
struct EdgeDetector
{
    bool prev = false;
    bool rising (float v) { bool h = isHigh (v); bool e = h && ! prev; prev = h; return e; }
    void reset() { prev = false; }
};

struct PulseGen // short trigger pulse, min 2 samples, 0/5 V
{
    int n = 0;
    void fire (float sr, float ms = 1.0f) { n = std::max (2, (int) (sr * ms * 0.001f)); }
    float tick() { if (n > 0) { --n; return kCV; } return 0.0f; }
    bool active() const { return n > 0; }
    void reset() { n = 0; }
};

struct DCBlock
{
    float x1 = 0, y1 = 0, R = 0.999f;
    void prepare (float sr) { R = 1.0f - 2.0f * 3.14159265f * 10.0f / sr; x1 = y1 = 0; }
    float tick (float x) { float y = x - x1 + R * y1; x1 = x; y1 = flushDenorm (y); return y1; }
};

} // namespace satk
