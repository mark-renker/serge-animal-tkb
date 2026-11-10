# Serge Animal TKB v2 — FROZEN ENGINE CONTRACT (foundation, 2026-08-31)

This document is the frozen interface contract for v2. It binds together:
`gauntlet/GAUNTLET.md` (rev B, the acceptance gates), `gauntlet/research/current-plugin-spec.md`
(the v1 behavioral contract), `gauntlet/research/hardware-spec.md` (hardware truth), and
`docs/REDESIGN_PLAN.md` (the how). Where this file and a research note disagree, THIS FILE WINS
for interfaces (jacks/ctls/indices/order); the gates win for behavior.

**THE APPEND-ONLY LAW.** Module ids, registration order, jack indices, ctl indices, the monitor
source list, and APVTS parameter IDs are **append-only forever**. Never insert, reorder, or
remove. Saved states serialize cables by (module string id, jack index) and params by
`{moduleId}_{ctlIndex}`; any reorder silently corrupts sessions (PF50 gotchas #3/#5).
New jacks/ctls/sources may be *appended* at the end of the respective list only.

The stubs in `src/engine/Modules*.h` register exactly the lists below and compile today.
Stage-2 builders fill `tick()` only — they must not touch registration, `DspUtils.h`,
`EngineCore.h`, or `System.h`.

---

## 1. Voltage conventions (frozen; GAUNTLET "voltage conventions")

| Class | Jack color | Range |
|---|---|---|
| Audio | **Black** | bipolar, ±2.5 V nominal (`kAudio`); "hot" outs may exceed |
| CV | **White** | 0..5 V DC (`kCV`); KEY KV is bipolar 1 V/oct (color is convention) |
| Pulses/gates | **Red** | 0/5 V; logic threshold **2.5 V** (`kLogicHi`, `isHigh`) |
| Hot outs | **Grey** | beyond-standard swing (SSG hot coupler: ≥ ±10 V) |
| Sync | **Purple** | edge-sensitive input |

System rails: **±12 V** (`kRails`); every summed jack input is rail-clamped (`Module::in`).
Plugin output normalization: `kAudio` volts = ±1.0 into the DAW.

v1→v2 unit translation (v1 "±1 unit" ≈ ±5 V): all v1 CV scalings restated in volts below.
v1 patch-slot attenuverters do not exist on v2 banana cables; per-input depth ctls and
sensible fixed scalings absorb them (default-patch loudness target: note-60 render
≈ rms 0.07 / peak 0.37, BUILD-NOTES v1 regression figure).

## 2. Engine architecture (frozen)

- `satk::Engine` per PF50: `Module` base (jacks/ctls/leds, per-sample `tick()`),
  undirected `Cable` list, `rebuild()` resolves cables into per-jack `float*` feeds,
  input summing = banana stacking, rail clamp on every `in()`.
- **No Bi jacks** (deliberate simplification vs PF50 — no router module). `connect()`
  requires exactly one Out and one In end.
- **Feedback**: no scheduler. `Engine::tick()` runs modules in registration order; reading a
  LATER-registered module's output sees the previous sample (implicit 1-sample delay).
  Any loop is legal (T13).
- **Instance seed** (T14/T17): `Engine::seed()`/`setSeed(uint64)`, default
  `kDefaultInstanceSeed = 0x53415432303236`. Engine derives each module's
  `moduleSeed = seedMix(instanceSeed, fnv1a(module.id))` and calls `Module::reseed()`;
  this happens in `setSeed()` and at the top of `Engine::prepare()`. Modules MUST derive
  every random quantity (tolerances, drift phase, noise stream seeds) deterministically
  from `moduleSeed` — same seed ⇒ bit-identical render; stream positions are never
  serialized.
- **Ctls** are normalized: continuous ctls 0..1 (module maps per §4); stepped ctls hold the
  step index in `.v`. `Ctl::labels` carries the host-facing choice labels.
- Shared primitives: `src/engine/DspUtils.h` — FROZEN, see its header comment. Engine-level
  helpers (`EdgeDetector`, `PulseGen`, `DCBlock`) live in EngineCore.h.

## 3. Module registry (FROZEN registration order, ids, titles)

Order mirrors v1's source ordering; OUT registers last (same-sample master tap).

| # | id | title | class | jacks | ctls |
|---|---|---|---|---|---|
| 0 | `nto` | NEW TIMBRAL OSCILLATOR | `NTO` | 12 | 5 |
| 1 | `dusg1` | SLOPE GENERATOR 1 | `DUSG` | 6 | 3 |
| 2 | `dusg2` | SLOPE GENERATOR 2 | `DUSG` | 6 | 3 |
| 3 | `ssg` | SMOOTH & STEPPED GENERATOR | `SSG` | 11 | 2 |
| 4 | `vcfq` | VARIABLE Q VCF | `VCFQ` | 10 | 6 |
| 5 | `vcm` | WAVE MULTIPLIERS | `VCM` | 12 | 5 |
| 6 | `ring` | RING MODULATOR | `RING` | 4 | 4 |
| 7 | `noise` | NOISE SOURCE | `NOISE` | 3 | 0 |
| 8 | `rvg` | RANDOM VOLTAGE GENERATOR | `RVG` | 5 | 1 |
| 9 | `tkb` | TOUCH ACTIVATED KEYBOARD SEQUENCER | `TKB` | 32 | 88 |
| 10 | `key` | KEY / MIDI | `KEY` | 4 | 1 |
| 11 | `vca1` | VCA 1 | `VCA` | 3 | 2 |
| 12 | `vca2` | VCA 2 | `VCA` | 3 | 2 |
| 13 | `out` | OUTPUT MIXER | `OUT` | 2 | 6 |

Totals: 113 jacks, 128 ctls (= 128 APVTS parameters).

Audible delay notes (accepted, frozen): with the default cables, KEY gate → DUSG1 trig and
VCM mid → VCFQ in each cross one sample of delay (KEY and VCM register after their readers),
exactly as v1's uniform one-sample matrix delay; NTO var → VCM and VCFQ → VCA1 → OUT are
same-sample.

## 4. Modules — FROZEN jack & ctl lists

Notation: `dir` In/Out; color per §1. "expo lo..hi" = `dsp::expmap(v, lo, hi)`.
Behavior lines cite the binding gates; algorithm choice inside a gate is builder freedom.

### 4.1 NTO — `nto` (gates T02, T03, T04; T17 drift bounds)

Jacks:

| idx | enum | name | dir | color | semantics |
|---|---|---|---|---|---|
| 0 | `jSine` | SINE | Out | Black | ±2.5 V, THD ≤ 2% at min shape CV (T04) |
| 1 | `jTri` | TRI | Out | Black | triangle core output |
| 2 | `jSaw` | SAW | Out | Black | band-limited saw (T03 floors) |
| 3 | `jVar` | VAR | Out | Black | VC waveshape morph sine→'M'/pulse-like |
| 4 | `jPulse` | PULSE | Out | Red | 0/5 V pulse (R*S addition; v2-new) |
| 5 | `jOctA` | 1V/OCT A | In | White | calibrated 1 V/oct (T02 ≤ 3 cents interval) |
| 6 | `jOctB` | 1V/OCT B | In | White | calibrated 1 V/oct |
| 7 | `jVcFreq` | VC FREQ | In | White | exp FM via `VC FREQ AMT`; full = ±0.4 oct/V (v1: ±2 oct/5 V) |
| 8 | `jLinFm` | LIN FM | In | Black | AC-coupled through-zero linear FM (T03) |
| 9 | `jVcLin` | VC LIN | In | White | FM index VCA: effective depth = clamp01(`LIN FM DEPTH` + V/5) |
| 10 | `jVcWave` | VC WAVE | In | White | shape = clamp01(`WAVESHAPE` + V/5) (v1 law) |
| 11 | `jSync` | SYNC | In | Purple | SOFT sync (locks near simple ratios, must NOT lock 20% off — T04) |

Ctls:

| idx | enum | name | mapping | default | unit |
|---|---|---|---|---|---|
| 0 | `cFreq` | FREQ | expo 16 Hz..16 kHz | 0.4045 → 261.63 Hz | Hz |
| 1 | `cFine` | FINE | ±100 cents (center 0.5) | 0.5 → 0 | cents |
| 2 | `cWave` | WAVESHAPE | 0..1 | 0.15 | — |
| 3 | `cVcFreqAmt` | VC FREQ AMT | attenuverter `av()`, full = ±0.4 oct/V | 0.5 → 0 | — |
| 4 | `cLinDepth` | LIN FM DEPTH | 0..1; full: Δf = (V/5)·3·f0 (v1 law) | 0.0 | — |

Frequency law: `f0 = expmap(cFreq)·2^(fineOct + octA_V + octB_V + av(cVcFreqAmt)·0.4·vcFreq_V)`,
via CV reachable ≥ 32 kHz finite/non-wrapped (T02). Instantaneous inc may go negative (TZFM).

### 4.2 DUSG — `dusg1`, `dusg2` (gate T05)

Jacks:

| idx | enum | name | dir | color | semantics |
|---|---|---|---|---|---|
| 0 | `jOut` | OUT | Out | White | 0..5 V envelope/slew output |
| 1 | `jEnd` | END/GATE | Out | Red | **LEVEL** (T05e): high during rise AND while OUT at bottom; low during fall; cycle duty = rise/(rise+fall), 5–95% |
| 2 | `jTrig` | TRIG | In | Red | full transient, non-retriggerable mid-transient (T05f) |
| 3 | `jIn` | IN | In | Black | DC follower (portamento/env-follower); gate in = ASR overriding TRIG (T05g) |
| 4 | `jVcRise` | VC RISE | In | White | 1 V/oct, +1 V = rise time halves (T05b, ±3%) |
| 5 | `jVcFall` | VC FALL | In | White | 1 V/oct, +1 V = fall time halves |

Ctls (expo 50 µs..10 s; effective time = knob·2^(−V), clamp 20 µs..30 s — v1 law):

| idx | enum | name | default dusg1 | default dusg2 |
|---|---|---|---|---|
| 0 | `cRise` | RISE | 0.2454 → 1 ms | 0.5659 → 50 ms |
| 1 | `cFall` | FALL | 0.6978 → 250 ms | 0.5659 → 50 ms |
| 2 | `cCycle` | CYCLE | 0 (OFF) — labels OFF/ON | 0 (OFF) |

Slopes linear by design, tanh-limited landing; curvature only via feedback patching
OUT→VC RISE/FALL (T05d). Cycle reaches ≥ 4 kHz (T05c).

### 4.3 SSG — `ssg` (gate T06)

Jacks:

| idx | enum | name | dir | color | semantics |
|---|---|---|---|---|---|
| 0 | `jSmOut` | SMOOTH OUT | Out | White | 0→4–5 V triangle when cycled |
| 1 | `jCycle` | CYCLE | Out | Red | pulse each cycle; patch → SMOOTH IN to self-cycle (doubles as pulse out) |
| 2 | `jSmIn` | SMOOTH IN | In | Black | bidirectional VC lag input |
| 3 | `jSmVcRate` | VC RATE | In | White | 1 V/oct, + = faster (rate < 1 Hz..4 kHz span) |
| 4 | `jHold` | HOLD | In | Red | high = exact freeze (track-and-hold) |
| 5 | `jStOut` | STEPPED OUT | Out | White | S&H output; static without SAMPLE pulses |
| 6 | `jStIn` | STEPPED IN | In | Black | |
| 7 | `jSample` | SAMPLE | In | Red | S&H clock |
| 8 | `jStVcRate` | ST VC RATE | In | White | VC step-size limiting, 1 V/oct (v2 addition; v1 had no stepped-rate CV) |
| 9 | `jCoupler` | COUPLER | Out | Red | comparator smooth>stepped, 0/5 V |
| 10 | `jHotCoupler` | HOT COUPLER | Out | Grey | same comparator, swings ≥ ±10 V |

Ctls: 0 `cSmRate` SMOOTH RATE (expo 50 µs..10 s, def 0.5659 → 50 ms);
1 `cStRate` STEPPED RATE (expo 50 µs..10 s, def 0.2454 → 1 ms).

### 4.4 VCFQ — `vcfq` (gate T07)

Jacks:

| idx | enum | name | dir | color | semantics |
|---|---|---|---|---|---|
| 0 | `jLp` | LP | Out | Black | 12 ± 2.5 dB/oct |
| 1 | `jBp` | BP | Out | Black | BP→IN self-osc patch stable, amplitude-limited |
| 2 | `jHp` | HP | Out | Black | |
| 3 | `jNotch` | NOTCH | Out | Black | simultaneous with the others |
| 4 | `jIn` | IN | In | Black | via GAIN, deliberately UNcompensated |
| 5 | `jAgc` | AGC IN | In | Black | Q-compensated only here; monotonic in Q, bounded (law fitted, logged in LOG.md) |
| 6 | `jOct` | 1V/OCT | In | White | calibrated 1 V/oct |
| 7 | `jVcF` | VC F | In | White | × `VC F AMT`; full = 1 V/oct (v1: 5 oct/5 V) |
| 8 | `jVcQ` | VC Q | In | White | × `VC Q AMT`; full = 0.6 oct/V on Q (v1: 3 oct/5 V) |
| 9 | `jTrig` | TRIG | In | Red | ping → damped sine at fc, decay monotonic in Q |

Ctls:

| idx | enum | name | mapping | default |
|---|---|---|---|---|
| 0 | `cFreq` | FREQ | expo 20 Hz..12 kHz | 0.5767 → 800 Hz |
| 1 | `cQ` | Q | expo 0.5..40 (CV extends to 60) | 0.3164 → 2 |
| 2 | `cGain` | GAIN | linear 0..2 | 0.5 → 1.0 |
| 3 | `cVcFAmt` | VC F AMT | attenuverter | 0.5 → 0 |
| 4 | `cVcQAmt` | VC Q AMT | attenuverter | 0.5 → 0 |
| 5 | `cPing` | PING LEVEL | 0..1 | 0.6 |

### 4.5 VCM — `vcm` (gate T08)

Jacks:

| idx | enum | name | dir | color | semantics |
|---|---|---|---|---|---|
| 0 | `jTopOut` | TOP OUT | Out | Black | |
| 1 | `jTopIn` | TOP IN | In | Black | |
| 2 | `jVcTop` | VC TOP | In | White | drive/gain CV of the top section |
| 3 | `jMidOut` | MID OUT | Out | Black | odd-harmonic folder (≥ 2 transfer turnarounds — T08) |
| 4 | `jMidIn1` | MID IN 1 | In | Black | two inputs mix before processing (hardware) |
| 5 | `jMidIn2` | MID IN 2 | In | Black | |
| 6 | `jVcFold` | VC FOLD | In | White | 0.4 oct/V of fold gain (v1: ±2 oct/5 V) |
| 7 | `jBotOut` | BOT OUT | Out | Black | even-harmonic rectifier cascade, level-compensated |
| 8 | `jBotSq` | BOT SQ OUT | Out | Black | "squared-up" PWM-like second output (distinct — T08) |
| 9 | `jBotIn1` | BOT IN 1 | In | Black | |
| 10 | `jBotIn2` | BOT IN 2 | In | Black | |
| 11 | `jVcBot` | VC BOT | In | White | rectifier-depth CV |

Ctls:

| idx | enum | name | mapping | default |
|---|---|---|---|---|
| 0 | `cTopMode` | TOP MODE | 2-way: "LO (VCA)" / "HI (SQUARE-UP)"; LO = LINEAR VCA (gain linear in CV, r² > 0.99) | 1 (HI) |
| 1 | `cTopDrive` | TOP DRIVE | expo 1×..10× | 0.301 → 2× |
| 2 | `cFold` | FOLD | 0..1; fold gain 0.5×·2^(4·v) (v1: 0.5×..8×, cap 16×) | 0.3 |
| 3 | `cBias` | BIAS | ± (center 0.5 = 0 V DC into folder) | 0.5 |
| 4 | `cBotDepth` | BOT DEPTH | 0..1 rectifier depth | 0.3 |

Anti-aliasing: folded 1 kHz @ 48 kHz ⇒ aliases ≤ −50 dBc (T08; ADAA + 2× OS per plan).

### 4.6 RING — `ring` (gate T09)

Jacks: 0 `jOut` OUT (Out, Black); 1 `jX` X IN (In, Black, signal); 2 `jY` Y IN (In, Black,
carrier, DC-coupled — DC carrier = clean VCA/AM, T09); 3 `jVcMorph` VC MORPH (In, White,
morph = clamp01(`MORPH` + V/5)).

Ctls:

| idx | enum | name | mapping | default |
|---|---|---|---|---|
| 0 | `cGain` | GAIN | expo 0.5×..4× | 0.3333 → 1× |
| 1 | `cMorph` | MORPH | 0 dry → 0.5 AM → 1 ring (monotonic) | 1.0 (v1 = pure ring) |
| 2 | `cTrimX` | X BLEED TRIM | 0 = calibrated min (≤ −80 dB); def band −60..−80 dB; max ≥ −50 dB audible bleed | 0.15 |
| 3 | `cTrimY` | Y BLEED TRIM | same law, carrier feedthrough; carrier suppression ≥ 55 dB at nominal | 0.15 |

### 4.7 NOISE — `noise` (gate T10)

Jacks: 0 `jWhite` WHITE (Out, Black, ±2.5 V, flat ±6 dB 100 Hz–6.4 kHz);
1 `jPink` PINK (Out, Black, −3 ± 1.5 dB/oct); 2 `jShSrc` S/H SOURCE (Out, White,
0..5 V randomly-modulated ~400 Hz saw — equi-probable S&H input, no autocorrelation
peak > 0.5 beyond 10 ms). Ctls: none. RNG: PCG32 streams from `moduleSeed`
(white and S/H on separate stream ids).

### 4.8 RVG — `rvg` (gate T10)

Jacks: 0 `jStepped` STEPPED (Out, White, 0..5 V, changes ONLY on trigger);
1 `jSmooth` SMOOTH (Out, White, slewed version — linear glide per hardware);
2 `jPulse` PULSE (Out, Red, internal random-timing clock); 3 `jTrig` TRIG (In, Red,
external trigger ORed with internal clock); 4 `jVcRate` VC RATE (In, White, 1 V/oct on
internal clock).
Ctls: 0 `cRate` RATE (expo 0.05..50 Hz, def 0.6344 → 4 Hz).

### 4.9 VCA — `vca1`, `vca2` (gate T11)

Jacks: 0 `jOut` OUT (Out, Black); 1 `jIn` IN (In, Black); 2 `jVc` VC (In, White).
Ctls: 0 `cLevel` LEVEL (0..1, adds 0..5 V to the VC sum, **default 0** — v1 contract: the
default patch is audible only because DUSG1 opens VCA1); 1 `cResponse` RESPONSE
(2-way "LIN"/"EXP", **default 1 = EXP**).
EXP law (hardware): ≈ 12 dB/V over the top of the range (±1.5 dB), ≤ −60 dB below 0.5 V,
max gain +6 dB ± 1 dB at 5 V; click bound per T11. LIN law: gain = clamp01(level + V/5)
(v1 compatibility), same ×2 ceiling.

### 4.10 TKB — `tkb` (gate T12)

Jacks:

| idx | enum | name | dir | color | semantics |
|---|---|---|---|---|---|
| 0 | `jClock` | CLOCK | In | Red | advance one stage, wrap at LENGTH |
| 1 | `jVertClock` | VERT CLOCK | In | Red | row A→B→C→D on ABCD out (64-step trick) |
| 2 | `jReset` | RESET | In | Red | jump to **last-touched** stage (NOT stage 1) |
| 3 | `jVertReset` | VERT RESET | In | Red | row → A |
| 4 | `jRandom` | RANDOM | In | Red | high arms random select (ORed with RANDOM ctl) |
| 5 | `jUpDown` | UP/DOWN | In | Red | high = step downward (wrap 1→LENGTH) |
| 6 | `jHold` | HOLD | In | Red | high = freeze |
| 7 | `jA` | A | Out | White | active stage row-A knob × RANGE volts |
| 8 | `jB` | B | Out | White | row B × RANGE |
| 9 | `jC` | C | Out | White | row C × RANGE |
| 10 | `jD` | D | Out | White | row D × RANGE |
| 11 | `jAbcd` | ABCD | Out | White | (active row × active stage) × RANGE |
| 12 | `jKv` | KV | Out | White | **last-touched pad**, semitone-calibrated: (pad−1)/12 V; GLIDE-slewed; distinct from row-A CV |
| 13 | `jPressure` | PRESSURE | Out | White | 0..5 V while a pad is touched (PRESSURE ctl × 5 V), 0 otherwise |
| 14 | `jGate` | GATE | Out | Red | GATE LEN gate on each step advance (v1 semantics) |
| 15 | `jKp` | KP | Out | Red | key pulse: high while any pad is touched |
| 16..31 | `jStage1`+n | ST 1..ST 16 | Out | Red | one-hot: high while stage n+1 active |

Ctls:

| idx | enum | name | mapping | default |
|---|---|---|---|---|
| 0 | `cRun` | RUN | OFF/ON (internal clock) | 0 |
| 1 | `cRate` | RATE | expo 0.1..30 Hz | 0.5252 → 2 Hz |
| 2 | `cLength` | LENGTH | 16-way "1".."16" (v1-ism, logged) | 7 → "8" |
| 3 | `cRandom` | RANDOM | OFF/ON (armed) | 0 |
| 4 | `cGateLen` | GATE LEN | expo 5 ms..1 s | 0.5654 → 100 ms |
| 5 | `cRange` | RANGE | linear 0.5..5 oct — scales row outs A–D/ABCD (v1-ism interpretation, logged) | 0.3333 → 2 oct |
| 6 | `cGlide` | GLIDE | 0..1 s slew on KV (interpretation of R*S row-B glide, logged) | 0 |
| 7..22 | `cRowA1`+n | A1..A16 | 0..1 knob (× RANGE volts at the outs) | 0.5 |
| 23..38 | `cRowB1`+n | B1..B16 | 〃 | 0.5 |
| 39..54 | `cRowC1`+n | C1..C16 | 〃 | 0.5 |
| 55..70 | `cRowD1`+n | D1..D16 | 〃 | 0.5 |
| 71..86 | `cPad1`+n | PAD 1..PAD 16 | momentary (steps=2) — **engine-visible touch pads**: touch emits KP gate + pressure and selects that stage; records "last touched" | 0 |
| 87 | `cPressure` | PRESSURE | 0..1, written by UI (click-Y / channel aftertouch — interpretation, logged) | 0 |

RANDOM armed + clock/reset pulse ⇒ random stage within LENGTH (uniform, PCG32 stream from
`moduleSeed`). LEDs: 16 stage + 4 row.

### 4.11 KEY — `key` (MIDI bridge; T15 contract)

Jacks: 0 `jKv` KV (Out, White, **1 V/oct, note 60 = 0.000 V exactly**, GLIDE-slewed);
1 `jGate` GATE (Out, Red, 0/5 V while any note held); 2 `jVel` VEL (Out, White, 0..5 V,
holds LAST note-on velocity — init 4.0 V ≡ v1 default gain 0.8); 3 `jClk` CLK (Out, Red,
host-transport 16th-note pulses, 15% duty, phase reset on transport start).
Ctls: 0 `cGlide` GLIDE (0..0.5 s one-pole portamento on KV, v1 law, def 0).
Fields written per-sample by the plugin (or tests): `pitchV, gateV, velV, clkV`.

### 4.12 OUT — `out` (output mixer + monitor bus; T01, T15)

Jacks: 0 `jIn1` IN 1 (In, Black); 1 `jIn2` IN 2 (In, Black).

Ctls:

| idx | enum | name | mapping | default |
|---|---|---|---|---|
| 0 | `cPan1` | PAN 1 | equal-power sqrt pan (v1) | 0.35 |
| 1 | `cPan2` | PAN 2 | 〃 | 0.65 |
| 2 | `cMonSrc` | MONITOR | 43-way source selector (§5) | 0 (OFF) |
| 3 | `cMonLvl` | MONITOR LEVEL | 0..1 (monitor mixed at ×0.7, v1) | 0.4 |
| 4 | `cMaster` | MASTER | 0..1 | 0.53 (was 0.8; amended 2026-11-03, see LOG) |
| 5 | `cDrift` | ANALOG DRIFT | 0..1 global analog-layer depth (static tolerance + thermal drift); **0 = ideal/disabled** (T02/T08 measure at 0; T17 at > 0) | 0.2 |

Mix law (implemented, v1-faithful): `mL = (in1·√(1−pan1) + in2·√(1−pan2) + mon·monLvl·0.7)
· master · vel`, mirrored for R; per-channel DC block then soft limit (`fastTanh`),
`kAudio` V → ±1.0 to the DAW. `vel` = KEY VEL / 5 (velocity scales the FINAL mix — v1
contract; hardwired pointer set by `System`, not patchable). Monitor makes ANY selected
source audible (T15). Exactly 0.0 out when nothing is connected and monitor is OFF (T01).

## 5. Monitor source list (FROZEN indices; append-only)

Indices 0..30 are v1's source list verbatim; 31+ are v2 additions.

```
 0 OFF           11 VCFQ LP        22 TKB A          33 SSG CYCLE
 1 NTO SINE      12 VCFQ BP        23 TKB B          34 SSG HOT COUPLER
 2 NTO SAW       13 VCFQ HP        24 TKB C          35 VCFQ NOTCH
 3 NTO VAR       14 VCM TOP        25 TKB D          36 VCM BOT SQ
 4 DUSG1 OUT     15 VCM MID        26 TKB GATE       37 NOISE S/H
 5 DUSG1 END     16 VCM BOT        27 KEY KV         38 RVG PULSE
 6 DUSG2 OUT     17 RING           28 KEY GATE       39 TKB ABCD
 7 DUSG2 END     18 NOISE WHITE    29 VCA1           40 TKB KV
 8 SSG SMOOTH    19 NOISE PINK     30 VCA2           41 TKB PRESSURE
 9 SSG STEPPED   20 RVG STEPPED    31 NTO TRI        42 TKB KP
10 SSG COUPLER   21 RVG SMOOTH     32 NTO PULSE
```

Wiring: `System::wireMonitor()` fills `OUT::monSrc` with jack-value pointers in this order.

## 6. Default cable set (FROZEN; `System::patchDefaults()`, called by the plugin ctor)

Cables 1–5 are v1's five default matrix connections (v1 amounts 1.0/0.8/0.8/1.0/1.0 —
amounts are absorbed into module input calibration; loudness regression target §1);
cables 6–7 replace v1's hardwired VCA→pan-mixer path; cable 8 restores v1's hardwired
MIDI pitch → NTO.

| # | from | to |
|---|---|---|
| 1 | `key.jGate` | `dusg1.jTrig` |
| 2 | `nto.jVar` | `vcm.jMidIn1` |
| 3 | `vcm.jMidOut` | `vcfq.jIn` |
| 4 | `vcfq.jLp` | `vca1.jIn` |
| 5 | `dusg1.jOut` | `vca1.jVc` |
| 6 | `vca1.jOut` | `out.jIn1` |
| 7 | `vca2.jOut` | `out.jIn2` |
| 8 | `key.jKv` | `nto.jOctA` |

Cable 8 (amendment 2026-11-05, Mark): v1 hardwired the MIDI key pitch into the NTO;
v2's frozen set had dropped it, so MIDI notes retriggered one pitch. KV is 0 V at
note 60, so every note-60 test/render is unchanged.

T15: MIDI note 60 on this patch ⇒ RMS ≥ 0.02 within 1 s; note-off decays below −60 dBFS;
removing cable 1 ⇒ exact 0.0; velocity scales the final mix; monitor auditions any source.

## 7. Plugin contract

- **Identity** (amended 2026-11-03): AU `aumu` / subtype `Sat2` / manufacturer `Rnky`;
  name "Serge Animal TKB v2"; company "Renky Denk"; bundle `com.renkydenk.SergeAnimalTKB2`;
  version 2.0.x; AU + VST3 + Standalone; universal (arm64 + x86_64) on macOS, x64 VST3 on Windows; `IS_SYNTH`, MIDI in. v1 keeps
  `aumu Satk Rnky` and stays installed alongside; v1 sessions keep opening v1.
- **Parameters**: one APVTS param per ctl, in module/ctl declaration order.
  ID = `{moduleId}_{ctlIndex}` (append-only). Name = human `{MODULEID} {CTL NAME}` (e.g.
  "NTO FREQ", "TKB PAD 3"). Continuous → `AudioParameterFloat` 0..1; stepped →
  `AudioParameterChoice` with the ctl's real labels. One `AudioProcessorParameterGroup`
  per module (group id = module id, group name = module title).
  Gotcha (inherited from PF50): `getRawParameterValue` on a choice returns the INDEX —
  correct for the engine; `setValueNotifyingHost` needs normalized 0..1.
- **Ctl transport**: block-rate copy (`applyParams`) of every raw param into `ctls[].v`;
  momentary flags (all steps==2 ctls, incl. the 16 TKB pads) override `.v = 1` while held.
- **Threading**: `juce::SpinLock engineLock`; UI-thread cable edits lock it; `processBlock`
  try-locks and outputs silence for a colliding block. `cableVer` atomic bumps for UI refresh.
- **MIDI** (mono last-note): note-on → `key->pitchV = (note−60)/12` V, gate 5 V,
  `velV = velocity·5` V (held until next note-on); note-off pops the held-note set (gate
  falls only when empty); all-notes-off clears. Host BPM → 16th pulses on `key->clkV`.
  **The engine ticks unconditionally** — no voice reclamation ever (fixes v1 generative bug).
- **State** (`ValueTree` root `"SATK"`): properties `editorScale` (double; the single source
  of truth for zoom AND drag-resize, U01/U02) and `seed` (hex string of the engine instance
  seed); children = APVTS state + `CABLES` (per cable: `a` module id, `ja` jack index, `b`,
  `jb`). Restore order: editorScale, params, then under the lock: seed → `setSeed`, cables.
- **Editor**: stage-1 = `GenericAudioProcessorEditor` placeholder; stage-3 replaces it with
  the PF50-style panel (zoom control, tooltips, aspect-locked resize) writing `editorScale`.

## 8. Test/verification contract (foundation slice)

- `satk_gauntlet` (no JUCE): registry runner `tests/GauntletTests.cpp` + `tests/Harness.h`
  (CHECK/GROUP, Rig, Goertzel, edgeRate, db, `measurePerf` = T16 protocol: ≥ 10 s render,
  min over ≥ 5 runs, cycles/sample via steady_clock @ assumed 3.504 GHz P-core; the GATE
  quantity is realtime ratio ≥ 6), plus `EngineBlob` save/load = the engine-level T14
  round-trip protocol (ctls + cables + seed; two fresh engines must render bit-identical
  512 samples).
- Per-module files (one per stage-2 builder, disjoint): `t_nto.h`, `t_dusg_ssg.h`,
  `t_vcfq.h`, `t_vcm_ring.h`, `t_noise_vca.h`, `t_tkb_key.h` — registration hooks today.
- `t_system.h` live at foundation: T00 DspUtils sanity, T01 silence, T13 scaffolding
  (`allFiniteWithinRails`), T14 (green), T16 protocol + stub perf spike.
- Stub perf baseline (this machine, Release, min-of-5×10 s): ~915× realtime,
  ~80 cycles/sample — the entire graph/param/mixer overhead is ~1.3% of the 6×-realtime
  budget (12,114 cycles/sample); per-module DSP budgets are unconstrained by the framework.

## 9. Logged interpretations (for gauntlet/LOG.md)

1. TKB LENGTH knob and RANGE knob are v1-isms kept for contract continuity (hardware uses
   pulse-out→RESET patching and fixed 0–5 V rows).
2. TKB GLIDE as a knob (hardware: row-B-sets-glide with GLIDE switch) — interpretation.
3. TKB pressure from mouse click-Y / MIDI channel aftertouch — interpretation.
4. TKB KEYS/KEYBOARD independence switch: out of scope (pads always select their stage).
5. RING bleed trims: 0 = calibrated (R*S "80 dB rejection"), default 0.15 lands in the
   −60..−80 dB "subtle feature" band, max ≥ −50 dB — resolves T09's trim wording.
6. VCA LIN mode kept for v1's linear law; hardware law is the EXP default.
7. Default cables 6–7 (VCA→mixer) make v1's hardwired output path patchable.
8. `kAssumedPCoreHz = 3.504 GHz` for the cycles/sample readout; the binding gate is the
   realtime ratio.

## 10. Perf spike (foundation stage 2, 2026-08-31)

Purpose (GAUNTLET T16 / REDESIGN_PLAN §2): before the stage-2 module builders
choose accuracy tiers, load every stub with REPRESENTATIVE hero-tier costs and
measure against the 6×-realtime budget. Tool: `tools/PerfSpike.cpp` →
`satk_perfspike` (scratch target, no JUCE, kept out of the gates; the cost
subclasses override only the stubs' `tick()`/`prepare()`/`reseed()` —
registration untouched — and are throwaway).

**Cost model** (per REDESIGN_PLAN §2): a tanh-class nonlinearity per active
signal path; 2× oversampling islands (23-tap halfband up+down) on VCFQ, all
THREE VCM sections, RING, and the NTO VAR shaper; ADAA1(tanh) on NTO/VCM-top,
ADAA2(hard-clip) inside the VCM-mid fold kernel; per-integrator tanh TPT SVF in
the VCFQ island; polyBLEP/BLAMP on NTO edges; PCG32 streams for NOISE/RVG/TKB;
control-rate OU drift (2 processes) on NTO; full TKB stage logic with all 32
jack writes; the real OUT mixer with the monitor path active. Full graph =
frozen registration order + default patch + 16 extra cables so every module
processes live signals ("all modules ticking").

**Protocol**: T16 (Release, ≥ 10 s render per run, min over 5 runs full-system
/ 3 runs per-module, cycles/sample at the assumed 3.504 GHz P-core clock).
Machine: Apple M2 Max, macOS 26.6.2, Apple clang (Xcode 26.6), arm64.
Budget at the 6× gate: **12,167 cycles/sample**.

**Results**:

| item | cyc/sample | % of 6× budget | tier recommendation |
|---|---|---|---|
| stub System (graph overhead) | 80 | 0.7 % | — (framework cost, incl. virtual dispatch + feeds + real OUT mixer) |
| NTO | 414 | 3.4 % | **hero** (BLEP/BLAMP core + 2×OS ADAA shaper + OU drift) |
| DUSG ×2 | 81 each | 0.7 % each | **hero** (tanh-landing slew; add 2×OS at audio rate if needed — headroom is ample) |
| SSG | 101 | 0.8 % | **hero** (dual slew cores + comparator + hot out) |
| VCFQ | 352 | 2.9 % | **hero** (2×OS TPT SVF, per-integrator tanh) |
| VCM | 552 | 4.5 % | **hero** (heaviest module: 3 sections × 2×OS islands, ADAA1+ADAA2) |
| RING | 188 | 1.5 % | **hero** (2×OS tanh·tanh four-quadrant + bleed) |
| NOISE | 17 | 0.1 % | **hero** (PCG32 + 3-pole pink + S/H saw) |
| RVG | 33 | 0.3 % | **hero** |
| TKB | 55 | 0.5 % | **hero** (full stage logic, 32 jack writes) |
| KEY | 16 | 0.1 % | **hero** |
| VCA ×2 | 18 each | 0.1 % each | **hero** (exp law + soft sat) |
| sum of parts + overhead | ≈ 2,004 | 16.5 % | |
| **FULL representative system (T16 protocol)** | **2,240** | **18.4 %** | **32.6× realtime — 5.4× headroom over the 6× gate** |

**Decision**: hero tier fits comfortably for EVERY module — no per-module
downgrades required. Even a uniform 3× cost overrun across all modules would
still clear the 6× gate (predicted ≈ 10.9× realtime). The pre-named fallback
levers (ADAA2→ADAA1, reduced VCFQ oversampling, REDESIGN_PLAN §5) stay unused
unless a builder's real implementation measures far above its spike figure;
any module whose t_*.h-verified implementation exceeds **3× its spike figure**
must re-run `satk_perfspike`'s full-system protocol before merging.
