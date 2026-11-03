// Serge Animal TKB v2 — TKB (Touch Activated Keyboard Sequencer) and KEY/MIDI.
// Jack/ctl registration FROZEN per docs/SPEC.md §4.10-§4.11 (indices
// append-only forever). Stage-2 implementation against gate T12 (+ the KEY
// GLIDE piece of the T15 MIDI contract), hero tier per
// gauntlet/research/dsp-techniques.md §1.8:
//   - full 16-stage × 4-row logic, RESET -> LAST-TOUCHED stage, RANDOM select
//     (PCG32 stream from moduleSeed), vertical row sequencer on ABCD, HOLD,
//     UP/DOWN, one-hot stage outs;
//   - KV/PRESSURE/KP follow the PADS, not the running stage (hardware truth);
//     KV factory-calibrated to semitones: (pad-1)/12 V, GLIDE one-pole slew;
//   - stage-transition slew ~30 µs on the CV outs + sub-sample edge-time
//     dither (±0.1 samples, CD4000 propagation jitter) so audio-rate clocking
//     doesn't produce naked correlated steps;
//   - per-stage static resistor-ladder tolerances (gain ±0.25%, offset ±3 mV)
//     drawn deterministically from moduleSeed (T14/T17 determinism);
//   - touch: contact-bounce jitter (0..2 samples, one RNG draw per touch) on
//     KP, finite ~2 ms finger-contact slew on PRESSURE.
#pragma once
#include "EngineCore.h"

namespace satk {

// 16 stages x 4 knob rows + 16 touch pads (engine-visible momentary ctls) +
// pressure. Sequential clock 1..LENGTH with wrap; RESET -> LAST-TOUCHED stage;
// RANDOM armed + pulse -> random stage within LENGTH; vertical clock cycles
// rows A->B->C->D on the ABCD output (64-step sequences); HOLD freezes;
// UP/DOWN reverses. KV = last-touched pad, semitone-calibrated, GLIDE slewed.
class TKB : public Module
{
public:
    enum J { jClock, jVertClock, jReset, jVertReset, jRandom, jUpDown, jHold,
             jA, jB, jC, jD, jAbcd, jKv, jPressure, jGate, jKp,
             jStage1 /* ..jStage1+15 = ST 1..ST 16 (one-hot stage levels) */ };
    enum C { cRun, cRate, cLength, cRandom, cGateLen, cRange, cGlide,
             cRowA1 = 7   /* ..cRowA1+15  */,
             cRowB1 = 23  /* ..cRowB1+15  */,
             cRowC1 = 39  /* ..cRowC1+15  */,
             cRowD1 = 55  /* ..cRowD1+15  */,
             cPad1  = 71  /* ..cPad1+15   */,
             cPressure = 87 };

    TKB()
    {
        addJack ("CLOCK",      JackType::In,  JackColor::Red);
        addJack ("VERT CLOCK", JackType::In,  JackColor::Red);
        addJack ("RESET",      JackType::In,  JackColor::Red);   // -> last-touched stage
        addJack ("VERT RESET", JackType::In,  JackColor::Red);
        addJack ("RANDOM",     JackType::In,  JackColor::Red);   // ORs with RANDOM ctl
        addJack ("UP/DOWN",    JackType::In,  JackColor::Red);   // high = step down
        addJack ("HOLD",       JackType::In,  JackColor::Red);   // high = freeze
        addJack ("A",          JackType::Out, JackColor::White); // active stage, row A (x RANGE)
        addJack ("B",          JackType::Out, JackColor::White);
        addJack ("C",          JackType::Out, JackColor::White);
        addJack ("D",          JackType::Out, JackColor::White);
        addJack ("ABCD",       JackType::Out, JackColor::White); // active row x active stage
        addJack ("KV",         JackType::Out, JackColor::White); // last-touched pad, semitones, GLIDE slew
        addJack ("PRESSURE",   JackType::Out, JackColor::White); // 0..5 V
        addJack ("GATE",       JackType::Out, JackColor::Red);   // GATE LEN gate per step (v1 semantics)
        addJack ("KP",         JackType::Out, JackColor::Red);   // key pulse: high while a pad is touched
        for (int i = 0; i < 16; ++i)
            addJack (kStageNames[i], JackType::Out, JackColor::Red); // one-hot stage levels

        addCtl ("RUN",      0.0f, 2, kOffOn);
        addCtl ("RATE",     0.5252f);           // expo 0.1..30 Hz -> 2 Hz
        addCtl ("LENGTH",   7.0f, 16, kLengths); // index 7 = "8" (v1-ism, logged)
        addCtl ("RANDOM",   0.0f, 2, kOffOn);
        addCtl ("GATE LEN", 0.5654f);           // expo 5 ms..1 s -> 100 ms
        addCtl ("RANGE",    0.3333f);           // linear 0.5..5 oct -> 2 oct (row scaling, v1-ism)
        addCtl ("GLIDE",    0.0f);              // 0..1 s slew on KV
        for (int i = 0; i < 64; ++i) addCtl (kRowNames[i], 0.5f);          // 0..1 -> 0..5 V
        for (int i = 0; i < 16; ++i) addCtl (kPadNames[i], 0.0f, 2, kOffOn); // momentary pads
        addCtl ("PRESSURE", 0.0f);              // 0..1 -> 0..5 V, written by UI / aftertouch
        leds.resize (20); // 16 stage LEDs + 4 row LEDs
    }

    // Every random quantity derives from moduleSeed (SPEC §2): static ladder
    // tolerances via SplitMix64, runtime streams via PCG32 with DISTINCT
    // stream ids (random-select never perturbed by jitter draws).
    void reseed() override
    {
        dsp::SplitMix64 tol (moduleSeed);
        for (int r = 0; r < 4; ++r)
            for (int s = 0; s < 16; ++s)
            {
                gainTol[r][s] = tol.tolerance (0.0025f);       // ±0.25% ladder gain
                offTol[r][s]  = tol.nextBipolar() * 0.003f;    // ±3 mV switch/buffer offset
            }
        rngSel.seed (moduleSeed, 0x746b6273656cULL); // "tkbsel": RANDOM stage select
        rngJit.seed (moduleSeed, 0x746b626a6974ULL); // "tkbjit": edge dither + contact bounce
    }

    void prepare (double sampleRate) override
    {
        Module::prepare (sampleRate);
        stage = 0; row = 0; lastTouched = 0;
        clockPhase = 0.0f; rateCache = -1.0f; rateHz = 2.0f;
        gateRemain = 0; kpDelay = 0;
        for (auto& p : prevPad) p = false;
        edClock.reset(); edVert.reset(); edReset.reset(); edVertReset.reset();
        for (auto& s : cv) s = 0.0f;
        kvState = 0.0f; pressState = 0.0f; glideCache = -1.0f; aGlide = 0.0f;
        aSlew  = std::exp (-1.0f / (sr * 30.0e-6f)); // ~30 µs stage-transition slew
        aPress = std::exp (-1.0f / (sr * 2.0e-3f));  // ~2 ms finger-contact slew
    }

    void tick() override
    {
        const int   len    = std::clamp ((int) std::lround (ctls[cLength].v), 0, 15) + 1;
        const float rangeV = dsp::linmap (ctls[cRange].v, 0.5f, 5.0f);

        // CV targets BEFORE this sample's events (old stage/row) — needed for
        // the two-segment sub-sample transition update below.
        float oldTgt[5];
        targets (oldTgt, rangeV);

        const bool hold  = isHigh (in (jHold));
        const bool down  = isHigh (in (jUpDown));
        const bool armed = ctls[cRandom].v >= 0.5f || isHigh (in (jRandom));

        // internal clock (RUN + RATE), ORed with external CLOCK edges
        bool clockEdge = edClock.rising (in (jClock));
        if (ctls[cRun].v >= 0.5f)
        {
            if (differs (ctls[cRate].v, rateCache))
            {
                rateCache = ctls[cRate].v;
                rateHz = dsp::expmap (rateCache, 0.1f, 30.0f);
            }
            clockPhase += rateHz / sr;
            if (clockPhase >= 1.0f) { clockPhase -= 1.0f; clockEdge = true; }
        }
        const bool vertEdge      = edVert.rising (in (jVertClock));
        const bool resetEdge     = edReset.rising (in (jReset));
        const bool vertResetEdge = edVertReset.rising (in (jVertReset));

        // touch pads: KV/KP/PRESSURE follow the PADS, not the running stage
        // (hardware truth); a touch also selects its stage (logged interp. #4)
        bool anyPad = false; int touched = -1;
        for (int i = 0; i < 16; ++i)
        {
            const bool dn = ctls[(size_t) (cPad1 + i)].v >= 0.5f;
            if (dn) anyPad = true;
            if (dn && ! prevPad[i]) touched = i;
            prevPad[i] = dn;
        }

        bool stepped = false;
        if (touched >= 0)
        {
            lastTouched = touched;
            stage = touched;
            stepped = true;
            kpDelay = (int) (rngJit.nextFloat01() * 3.0f); // contact bounce, 0..2 samples
        }
        if (resetEdge) // RESET -> LAST-TOUCHED stage (not stage 1); random if armed
        {
            stage = armed ? randStage (len) : lastTouched;
            stepped = true;
        }
        if (clockEdge && ! hold)
        {
            if (armed)
                stage = randStage (len);
            else
            {
                int next = down ? stage - 1 : stage + 1; // UP/DOWN high = step down
                if (next >= len) next = 0;               // wrap LENGTH -> 1
                if (next < 0)    next = len - 1;         // wrap 1 -> LENGTH
                stage = next;
            }
            stepped = true;
        }
        if (vertResetEdge)        row = 0;               // VERT RESET -> row A
        if (vertEdge && ! hold)   row = (row + 1) & 3;   // A->B->C->D->A

        if (stepped) // GATE LEN gate on each step event (v1 semantics)
            gateRemain = (int) (dsp::expmap (ctls[cGateLen].v, 0.005f, 1.0f) * sr);

        // ---- CV outs: ~30 µs transition slew, sub-sample edge dither -------
        float tgt[5];
        targets (tgt, rangeV);
        bool moved = false;
        for (int k = 0; k < 5; ++k) if (differs (tgt[k], oldTgt[k])) { moved = true; break; }
        if (moved)
        {
            // edge occurs at fractional time t0 within this sample (dither
            // ±0.1 smp — propagation-jitter decorrelation when audio-clocked)
            const float t0 = std::clamp (0.5f + 0.1f * rngJit.nextBipolar(), 0.05f, 0.95f);
            const float a1 = std::pow (aSlew, t0);
            const float a2 = std::pow (aSlew, 1.0f - t0);
            for (int k = 0; k < 5; ++k)
            {
                const float mid = oldTgt[k] + (cv[k] - oldTgt[k]) * a1;
                cv[k] = tgt[k] + (mid - tgt[k]) * a2;
            }
        }
        else
        {
            for (int k = 0; k < 5; ++k) cv[k] = tgt[k] + (cv[k] - tgt[k]) * aSlew;
        }
        for (int k = 0; k < 5; ++k) // snap when settled (denormal-free path)
            if (std::fabs (cv[k] - tgt[k]) < 1.0e-5f) cv[k] = tgt[k];

        // ---- KV: last-touched pad, SEMITONE calibrated: (pad-1)/12 V -------
        const float kvTgt = (float) lastTouched * (1.0f / 12.0f);
        const float g = ctls[cGlide].v;
        if (differs (g, glideCache)) // GLIDE = one-pole slew, tau = GLIDE * 1.0 s
        {
            glideCache = g;
            aGlide = g <= 0.0f ? 0.0f : std::exp (-1.0f / (sr * g));
        }
        kvState = kvTgt + (kvState - kvTgt) * aGlide;
        if (std::fabs (kvState - kvTgt) < 1.0e-6f) kvState = kvTgt;

        // ---- PRESSURE: 0..5 V while touched (ctl x 5 V), finite rise -------
        const float pTgt = anyPad ? std::clamp (ctls[cPressure].v, 0.0f, 1.0f) * kCV : 0.0f;
        pressState = pTgt + (pressState - pTgt) * aPress;
        if (std::fabs (pressState - pTgt) < 1.0e-4f) pressState = pTgt;

        // ---- KP: high while any pad touched (after contact-bounce delay) ---
        float kp = 0.0f;
        if (anyPad) { if (kpDelay > 0) --kpDelay; else kp = kCV; }

        float gate = 0.0f;
        if (gateRemain > 0) { --gateRemain; gate = kCV; }

        jacks[jA].value        = cv[0];
        jacks[jB].value        = cv[1];
        jacks[jC].value        = cv[2];
        jacks[jD].value        = cv[3];
        jacks[jAbcd].value     = cv[4];
        jacks[jKv].value       = kvState;
        jacks[jPressure].value = pressState;
        jacks[jGate].value     = gate;
        jacks[jKp].value       = kp;
        for (int i = 0; i < 16; ++i)
        {
            jacks[(size_t) (jStage1 + i)].value = i == stage ? kCV : 0.0f; // one-hot levels
            leds[(size_t) i] = i == stage ? 1.0f : 0.0f;
        }
        for (int r = 0; r < 4; ++r)
            leds[(size_t) (16 + r)] = r == row ? 1.0f : 0.0f;
    }

private:
    // active-stage knob voltage with the per-stage static ladder tolerance
    float knobV (int r, int s, float rangeV) const
    {
        return ctls[(size_t) (cRowA1 + 16 * r + s)].v * rangeV * gainTol[r][s] + offTol[r][s];
    }
    void targets (float* t, float rangeV) const
    {
        t[0] = knobV (0, stage, rangeV);
        t[1] = knobV (1, stage, rangeV);
        t[2] = knobV (2, stage, rangeV);
        t[3] = knobV (3, stage, rangeV);
        t[4] = knobV (row, stage, rangeV); // ABCD = active row x active stage
    }
    int randStage (int len)
    {
        return (int) (((uint64_t) rngSel.next() * (uint64_t) len) >> 32); // uniform 0..len-1
    }

    // runtime state (reset in prepare; all randomness derives from moduleSeed)
    int   stage = 0, row = 0, lastTouched = 0;
    float clockPhase = 0.0f, rateHz = 2.0f, rateCache = -1.0f;
    int   gateRemain = 0, kpDelay = 0;
    bool  prevPad[16] = {};
    EdgeDetector edClock, edVert, edReset, edVertReset;
    float cv[5] = {};                    // A,B,C,D,ABCD slew states
    float kvState = 0.0f, pressState = 0.0f;
    float aSlew = 0.5f, aPress = 0.99f, aGlide = 0.0f, glideCache = -1.0f;
    float gainTol[4][16] = {};
    float offTol[4][16] = {};
    dsp::PCG32 rngSel, rngJit;

    static constexpr const char* kOffOn[2] = { "OFF", "ON" };
    static constexpr const char* kLengths[16] =
        { "1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16" };
    static constexpr const char* kStageNames[16] =
        { "ST 1","ST 2","ST 3","ST 4","ST 5","ST 6","ST 7","ST 8",
          "ST 9","ST 10","ST 11","ST 12","ST 13","ST 14","ST 15","ST 16" };
    static constexpr const char* kPadNames[16] =
        { "PAD 1","PAD 2","PAD 3","PAD 4","PAD 5","PAD 6","PAD 7","PAD 8",
          "PAD 9","PAD 10","PAD 11","PAD 12","PAD 13","PAD 14","PAD 15","PAD 16" };
    static constexpr const char* kRowNames[64] =
        { "A1","A2","A3","A4","A5","A6","A7","A8","A9","A10","A11","A12","A13","A14","A15","A16",
          "B1","B2","B3","B4","B5","B6","B7","B8","B9","B10","B11","B12","B13","B14","B15","B16",
          "C1","C2","C3","C4","C5","C6","C7","C8","C9","C10","C11","C12","C13","C14","C15","C16",
          "D1","D2","D3","D4","D5","D6","D7","D8","D9","D10","D11","D12","D13","D14","D15","D16" };
};

// KEY: the MIDI bridge (mono, last-note priority — plugin layer writes the
// fields each sample). KV is 1 V/oct with note 60 (C4) = 0 V exactly; VEL
// holds the last note-on velocity (init 4.0 V = v1's default gain 0.8) and
// hard-scales the OUTPUT MIXER's final mix (v1 contract, wired in System).
// GLIDE = v1 law: one-pole portamento on KV, tau = max(0.1 ms, ctl * 0.5 s).
class KEY : public Module
{
public:
    enum J { jKv, jGate, jVel, jClk };
    enum C { cGlide };

    KEY()
    {
        addJack ("KV",   JackType::Out, JackColor::White); // 1 V/oct, C4 = 0 V (bipolar)
        addJack ("GATE", JackType::Out, JackColor::Red);
        addJack ("VEL",  JackType::Out, JackColor::White); // 0..5 V, holds last velocity
        addJack ("CLK",  JackType::Out, JackColor::Red);   // host-transport 16th pulses

        addCtl ("GLIDE", 0.0f); // 0..0.5 s portamento on KV
        leds.resize (1);
    }

    void prepare (double sampleRate) override
    {
        Module::prepare (sampleRate);
        kv = pitchV;
        glideCache = -1.0f; aGlide = 0.0f;
    }
    void tick() override
    {
        const float g = ctls[cGlide].v;
        if (differs (g, glideCache)) // v1 law: tau = max(glide seconds, 0.1 ms)
        {
            glideCache = g;
            const float tau = std::max (1.0e-4f, g * 0.5f);
            aGlide = std::exp (-1.0f / (sr * tau));
        }
        kv = pitchV + (kv - pitchV) * aGlide;
        if (std::fabs (kv - pitchV) < 1.0e-6f) kv = pitchV; // snap (denormal-free)

        jacks[jKv].value   = kv;
        jacks[jGate].value = gateV;
        jacks[jVel].value  = velV;
        jacks[jClk].value  = clkV;
        leds[0] = gateV > kLogicHi ? 1.0f : 0.0f;
    }

    // written by the plugin layer (or tests) each sample:
    float pitchV = 0.0f, gateV = 0.0f, velV = 4.0f, clkV = 0.0f;

private:
    float kv = 0.0f, glideCache = -1.0f, aGlide = 0.0f;
};

} // namespace satk
