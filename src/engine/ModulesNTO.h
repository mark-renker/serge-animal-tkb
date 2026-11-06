// Serge Animal TKB v2 — NTO (New Timbral Oscillator).
// Jack/ctl registration FROZEN per docs/SPEC.md §4.1 (indices append-only
// forever). DSP (gates T02-T04, T17; dsp-techniques.md §1.1 hero tier):
//   - analytic triangle core = signed phase accumulator (through-zero linear FM
//     simply runs the core backwards), run at 2x the host rate;
//   - polyBLEP on the saw wrap (2x) and both pulse edges (base rate),
//     polyBLAMP on both triangle corners (DspUtils kernels, |inc| as dt so TZFM stays clean);
//   - tri -> sine via the sine shaper at exact drive (sin(pi/2*tri) is a
//     pure sine of the core phase) plus a gentle smooth odd compression as
//     the hardware-like residual (~0.25% THD, H3 only; T04 THD <= 2%; was an
//     under-driven shaper with a peak kink until 2026-11-05, LOG.md); VAR =
//     the same shaper from an exact-sine start (drive 1; an under-driven
//     0.94 with a peak kink until 2026-11-06), overdriven until it folds
//     into an 'M', then squared up toward a pulse (VC shape);
//   - SINE/TRI/SAW/VAR decimated by the low-latency IIR halfband (LOG 2026-11-02:
//     the NTO sits inside patchable sync/FM loops, keep loop latency small);
//   - SOFT sync = bounded phase nudge: on a SYNC edge the core is pulled a
//     fraction of the way toward its wrap point, only inside a capture window,
//     the correction spread over a few internal samples (no discontinuity, no
//     BLEP needed). Locks near 1:1, 2:1, 1:2 and refuses 20% detune (T04);
//   - analog layer (T17): static pitch + V/oct scale tolerance from
//     SplitMix64(moduleSeed); thermal wander = 3 summed OU processes on a
//     dedicated PCG32 stream at 1 kHz control rate, linearly interpolated and
//     soft-bounded. Depth = *driftDepth (OUT ANALOG DRIFT); nullptr or 0 =
//     ideal (T02 measures at 0).
// Fitted laws / interpretations are logged in gauntlet/LOG.md (NTO entry).
#pragma once
#include "EngineCore.h"

namespace satk {

// Triangle-core VCO: sine/tri/saw/variable + pulse outs, two calibrated 1V/oct
// inputs, attenuverted exponential FM, AC-coupled through-zero linear FM with
// VC index, VC waveshape (sine -> 'M'/pulse-like), SOFT sync.
class NTO : public Module
{
public:
    enum J { jSine, jTri, jSaw, jVar, jPulse,
             jOctA, jOctB, jVcFreq, jLinFm, jVcLin, jVcWave, jSync };
    enum C { cFreq, cFine, cWave, cVcFreqAmt, cLinDepth };

    NTO()
    {
        addJack ("SINE",    JackType::Out, JackColor::Black);
        addJack ("TRI",     JackType::Out, JackColor::Black);
        addJack ("SAW",     JackType::Out, JackColor::Black);
        addJack ("VAR",     JackType::Out, JackColor::Black);
        addJack ("PULSE",   JackType::Out, JackColor::Red);
        addJack ("1V/OCT A",JackType::In,  JackColor::White);
        addJack ("1V/OCT B",JackType::In,  JackColor::White);
        addJack ("VC FREQ", JackType::In,  JackColor::White);
        addJack ("LIN FM",  JackType::In,  JackColor::Black);
        addJack ("VC LIN",  JackType::In,  JackColor::White);
        addJack ("VC WAVE", JackType::In,  JackColor::White);
        addJack ("SYNC",    JackType::In,  JackColor::Purple);

        addCtl ("FREQ",        0.4045f); // expo 16 Hz..16 kHz -> 261.63 Hz
        addCtl ("FINE",        0.5f);    // ±100 cents, center = 0
        addCtl ("WAVESHAPE",   0.15f);   // 0..1, sums with VC WAVE
        addCtl ("VC FREQ AMT", 0.5f);    // attenuverter, full = ±0.4 oct/V
        addCtl ("LIN FM DEPTH",0.0f);    // 0..1, full = ±3·f0 per 5 V
        leds.resize (1);
    }

    // Global analog-layer depth (OUT ANALOG DRIFT ctl, 0..1). Wired by the
    // integration stage (System); nullptr = 0 = ideal/disabled.
    const float* driftDepth = nullptr;

    // Read-only probes (tests / UI): last instantaneous core frequency (Hz,
    // signed under TZFM) and the net number of completed core cycles.
    float instHz() const { return lastHz; }
    int64_t cycles() const { return wrapCount; }

    // Every random quantity derives from moduleSeed (SPEC §2). Idempotent for
    // a given seed: static tolerances via SplitMix64, drift excitation on its
    // own PCG32 stream, OU states started from their stationary distribution.
    void reseed() override
    {
        dsp::SplitMix64 tol (moduleSeed);
        tolCents = tol.nextBipolar() * kTolCents;
        tolScale = tol.nextBipolar() * kTolScaleCentsPerOct * (1.0f / 1200.0f);
        rngDrift.seed (moduleSeed, kStreamDrift);
        for (int i = 0; i < kNumOU; ++i)
        {
            ou[i].configure (kOuTau[i], kOuSigma[i], kCtrlHz);
            ou[i].reset (kOuSigma[i] * rngDrift.nextGauss());
        }
        wander = wanderTarget = sumOU();
        wanderStep = 0.0f;
        ctrlCount = 0;
    }

    void prepare (double sampleRate) override
    {
        Module::prepare (sampleRate);
        invFs2   = 1.0f / (2.0f * sr);
        sineNorm = 1.0f / (1.0f - kSineComp);
        ctrlN    = std::max (1, (int) std::lround (sr / kCtrlHz));
        smoothK  = 1.0f - std::exp (-1.0f / (0.002f * sr)); // ~2 ms knob lag (v1 smoothing)
        acK      = 1.0f - std::exp (-2.0f * kPiF * kLinFmHpHz / sr);
        reseed();

        phase = 0.0f; wrapCount = 0;
        nudgePer = 0.0f; nudgeLeft = 0;
        acState = 0.0f;
        syncArmed = false; syncPrev = 0.0f;
        freqCache = fineCache = -1.0f;
        refreshKnobs();
        octK = octTarget;
        shapeK = std::clamp (ctls[cWave].v, 0.0f, 1.0f);
        lastOct = 1.0e9f; f0 = 261.63f;
        fPrev = 0.0f; primed = false; lastHz = 0.0f;
        shapeCache = -1.0f;
        for (auto* d : { &dnSine, &dnTri, &dnSaw, &dnVar }) d->prepare();
    }

    void tick() override
    {
        // ---- knobs (smoothed), shape --------------------------------------
        refreshKnobs();
        octK   += smoothK * (octTarget - octK);
        shapeK += smoothK * (std::clamp (ctls[cWave].v, 0.0f, 1.0f) - shapeK);

        // ---- analog layer (control rate, interpolated) ----------------------
        if (--ctrlCount <= 0)
        {
            ctrlCount = ctrlN;
            for (auto& o : ou) o.tick (rngDrift);
            wanderTarget = sumOU();
            wanderStep = (wanderTarget - wander) / (float) ctrlN;
        }
        wander += wanderStep;
        const float depth = driftDepth != nullptr ? std::clamp (*driftDepth, 0.0f, 1.0f) : 0.0f;

        // ---- exponential frequency law (SPEC §4.1) ---------------------------
        const float octCv = in (jOctA) + in (jOctB);
        float oct = octK + dsp::av (ctls[cVcFreqAmt].v) * 0.4f * in (jVcFreq);
        if (depth > 0.0f)
        {
            const float w = depth * wander;
            oct += octCv * (1.0f + depth * tolScale)
                 + (depth * tolCents + kWanderLim * dsp::fastTanh (w * (1.0f / kWanderLim))) * (1.0f / 1200.0f);
        }
        else oct += octCv;
        if (differs (oct, lastOct)) { lastOct = oct; f0 = std::clamp (std::exp2 (oct), kMinHz, kMaxHz); }

        // ---- AC-coupled through-zero linear FM with VC index -----------------
        float lin = in (jLinFm);
        acState = flushDenorm (acState + acK * (lin - acState));
        lin -= acState;
        const float idx = std::clamp (ctls[cLinDepth].v + in (jVcLin) * (1.0f / kCV), 0.0f, 1.0f);
        const float f = std::clamp (f0 * (1.0f + lin * (3.0f / kCV) * idx), -kMaxHz, kMaxHz);
        if (! primed) { fPrev = f; primed = true; }
        lastHz = f;

        // ---- VC waveshape (v1 law: shape = clamp01(knob + V/5)) --------------
        setShape (std::clamp (shapeK + in (jVcWave) * (1.0f / kCV), 0.0f, 1.0f));

        // ---- SYNC edge (Schmitt, sub-sample half chosen by interpolation) ----
        const float sv = in (jSync);
        int syncAt = -1;
        if (! syncArmed) { if (sv < kSyncLo) syncArmed = true; }
        else if (sv > kSyncHi)
        {
            syncArmed = false;
            const float d = sv - syncPrev;
            const float tc = d > 1.0e-9f ? (kSyncHi - syncPrev) / d : 1.0f;
            syncAt = tc < 0.5f ? 0 : 1;
        }
        syncPrev = sv;

        // ---- 2x core, then low-latency halfband decimation -------------------
        Out a, b;
        incSum = 0.0f;
        step ((0.5f * (fPrev + f)) * invFs2, syncAt == 0, a);
        step (f * invFs2, syncAt == 1, b);
        fPrev = f;

        jacks[jSine].value  = flushDenorm (dnSine.process  (a.sine, b.sine));
        jacks[jTri].value   = flushDenorm (dnTri.process   (a.tri,  b.tri));
        jacks[jSaw].value   = flushDenorm (dnSaw.process   (a.saw,  b.saw));
        jacks[jVar].value   = flushDenorm (dnVar.process   (a.var,  b.var));
        // PULSE is a 0/5 V logic-grade (RED) output: polyBLEP at the BASE rate
        // gives a monotone edge ramp. Routed through the halfband instead, its
        // Gibbs ringing swung the jack to -1..+6 V (and clamping that ringing
        // cost ~20 dB of in-band alias floor), so it bypasses the 2x path.
        {
            const float p = phase, dtb = std::fabs (incSum);
            float q = p + 0.5f; if (q >= 1.0f) q -= 1.0f;
            const float pul = (p < 0.5f ? 1.0f : 0.0f)
                            + 0.5f * (dsp::polyBLEP (p, dtb) - dsp::polyBLEP (q, dtb));
            jacks[jPulse].value = kCV * pul;
        }
        leds[0] = phase < 0.5f ? 1.0f : 0.0f;
    }

private:
    // ---- constants (fitted/interpretive values logged in LOG.md) -------------
    static constexpr float kPiF       = 3.14159265f;
    static constexpr float kHalfPi    = 1.57079633f;
    static constexpr float kMinHz     = 0.05f;
    static constexpr float kMaxHz     = 40000.0f;  // > 32 kHz via CV (T02); 2x core inc <= 0.42
    static constexpr float kMaxInc    = 0.45f;     // per internal sample (polyBLEP needs < 0.5)
    static constexpr float kLinFmHpHz = 2.0f;      // LIN FM AC coupling corner
    static constexpr float kVarBase   = 1.0f;      // VAR shaper drive at shape 0: exact sine (r3; was 0.94, a kinked start)
    static constexpr float kSineComp  = 0.01f;     // SINE: gentle odd compression of the exact sine, ~0.25% THD
    static constexpr float kVarFold   = 2.0f;      // shaper drive at shape 1 (folds into 'M')
    static constexpr float kVarSquare = 6.0f;      // tanh squaring drive at shape 1
    // soft sync (bounded phase nudge)
    static constexpr float kSyncK     = 0.6f;      // fraction of the phase error removed per edge
    static constexpr float kSyncWin   = 0.25f;     // capture window (cycles either side of wrap)
    static constexpr int   kSyncSpread = 8;        // internal samples the nudge is spread over
    static constexpr float kSyncHi    = 1.0f;      // Schmitt thresholds (V): pulses AND audio
    static constexpr float kSyncLo    = 0.5f;
    // analog layer (at ANALOG DRIFT = 1; scaled by depth)
    static constexpr float kTolCents  = 2.0f;      // static pitch offset, uniform ±
    static constexpr float kTolScaleCentsPerOct = 0.3f; // residual V/oct trim error, uniform ±
    static constexpr float kWanderLim = 3.8f;      // soft bound on thermal wander (cents)
    static constexpr float kCtrlHz    = 1000.0f;
    static constexpr int   kNumOU     = 3;
    static constexpr float kOuTau[kNumOU]   = { 0.5f, 10.0f, 180.0f }; // seconds
    static constexpr float kOuSigma[kNumOU] = { 0.1f, 0.5f, 1.2f };    // cents
    static constexpr uint64_t kStreamDrift = 0x6e746f64726674ULL;     // "ntodrft"

    struct Out { float sine, tri, saw, var; };

    float sumOU() const { float s = 0.0f; for (auto& o : ou) s += o.value(); return s; }

    void refreshKnobs()
    {
        bool changed = false;
        if (differs (ctls[cFreq].v, freqCache))
        { freqCache = ctls[cFreq].v; knobOct = std::log2 (dsp::expmap (freqCache, 16.0f, 16000.0f)); changed = true; }
        if (differs (ctls[cFine].v, fineCache))
        { fineCache = ctls[cFine].v; fineOct = (std::clamp (fineCache, 0.0f, 1.0f) - 0.5f) * (200.0f / 1200.0f); changed = true; }
        if (changed) octTarget = knobOct + fineOct;
    }

    // shaper parameters for one base sample
    void setShape (float s)
    {
        if (! differs (s, shapeCache)) return;
        shapeCache = s;
        varG = kVarBase + (kVarFold - kVarBase) * s;
        varNorm = 1.0f / std::sin (kHalfPi * std::min (varG, 1.0f));
        varD = kVarSquare * s * s;
        varDNorm = varD > 1.0e-3f ? 1.0f / dsp::fastTanh (varD) : 1.0f;
    }

    // one internal (2x-rate) core step
    void step (float inc, bool syncEdge, Out& o)
    {
        if (syncEdge)
        {
            const float e = phase < 0.5f ? phase : phase - 1.0f; // signed distance to wrap
            if (std::fabs (e) < kSyncWin)
            {
                const float pending = nudgePer * (float) nudgeLeft;
                nudgePer  = (pending - kSyncK * e) * (1.0f / (float) kSyncSpread);
                nudgeLeft = kSyncSpread;
            }
        }
        if (nudgeLeft > 0) { inc += nudgePer; --nudgeLeft; }
        inc = std::clamp (inc, -kMaxInc, kMaxInc);
        incSum += inc;

        phase += inc;
        if (phase >= 1.0f)     { phase -= 1.0f; ++wrapCount; }
        else if (phase < 0.0f) { phase += 1.0f; --wrapCount; }
        const float p  = phase;
        const float dt = std::fabs (inc);
        float q = p + 0.5f; if (q >= 1.0f) q -= 1.0f;

        // saw: drops by 2 at the wrap
        const float saw = 2.0f * p - 1.0f - dsp::polyBLEP (p, dt);
        // triangle: -1 at p=0, +1 at p=0.5; corners rounded by polyBLAMP
        const float triRaw = p < 0.5f ? 4.0f * p - 1.0f : 3.0f - 4.0f * p;
        const float tri = triRaw + (dsp::polyBLAMP (p, dt) - dsp::polyBLAMP (q, dt)) * dt * 4.0f;
        // tri -> sine: sin(pi/2 * tri) is exactly a sine of the core phase
        // (the triangle is linear in phase), and the shaper's zero slope at
        // the corners makes the corner smooth, so it takes the UNROUNDED
        // triangle (the BLAMP rounding would leave a small bump at each peak).
        // The hardware-like residual is a gentle, smooth odd compression
        // (s - c*s^3: H3 only, compressive sign, no peak kink -> no long odd
        // series; LOG.md 2026-11-05).
        const float s0   = std::sin (kHalfPi * triRaw);
        const float sine = (s0 - kSineComp * s0 * s0 * s0) * sineNorm;
        // VAR: same shaper driven harder (folds into an 'M'), then squared up
        float var = std::sin (kHalfPi * varG * tri) * varNorm;
        if (varD > 1.0e-3f) var = dsp::fastTanh (varD * var) * varDNorm;

        o.sine  = kAudio * sine;
        o.tri   = kAudio * tri;
        o.saw   = kAudio * saw;
        o.var   = kAudio * var;
    }

    // ---- state ---------------------------------------------------------------
    float invFs2 = 1.0f / 96000.0f;
    float sineNorm = 1.0f;
    float phase = 0.0f;
    float incSum = 0.0f; // phase advanced this base sample (both substeps)
    int64_t wrapCount = 0;
    float nudgePer = 0.0f; int nudgeLeft = 0;
    float acState = 0.0f, acK = 0.0f;
    bool syncArmed = false; float syncPrev = 0.0f;
    float freqCache = -1.0f, fineCache = -1.0f;
    float knobOct = 8.031f, fineOct = 0.0f, octTarget = 8.031f, octK = 8.031f;
    float shapeK = 0.15f, smoothK = 0.01f;
    float lastOct = 1.0e9f, f0 = 261.63f, fPrev = 0.0f, lastHz = 0.0f;
    bool primed = false;
    float shapeCache = -1.0f, varG = 1.0f, varNorm = 1.0f, varD = 0.0f, varDNorm = 1.0f;
    // analog layer
    float tolCents = 0.0f, tolScale = 0.0f;
    dsp::OUDrift ou[kNumOU];
    dsp::PCG32 rngDrift;
    float wander = 0.0f, wanderTarget = 0.0f, wanderStep = 0.0f;
    int ctrlCount = 0, ctrlN = 48;
    // decimators (low-latency IIR halfband — sync/FM loops)
    dsp::IirDownsampler2x dnSine, dnTri, dnSaw, dnVar;
};

} // namespace satk
