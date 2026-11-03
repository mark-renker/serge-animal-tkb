# Serge Animal TKB v2 — Redesign Plan (2026-08-31)

Companion to `gauntlet/GAUNTLET.md` (the frozen acceptance spec) and the four
research reports in `gauntlet/research/`. This document is the *how*.

## 1. What v2 is

v1 is a 350-line Faust program (`gen_dsp.py` → `SergeAnimalTKB.dsp`) built with
`faust2juce`: sonically a naive behavioral model, and its UI is an
auto-generated tab-and-slider wall where "patching" means picking sources from
31 dropdown menus. v2 rebuilds the same instrument — same module complement,
its own plugin identity (`aumu Sat2 Rnky`, "Serge Animal TKB v2", amended
2026-11-03 so v1 stays installed alongside) — as a hand-written
C++17/JUCE 8 plugin on the Paperface 50 framework, with real banana-cable
patching, the PF50 cream-panel look, and circuit-informed DSP.

The Faust v1 sources move to `legacy/` (still buildable, still the reference
for A/B sound comparisons); `demos/` stays as the v1 render reference.

## 2. DSP: v1 vs hardware, and the v2 algorithm per module

Full details: `research/current-plugin-spec.md` (v1 as-is),
`research/hardware-spec.md` (hardware targets + citations),
`research/dsp-techniques.md` (algorithms + CPU). Summary table — "v2" aims at
the hero tier throughout (estimate ≈ 12% of one M2 Max P-core, ±40%); the
frozen gate is ≥ 6× realtime (≤ 16.7% of a P-core, min-of-runs protocol) and
the foundation perf spike decides per-module tier downgrades as a plan
decision, not a gate failure:

| Module | v1 (naive) | Hardware truth | v2 algorithm |
|---|---|---|---|
| **NTO** | naive phase-accumulator, aliasing saw, hard-reset sync, morph = crossfade | triangle core 16 Hz–16 kHz, two 1V/oct ins, VC morph sine→'M', AC-coupled TZ linear FM, *soft* sync, pulse out (R*S) | phase-acc core with signed increment (exact TZFM), polyBLEP/BLAMP edges + corners, ADAA'd shaper LUT for the morph, soft-sync as bounded phase nudge (must NOT lock at large detune), AC-coupled lin-FM input, added PULSE output |
| **DUSG ×2** | pure-linear slew, `max(sig, active)` target, 4 ms END pulse | slopes **linear by design** (catalog); tanh current-limit gives only the soft landing at the target (Stinchcombe); curves come from feedback-patching OUT→rate CV; END/GATE is a **level** (high in rise + at bottom, low in fall, 5–95% duty in cycle); 1 ms–5 s, cycle to ≥4 kHz, non-retriggerable, gate-in = ASR | linear-drive slew core with tanh-limited approach (one tanh/sample), follow-diode knee, hardware-law END/GATE level with BLEP'd edges, 1V/oct rate CV, 2× OS only when rate reaches audio |
| **SSG** | ad-hoc smooth/stepped/comparator | smooth = VC lag w/ cycle, hold = track-and-hold; stepped = S&H with VC step-size limit; coupler comparator; CYCLE jack doubles as pulse out; hot outs swing near ±12 V rails | same topology with slew core shared with DUSG, comparator with BLEP edges, added cycle-pulse jack + hot (±10 V+) coupler output |
| **VCFQ** | ideal linear TPT SVF + input clip; fake 1-sample "ping" impulse | 2-pole SVF, simultaneous LP/BP/HP/N, AGC input compensation (~1/Q), trigger ping → damped sine, self-osc only via patching, integrator saturation limits amplitude | Zavalishin TPT/ZDF SVF at 2× OS with per-integrator tanh; AGC as 1/Q input gain law; ping = 0.5–1 ms shaped pulse injected at the BP integrator node |
| **VCM** | top = tanh, mid = `sin()` folder, bot = triangle wrap; no AA | top = VC "square-up" rounded clipping in HI, **linear VCA in LO**; **mid = odd-harmonic folder** (real folding: transfer turnarounds, centroid rises with drive); **bot = 3 cascaded full-wave rectifiers (even harmonics) + squared-up PWM-ish 2nd out** | Esqueda/Parker-style piecewise nonlinear folding cascade with 1st/2nd-order **ADAA** + 2× OS; correct odd/even sections; top LO/HI switch; bottom squared-up 2nd output; op-amp rail clip inside the ADAA kernel |
| **RING** | `t3(x·y·2)`; zero bleed | classic Serge ring is a **four-quadrant transconductance multiplier** (R6/CGS), NOT a diode ring; R*S R9: VC dry→AM→ring morph, ~80 dB carrier rejection, DC-coupled (unipolar carrier = VCA/AM) | tanh·tanh four-quadrant transconductance model, DC-coupled, explicit feedthrough trims (default −60..−80 dB), VC dry→AM→ring morph |
| **Noise/RVG** | white/pink + basic S&H | white/pink outs; S/H SOURCE = randomly-modulated ~400 Hz saw 0–5 V; RVG = SSG+noise patch | per-instance PCG32 streams; proper pink filter; S/H-source saw; RVG stepped+smooth with slew |
| **VCA ×2** | cubic "exp" | exponential 12 dB/V, sharp cutoff < 0.5 V, max gain 2× | fitted expo law with cutoff knee, soft LM13700-ish saturation at hot levels |
| **TKB** | 4×16 knobs, clock/reset, random mode; no touch, no pressure | touch pads: KV/pressure/gate follow the **pads** (not the running stage); RESET → last-touched stage; vertical 4-stage sequencer → 64 steps; per-stage pulses; R*S semitone KV + row-B glide | full model: clickable pads as engine-visible ctls (mouse = touch, click-Y or aftertouch = pressure), last-touched reset, vertical clock + ABCD out, RANDOM select, per-stage pulse jacks, semitone-calibrated KV distinct from row-A CV, GLIDE slew ctl on KV |
| **Analog layer** | none | unit-to-unit tolerance, thermal drift | SplitMix64 per-instance component tolerances + 2–3 summed Ornstein-Uhlenbeck drift processes at control rate; deterministic per seed (T17) |

Anti-aliasing strategy (from `dsp-techniques.md`): ADAA for static shapers,
polyBLEP/BLAMP for discontinuities, 2× oversampling only inside nonlinear
islands (VCFQ, VCM, ring, NTO shaper) — never global.

## 3. Architecture (reuse from PF50)

- `src/engine/EngineCore.h` — copied near-verbatim: `Module` base (jacks/ctls/
  leds, per-sample `tick()`), undirected cable list, `rebuild()` float*-feed
  wiring, ±12 V rail clamps, implicit 1-sample feedback via registration order.
- `src/engine/Modules*.h` — new Animal/TKB modules (seeded from PF50's
  PosSlew/NegSlew/SSG/VCF1973/Seq4 where applicable, upgraded to hero tier).
- `src/engine/System.h` — module registry; **registration order frozen at
  foundation** (it is audible: feedback delay points).
- `src/engine/DspUtils.h` — **frozen at foundation, shared by all module
  builders**: ADAA kernels (1st/2nd order), polyBLEP/polyBLAMP, halfband 2×
  oversampling wrapper, tanh approximation, SplitMix64/PCG32, OU drift
  process. Fan-out builders may use, never fork, these.
- `src/plugin/` — PF50 pattern with fixes: APVTS param per ctl (append-only
  IDs) but **human-readable names, real choice labels, and
  AudioProcessorParameterGroup per module**; block-rate ctl copy; SpinLock
  cable edits; ValueTree state = params + cables + **editor scale (float,
  single source of truth for zoom AND resize)** + **engine instance seed**
  (required for the bit-identical round-trip gates); mono last-note MIDI →
  KEY module; velocity scales the final mix (v1 contract); engine ticks
  unconditionally — fixes v1's voice-reclamation bug that killed generative
  patches. Output mixer keeps v1's **monitor bus** (source selector + level).
- `src/ui/` — `Look.h`/`Widgets.h` ported (house style), plus: `TooltipWindow`
  + per-widget tooltip strings, a zoom control (discrete steps writing the
  persisted scale; drag-resize writes the same value), TKB touch-pad widget
  (pads drive engine ctls via the PF50 MomentaryFlag pattern), aspect-locked
  resize (PF50's, wider range). Snapshot/UX-test targets need
  JUCE_MODAL_LOOPS_PERMITTED=1 and a logged-in GUI session (documented
  constraint).
- `tools/Snapshot.cpp` — headless editor→PNG for blind critics.
- `tests/GauntletTests.cpp` — JUCE-free suite implementing G2; separate small
  JUCE-linked target for G2u.
- CMake mirrors PF50 (uses `~/JUCE` 8.0.6, else FetchContent).

Default patch = default cable set reproducing v1's out-of-the-box voice:
KEY gate→DUSG1 trig, NTO var→VCM in, VCM mid→VCFQ in, VCFQ LP→VCA1 in,
DUSG1 out→VCA1 CV. A PATCHES menu ships the six v1 demo patches re-expressed
as cables.

## 4. Build phases (benchmark-loop phasing)

0. **v1 reference renders (before anything installs)**: render every sound-bar
   patch offline from the legacy Faust sources → `gauntlet/renders/v1/`
   (avoids the AU identity collision: v2 replaces v1 at `aumu Satk Rnky`).
1. **Foundation (serial, frozen before fan-out)**: repo restructure (Faust v1
   → `legacy/`), CMake, engine core port, `DspUtils.h`, System skeleton with
   **frozen module/jack/ctl lists** (incl. TKB pad-touch/pressure ctls),
   plugin shell passing a smoke build, per-module test-file registry +
   snapshot tool compiling, and a **perf spike** (representative stub costs)
   that informs each module's accuracy-tier choice against the T16 budget.
2. **Fan-out — DSP modules**: one builder per module family (NTO; DUSG+SSG
   shared slew core; VCFQ; VCM+RING; Noise/RVG+VCA; TKB+KEY), each delivering
   module code + its own `tests/t_<module>.h` green + render evidence.
   Genuinely disjoint files: shared primitives live in frozen DspUtils.h.
3. **Fan-out — UI**: panel layout spec from hardware photos → module panels,
   MainView, cables, zoom/tooltips/resize (G2u green).
4. **Judge loop**: hard gates first, then rubric-scored fresh critics (UI +
   sound) per GAUNTLET.md rev B — criteria-blind, negative controls; fix
   named gaps; fresh critics each round.
5. **Integration**: presets (six v1 demo patches as cables), README rewrite,
   auval + pluginval 1.0.4 + install, final full-system judge round,
   commit/push.

## 5. Risks / notes

- pluginval strictness 10 on a freshly-generated plugin often surfaces
  threading nits — budgeted for in the judge loop.
- VCFQ AGC exact law is fitted, not published: implement a monotonic
  compensation on the AGC input only, verify boundedness (T07), log the
  fitted law.
- TKB "pressure" from a mouse is an interpretation (click-Y + optional MIDI
  aftertouch); GLIDE as a knob instead of hardware's row-B routing is an
  interpretation; both logged in LOG.md.
- v1 state does not migrate (different parameter space) — v2.0.0, noted in
  README.
- AU identity collision: after the first v2 install, the v1 AU is unreachable
  at `aumu Satk Rnky` — hence phase 0 pre-renders all v1 references offline.
- If T16 (6× realtime) is threatened, per-module fallbacks are pre-named in
  dsp-techniques.md (good tier ≈ 3–4%); dropping 2nd-order ADAA to 1st and
  reducing VCFQ oversampling are the first two levers.
