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

## 2026-11-02 — Stage 2 slopes builder: DUSG ×2 + SSG (T05, T06)

- **Shared slew core** (`satk::slew` in ModulesSlopes.h): dy/dt = R·tanh(e/Vk)
  (diff-pair current limit, Stinchcombe), Vk = 25 mV. It is stepped with its
  exact closed form, sinh(u(t)) = sinh(u0)·e^(−Rt/Vk), in double precision.
  The core is therefore monotonic and overshoot-free at any rate, and the
  DUSG state machine knows the exact sub-sample instant a slope completes
  (fractional carry: dialed 125+125 µs cycles at 4000.0 Hz; END/GATE is
  box-filtered over the sample). Slope R = C/T, with C calibrated so a full
  0→5 V transient including the tanh landing takes exactly the dialed T. The
  time law is unchanged from v1: expo 50 µs..10 s × 2^(−V), clamped
  20 µs..30 s.
- **Interpretations (T05)**:
  - Non-retriggerable for the WHOLE transient (rise and fall), per the T05f
    wording. This drops v1's retrigger-during-fall subharmonic-divider quirk.
  - IN is a DC follower (target clamped ±10 V) that runs whenever no transient
    is active. A transient's fall ends at the IN level, so a gate into IN
    holds the output at the top (ASR) and TRIGs during the sustain do nothing.
  - END/GATE goes high at the end of a transient and stays high through an IN
    sustain. It goes low while the follower is falling and high again once OUT
    is at the bottom (≤ 2.5 mV), so in ASR use it reads as a gate (A+S high,
    R low).
- **Interpretations (T06)**:
  - CYCLE is a Schmitt comparator on SMOOTH OUT (high at ≤ 0.05 V, low after
    ≥ 4.75 V), so CYCLE→SMOOTH IN gives a 0.05..4.75 V triangle with one pulse
    per cycle.
  - COUPLER is smooth > stepped with 1 mV hysteresis. HOT COUPLER is ±10.5 V.
  - STEPPED is S&H on SAMPLE edges, slew-limited by STEPPED RATE ×
    2^(−ST VC RATE).
  - HOLD freezes SMOOTH exactly.
- **Measured**:
  - Timing error ≤ 0.05 % (10 ms–5 s).
  - 1 V/oct ratio error ≤ 0.03 % (−1..+3 V).
  - Knob-minimum cycle reaches 10 kHz.
  - Mid-80 % deviation is 0.0000 % FS; OUT→VC RISE feedback bends it by
    24 % FS.
  - END duty is within 1e-7 of rise/(rise+fall) over 5–95 %.
  - SSG cycles from 0.615 Hz to 8 kHz.
  - Hot coupler swings ±10.5 V.
- **T15 note**: there is no T15 test in t_system.h yet, and NTO/VCM are still
  stubs, so the default patch is silent upstream of VCFQ. t_dusg_ssg.h has a
  "T15 piece" that checks the real KEY→DUSG1→VCA1→OUT chain with a 2.5 V
  220 Hz sine injected at VCA1 IN in place of the VCFQ path. Results: audible
  (rms 0.081), below −60 dBFS after note-off, and exact 0.0 with cable 1
  removed. The SPEC §1 loudness target (rms ≈ 0.07, peak ≈ 0.37) can only be
  checked once NTO and VCM are real. DUSG1 peaks at 5 V, the same as v1's
  1-unit envelope.
- Gauntlet 113/113 (80 + 33 new), T16 63.5× realtime, UX 22/22, zero
  warnings in our sources.

## 2026-11-02 — Stage 2: NTO (T02, T03, T04, T17 NTO half)

Self-contained section (NTO builder). Files: `src/engine/ModulesNTO.h`
(tick/reseed/private state only; registration untouched), `tests/t_nto.h`.

- **Core**: analytic triangle core (signed phase accumulator; TZFM runs it
  backwards) at 2x; polyBLEP on the saw wrap, polyBLAMP on both triangle
  corners, `|inc|` as dt. SINE/TRI/SAW/VAR are decimated by the IIR halfbands
  (the NTO sits in sync/FM feedback loops, same reasoning as the VCFQ
  amendment). Measured: saw @ 1975 Hz worst non-harmonic **−83.2 dBc in
  0–6 kHz, −53.2 dBc over 0–24 kHz** (gate −60/−30; v1 −27). 1x polyBLEP
  alone measures −59 dBc in-band (T00), which is why the core is oversampled.
- **PULSE (interpretation)**: computed with polyBLEP at the BASE rate, not
  through the halfband. Through the halfband the band-limited square rang to
  −1..+6 V on a RED 0/5 V jack, and clamping the ringing cost ~20 dB of alias
  floor. The base-rate pulse stays inside 0/5 V (alias floor −69 dBc in-band,
  −32 dBc full band @ 1975 Hz, ungated) and leads the halfband outputs by
  ~1.5 samples. At 16 kHz it is tested as a logic clock (one edge per cycle),
  not for sine-like amplitude.
- **Sine shaper (fitted)**: `sin(π/2·k·tri)/sin(π/2·k)`, k = 0.94 →
  THD 1.55% (hardware ~1–2%; gate ≤ 2%).
- **VAR law (fitted)**: drive G = 0.94 + 1.06·s on the same shaper (folds into
  an 'M' as G → 2), then squaring `tanh(D·y)/tanh(D)`, D = 6·s². shape s =
  clamp01(WAVESHAPE + VC WAVE/5) (v1 law). Centroid over 9 CV steps:
  311 → 2105 Hz, strictly increasing; THD at s = 0 is 1.55%.
- **Soft sync (fitted law)**: Schmitt edge (rise > 1.0 V, re-arm < 0.5 V,
  which accepts both 0/5 V pulses and ±2.5 V audio; interpretation). On an
  edge, if the core is within ±0.25 cycle of its wrap, 60% of that phase
  error is removed, spread over 8 internal samples (no discontinuity, so no
  BLEP). Theoretical lock range ≈ ±15% at 1:1 and 1:2, ≈ ±7.5% at 2:1.
  Measured: ±5% of 1:1/2:1/1:2 locks within 100 ms (≤ 0.2% ratio error over
  100–500 ms); at 20% detune the slave stays 20% away from every simple
  ratio (free-running).
- **≥ 32 kHz (interpretation of "finite, non-wrapped")**: frequency ceiling
  40 kHz (2x-rate inc ≤ 0.42, so the core genuinely runs there), verified by
  the core's cycle counter (33,128 cycles/s at FREQ max + 1.05 V), monotonic
  in CV, all outputs finite and ≤ 6 V. Content above 24 kHz is removed by
  the halfband (band-limited), which is not wrapping.
- **LIN FM**: AC coupling 2 Hz one-pole HP; Δf = (V/5)·3·f0·clamp01(DEPTH +
  VC LIN/5) (v1 law); |f| clamped to 40 kHz. TZFM test: fc 1 kHz, fm 230 Hz,
  β 6.5, inst. f −500..2500 Hz; sideband pairs n = 1..4 symmetric within
  0.25 dB.
- **Analog layer (fitted; at ANALOG DRIFT = 1, scaled by depth)**: static
  pitch offset uniform ±2 c and V/oct scale error ±0.3 c/oct
  (SplitMix64(moduleSeed)); wander = OU sum τ 0.5 s / 10 s / 180 s,
  σ 0.1 / 0.5 / 1.2 c, PCG32 stream "ntodrft", 1 kHz control rate, linear
  interpolation, soft bound 3.8·tanh(x/3.8) c ⇒ |absolute error| < 5.8 c by
  construction. Measured at depth 1: 10-min absolute pitch −0.62..+2.84 c
  and −2.74..+2.64 c (two seeds); interval error 0.71 c (depth 1) / 0.11 c
  (depth 0.2); depth 0 = bit-identical across seeds.
- **Integration TODO (System.h is frozen to this builder)**: the NTO reads the
  depth through a public `const float* driftDepth` (nullptr ⇒ 0 ⇒ ideal).
  Until System wires `nto->driftDepth = &out->ctls[OUT::cDrift].v`, the
  ANALOG DRIFT knob does not reach the NTO in the plugin; the T17 tests wire
  it explicitly.
- Read-only probes added (not registration): `instHz()`, `cycles()`.
- Perf: NTO ≈ 350 cyc/sample bare (spike 414); full-system T16 ≈ 60× realtime.
- Evidence: `gauntlet/renders/v2work/nto/nto_evidence.wav` (12 s mono: saw
  1975 Hz, VAR shape sweep, TZFM with rising index, soft-sync sweep).

## 2026-11-02 — Integration amendment: ANALOG DRIFT wired to NTO (System.h)

- `System()` now sets `nto->driftDepth = &out->ctls[OUT::cDrift].v`, so the
  global ANALOG DRIFT ctl (default 0.2) reaches the NTO drift/tolerance layer.
  One-line wiring only; no jack/ctl/index changes. Approved by Mark.
  Before this, the knob had no effect on the NTO (only the T17 tests set the
  pointer). Gauntlet 151/151 with drift live.

## 2026-11-02 — Stage 2 shapers: VCM + RING (T08/T09)

Self-contained entry from the shapers builder (`ModulesShapers.h`, `t_vcm_ring.h`).
All islands use the low-latency IIR halfbands (feedback-loop rule above).

- **VCM MID: fitted folding law (centroid monotonicity).** A constant-amplitude
  folder (triangle or sine) has a built-in ripple in spectral centroid vs gain.
  The ripple has period 2 in folder units, because the input sine's peak
  alternately dwells on a flat turnaround or a steep flank. With a plain
  rounded triangle, the centroid FELL across VC FOLD steps (487→895→782 Hz at
  2^0.286 per step), which fails T08. Fitted replacement, modelled as a
  reflector cascade: 6 turnarounds, fold-peak amplitude ×0.5 per turnaround,
  fold spacing ÷1.15 per turnaround (uneven, denser outward, as the v1 critique
  says of the real unit), then hold. Knees are box-rounded over ±0.6 folder
  units. A scan across all input levels (gain×amplitude 0.5–16) gives a
  worst-case centroid ratio of ≥ 1.059 over any ×1.215 gain step and ≥ 1.16
  over any ×1.48 step. The whole composite is ONE closed-form ADAA2 kernel,
  with P1–P3 tabulated per segment. Gain law per SPEC: 0.5×·2^(4·FOLD+0.4·V),
  cap 16×. BIAS gives ±1 folder unit (a quarter fold), v1-scaled.
- **VCM BOT**: three cascaded rectifier stages, each `2·|y| − env` (an ADAA1
  |x| plus a peak follower with 100 ms release). The follower is the "level
  compensation": a sine of peak A doubles in frequency and still spans ±A.
  DEPTH ×3 crossfades the stages in one after another. **BOT SQ OUT** is
  interpreted as a high-gain rounded comparator (ADAA1 tanh, ×8) of BOT
  against its own 4 Hz running mean, normalised by the input envelope. Its
  duty therefore moves with depth (0.38–0.55), which is what "resembling
  enhanced PWM" means here.
- **VCM TOP**: LO = two-quadrant linear VCA, gain = (DRIVE/2)·CV/5, so it is
  unity at 5 V with the default 2× knob. **Interpretation:** an unpatched VC
  TOP normals to +5 V, so in LO the knob alone sets a static gain. HI = ADAA1
  tanh with drive DRIVE·2^(0.4·V), the same CV law as fold. Top and bottom
  sections skip processing (outputs exactly 0) while their inputs are
  unpatched. The mid section always runs because BIAS alone produces DC.
- **RING**: normalised tanh(0.35·X)·tanh(0.35·Y) transconductance core.
  Morph is X·[(1−m) + m·Y]: 0 = dry, 0.5 = 100% AM, 1 = balanced ring. The
  output stage has a rail limiter that is exactly linear below 10 V. **Bleed
  law** (resolves SPEC §9.5): feedthrough = −90 + 50·√trim dB re unity, i.e.
  −90 / −70.6 (default 0.15) / −40 dB at 0 / default / max, for X and Y
  alike. Per-unit tolerance is only the POLARITY of each residual
  (moduleSeed); magnitudes are calibrated, so T09 holds for every seed. VCM
  has no tolerance layer. Neither module can read OUT's ANALOG DRIFT (no
  engine hook), so T08's "tolerance disabled" condition holds trivially.
- **T08/T09 measurement interpretations** (in `t_vcm_ring.h`): inputs are
  injected straight into the jacks (no KEY KV). The nominal level is 2.5 V
  peak. Harmonic tests use 220 Hz over 1 s (leakage-free Goertzel). "≥ 5 odd
  harmonics ≥ −40 dBc" must hold re H1 AND re the strongest component. The
  centroid is amplitude-weighted and must be strictly increasing over 8 FOLD
  knob steps (k/7) AND over 8 VC FOLD steps (0–5 V at knob 0.5). The alias
  test uses **997 Hz** (exactly 1 kHz aliases onto harmonic bins at 48 kHz):
  max fold, 65536-point Blackman-Harris, harmonics ±20 Hz excluded, judged re
  H1 and re the strongest component. Ring suppression is measured with X
  300 Hz × Y 80 Hz, both 2.5 V, sidebands vs the carrier.
- **Measured** (Release, M2 Max):
  - MID at max fold: 32 odd harmonics re H1 and 12 re max; odd:even 268 dB;
    8 transfer reversals.
  - MID centroid: knob 228→2485 Hz and VC 487→2485 Hz, both strictly rising.
  - MID aliases: −58.8 dBc re H1 / −80.3 re max. At the 16× CV cap (beyond the
    gate) they are −25 / −33 dB. This is a known headroom item, not gated.
  - BOT even:odd 150 dB; level within 1.2 dB of dry.
  - BOT SQ: dwells at its rails 85% of the time; correlation with BOT 0.92.
  - TOP LO r² = 1.0000.
  - RING suppression 64.4 dB; DC-carrier VCA r² 0.996 with THD 1.0%; morph
    monotonic.
  - VCM bare module, all 3 sections live: 940 cyc/smp (spike 552, under the
    3× bound). RING: 418 (spike 188, under the 3× bound).
  - Full suite T16: 60× realtime. perfspike: 33.4×.
- **T15 surrogate**: NTO and DUSG are still stubs, so `t_vcm_ring.h` emulates
  NTO VAR (a 2.5 V sine) and DUSG1 (a 1/250 ms AD to 5 V) at their jacks.
  The real folder path gives rms 0.097 over 1 s and peak 0.76. This is above
  the SPEC §1 regression target (0.07 / 0.37), but the excess does not come
  from VCM: MID is level-neutral (peak in = peak out at the default fold of
  1.15×). The excess is the VCA's +6 dB at a 5 V envelope plus the
  surrogate's assumptions. Re-check once NTO/DUSG land.
- **T13 finding**: a self-patched stress run (all VCM sections and RING into
  their own inputs/CVs, 10 V kick) took the level-compensated BOT past the
  rails (16.7 V: a rail-hot input plus halfband overshoot). Every VCM output
  now has the rail limiter that is linear below 10 V, and the check is kept as
  a module-level T13 contribution.
- Gauntlet 120/120, UX 22/22, zero warnings in our sources.

## 2026-11-03 — UI amendment: proportional window (supersedes 2026-11-02 free-shape)

Mark, after trying the free-shape viewport: dragging the corner should zoom the
panel, never leave blank space. Editor reverted to proportional scaling with a
custom constrainer (height = panel × zoom + fixed header; vertical-only drags
convert to width). ZOOM menu sets preset scales; Fit, scrolling, Cmd+wheel and
drag-to-pan removed (nothing to scroll). Window size is no longer persisted
separately (it follows the zoom); the opening zoom is capped to 92 % of screen
height. U01/U02 rewritten (UX 23/23). Also: toggle lever shortened so its ball
can't clip at the widget edge in either throw.

UI review rounds 1–4 (blind critics, bar mean ≥ 4.0 ×2): 3.00/3.50,
3.83/3.50, 3.67/3.83, 3.83/3.83. Round 5 fixes: rows equalised via a
horizontal spread factor, NTO/VCF re-balanced, MONITOR as a rotary selector
(SelectorW) instead of an on-panel dropdown, wide TKB pads filling the bar,
grouped/dividered I/O strip.

## 2026-11-03 — T15 on the real modules (tests/t_system.h)

- T15 now runs on the full default voice (KEY → DUSG1 → NTO var → VCM mid →
  VCFQ LP → VCA1 → OUT), replacing the per-builder surrogates as the gate.
  Measured: note 60 @ vel 4 V → rms 0.0999 / peak 0.775 over 1 s (SPEC §1
  loudness target ~0.07 / 0.37, NOT a gate — ~+6 dB hot, open item for Mark);
  note-off tail exactly 0; DUSG1 trigger removed → exact 0.0; velocity 0 V →
  exact 0.0; vel 2 V → 5 V rms ratio 2.18 (linear law 2.5, final tanh
  compresses); same seed → bit-identical voice.
- Monitor bus: with no mixer cables, a stimulus patch animates every source
  and each of the 42 monitor sources must exceed −60 dBFS at the output
  (quietest: TKB KV, 4.1e-3). Interpretation: "audible" for CV/gate sources
  means a MOVING source is heard — the output DC blocker removes static CV by
  design, so the stimulus keeps each source moving (TKB clock ~7 Hz so the
  100 ms GATE actually toggles). Monitor OFF + no cables → exact 0.0.
- Gauntlet 200/200.

## 2026-11-03b — UI amendment: zoom with screen cap + scrolling

Mark: keep corner-drag zoom, but once the window reaches a size limit it should
stop growing and the panel should scroll. Below the cap (95 % × 92 % of the
screen the editor is on) the window is exactly the panel at the zoom; past it
the window stays capped, the drag keeps raising the zoom, and the panel
scrolls (scrollbars / wheel / trackpad). ZOOM menu presets obey the same cap.
The opening zoom is no longer clamped to the screen (scrolling covers it).
U01 rewritten (UX 24/24).

## 2026-11-03 — Amendment: MASTER default 0.8 → 0.53 (default-voice loudness)

- With the real modules the default voice (note 60, vel 4 V) measured
  rms 0.0999 / peak 0.775, ~+3 dB over the SPEC §1 target (~0.07 / 0.37).
  Trimming the VCA was ruled out (T11 freezes max gain at +6 dB ± 1). Approved
  by Mark: lower the OUT MASTER ctl default instead (SPEC §4.12 table updated;
  ctl index/ID unchanged, v2 has no saved sessions yet).
- Now rms 0.0704 / peak 0.594. RMS matches v1; peak stays above v1's 0.37
  because v2's voice has a higher crest factor — not matchable with one gain.
- Gauntlet 200/200, UX 23/23.

## 2026-11-03 — Amendment (Mark): v2 gets its own plugin identity

- Goal 3 "identity preserved" and gate G3 amended. v1 and v2 shared the AU
  type/subtype/manufacturer (`aumu Satk Rnky`), the display name
  ("Renky Denk: Serge Animal TKB"), the bundle ID, and (derived from the codes)
  the VST3 class ID, so installing v2 would have replaced v1 in every host.
- v2 is now `aumu Sat2 Rnky`, PRODUCT_NAME "Serge Animal TKB v2", bundle
  `com.renkydenk.SergeAnimalTKB2`. Manufacturer stays `Rnky` (both appear under
  Renky Denk). G3 is now `auval -v aumu Sat2 Rnky`. Updated: CMakeLists.txt,
  GAUNTLET.md (goal 3 + G3), SPEC.md §7, REDESIGN_PLAN.md §1.
- Side effects: bundle filenames become "Serge Animal TKB v2.{component,vst3,app}";
  the standalone's settings file becomes "Serge Animal TKB v2.settings".
  The REDESIGN_PLAN phase-0 / risk notes about the collision are now historical.

## 2026-11-04 — Phase 4: UI soft bar CLOSED by Mark's approval

- The UI piece reached the per-piece cap (5 judge rounds, two fresh critics
  each, run 2026-11-03; means r1 3.00/3.50, r2 3.83/3.50, r3 3.67/3.83,
  r4 3.83/3.83, r5 3.50/3.50 vs exit 4.0). Recurring named gaps at r4/r5
  (diminishing-returns stop): lettering heavier than PF50, modules more boxed
  than hardware flow, no hover cue, TKB pad/knob texture.
- Per the stop conditions this was reported to Mark, who chose to ACCEPT the
  UI as of f07ad90/c58f31c ("happy with it"). The UI soft bar is closed by
  owner approval, not by critic score; the open gaps above are recorded as
  candidates for a later polish pass. Zoom behaviour (corner drag, 95%×92%
  screen cap, scroll beyond) is Mark's explicit choice and out of scope.
- Phase 4 continues on the sound bar only.

## 2026-11-04 — Hard gates G1/G3/G4/G5: all green

- **G1 build**: clean `cmake -B /tmp/satk-g1 -DCMAKE_BUILD_TYPE=Release` + full
  build of f9d36db (AU, VST3, Standalone, satk_gauntlet, satk_uxtests,
  satk_snapshot, satk_perfspike): exit 0, **0 warnings in src/ tests/ tools/**
  (67 warnings total, all inside JUCE).
- **G3 auval**: `auval -v aumu Sat2 Rnky` → AU VALIDATION SUCCEEDED, 0 warnings
  (installed build of 89c647b). `auval -a` lists both `aumu Sat2 Rnky` (v2) and
  `aumu Satk Rnky` (v1) — coexisting as intended.
- **G4 pluginval 1.0.4, strictness 10**: AU (installed 89c647b) SUCCESS, 0
  failures — one informational line "Current program is -1" (no factory
  presets yet; phase-5 presets will add programs). VST3: SUCCESS, 0 failures,
  re-run on the *installed* 89c647b bundle (an earlier run used an e0f7ccd
  build differing only in the MASTER default).
- **G5 install**: fresh "Serge Animal TKB v2.component" + ".vst3" in
  ~/Library/Audio/Plug-Ins/{Components,VST3} (2026-11-04 15:03), valid bundles;
  v1 remains installed alongside.
- Remaining for Win: sound soft bar (phase 4, in progress). Phase 5 split:
  presets + program support (UI/plugin session), README + final validation and
  install after merge (sound session).

## 2026-11-05 — Amendment (Mark): default cable 8, KEY KV -> NTO 1V/OCT A

- Found while building presets: v2's frozen default set (SPEC §6) had no path
  from MIDI pitch to the oscillator — notes retriggered the envelope at one
  pitch (measured: KV 0 V and 1 V both -> NTO 262.2 Hz). v1 hardwired the KEY
  frequency into the NTO. Mark approved adding the cable.
- `System::patchDefaults()` gains `key.jKv -> nto.jOctA`; SPEC §6 table +1 row.
  Now KV 1 V -> 522 Hz (octave). Note 60 = KV 0 V, so T15, the sound-bar
  note-60 render and the loudness target are unchanged. UX round-trip count
  7+1 -> 8+1. Gauntlet 200/200, UX 24/24.

## 2026-11-05 — Phase 5: factory presets + program support

- `src/plugin/Presets.h` (JUCE-free data + engine-level apply): program 0
  "Default Voice" (== `patchDefaults()`, all-default ctls) and the six v1 demo
  patches as v2 cable sets: Fold Sweep Voice, Filter Pings, Slope Bongos, Ring
  Fold Drone, Random Melody, Wild Animal.
- Translation (interpretations): v1 per-slot matrix amounts -> the
  destination's attenuverter (0.5 + amount/2) where one exists, else absorbed
  into a knob giving the same depth (e.g. TKB RANGE for row CV, VCFQ GAIN for
  input level); v1 "KEY held at f" drones -> NTO FREQ at f with VCA LEVEL
  holding the VCAs open; demo 2 stays on the monitor bus; v1's VCM "fold CV"
  on a bottom-section patch maps to VC BOT. Drone levels trimmed so every
  program sits within +12 dB of the default voice (rms 0.036..0.129).
- Plugin: getNumPrograms/getProgramName/setCurrentProgram (every param back to
  default with host notification, then preset ctls + its full cable set);
  program index saved in state (label only — params + cables authoritative).
  Editor header gains a PRESET menu (follows host program changes at 5 Hz).
- Tests: t_presets (7 programs, program 0 == patchDefaults, each program
  applies / finite / audible / level-matched); UX U05 (program API + PRESET
  menu). Gauntlet 209/209, UX 30/30, 0 warnings in our sources; pluginval
  1.0.4 strictness 10 on the new VST3: SUCCESS, "Num programs: 7, all program
  names checked" (clears the "Current program is -1" note).

## 2026-11-04 — VCM MID fold-sweep fix (sound-bar miss T08 centroid)

Self-contained entry, branch `vcm-sweep-fix` (from `sound-bar-evidence`).

- **The miss.** The sound bar measures `v2/vcm_mid_foldsweep.wav`: NTO SINE
  220 Hz → VCA1 (LIN, LEVEL 0.8) → VCM MID IN 1, BIAS centred, FOLD stepped
  k/7. The pre-registered band (T08: centroid strictly rising over 8 fold
  steps) came out OUT: 265, **256**, 275, 322, 451, 825, 1217, 1899 Hz. The
  ideal-sine gate test passed.
- **Probes** (scratch, not committed; jack-level, 1 s Goertzel bank H1–H100
  on the zero-crossing f0, the same centroid as the gate):
  - Input. The NTO SINE is `sin(π/2·0.94·tri)`, an under-driven shaper. It
    leaves a small kink at each peak, a long odd series in the same phase as a
    triangle's: H3 −36.9, H5 −46.3, H21 −72, H51 −88 dBc, THD 1.55 % (inside
    T04). The VCA's soft-sat is compressive, so it trims H3 to −40.7 dBc.
  - The folder. With w = 0.6 the box-rounded transfer was exactly linear for
    |u| < 0.4. FOLD step 1 (gain 0.5×, u peak 0.38 on the 1.92 V feed) sat
    in that linear zone. Step 2 (0.74×, u 0.57) just entered the soft knee.
  - Mechanism, step 1 → 2 at MID OUT. The centroid fell from 261.8 to 251.3
    Hz. Split by band, the contributions above 220 Hz were H3 4.0 → 2.1,
    H5–9 9.2 → 5.9, H11–29 12.1 → 9.0, and H30–100 16.5 → 14.3 Hz. H3 went
    −40.7 → −46.4 dBc and H5 −46.8 → −53.9 dBc. Their phase flipped from
    0° to 180°: the knee's compressive odd distortion has the opposite sign
    and partly CANCELS the input's H3/H5. More broadly, the knee lowers the
    slope under the peaks, where the kink lives. That smooths away the whole
    residual series (H21 −72.6 → −77.7 dBc) before the knee adds much of its
    own. Step 3 recovers (270 Hz).
  - Level compensation is not involved. MID has none, only the static
    peak normalisation `n`, which is fixed at configure time.
  - Each stage in isolation, original folder, FOLD-knob centroids:
    | feed | centroids (Hz) | result |
    |---|---|---|
    | VCA 0.8 feed | 262, 251, 270 … | dip |
    | NTO direct, 2.43 V | 256, 262, 294 … | rises, barely |
    | ideal sine | gate test, rising | rises |
    VCA level scan 0.5–1.0 (1.24–2.42 V): worst step ratios 0.949 / 0.983 /
    0.960 / 0.960 / 0.994 / 1.038, so the dip shows at every level below
    ~2.4 V. The cause is the folder's linear zone plus late soft onset, which
    meets an input whose peaks carry a kink. Neither the VCA nor the NTO is
    out of spec on its own.
  - Theory check. With a residual of opposite sign, any folder that starts
    exactly linear and softens later has a gain range where its first
    distortion only removes input harmonics. A ×1.486 FOLD step lands in that
    range for most levels. The way out is the hardware behaviour: a
    transistor folder is never perfectly linear, so its own harmonics outrun
    the input's residue from the bottom of the knob.
- **Fix** (`ModulesShapers.h`, VCM internals only, one constant):
  `kFoldW` 0.6 → **1.0**. The first knee's box rounding now reaches the
  origin, so near zero the transfer is u − 0.44·u|u|: gently compressive
  from the first millivolt, with no clean linear zone. Turns, decay, shrink,
  gain law, jacks and ctls are unchanged, and it is still one closed-form
  ADAA2 kernel. Other options were scanned in an offline model of the
  transfer (NTO shaper drive 0.92–0.98, 1.5–2.6 V, FOLD-knob and VC-FOLD
  sweeps, ideal-sine ×1.215 steps):
  - Sharper first knee: the dip gets deeper.
  - Wider first knee only: smaller margins.
  - An input scale on top of w = 1 (1.15–1.3): more margin, but the 997 Hz
    max-fold alias test drops to −33…−54 dBc re H1, against −63 for w = 1
    alone.
  w = 1 alone was the best trade.
- **After** (same probes):
  | feed | centroids (Hz) | worst step ratio |
  |---|---|---|
  | VCA 0.8 feed | 263.2, 271.4, 291.5, 338.6, 457.6, 794.4, 1213.5, 1879.1 | 1.031 |
  | NTO direct | 265.8, 279.0, 312.5, 391.5, 616.5, 1012.4, 1570.8, 2323.3 | 1.050 |
  | ideal 2.0 V | 246.9, 262.6, 290.1 … | — |
  VCA level scan 1.24–2.42 V: worst ratios 1.011 / 1.014 / 1.022 / 1.031 /
  1.041 / 1.051, all rising. Gate (ideal 2.5 V) FOLD knob: 255, 276, 315,
  401, 650, 1033, 1654, 2454 (was 228 … 2485). VC FOLD: 490 … 2454.
  Max fold: 34 odd harmonics re H1 / 14 re max. Alias at 997 Hz:
  **−63.4 dBc** re H1 (was −58.8; band −50). The 16× cap: −27.8 (info).
- **Audible cost.** MID at minimum FOLD is no longer clean. A 2.5 V sine
  now has 4.6 % THD (H3 −26.8 dBc), against 0.57 % before; v1's sin() folder
  was ~2.6 %. At the default FOLD 0.3 THD goes 13.8 → 15.1 % and the
  centroid 298 → 321 Hz.
- **Default voice** (NTO VAR → VCM MID): T15 note 60 rms 0.0704 / peak
  0.5943 → 0.0707 / 0.5974. Render `default_note60.wav`: peak 0.5877 → 0.5907,
  RMS L 0.03270 → 0.03283, note-window RMS 0.0694 → 0.0697.
- **Regression test** `t_vcm_ring::testMidRealSine`. It runs the real NTO
  SINE into MID via VCA1 LIN 0.8 (the manifest patch) and direct, over 8 FOLD
  steps, and requires the centroid to rise strictly. It FAILS on the old
  kernel (VCA feed, worst ratio 0.960) and passes now.
- **Sound bar re-run** (`satk_render`, `satk_measure`): only
  `vcm_mid_foldsweep.wav` and `default_note60.wav` changed; every other WAV
  is byte-identical.
  - The band is now **IN**: 267, 276, 297, 345, 462, 801, 1215, 1882 Hz.
  - MID count 26/11, odd:even 73.7 dB, reversals 6.0, 220 Hz max-fold
    non-harmonic −80.3 dBc (was −73.7).
  - Every other v2 band is unchanged. The only v2 OUT is still T09 X
    suppression on the unipolar-modulator manifest patch, which is explained
    in MEASUREMENTS.md.
  - Spectrograms for the 2 files were regenerated with the same ffmpeg
    settings. The blind packet `judge/sound-r1` was left as issued, so its
    vcm_mid_fold and default_voice pair now predates this fix.
- Gauntlet **202/202** (+2), UX unchanged. Zero warnings in our sources. T16
  stub-baseline perf 40.0× realtime; VCM bare 934 cyc/smp (3× bound 1656).

## 2026-11-05 — Sound bar round 2 (fixes for the sound-r1 blind critic)

Self-contained entry, branch `sound-r2` (from `vcm-sweep-fix` e219bf1).

- **T09 band amendment — approved by Mark.** The ring suppression band
  (55–85 dB, Y and X re the weaker sideband) is now measured on the
  **bipolar (AC-coupled) modulator** render `supplementary/ring_bipolar.wav`.
  The v1 unipolar `ring_sines` patch stays as listening material only
  (informational rows). To make the band a real pair, v1's bipolar render
  (and a v1 END-duty render, below) were added to
  `legacy/render_gauntlet.cpp` → `renders/v1/supplementary/`; the same Faust
  2.85.9 build re-renders the ten phase-0 v1 files byte-identically.
- **Measurement-bug fixes** (`satk_measure`; no band moved): (1) `vcm_bot`
  refined f0 at 220 Hz where BOT has no energy → off-frequency harmonic bank;
  "H1 −38.4 dB" was leakage (steady state −96 … −116). f0 now from the
  strongest of H1..H16, window 1.0–4.5 s. (2) `default_voice` RMS window was
  ~75 % silence → RMS over the sounding span. (3) the `dusg_cycle` "T05e"
  row read MAIN OUT → relabelled T05c (info); T05e is measured on END in the
  new `dusg_end` pair (v1 END is a fixed pulse: duty 0.200 at every ratio,
  OUT; v2 0.050/0.250/0.500/0.750/0.950, IN).
- **Finding 1 — VCM BOT on the wrong orders.** Root cause: each stage was
  `2·|y| − env`. A rectified sine is not half-wave symmetric (zero crossings
  at 30°/150°), so the next stage cannot double it again: its minima are
  uneven and the energy spreads over H2/H4/H6/H10/H12 with H8 starved
  (ideal 2.5 V sine, full depth: H2 −1, H4 0, H6 −2, H8 −12, H10 0, H12 0;
  render: strongest H10, H8 −42 dB). The per-stage peak followers also sagged
  ~4 % per 220 Hz period, and the linear crossfade dipped up to 3.2 dB
  between stages. Fix (`ModulesShapers.h`, internals only): the input is
  normalised by a ripple-free peak (max over a sliding 50–100 ms window,
  instant attack, 5 ms release), and every stage is the level-compensated
  square-law full-wave rectifier `r(y) = 2y² − 1`, which maps ±1 onto ±1 and
  keeps the doubled wave centred, so a sine goes H1 → H2 → H4 → H8 (T2∘T2∘T2
  = T8). Depth crossfades stage by stage with power compensation. The whole
  cascade is one degree-8 polynomial of u, ADAA1 as one kernel, rebuilt only
  on depth changes. SQ out and the rail limiter are unchanged in structure.
  After: ideal sine depth 1/3 → H2, 2/3 → H4 (next −77 dB), 1 → H8 (next
  −71 dB); level over 13 depth steps within 0.01 dB (r1 3.2 dB); render H8
  0, H6/H10 −24, H4 −58, H12 −53 dB (the H6/H10 pair is the ×8 phase
  sensitivity to the feed's own small H3), even:odd +95.0 dB, worst
  non-harmonic −119 dB re strongest. Critic notes checked: the −35.8 dBc
  18.3 kHz grid and the ~150 ms fade-in are both in **v1's** file (v1 peak
  0.158 → 0.224 over 150 ms; v2 r1 0.221 from the first 50 ms). SQ: corr
  with BOT 0.933, rail dwell 0.88, duty span 0.147 (T08 checks pass).
- **Finding 2 — RING leakage and X ± 2·fY products.** Root cause of the
  products: the core was `tanh(0.35·X)·tanh(0.35·Y)`; an offset 0..5 V
  carrier sits on the tanh's curvature, which grows the carrier's 2nd
  harmonic → 300 ± 160 Hz at −33 dB re strongest at the RING jack (the bleed
  model is linear and was not involved). The ~80 Hz leak (−72 dB re
  strongest) was exactly the default Y bleed (−70.6 dB re input). Fix:
  linear-then-soft input stages (`LinSoftKernel`: exactly linear to ±5 V,
  tanh shoulder above, ADAA1) and the bleed law retuned to
  `−102 + 62·√trim` dB → trim 0 −102, **default −78.0** (still inside the
  SPEC §4.6 −60..−80 band; ≈ the documented 80 dB rejection), max −40.
  After (unipolar patch at the jack): 80 Hz −72.1 → −80.3, 140/460 Hz
  −33.3/−33.1 → −104.6/−109.4 dB re strongest. In the render file the
  products read −48.9 dB: that residue is the shared OUT-bus `fastTanh`
  limiter (out of scope). T09: suppression 64.4 → 72.0 dB, DC-carrier VCA
  r² 0.996 → 1.0000, THD 1.0 % → 0. Band patch note: the bipolar render's
  VCFQ coupler itself soft-clips the offset triangle (2nd harmonic −30 dBc
  at unity GAIN), so the patch runs it at GAIN 0.15 (0.3×, −51 dBc) in both
  versions. Band: Y 72.3 dB IN, X 59.2 dB IN (v1 116.9 OUT / 71.1 IN).
- **Finding 3 — VCM MID: clean minimum fold AND rising sweep, held level.**
  Root cause (as the brief suspected): the 2026-11-04 knee widening (w 1.0)
  was a workaround for the NTO SINE's peak kink (`sin(π/2·0.94·tri)`), whose
  long odd residue had the opposite sign to the folder knee. Fix at the
  source (`ModulesNTO.h`, internals only): SINE = `sin(π/2·tri)` on the
  unrounded triangle (an exact sine of the core phase; the shaper's zero
  slope makes the corner smooth) plus a smooth compressive residual
  `s − 0.01·s³` (H3 only, THD 0.25 %; T04 ≤ 2 % and the > 0.2 % info bound
  pass; T02 amplitude intact). VAR keeps its 0.94 shaper start, so VAR and
  the T04 shape sweep are byte-for-byte unchanged. Folder: knees are now a
  **triangle-kernel** smoothing of half-support W 0.8 (C2 knees, linear zone
  |u| < 0.2; f/F1/F2 = second differences of P2/P3/P4, still one ADAA2
  kernel), and the section has a **level-compensation makeup** vs fold gain
  (tabulated from the kernel's own sine RMS, 32 points/oct over 0.5×–16×,
  unity at the default FOLD 0.3 so the default voice keeps its level,
  capped at 2× so fold peaks stay ≤ 5 V). Box knees at w 0.6/0.65/0.7 and
  triangle W 0.6/0.9/1.0/1.2/1.4 were scanned: W 0.8 was the best trade of
  min-fold THD, centroid margin and alias floor (W 0.9 has a −50 dBc alias
  spike at a 2.0 V feed).
  After: ideal 2.5 V sine at minimum FOLD THD **4.61 % → 0.85 %**; render
  step 1 2.77 % → 1.28 % (NTO + VCA residue included). Render centroid 267,
  276, 297, 345, 462, 801, 1215, 1882 → **241, 248, 265, 311, 457, 870, 1252,
  1995 Hz**. `testMidRealSine` worst step ratio via VCA 1.031 → 1.028, direct
  1.050 → 1.053; VCA level scan 0.5–1.0: 1.006 / 1.012 / 1.020 / 1.028 /
  1.037 / 1.047 (all rising). Gate centroid: FOLD 226 … 2477 Hz, VC 481 …
  2477 Hz. Per-step RMS (render) −18.5, −18.3, −17.6, −16.0, −14.9, −16.3,
  −15.8, −17.6 dBFS: span **6.6 → 3.6 dB**; ideal 2.5 V sine 6.2 dBV flat to
  step 7, 4.2 at max fold (cap). Alias 997 Hz max fold −63.4/−87.0 →
  **−56.9/−78.3 dB** re H1/strongest (band −50); 16× cap −25.0 (info).
- **Tests** (`t_vcm_ring.h`, strengthened, +8 checks): MID min-fold THD
  ≤ 1.5 %; MID level span ≤ 4 dB over the FOLD knob (ideal) and over the
  sound-bar sweep (real sine via VCA 0.8); BOT octave cascade H2/H4/H8
  dominant ≥ 20 dB at depth 1/3, 2/3, 1; BOT level over 13 depth steps
  ≤ 1.5 dB; BOT no fade-in; RING unipolar carrier X ± 2fY ≤ −80 dB and 3fX
  ≤ −80 dB. One existing check changed meaning: "MID transfer bounded at
  the ±2.5 V fold peaks" is now "±2.5 V × makeup, makeup ≤ 2" (the static
  ramp at max fold peaks at 5.0 V by design of the level compensation; the
  T08 gate itself — ≥ 2 slope reversals — is unchanged).
- **Default voice** (NTO VAR → VCM MID → VCFQ → VCA): VAR unchanged; MID
  makeup is exactly 1 at the default FOLD; only the knee shape changes. T15
  note 60 rms/peak 0.0707/0.5974 → **0.0698/0.5887**. Render
  `default_note60.wav` peak 0.5907 → 0.5823, RMS L 0.03283 → 0.03241; first
  100 ms of the note: H3 −10.5 → −11.3 dBc, H5 −30.6 → −32.8 dBc, centroid
  718 → 711 Hz — marginally less bright, otherwise the same voice.
- **Renders / packet.** `satk_render` re-run (deterministic, `cmp`-verified):
  changed `vcm_mid_foldsweep`, `vcm_bot_full`, `ring_sines`, `dusg_env_shape`
  (NTO SINE carrier), `default_note60`, `supplementary/ring_bipolar`,
  `ring_morph_sweep`; monitor levels re-trimmed to keep pairs within ±1 dB
  (MID 0.41, BOT 0.52, ring_sines 0.35, bipolar 1.0). New negative control
  `control/ring_bipolar_leaky.wav` (bare X·Y with −20 dB of both inputs
  leaking: Y 14.4 dB / X 0.9 dB suppression). Spectrograms regenerated with
  the same ffmpeg settings. Blind packet `judge/sound-r2/` (new A/B draws, the
  bipolar ring pair, the new control as `ring_C`; key in
  `judge/KEY-sound-r2.md`).
- Gauntlet **210/210** (+8), UX 24/24, zero warnings in our sources (full
  Release build). Perf (same-session runs, ±15 % noise): VCM bare 1174 → 917 cyc/smp (3× bound 1656), RING
  527 → 200; T16 stub baseline 38.7× → 38.2× (≥ 6×); scratch full-engine check
  (default patch + BOT/TOP/RING patched, min of 5 × 10 s, -O3) 30.8× vs
  28.7× before.
