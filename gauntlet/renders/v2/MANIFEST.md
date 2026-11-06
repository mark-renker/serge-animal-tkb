# v2 Renders — Gauntlet Sound Bar (phase 4 evidence)

The v2 equivalent of every v1 reference patch in `../v1/MANIFEST.md`, same
filenames, rendered 2026-11-04 from branch `sound-bar-evidence` (base
`89c647b`). Measurements of both sets: `../MEASUREMENTS.md`.

**Round 2 (2026-11-05, branch `sound-r2`).** Re-rendered after the round-2
DSP fixes (LOG.md "Sound bar round 2": VCM BOT octave cascade, RING linear
core + bleed law, VCM MID knee + level compensation, purer NTO SINE). Changed
files: `vcm_mid_foldsweep`, `vcm_bot_full`, `ring_sines`, `dusg_env_shape`
(its 1 kHz carrier is the NTO SINE), `default_note60`, and
`supplementary/ring_bipolar` + `ring_morph_sweep`; every other file is
byte-identical. Monitor levels re-trimmed so the pairs stay within ±1 dB
RMS: VCM MID 0.5 → 0.41, VCM BOT 0.6 → 0.52, ring_sines 0.4 → 0.35. The
bipolar ring patch now runs the VCFQ coupler at GAIN 0.15 (0.3×; see
`supplementary/`). `ring_bipolar` and `dusg_end_duty` now have v1
counterparts in `../v1/supplementary/` (pairs; T09 band amended to the
bipolar render with Mark's approval). New negative control
`../control/ring_bipolar_leaky.wav`.

**Round 3 (2026-11-06, branch `sound-r3`).** Re-rendered after the round-3
DSP fixes (LOG.md "Sound bar round 3": VCM BOT rectifier cusps, VCM/RING
2-pole knob smoothing, NTO VAR exact-sine start, VCFQ integrators linear
below 5 V, VCA output stage linear below 5 V). Changed files:
`nto_shape_sweep`, `vcm_mid_foldsweep`, `vcm_bot_full`, `vcfq_ping`,
`dusg_env_shape` (the VCA), `default_note60`, `supplementary/ring_bipolar` +
`ring_morph_sweep` (the VCFQ coupler) and `../control/ring_bipolar_leaky`
(same coupler); every other file is byte-identical. No patch or monitor
level changed — every pair stays within ±1 dB RMS. New negative control
`../control/dusg_env_rc.wav` (below).

**Round 4 (2026-11-06, branch `sound-r4`).** Re-rendered after the round-4
fixes (LOG.md "Sound bar round 4": RING default bleed −79.8 dB, VCM BOT
rectifier on the active stage, VCM MID peak + RMS makeup and outer-fold
reshape). Changed files: `vcm_mid_foldsweep` (monitor 0.41 → **0.48**, see
file 3), `vcm_bot_full`, `ring_sines` (bleed only), `default_note60` (the
default voice runs through MID), `supplementary/ring_bipolar` +
`ring_morph_sweep` (**new band patch: full-level bipolar modulator**, monitor
1.0 → **0.31**) and `../control/ring_bipolar_leaky` (same patch, 0.8 →
**0.30**); its v1 counterpart `../v1/supplementary/ring_bipolar.wav` was
re-rendered with the identical patch semantics. Every other file is
byte-identical; every pair within ±1 dB RMS. New negative control
`../control/vcm_bot_single_abs.wav` (below).

## How these were made

- Driver: `tools/RenderGauntlet.cpp` → CMake target **`satk_render`** (pure
  C++17, no JUCE). `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build
  build --target satk_render && ./build/satk_render gauntlet/renders`.
- It drives `satk::System` the way the gauntlet tests do: ctls poked directly,
  banana cables via `sys.patch`, KEY fields (`gateV`, `velV`, `pitchV`) written
  per sample, knob smoothers snapped by `sys.prepare(48000)` after the patch is
  set up, then events applied sample-accurately. The WAV is `OUT.outL/outR` —
  every file goes through the real v2 OUT mixer (monitor bus, DC block,
  `fastTanh` soft limiter), the same position as v1's monitor tap.
- Format: **48 kHz, 16-bit PCM stereo**, **fixed gain**: the writer is the v1
  writer (`clip to ±1, × 32000`, no normalisation), shared in `tools/WavIO.h`.
- **Determinism**: instance seed `kDefaultInstanceSeed` (0x53415432303236) on
  every file; **ANALOG DRIFT = 0** (ideal, the T02/T08 measurement condition)
  on every file except `default_note60.wav`, which is the untouched default
  (DRIFT 0.2). Re-running reproduces byte-identical WAVs (verified with `cmp`).
- f0 below = zero-crossing count on the NTO SINE jack during the render
  (`t_nto::zcFreq`); NTO pitch set with the FREQ knob (`t_nto::freqKnob`), FINE
  centred, no 1 V/oct input — v1 used KEY freq + keytrack for the same purpose.

### Level convention (matching v1's monitor tap)

v1: `src × monLvl × 0.7 × master 0.8 × midiGain 0.8 → dcblocker → t3`.
v2 OUT has the same law (SPEC §4.12), `kAudio` 2.5 V = 1.0 FS. v1's ±1-unit
audio corresponds to v2's ±2.5 V audio, so monitor renders use **MASTER 0.8**
(v1's master; v2's default is 0.53) with KEY VEL at its 4.0 V init (= v1's 0.8)
and **v1's monitor level**. A 0..5 V CV source is twice v1's swing in FS terms,
so CV sources use **half** v1's monitor level. Where that convention left the
v2 RMS more than ±1.5 dB from v1's, the monitor level was trimmed (hard-coded,
listed below) so the A/B listening comparison is not a loudness comparison.
Result: all pairs within ±1 dB RMS (table at the end), except the near-silent
`_monitor_direct` evidence file.

Monitor source indices are v2's (SPEC §5; 0..30 = v1's list verbatim).

---

## 1. `nto_saw_1975.wav` — NTO SAW @ 1975 Hz (T03)

`NTO FREQ = freqKnob(1975)`; **NTO SAW → monitor** (src 2), monitor level 0.5,
MASTER 0.8; drift 0. Nothing else patched. **Achieved f0 1974.999 Hz**
(file: 1974.998 Hz by interpolated FFT peak).
Measured: worst non-harmonic **−78.6 dBc in 0–6 kHz** (2575 Hz), **−52.8 dBc
full-band** (22925 Hz) — inside T03 (v1: −26.5 / −21.7 dBc).
Peak 0.2725 (band-limited edge overshoot), RMS 0.1236.

## 2. `nto_shape_sweep.wav` — NTO VAR, WAVESHAPE ×8 @ 220 Hz (T04)

`NTO FREQ = 220 Hz`; **NTO VAR → monitor** (src 3); WAVESHAPE stepped at
t = 0..7 s to k/7. Monitor level **0.375** (trimmed: convention 0.5 gave
+2.5 dB over v1 — VAR is hotter than v1's var out). Achieved f0 220.001 Hz.
Harmonic centroid per step: 270, 295, 386, 429, 505, 661, 963, 1794 Hz —
strictly rising sine → 'M' (v1 rises then falls).
**Round 3:** the minimum shape is the exact sine (VAR drive starts at 1.0,
not an under-driven 0.94): step 1 THD 0.23 % in the file (the bus limiter's
H3; 0.000 % at the jack; r2 1.55 %); centroid 236, 331, 413, 450, 523, 680,
986, 1794 Hz.

## 3. `vcm_mid_foldsweep.wav` — VCM MID fold sweep (T08)

NTO SINE 220 Hz → **VCA1 (LIN, LEVEL 0.8 → gain 0.8)** → **VCM MID IN 1**;
BIAS 0.5 (centred); FOLD stepped at t = 0..7 s to k/7; **VCM MID OUT →
monitor** (src 15), level **0.48** (r4; r2/r3 0.41, r1 0.5 — MID holds its
level as it folds; matched to v1's RMS). Achieved f0 220.001 Hz.
*Reproduction note:* v1's feed was "NTO sin, amt 0.8"; v2 banana cables have
no attenuator, so the 0.8 depth is the VCA (its `kSatV` soft-sat adds a
symmetric 3rd harmonic ≈ −46 dBc at 2 V). This is the closest patch-level
translation; it matters — see the centroid finding in MEASUREMENTS.md.
**Round 2:** centroid per step 241, 248, 265, 311, 457, 870, 1252, 1995 Hz
(strictly rising → IN); max fold 32 odd harmonics ≥ −40 dBc re H1 (13 re
max), odd:even 73.0 dB, ≈ 6 transfer reversals; THD at minimum fold 1.28 %
(r1 2.77 %); per-step RMS −18.5, −18.3, −17.6, −16.0, −14.9, −16.3, −15.8,
−17.6 dBFS (span 3.6 dB; r1 6.6 dB). Round 1 (vcm-sweep-fix): 267, 276, 297,
345, 462, 801, 1215, 1882 Hz; 26/11 odd harmonics; odd:even 73.7 dB.
**Round 3:** the VCA in the feed is now linear at this level (its old
`kSatV` soft-sat H3 is gone) and the FOLD knob is 2-pole smoothed (no click
at the steps: worst 4–12 kHz excess +21.8 → −0.0 dB). Centroid 240, 247, 265,
314, 465, 888, 1289, 1993 Hz; THD at minimum fold 0.81 %; per-step RMS span
3.4 dB; 36 odd harmonics ≥ −40 dBc re H1 (11 re max); odd:even 73.2 dB.
**Round 4:** the makeup holds peak AND RMS (level metric peak^0.58·rms^0.42)
and the outer folds decay 0.85 instead of 0.5: per-step peak −14.8 … −10.9
dBFS (span **3.8 dB**; r3 −15.5 … −9.1, span 6.4), RMS span 3.8 dB (r3 3.4);
centroid 240, 247, 267, 318, 483, 859, 1343, 2082 Hz; 25 odd harmonics
≥ −40 dBc re H1 (13 re max); odd:even 72.0 dB; THD at minimum fold 0.86 %;
max-fold non-harmonic −81.8 dBc (r3 −67.5); click excess −0.6 dB.
*Re-rendered on branch `vcm-sweep-fix`* after the VCM-mid knee fix (LOG.md):
the first render went 265, **256**, 275, 322, 451, 825, 1217, 1899 Hz (OUT).
The patch is unchanged.

## 4. `vcm_bot_full.wav` — VCM BOT @ full depth (T08)

Same feed (NTO SINE 220 Hz → VCA1 0.8 → **VCM BOT IN 1**); BOT DEPTH 1.0;
**VCM BOT OUT → monitor** (src 16), level **0.52** (r2; r1 0.6).
**Round 2** (steady state 1.0–4.5 s, f0 from the strongest harmonic):
strongest **H8**; H1..H16 re strongest −116, −72, −121, −58, −115, −24, −100,
**0**, −100, −24, −113, −53, −124, −85, −116, −112 dB → even:odd **+95.0 dB**
(T08 ≥ +12; v1 −85.3 dB, purely odd). r1's file re-measured with the fixed
f0: strongest H10, H8 −42 dB (its table row "H1 −38.4" was the f0 bug).
**Round 3** (rectifier cusps): strongest H8; H1..H16 re strongest −123, −67,
−123, −25, −119, −33, −99, **0**, −99, −35, −115, −32, −130, −51, −117, −27 dB
and a dense even series to the top of the band; even:odd +95.6 dB; worst
non-harmonic −102 dB re strongest over 0–20 kHz (−82 dB at 23.8 kHz).
**Round 4** (rectifier on the active stage, exact doublers under it):
H1..H16 re strongest −119, −63, −126, −66, −117, −34, −100, **0**, −100,
−34, −118, −74, −120, −49, −112, **−21** dB — the lower-stage residue is gone
(H4 −25 → −66, H12 −32 → −74), the top-stage cusp is stronger (H16 −27 →
−21); H6/H10 −34 are the feed's H3 × the cascade's ×8 phase gain. Even:odd
+94.9 dB; non-harmonic −107.7 dB re strongest over 0–20 kHz (−80.5 dB at
23.8 kHz, halfband transition band).

## 5. `vcfq_ping.wav` — VCFQ ping, Q ×4 (T07)

TKB RUN on, RATE 2 Hz, GATE LEN 20 ms; **TKB GATE → VCFQ TRIG**; VCFQ FREQ
800 Hz, PING LEVEL 0.9; Q stepped at t = 0/2/4/6 s to **2, 6, 15, 35**
(`t_vcfq::qKnob`); nothing into VCFQ IN; **VCFQ BP → monitor** (src 12),
level **0.15** (trimmed from v1's 0.9: v2's shaped ping is ~16 dB hotter than
v1's one-sample impulse at the same tap). First ping at t ≈ 0.5 s (TKB clock
phase starts at 0), then every 0.5 s.
Decay peak→−40 dB (median per step): **4.0, 10.8, 27.1, 64.1 ms** — monotonic
(v1: 3.8, 10.7, 27.5, 64.4 ms). Rings at ≈ 800 Hz.
**Round 3:** the integrators are exactly linear below 5 V, so the ring pitch
no longer sags with ping level: Q 35 rings at **799.9 Hz** (5–45 ms after the
onset; r2 797.3 Hz in the same window). Decays 4.0, 10.8, 27.5, 64.4 ms.

## 6. `dusg_cycle.wav` — DUSG1 cycling @ ~200 Hz (T05c)

DUSG1 CYCLE on, RISE = FALL = 2.5 ms (`t_dusg_ssg::dial`); nothing patched;
**DSG1 OUT → monitor** (src 4), level 0.25 (CV source). Cycle frequency
**200.00 Hz**. Peak 0.204 is the DC-blocker start-up transient of the 0..5 V
source (first ~50 ms); steady state ±0.111 (v1 ±0.109).

## 7. `dusg_env_shape.wav` — DUSG1 rise 0.5 s / fall 1.0 s (T05d)

**KEY GATE → DUSG1 TRIG**, gate pulsed 0.2 s at t = 0.3, 2.4, 4.5 s; RISE
0.5 s, FALL 1.0 s; **DSG1 OUT → VCA2 VC** (VCA2 LIN, LEVEL 0 → gain = env/5 V);
**NTO SINE 1 kHz → VCA2 IN; VCA2 OUT → OUT IN 2**, PAN 2 = 0.5; **MASTER 0.4**
(= v1 master 0.8 × v1's carrier amt 0.5, which has no cable equivalent). The
amplitude envelope of the 1 kHz carrier is the DUSG shape, exactly v1's
audition patch. Achieved carrier 1000.001 Hz.
Mid-80 % rise deviation from a line **0.85 % FS** (worst of 3; T05d ≤ 3 %),
fall 0.86 %; rise/fall ≈ 0.492 / 0.985 s.
**Round 3:** the DUSG OUT itself was always linear (jack-level probe: 0.000 %
FS); the bend was VCA2's output soft-sat (8·tanh(y/8): ~9 % less slope at
the 2.5 V top of the swing, and H3 −40 dBc on the carrier). With the VCA
linear below 5 V: rise **0.31 %** / fall 0.30 % FS (v1 0.28 / 0.26 %),
rise/fall 0.494 / 0.989 s; carrier H3 at the top of the rise −43.8 dBc in the
file (VCA jack −51.6 dBc = the NTO SINE's own residual; the rest is the OUT
bus limiter at 0.22 FS). The patch is unchanged: it already maps v1's
semantics (v1 MANIFEST file 7: VCA2 linear, level 0, carrier amt 0.5 →
v2 MASTER 0.4, same output level).

### 7b. `dusg_env_shape_monitor_direct.wav` — same transients, DSG1 OUT → monitor

Same triggers and slopes, **DSG1 OUT → monitor** (src 4), level 0.25.
**Finding:** v2's monitor bus does *not* pass the slow envelope either — OUT
runs a 10 Hz one-pole DC blocker per channel (`EngineCore.h` `DCBlock`) before
the limiter, so only the envelope's derivative leaks through (peak 0.0072,
RMS 0.0041; v1 0.0019 / 0.0011). The v1 manifest's expectation that "v2's
monitor must pass DC/slow CV" is not met by the current OUT module; file 7's
VCA audition is the usable evidence for both versions.

## 8. `ring_sines.wav` — RING, NTO SINE 300 Hz × DUSG2 ~80 Hz (T09)

NTO SINE 300 Hz → **RING X**; DUSG2 CYCLE, RISE = FALL = 6.25 ms (79.9 Hz
triangle, **unipolar 0..5 V**, exactly v1's modulator) → **RING Y**; RING
ctls default (GAIN 1×, MORPH 1 = full ring, trims 0.15); **RING → monitor**
(src 17), level **0.4** (trimmed from 0.5: +2.1 dB). Achieved f0 300.001 Hz.
Because Y carries a 2.5 V DC component, the four-quadrant core produces AM:
the 300 Hz X component is 8.1 dB *above* the sidebands (v1: 10.4 dB). The
~80 Hz Y fundamental is suppressed 72.3 dB (r1 63.6; v1 111.6 — no bleed at
all); re the strongest component it is −80.4 dB (r1 −72.2). X ± 2·fY
products −48.9 dB re strongest (r1 −32.2; at the RING jack −105 dB, the rest
is the OUT bus soft limiter). Monitor level **0.35** (r2; r1 0.4).
**Round 4:** default bleed −79.8 dB (r3 −78.0): Y suppression 74.1 dB,
~80 Hz leak −82.2 dB re strongest.
**Listening material only since round 2** — the T09 band is measured on the
bipolar pair (`supplementary/ring_bipolar.wav`, both versions).

## 9. `default_note60.wav` — untouched default patch, MIDI note 60

`System::patchDefaults()` (the seven default cables), **all ctls at default**
(incl. MASTER 0.53, ANALOG DRIFT 0.2, PAN 1 0.35); KEY: velocity 100
(`velV = 100/127 × 5 V`), pitch 0 V, gate high 0.25 → 2.25 s, then 2.25 s
tail. NTO free-runs at the default 261.63 Hz knob (achieved 261.609 Hz with
drift 0.2; no KV cable in the default set, as in v1). Stereo.
**Round 4:** peak 0.6595, RMS L 0.03436 / R 0.02597; sounding-span RMS
0.1732 over 0.177 s (the MID folder's reshaped outer folds; makeup 1 at
the default FOLD is unchanged).
**Round 3:** peak 0.6552, RMS L 0.03425 / R 0.02588; sounding-span RMS
0.1726 over 0.177 s (VAR at the default shape 0.15 is now drive 1.15, and
VCA1 no longer soft-compresses the pluck's ~5.6 V peaks).
**Round 2:** peak 0.5823, RMS L 0.03241 / R 0.02436; sounding-span RMS
0.1616 over 0.181 s (v1 0.1548 over 0.247 s); tail after 3.0 s exactly 0.
Round 1: peak 0.591, RMS L 0.0328 / R 0.0247; note-window (0.25–1.25 s) RMS
0.0697 (v1 0.0770). (Re-rendered on `vcm-sweep-fix`: the
default voice runs through VCM MID. The first render had peak 0.588, RMS L 0.0327,
note-window RMS 0.0694.)

---

## v2-only supplementary renders (`supplementary/`)

Since round 2, `ring_bipolar` and `dusg_end_duty` have v1 counterparts
(`../v1/supplementary/`, rendered by `legacy/render_gauntlet.cpp`) and are
A/B pairs; `ring_morph_sweep` stays v2-only. Measured in MEASUREMENTS.md.

| file | patch | why |
|---|---|---|
| `ring_bipolar.wav` | as file 8, but **DUSG2 OUT + KEY KV (held at −2.5 V: `KEY.pitchV`) stacked into VCFQ IN** (a centred ±2.5 V triangle), VCFQ GAIN 0.5 (**1×**), **VCFQ HP** (FREQ 20 Hz, Q 0.5) → RING Y; monitor level **0.31** (r4; level-matched to v1's bipolar render, +0.12 dB) | **T09 band patch** (amended 2026-11-05, Mark). **Round 4 — full-level modulator.** r2/r3 ran the 0..5 V triangle into the coupler at GAIN 0.3× (an OFFSET swing bends in the VCFQ input stage into its own 2nd harmonic → X ± 2·fY products), which left the modulator, and so the sidebands, 10.5 dB down against RING's fixed bleed (X 59.3 dB). Stacking an exact −2.5 V DC onto the triangle first makes the coupler's swing symmetric — its curvature is then odd-only (the triangle's own odd series, no even products: X ± 2·fY −89.4 dB re strongest) — so it runs at unity, and the HP only strips the triangle's residual DC. v1 uses identical semantics (KEY CV −0.5 = −2.5 V on the VCFQ audio-in B slot, input gain 1). r4: Y 74.1 dB, X 71.8 dB (v1 110.0 / 68.1) |
| `ring_morph_sweep.wav` | same bipolar patch, MORPH stepped k/8 at t = 0..8 s | T09 morph dry → AM → ring monotonic (v1 has no morph control) |
| `dusg_end_duty.wav` | DUSG1 CYCLE, 20 ms period, rise/(rise+fall) = .05/.25/.50/.75/.95 for 1.5 s each; **DSG1 END → monitor** (src 5), level 0.25 | T05e END duty law (v1 counterpart since r2: its END is a fixed-width pulse, duty 0.200 at every ratio) |

## Negative controls (`../control/`)

**Round 4 — `vcm_bot_single_abs.wav`** (sound-r4 packet, vcm_bot piece C):
file 4's patch and level path (NTO SINE 220 Hz → VCA1 LIN 0.8 → BOT IN 1,
BOT monitor tap, MASTER 0.8), but the BOT monitor pointer re-aimed at a
degraded wave multiplier: **one plain full-wave rectifier**, `2·|x|` minus
its sine mean, at 48 kHz with no oversampling and no anti-aliasing. One
stage instead of three (strongest **H2**, H4 −14 dB) and its cusp series
folds freely (non-harmonic −75.0 dB re strongest over 0–20 kHz; v2 −107.7).
Monitor 0.59 → RMS 0.12868 (pair 0.12746 / 0.13042, +0.08 dB).

**Round 3 — `dusg_env_rc.wav`** (sound-r3 packet, dusg_env piece C): file
7's patch and level path (NTO SINE 1 kHz → VCA2 LIN, LEVEL 0 → OUT IN 2,
PAN 0.5, MASTER 0.4, same three transients), but VCA2's VC is fed by a
degraded "slope generator": an **RC capacitor-charge envelope** —
exponential approach toward 5 V over the 0.5 s rise (τ = 0.5 s / ln 100) and
exponential discharge over the 1.0 s fall (τ = 1.0 s / ln 100), the ADSR-style
curve the Serge's linear-by-design slopes are not. T05d rise deviation
15.7 % FS (OUT), 10–90 % times 0.28 / 0.60 s. RMS 0.0721 (pair 0.0765 /
0.0763, −0.5 dB).

**Round 2 — `ring_bipolar_leaky.wav`** (sound-r2 packet, ring piece C): the
`ring_bipolar` patch and level path (RING monitor tap, MASTER 0.8, through the
v2 OUT bus, monitor level 0.8 for ±0.5 dB RMS vs the pair), but the RING
monitor pointer re-aimed at a degraded "ring": the bare product X·Y of the
same NTO SINE / VCFQ HP jack values plus **−20 dB of each input leaking
straight through** (an untrimmed, unbalanced modulator). Y suppression 14.4
dB, X 0.9 dB — far outside 55–85 dB; v1 116.9 / 71.1, v2 72.3 / 59.2.
Round 4: re-rendered with the new full-level band patch (monitor 0.30,
pair ±0.1 dB): Y 14.4, X 11.3 dB.

**Round 1 — `nto_saw_1975_naive_zoh12k.wav`:**

`nto_saw_1975_naive_zoh12k.wav`: file 1's patch and level (NTO SAW monitor
tap, 0.5, MASTER 0.8, through the v2 OUT bus), but the monitor pointer is
re-aimed at a **naive non-band-limited phase-accumulator saw evaluated at
12 kHz and zero-order-held to 48 kHz**. A plain 48 kHz naive saw would be
near-identical to v1 (v1's saw *is* one), so the control is degraded further
to be unambiguously worst: −12.7 dBc in-band, −12.4 dBc full-band.

## Verification summary

| file | len (s) | peak | RMS(L) | v1 RMS(L) | Δ RMS | monitor lvl / master | drift |
|---|---|---|---|---|---|---|---|
| nto_saw_1975.wav | 5.00 | 0.2725 | 0.12361 | 0.12776 | −0.29 dB | 0.5 / 0.8 | 0 |
| nto_shape_sweep.wav | 8.00 | 0.1871 | 0.14169 | 0.14072 | +0.06 dB | 0.375 / 0.8 | 0 |
| vcm_mid_foldsweep.wav | 8.00 | 0.2892 | 0.14670 | 0.14718 | −0.03 dB | 0.48 / 0.8 | 0 |
| vcm_bot_full.wav | 5.00 | 0.2236 | 0.13042 | 0.12746 | +0.20 dB | 0.52 / 0.8 | 0 |
| vcfq_ping.wav | 8.00 | 0.1148 | 0.00621 | 0.00568 | +0.77 dB | 0.15 / 0.8 | 0 |
| dusg_cycle.wav | 5.00 | 0.2043 | 0.06509 | 0.06320 | +0.26 dB | 0.25 / 0.8 | 0 |
| dusg_env_shape.wav | 6.50 | 0.2222 | 0.07649 | 0.07633 | +0.02 dB | (IN 2 path) / 0.4 | 0 |
| dusg_env_shape_monitor_direct.wav | 6.50 | 0.0072 | 0.00413 | 0.00110 | +11.5 dB (both near-silent) | 0.25 / 0.8 | 0 |
| ring_sines.wav | 5.00 | 0.2881 | 0.12671 | 0.12613 | +0.04 dB | 0.35 / 0.8 | 0 |
| default_note60.wav | 4.50 | 0.6595 | 0.03436 | 0.03631 | −0.48 dB | defaults (0.53) | 0.2 |
| supplementary/ring_bipolar.wav | 5.00 | 0.1558 | 0.05270 | 0.05198 | +0.12 dB | 0.31 / 0.8 | 0 |
| supplementary/dusg_end_duty.wav | 7.50 | 0.2203 | 0.08359 | 0.07908 | +0.48 dB | 0.25 / 0.8 | 0 |
| control/ring_bipolar_leaky.wav | 5.00 | 0.1310 | 0.05236 | 0.05198 | +0.06 dB | 0.30 / 0.8 | 0 |
| control/dusg_env_rc.wav | 6.50 | 0.2203 | 0.07212 | 0.07633 | −0.49 dB | (IN 2 path) / 0.4 | 0 |
| control/vcm_bot_single_abs.wav | 5.00 | 0.2638 | 0.12868 | 0.12746 | +0.08 dB | 0.59 / 0.8 | 0 |

(Round 4 values. Rows unchanged since round 2/3 are byte-identical files.)

No NaN/Inf in any render (checked at write time).
