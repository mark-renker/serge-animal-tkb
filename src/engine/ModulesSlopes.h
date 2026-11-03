// Serge Animal TKB v2 — DUSG (x2) and SSG (slew-core family).
// Jack/ctl registration FROZEN per docs/SPEC.md §4.2-§4.3 (indices
// append-only forever). Stage-2 slopes builder: one shared exact
// tanh-landing slew core (namespace slew) drives DUSG (T05) and SSG (T06).
#pragma once
#include "EngineCore.h"

namespace satk {

// ---------------------------------------------------------------------------
// Shared slew core. Model: the integrator is driven by a diff-pair current
// that saturates as tanh (Stinchcombe, "The Serge VCS: How it works"):
//     dy/dt = R * tanh((target - y) / Vk)
// i.e. a CONSTANT slope R (linear by design) away from the target and a soft
// tanh current-limit landing within ~Vk of it. The ODE has an exact solution:
// with u = |target - y| / Vk,   sinh(u(t)) = sinh(u0) * exp(-R t / Vk).
// Stepping with the exact solution (log domain, double precision) makes the
// core unconditionally monotonic and overshoot-free at ANY rate, and lets the
// DUSG state machine find the exact sub-sample instant a slope completes
// (fractional carry -> cycle pitch is not quantized to sample boundaries).
// Curvature (expo/log shapes) comes only from patching OUT -> VC RISE/FALL.
// ---------------------------------------------------------------------------
namespace slew {

constexpr double kVk   = 0.025;  // tanh knee (V): landing zone ~0.5% of full scale
constexpr double kTol  = 0.0025; // completion comparator distance (V) = 0.1 Vk
constexpr double kSnap = 1.0e-7; // closer than this = exactly on target (no denormals)
constexpr double kLn2  = 0.69314718055994531;

inline double lnSinh (double u) // u > 0
{ return u > 20.0 ? u - kLn2 : std::log (std::sinh (u)); }
inline double invLnSinh (double L)
{ return L > 19.0 ? L + kLn2 : (L < -80.0 ? 0.0 : std::asinh (std::exp (L))); }

// Calibration: slope R = spanC() / T makes a full 0 -> 5 V transient
// (tol .. 5 V - tol, tanh landing included) take exactly T seconds.
inline double spanC()
{
    static const double c = kVk * (lnSinh (((double) kCV - kTol) / kVk) - lnSinh (kTol / kVk));
    return c;
}

// Effective time law (v1, SPEC §4.2): knob expo 50 us..10 s, x 2^(-V) (1 V/oct,
// + = faster), clamped 20 us..30 s.
inline double timeLaw (float knob01, float volts)
{
    const double t = (double) dsp::expmap (knob01, 50.0e-6f, 10.0f)
                   * std::exp2 (-(double) std::clamp (volts, -kRails, kRails));
    return std::clamp (t, 20.0e-6, 30.0);
}

// Advance y toward tgt with slope R (V/s) for at most `budget` seconds.
// doneTol > 0: if |tgt - y| reaches doneTol within the budget, stop exactly
// there, subtract the time used from `budget` and return true.
// doneTol == 0: plain follower (spends the whole budget, lands exactly on tgt).
inline bool advance (double& y, double tgt, double R, double& budget, double doneTol)
{
    const double e = tgt - y;
    const double a = std::fabs (e);
    if (a <= doneTol) return true;
    if (a <= kSnap) { y = tgt; budget = 0.0; return false; }
    const double s  = e > 0.0 ? 1.0 : -1.0;
    const double k  = R / kVk;
    const double u0 = a / kVk;
    const double u1lin = u0 - k * budget;
    if (doneTol <= 0.0 && u1lin > 21.0) // deep in the linear region: exact and cheap
    { y += s * R * budget; budget = 0.0; return false; }
    const double L0 = lnSinh (u0);
    if (doneTol > 0.0)
    {
        const double need = (L0 - lnSinh (doneTol / kVk)) / k;
        if (need <= budget) { budget -= need; y = tgt - s * doneTol; return true; }
        if (u1lin > 21.0) { y += s * R * budget; budget = 0.0; return false; }
    }
    double a1 = kVk * invLnSinh (L0 - k * budget);
    if (a1 <= kSnap) a1 = 0.0;
    y = tgt - s * a1;
    budget = 0.0;
    return false;
}

} // namespace slew

// Universal Slope Generator (one channel of the hardware DUSG). Linear-by-
// design slopes with tanh current-limit soft landing; END/GATE is a LEVEL
// (high during rise and while OUT sits at bottom, low during fall — T05e);
// non-retriggerable during a transient; gate into IN = ASR; CYCLE ctl
// self-patches END->TRIG conceptually.
class DUSG : public Module
{
public:
    enum J { jOut, jEnd, jTrig, jIn, jVcRise, jVcFall };
    enum C { cRise, cFall, cCycle };

    // riseDef/fallDef are normalized positions on the expo 50 µs..10 s map:
    // dusg1: rise 1 ms (0.2454), fall 250 ms (0.6978)
    // dusg2: rise 50 ms (0.5659), fall 50 ms (0.5659)
    DUSG (float riseDef, float fallDef)
    {
        addJack ("OUT",      JackType::Out, JackColor::White); // 0..5 V
        addJack ("END/GATE", JackType::Out, JackColor::Red);   // level, 0/5 V
        addJack ("TRIG",     JackType::In,  JackColor::Red);
        addJack ("IN",       JackType::In,  JackColor::Black); // DC follower / ASR gate
        addJack ("VC RISE",  JackType::In,  JackColor::White); // 1 V/oct, + = faster
        addJack ("VC FALL",  JackType::In,  JackColor::White); // 1 V/oct, + = faster

        addCtl ("RISE",  riseDef); // expo 50 µs..10 s
        addCtl ("FALL",  fallDef); // expo 50 µs..10 s
        addCtl ("CYCLE", 0.0f, 2, kOffOn);
        leds.resize (1);
    }

    void reseed() override { resetState(); } // no random state; deterministic reset

    void prepare (double sampleRate) override
    {
        Module::prepare (sampleRate);
        dt = 1.0 / sampleRate;
        resetState();
    }

    void tick() override
    {
        const double C  = slew::spanC();
        const double Rr = C / slew::timeLaw (ctls[cRise].v, in (jVcRise));
        const double Rf = C / slew::timeLaw (ctls[cFall].v, in (jVcFall));
        const double follow = std::clamp ((double) in (jIn), -10.0, 10.0);
        const bool cycle = ctls[cCycle].v >= 0.5f;

        // TRIG: a full transient, accepted only when idle (non-retriggerable
        // during rise AND fall — T05f). The edge detector always runs, so a
        // gate held across the end of a transient cannot fire late.
        if (trigEdge.rising (in (jTrig)) && phase == Phase::Idle)
            phase = Phase::Rise;

        // Walk this sample's time budget through the state machine with exact
        // sub-sample transition instants. END/GATE is the time-average of its
        // level over the sample (box-filtered edge: no period quantization).
        double budget = dt, highT = 0.0;
        for (int iter = 0; iter < 6 && budget > 0.0; ++iter)
        {
            const double b0 = budget;
            if (phase == Phase::Rise)
            {
                const bool done = slew::advance (y, (double) kCV, Rr, budget, slew::kTol);
                highT += b0 - budget;
                if (done) phase = Phase::Fall;
            }
            else if (phase == Phase::Fall)
            {
                // Falls toward the IN level (0 V unpatched). An IN level at or
                // above OUT ends the fall at once and the follower takes over
                // (gate into IN = ASR sustain, overriding TRIG — T05g).
                // END of the transient: END/GATE goes high again (at the bottom,
                // or held high through an ASR sustain until the follower falls).
                if (y - follow <= slew::kTol
                    || slew::advance (y, follow, Rf, budget, slew::kTol))
                { phase = Phase::Idle; followUp = true; }
            }
            else // Idle: DC follower on IN (slew limiter / env follower / ASR)
            {
                if (cycle) { phase = Phase::Rise; continue; } // END->TRIG self-patch
                const double e = follow - y;
                if (e >  slew::kTol) followUp = true;
                if (e < -slew::kTol) followUp = false;
                slew::advance (y, follow, e > 0.0 ? Rr : Rf, budget, 0.0);
                if (followUp || y <= slew::kTol) highT += b0;
                budget = 0.0;
            }
        }
        if (budget > 0.0 && phase != Phase::Fall) highT += budget; // zero-time loop guard

        const float out = (float) y;
        jacks[jOut].value = out;
        jacks[jEnd].value = (float) (kCV * std::min (1.0, highT / dt));
        leds[0] = std::clamp (out * (1.0f / kCV), 0.0f, 1.0f);
    }

private:
    enum class Phase : uint8_t { Idle, Rise, Fall };

    void resetState()
    {
        y = 0.0; phase = Phase::Idle; followUp = false;
        trigEdge.reset();
    }

    static constexpr const char* kOffOn[2] = { "OFF", "ON" };

    EdgeDetector trigEdge;
    double y = 0.0, dt = 1.0 / 48000.0;
    Phase  phase = Phase::Idle;
    bool   followUp = false; // follower last moved up (END/GATE high through ASR sustain)
};

// Smooth & Stepped Generator. Smooth = VC lag with CYCLE pulse out + HOLD
// track-and-hold; Stepped = S&H with VC step-size limiting (static without
// SAMPLE pulses); Coupler comparator 0/5 V plus HOT bipolar out (≥ ±10 V).
class SSG : public Module
{
public:
    enum J { jSmOut, jCycle, jSmIn, jSmVcRate, jHold,
             jStOut, jStIn, jSample, jStVcRate,
             jCoupler, jHotCoupler };
    enum C { cSmRate, cStRate };

    SSG()
    {
        addJack ("SMOOTH OUT",  JackType::Out, JackColor::White); // 0..4-5 V
        addJack ("CYCLE",       JackType::Out, JackColor::Red);   // pulse each cycle; patch to SMOOTH IN to cycle
        addJack ("SMOOTH IN",   JackType::In,  JackColor::Black);
        addJack ("VC RATE",     JackType::In,  JackColor::White); // 1 V/oct, + = faster
        addJack ("HOLD",        JackType::In,  JackColor::Red);   // high = freeze (track & hold)
        addJack ("STEPPED OUT", JackType::Out, JackColor::White);
        addJack ("STEPPED IN",  JackType::In,  JackColor::Black);
        addJack ("SAMPLE",      JackType::In,  JackColor::Red);
        addJack ("ST VC RATE",  JackType::In,  JackColor::White); // VC step-size limit
        addJack ("COUPLER",     JackType::Out, JackColor::Red);   // 0/5 V comparator
        addJack ("HOT COUPLER", JackType::Out, JackColor::Grey);  // swings ≥ ±10 V

        addCtl ("SMOOTH RATE",  0.5659f); // expo 50 µs..10 s -> 50 ms
        addCtl ("STEPPED RATE", 0.2454f); // expo 50 µs..10 s -> 1 ms
        leds.resize (2);
    }

    void reseed() override { resetState(); } // no random state; deterministic reset

    void prepare (double sampleRate) override
    {
        Module::prepare (sampleRate);
        dt = 1.0 / sampleRate;
        resetState();
    }

    void tick() override
    {
        const double C = slew::spanC();

        // SMOOTH: bidirectional VC lag on the shared slew core (5 V per
        // SMOOTH RATE time); HOLD high freezes the integrator exactly.
        if (! isHigh (in (jHold)))
        {
            double budget = dt;
            const double R = C / slew::timeLaw (ctls[cSmRate].v, in (jSmVcRate));
            slew::advance (ySm, (double) in (jSmIn), R, budget, 0.0);
        }
        // CYCLE: Schmitt comparator on the smooth output — high from the
        // bottom (<= 0.05 V) until the output passes 4.75 V, then low until it
        // is back at the bottom. CYCLE -> SMOOTH IN therefore self-oscillates
        // as a 0..~4.75 V triangle with one CYCLE pulse per period.
        if (cycHigh && ySm >= kCycHi)        cycHigh = false;
        else if (! cycHigh && ySm <= kCycLo) cycHigh = true;

        // STEPPED: S&H on SAMPLE rising edges; the output is step-size (slew)
        // limited by STEPPED RATE x 2^(-ST VC RATE). Static without SAMPLE.
        if (sampleEdge.rising (in (jSample)))
            held = (double) in (jStIn);
        {
            double budget = dt;
            const double R = C / slew::timeLaw (ctls[cStRate].v, in (jStVcRate));
            slew::advance (ySt, held, R, budget, 0.0);
        }

        // COUPLER: comparator smooth > stepped (1 mV hysteresis against chatter).
        const double d = ySm - ySt;
        if (! cplHigh && d > kCplHyst)       cplHigh = true;
        else if (cplHigh && d < -kCplHyst)   cplHigh = false;

        jacks[jSmOut].value      = (float) ySm;
        jacks[jCycle].value      = cycHigh ? kCV : 0.0f;
        jacks[jStOut].value      = (float) ySt;
        jacks[jCoupler].value    = cplHigh ? kCV : 0.0f;
        jacks[jHotCoupler].value = cplHigh ? kHotV : -kHotV;
        leds[0] = std::clamp ((float) ySm * (1.0f / kCV), 0.0f, 1.0f);
        leds[1] = std::clamp ((float) ySt * (1.0f / kCV), 0.0f, 1.0f);
    }

private:
    static constexpr double kCycHi   = 4.75;  // V, CYCLE comparator upper threshold
    static constexpr double kCycLo   = 0.05;  // V, lower threshold
    static constexpr double kCplHyst = 0.001; // V
    static constexpr float  kHotV    = 10.5f; // hot coupler swing (>= +-10 V, inside rails)

    void resetState()
    {
        ySm = ySt = held = 0.0;
        cycHigh = true; cplHigh = false;
        sampleEdge.reset();
    }

    EdgeDetector sampleEdge;
    double ySm = 0.0, ySt = 0.0, held = 0.0, dt = 1.0 / 48000.0;
    bool   cycHigh = true, cplHigh = false;
};

} // namespace satk
