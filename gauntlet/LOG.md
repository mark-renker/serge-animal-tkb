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

## 2026-11-03 — Amendment: MASTER default 0.8 → 0.53 (default-voice loudness)

- With the real modules the default voice (note 60, vel 4 V) measured
  rms 0.0999 / peak 0.775, ~+3 dB over the SPEC §1 target (~0.07 / 0.37).
  Trimming the VCA was ruled out (T11 freezes max gain at +6 dB ± 1). Approved
  by Mark: lower the OUT MASTER ctl default instead (SPEC §4.12 table updated;
  ctl index/ID unchanged, v2 has no saved sessions yet).
- Now rms 0.0704 / peak 0.594. RMS matches v1; peak stays above v1's 0.37
  because v2's voice has a higher crest factor — not matchable with one gain.
- Gauntlet 200/200, UX 23/23.
