# GAUNTLET — Serge Animal TKB v2 (FROZEN 2026-08-31, pre-implementation, rev B)

Variant: **Verifiable Gauntlet** (gauntlet-loop skill), composed with
benchmark-loop phasing (foundation freeze before fan-out). Rev B incorporates
the pre-freeze adversarial design review (3 critics; amendments logged in
LOG.md). This file is frozen before the first line of v2 implementation code.
Checks may be strengthened, never weakened; every change is logged in LOG.md
with a reason.

## Goal

Rebuild **SergeAnimalTKB** — currently a Faust/faust2juce plugin with a
menu-based 30×31×2 patch matrix and auto-generated UI — as a hand-written
C++/JUCE plugin on the Paperface 50 framework (`~/dev/paperface50-emulation`):

1. **UI/UX**: programmatic Serge cream-panel UI in the PF50 house style with
   real banana-cable drag patching, PLUS three UX capabilities PF50 lacks:
   a visible **zoom control**, **hover tooltips** on every knob/jack/button/pad
   describing its function, and (retained/improved from PF50) smooth
   **window resizing**.
2. **DSP**: upgrade every module from naive behavioral (v1) to circuit-informed
   modeling per `gauntlet/research/dsp-techniques.md`, with hardware behavior
   targets from `gauntlet/research/hardware-spec.md`. Accuracy tier per module
   is implementation freedom; the gates below are the contract.
3. **Identity preserved**: AU `aumu` / subtype `Satk` / manufacturer `Rnky`,
   name "Serge Animal TKB", bundle `com.renkydenk.SergeAnimalTKB`, AU + VST3 +
   Standalone, arm64 required. v1's module complement is kept (NTO, DUSG×2,
   SSG, VCFQ, VCM, RING, Noise, RVG, TKB, KEY/MIDI, VCA×2, output mixer with
   **monitor bus**) — each modeled on its closest real circuit.
4. Installed to `~/Library/Audio/Plug-Ins/{Components,VST3}` for Mark to test.

Research inputs (committed, part of this freeze): `gauntlet/research/*.md`.

**Sequencing constraint (identity collision):** v2 replaces v1 at the same AU
identity. ALL v1 reference renders for the sound bar are produced **offline
from the legacy Faust sources (`legacy/render.cpp` path — no installed AU
involved) before the first v2 install**, stored in `gauntlet/renders/v1/`.

## Hard gates (all must pass; executable)

- **G1 build** — `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j`
  builds AU + VST3 + Standalone + `satk_gauntlet` (headless DSP tests) +
  `satk_uxtests` (JUCE-linked UI tests) + `satk_snapshot` (headless editor→PNG)
  with **zero warnings in our sources**.

- **G2 dsp-suite** — `satk_gauntlet` (PF50 harness pattern: no JUCE, drives
  `System` directly; per-module test files + registry runner), all green.
  Engine determinism substrate for the suite: the engine takes an explicit
  **instance seed**; analog tolerance/drift derive only from it.
  - **T01 silence**: no cables → exact 0.0 output; master path denormal-free.
  - **T02 NTO tracking**: 1V/oct **interval** error (ratio between test
    voltages; offset cancels) ≤ 3 cents across −3..+3 V around C4 = 261.63 Hz,
    drift disabled; amplitude within 1 dB of nominal and correct waveform at
    both 16 Hz and 16 kHz knob extremes; via CV the oscillator reaches
    ≥ 32 kHz with finite, non-wrapped output.
  - **T03 NTO band-limiting**: saw output, f0 = 1975 Hz @ 48 kHz — strongest
    non-harmonic component ≤ **−60 dBc** in 0–6 kHz AND ≤ **−30 dBc** over the
    full 0–24 kHz band (harmonic bins excluded ± 20 Hz). Through-zero linear
    FM crossing zero: no NaN, bounded, spectrum symmetric about the carrier
    for symmetric modulation.
  - **T04 NTO shape & soft sync**: waveshape CV swept over ≥ 8 steps →
    spectral centroid monotonically increases sine→'M'/pulse-like; at minimum
    CV, THD ≤ 2% (near-pure sine). Soft sync: slave detuned within ±5% of a
    1:1, 2:1, or 1:2 ratio locks to that integer relation within 100 ms;
    slave detuned 20% off any simple ratio must **NOT** lock (this is what
    makes it soft, not hard, sync).
  - **T05 DUSG**: (a) timing: rise/fall each within ±10% of dialed over the
    10 ms–5 s span; times extend below 1 ms at knob+CV extremes. (b) rate CV
    1V/oct (time halves per volt) within 3%. (c) cycle mode reaches ≥ 4 kHz.
    (d) slope shape per hardware: the mid 80% of a triggered rise deviates
    from a straight line by ≤ 3% of full scale (linear-by-design), with
    monotonic, non-overshooting approach to the target (tanh current-limit
    soft landing); curved expo/log shapes are achieved by feedback patching
    OUT→rate CV, and such a patch must measurably bend the slope.
    (e) **END/GATE is a level, not a pulse** (Ken Stone doc): high during
    rise and whenever OUT is at bottom, low during fall; in cycle mode duty
    tracks rise/(rise+fall) over 5–95%. (f) non-retriggerable during a
    transient. (g) gate into signal IN produces ASR sustain overriding TRIG.
  - **T06 SSG**: smooth section cycles 0→4–5 V triangle, rate < 1 Hz–4 kHz;
    CYCLE jack emits a pulse each cycle (doubles as pulse out); HOLD freezes
    exactly (track-and-hold); stepped side = S&H with VC step-size limiting,
    static without SAMPLE pulses; coupler comparator output 0/5 V; **hot**
    coupler output swings ≥ ±10 V; smooth self-patch self-oscillates.
  - **T07 VCFQ**: LP slope 12 ± 2.5 dB/oct (1→3 octaves above fc);
    simultaneous LP/BP/HP/**Notch** outputs correct; resonance compensation
    applies to the dedicated **AGC input only** — the standard GAIN input is
    deliberately uncompensated; compensation is monotonic in Q and output
    never grows unbounded (fitted law logged in LOG.md); **ping**: pulse to
    trigger → damped sine at fc, decay time monotonically increasing with Q;
    self-oscillation via BP→IN patch stable and amplitude-limited (integrator
    saturation), never NaN/Inf.
  - **T08 VCM** (measurement conditions: bias centered, tolerance layer
    disabled, sine input at stated depths): middle section at max fold →
    ≥ 5 significant odd harmonics (each ≥ −40 dBc), odd:even energy ratio
    ≥ 12 dB, AND spectral centroid monotonically increasing over ≥ 8 fold-CV
    steps, AND measured static transfer curve shows ≥ 2 slope-sign reversals
    (it must FOLD, not saturate); bottom section at full depth → even
    harmonics dominate (≥ 12 dB) and the second "squared-up" PWM-like output
    is present and distinct; top section = monotonic VC rounded clipping in
    HI mode and **linear VCA in LO mode** (gain linear in CV, r² > 0.99);
    anti-aliasing: folded 1 kHz sine @ 48 kHz, aliased components ≤ −50 dBc.
  - **T09 RING** (four-quadrant transconductance model per R6/CGS/R9 — NOT a
    diode ring): carrier suppression ≥ 55 dB at nominal trims with sidebands
    dominant; feedthrough trim reaches ≤ −50 dB when maxed (bleed is a
    feature, defaulted subtle: −60..−80 dB band); VC morph dry→AM→ring
    monotonic; **DC carrier → clean VCA/AM behavior** (v1 bonus-VCA use and
    demo4 depend on it).
  - **T10 noise/RVG**: white ≈ flat (±6 dB trend 100 Hz–6.4 kHz), pink falls
    3 ± 1.5 dB/oct; S/H source bounded 0..5 V with no autocorrelation peak
    > 0.5 beyond 10 ms lag over 5 s; RVG stepped changes only on trigger,
    smooth output is the slewed version.
  - **T11 VCA**: exponential law ≈ 12 dB/V over the top of the range within
    ±1.5 dB; ≤ −60 dB below 0.5 V CV; max gain +6 dB ± 1 dB; click bound:
    unity 1 kHz sine, CV stepped 0→5 V → energy in 5–20 kHz within the
    following 50 ms ≤ −35 dBFS.
  - **T12 TKB**: clock advances stages 1→LENGTH and wraps (LENGTH knob is a
    v1-ism, logged); RESET jumps to the **last-touched** stage (not stage 1);
    RANDOM armed + pulse → random stage within LENGTH; vertical clock cycles
    rows A→B→C→D on the ABCD output (64-step sequences); HOLD freezes;
    UP/DOWN reverses; pad touch (a ctl, engine-visible) emits gate + pressure
    CV and selects its stage; **KV output = last-touched pad, semitone
    calibrated** (distinct from row-A stage CV), with GLIDE slew ctl
    (interpretation of R*S row-B glide, logged); per-stage pulse outs one-hot.
  - **T13 feedback stress**: pathological multi-module feedback patch, 10 s
    @ 48 kHz: no NaN/Inf, buses within ±12 V rails, output bounded.
  - **T14 engine state round-trip**: engine-level state (ctls + cables +
    **instance seed**) serialized; TWO fresh engines constructed from the same
    blob render **bit-identical** first 512 samples (drift phase derives
    deterministically from seed; stream positions are not serialized).
  - **T15 default patch & monitor**: fresh engine with default cables, MIDI
    note 60 → output RMS ≥ 0.02 within 1 s; note-off → decays below −60 dBFS;
    removing the DUSG1 trigger cable → exact 0.0; velocity scales the final
    mix (v1 contract); **monitor bus**: selecting any source on the monitor
    selector makes it audible at the output.
  - **T16 perf**: full default patch, all modules ticking, Release build,
    ≥ 10 s render, minimum of ≥ 5 runs (cycles/sample via mach time, plugged
    in): ≥ **6× realtime** single-thread at 48 kHz on the M2 Max (≤ 16.7% of
    one P-core). Working target 8×; 6× is the gate. Foundation includes a
    perf spike with representative per-module stub costs before module
    algorithm tiers are chosen.
  - **T17 analog layer**: same instance seed → bit-identical renders; with
    drift enabled, NTO 1V/oct **interval** error still ≤ 4 cents and absolute
    pitch wander bounded within ±6 cents over 10 minutes; different seeds →
    measurably different tolerance offsets.

- **G2u ux-suite** — `satk_uxtests` (JUCE-linked, headless; requires a
  logged-in GUI session and JUCE_MODAL_LOOPS_PERMITTED=1 — documented
  constraint, not a defect to "fix"):
  - U01: editor resizes via constrainer over at least 0.5×–2.4× base size,
    aspect-locked; the **persisted value is the actual editor scale** (a
    float).
  - U02: zoom control (UI-visible) sets discrete scale steps by writing that
    same persisted scale; a drag-resize updates it too (the two mechanisms
    share one source of truth); the scale survives a full plugin
    getStateInformation/setStateInformation round-trip, as do cables + params
    (plugin-level state test lives HERE, not in G2).
  - U03: **every** interactive widget (knob, jack, button, toggle, TKB pad,
    menu) returns a tooltip that is non-empty and ≥ 15 characters
    (executable part); tooltip *quality* (does it describe the function?) is
    judged by the UI critic; a shared TooltipWindow is installed.
  - U04: `satk_snapshot` renders the full editor to PNG headlessly.
- **G3 auval** — `auval -v aumu Satk Rnky` exits PASS.
- **G4 pluginval** — pluginval **1.0.4** (`/Applications/pluginval.app`),
  `--strictness-level 10`, run on **both** the VST3 and the AU: 0 failures.
- **G5 install** — fresh `.component` + `.vst3` in
  `~/Library/Audio/Plug-Ins/{Components,VST3}` with valid bundle structure.

## Soft bar (fresh critics, criteria-blind rubric)

Critics receive artifacts, references, and the rubric — **no version labels,
no round numbers, no build narrative**. Each round may include a negative
control (a deliberately degraded artifact); a critic that fails to flag it
voids that round's verdict.

**UI bar.** Artifacts: `satk_snapshot` PNG(s).
References: (A) PF50 `docs/ui_preview.png` (same-medium software render);
(B) `gauntlet/reference/` hardware photos (STS Animal, La Bestia II/III, R*S
TKB) for layout truth.
Rubric — score 1–5 per dimension, ranked: (1) PF50 family cohesion (cream
panel, rosette knobs, banana jacks, lettering — "same developer" test vs
reference A), (2) module layout/grouping fidelity to the Animal + TKB
hardware (vs reference B), (3) jack color code and knob/pad shapes, (4) TKB
4×16 pad/knob field readability, (5) labels/lettering, (6) discoverability of
zoom/resize/tooltip affordances.
**Exit**: mean ≥ 4.0, no dimension < 3, across two consecutive fresh critics;
each round's critic names the 2–3 largest gaps with pixel evidence.

**Sound bar.** Decided primarily by **pre-registered measurement bands**
(v2 render must land in band; v1 lands out of band — measured, not vibes):
odd-harmonic count/ratio + transfer folding (T08 numbers) for the VCM mid;
even-dominance for the VCM bottom; ping decay-vs-Q monotonic law (T07);
DUSG mid-slope linearity band + END duty law (T05); ring carrier suppression
55–85 dB with morph monotonicity (T09); NTO alias floors (T03).
Renders: deterministic offline patches rendered by v2 vs `gauntlet/renders/v1/`
(pre-rendered before first install). A fresh critic receives unlabeled audio
+ spectrograms + the measurement table and the hardware-spec excerpts, scores
each dimension "closer to documented hardware: A / B / tie" — this is the
tiebreaker and gap-namer, not the decider.
**Exit**: all pre-registered bands met, and the critic scores v2 closer-or-tie
on every targeted dimension with no new in-scope gap.

## Out of scope (declared now, not negotiated later)

- Session/state compatibility with the Faust v1 plugin (accepted break, v2.0.0).
- Matching the real STS panel's PCO/DTG/UAP complement where v1 already
  diverged — v2 keeps v1's instrument identity; each module is modeled on its
  closest real circuit (interpretations logged in LOG.md).
- Hardware-only properties: touch-plate capacitance curves, oven warm-up,
  engraving, banana ground physics.
- TKB KEYS/KEYBOARD independence switch; host tempo sync; MPE/poly voices
  (candidates for v2.1, logged not built).

## Stop conditions

- **Win** — all hard gates green + both soft-bar exits met.
- **Per-piece cap** — 5 judge rounds on one piece → pause and report to Mark.
- **Diminishing returns** — two rounds with unchanged named gaps → pause.
- **Gate dispute** — a hard gate is believed impossible, ambiguous, or
  self-contradictory in light of evidence: halt work on that gate, write the
  evidence to LOG.md, report to Mark; amendment requires Mark. (Gates are
  never silently weakened.)
- **Global cap** — 25 total judge rounds across all pieces → pause and report.
- **Wrong bar** — out-of-scope critic findings are logged, never silently
  adopted; re-scoping requires Mark.
