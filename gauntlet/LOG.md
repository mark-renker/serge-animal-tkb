# Gauntlet round log — Serge Animal TKB v2 (append-only)

## 2026-08-31 — Phase 0/1: research + spec

- Research fan-out (4 agents) → `research/*.md`. Notable correction: no
  "Random*Source Animal" product exists; the Animal is an STS shop panel and
  R*S's equivalent is La Bestia II/III. v2 keeps v1's module identity and
  models each module on its closest real circuit (see GAUNTLET.md
  out-of-scope).
- Toolchain validated: PF50 baseline builds clean on this machine (Xcode 26.6,
  JUCE 8.0.6 at ~/JUCE, CMake 4.0.1) and its 73-assertion suite passes 73/73.
  pluginval installed.
- GAUNTLET.md drafted; sent to a 3-critic adversarial design review
  (feasibility / DSP-accuracy / gauntlet-mechanics) BEFORE freeze.

### Pre-freeze amendments (rev A → rev B), all from the design review

1. **T05 rewritten** — rev A froze two anti-hardware behaviors: unconditional
   tanh curvature (hardware slopes are linear by design; tanh is only the
   soft landing) and END as a 4 ms pulse (hardware END/GATE is a level:
   high in rise + at bottom, 5–95% duty in cycle). Also resolved the
   1 ms-floor vs 4 kHz-cycle arithmetic contradiction (times extend below
   1 ms at extremes; ±10% accuracy tested over 10 ms–5 s).
2. **RING re-classed** — Parker diode-ring dropped; the Serge ring is a
   four-quadrant transconductance multiplier (R6/CGS), DC-coupled (v1's
   bonus-VCA use / demo4 depends on DC behavior). Suppression gate set to
   ≥ 55 dB; feedthrough moved out of the sound-bar "must win" dimensions.
3. **T14/T17 seed rule** — engine instance seed is part of state and the
   engine exposes it; round-trip test = two fresh engines from one blob;
   drift phase derives from seed, positions not serialized. T02 re-defined
   as 1V/oct INTERVAL error (offset drift cancels); T17 bounds drift
   separately (±6 cents wander, interval ≤ 4 cents with drift on).
4. **G2/G2u split fixed** — zoom + full plugin getState/setState round-trip
   moved to G2u (G2 is JUCE-free); zoom and resize share one persisted
   scale float (single source of truth).
5. **T16 re-set** — 12.5% ceiling had 0.5-point margin vs a ±40% estimate:
   gate now ≥ 6× realtime with a min-of-5-runs measurement protocol; 8× is
   the working target; foundation perf spike decides tier downgrades.
6. **T08/T04 de-gamed** — folding (transfer turnarounds + centroid
   monotonicity + ≥5 odd harmonics) required, not just odd/even ratios
   (a plain tanh would have passed rev A); soft sync now has a
   must-NOT-lock negative assertion at large detune.
7. **v1 preservation restored** — monitor bus + velocity-scales-final-mix
   added to plan and T15.
8. **Sound/UI bars made honest** — identity-blindness was impossible
   (aliasing fingerprints, render-vs-photo); replaced with criteria-blind
   rubric scoring + pre-registered measurement bands as the decider +
   negative controls. UI exit = mean ≥ 4.0, no dimension < 3, two
   consecutive fresh critics.
9. **Stop conditions completed** — gate-dispute path (halt gate, evidence to
   LOG, amendment requires Mark) + global 25-round cap added.
10. **Fan-out choke points removed** — foundation now delivers frozen
    DspUtils.h (ADAA/polyBLEP/halfband/RNG/OU) + per-module test files with
    a registry runner. pluginval pinned to 1.0.4, AU + VST3 both required.
11. Hardware completeness adds: NTO pulse out, VCM top LO=linear-VCA mode +
    bottom squared-up 2nd output, SSG cycle-pulse jack + hot ±10 V coupler
    out, TKB KV = last-touched pad semitone + GLIDE ctl (interpretations
    logged), T03 full-band alias bound, T10/T11 made falsifiable.

**GAUNTLET.md rev B is now FROZEN.** Deliberate interpretations so far:
mouse click-Y = TKB pressure; GLIDE knob stands in for hardware row-B glide
routing; LENGTH knob is a v1-ism kept; VCFQ AGC law is fitted (logged when
fitted).

## 2026-08-31 — Foundation stage 2: UI skeleton + UX gates live + perf spike

- `src/ui/` landed: Look.h/Widgets.h ported from PF50 (house style unchanged;
  deprecated `Font::getStringWidthFloat` replaced with the JUCE 8
  GlyphArrangement API), plus v2 additions — mandatory >= 15-char tooltip on
  every widget type (Tooltips.h curates the texts per SPEC §4), TouchPadW
  (momentary pad via MomentaryFlag, click height -> PRESSURE ctl, active-stage
  highlight), ChoiceW menus, zoom control, shared TooltipWindow, aspect-locked
  0.5x–2.4x resize with the persisted editor scale as single source of truth.
  MainView is the foundation SKELETON (generic auto-layout per module + blocked
  TKB strip); the UI fan-out replaces layouts only.
- G2u live: satk_uxtests implements U01–U04, 22/22 green. Gotcha fixed during
  bring-up: `setResizeLimits()` in the editor ctor constrains the initial 0x0
  bounds up to the minimum, firing resized() and clobbering the restored scale
  — the ctor must capture the persisted scale before installing limits.
- **Perf spike (T16 plan decision)**: `satk_perfspike` loads the stubs with
  representative hero-tier costs (2x-OS islands on VCFQ / 3x VCM / RING / NTO
  shaper, ADAA kernels, PCG32, OU drift). Full graph, T16 protocol: 2,240
  cycles/sample = **32.6x realtime** (gate 6x, headroom 5.4x). Decision: hero
  tier for every module, no downgrades; results + re-measure rule frozen in
  docs/SPEC.md §10.

### Foundation interpretation notes (copied from SPEC.md §9 per core builder)

- **TKB RANGE** is frozen as scaling ALL row outputs A–D/ABCD (a v1-ism kept
  for continuity; hardware range semantics differ per row usage).
- **RING trims**: 0 = calibrated minimum bleed (≥ 55 dB suppression, T09),
  max ≥ −50 dB bleed — resolves T09's trim-range wording.
- v1 render findings worth keeping (from gauntlet/renders/v1/MANIFEST.md):
  v1's VCM bottom is purely ODD-harmonic (hardware/T08 wants even-dominant);
  v1's shape-sweep centroid is non-monotonic; v1's ring is unsuppressed AM;
  v1's NTO saw alias floor is −27 dBc (T03 wants ≤ −60/−30). These are the
  sound-bar dimensions v2 must win.

## 2026-11-02 — Stage 2 test-green pass (NOISE/RVG/VCA, VCFQ, TKB/KEY)

- **Foundation amendment — DspUtils.h**: added `IirUpsampler2x` /
  `IirDownsampler2x` (polyphase IIR allpass halfband, 8 coefs, transition 0.04,
  HIIR design formula). Existing FIR primitives untouched. Measured (T00):
  image −82 dB, flat to 18 kHz (±0.1 dB), round-trip latency 3.1 base samples
  vs ~11 for the FIR pair. Reason: an oversampled island inside a patchable
  feedback loop adds its latency to the loop; at 800 Hz the FIR pair's ~72°
  phase lag stopped BP→IN from oscillating near fc at all.
- **VCFQ**: switched to the IIR halfbands; amplitude limiting is now
  amplitude-dependent damping on BP above 6 V (slope 1) plus a rail safety
  limiter on the states that is exactly linear below 10 V. Clipping the states
  directly flat-topped the waveform and pinned self-osc at ~668 Hz at any gain.
  Near-critical self-osc now 716 Hz (0.9 fc), the remaining flattening coming
  from the deliberate per-integrator tanh (hardware-consistent).
- **T07 test amendment**: self-osc frequency is checked on a near-critical loop
  (Q 10, GAIN 0.13) with band 0.85–1.02 fc; the old GAIN 1.5 setting (~15×
  loop gain → relaxation regime, ~480 Hz on any 2-pole model) is kept as a
  separate overdrive check for stable/bounded/finite only. VCFQ audio-rate
  probes now inject straight into the jack — KEY KV's v1 glide floor (τ 0.1 ms)
  is a ~1.6 kHz one-pole that skewed the LP slope reading by ~4.6 dB/oct.
- **T10 test fix**: warm-up tick before baselining (jacks read 0 until the
  first tick) and 0.1 % slew tolerance (RATE default 0.6344 = ~4.001 Hz).
- Gauntlet 80/80, UX 22/22, zero warnings in our sources.

## 2026-11-02 — UI amendment: free-shape window + independent zoom (U01/U02)

Requested by Mark (option 3 of: reflow / stretch / scroll-and-zoom). Supersedes
the rev-B "aspect-locked resize, zoom and drag write the same scale" wording.
- Window resizes to any shape (min 420×300, max = panel at 2.4×); it is a
  viewport onto the panel and never changes the zoom. Scrollbars when the panel
  overflows; wheel/trackpad scrolls; drag on empty panel space pans 1:1.
- ZOOM menu (Fit, 50–240 %) and Cmd+wheel (toward the pointer) scale the panel
  only. Knobs now turn with Option+wheel (plain wheel scrolls the panel).
- Header (title, ZOOM, CLEAR) pinned above the scrolling panel.
- Persisted state: `editorScale` (zoom) + new `editorW`/`editorH` (window).
  First open: whole panel at the saved zoom, clipped to 92 %×88 % of the screen.
- U01/U02 rewritten to these contracts (29/29 UX). Label fixes alongside: long
  module titles wrap to two lines (RVG), VCA LIN/EXP labels moved to the switch.
