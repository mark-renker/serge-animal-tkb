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
