# Serge Animal TKB v2

A patchable Serge-style modular synthesizer voice for macOS (AU, VST3, standalone) and
Windows (VST3, standalone). It models the modules of a Random\*Source Serge **"Animal"**
system plus the **TKB** touch-activated keyboard sequencer, with real banana-cable patching
on a cream Serge-style panel.

v2 is a ground-up C++17 / JUCE 8 rewrite of the original Faust plugin (v1, kept in
[`legacy/`](legacy/)). Every module was rebuilt from a naive behavioural model into a
circuit-informed one, and verified against measured targets (see
[How it was verified](#how-it-was-verified)).

> **Install:** grab an installer from the
> [Releases page](https://github.com/mark-renker/serge-animal-tkb/releases). Step-by-step
> instructions, including Gatekeeper notes for the unsigned installer, are in
> [`packaging/INSTALL.md`](packaging/INSTALL.md).

v1 and v2 install side by side, under different plug-in identities, so old projects keep
opening v1:

| | AU code | Name in your DAW |
|---|---|---|
| v2 | `aumu Sat2 Rnky` | Renky Denk: Serge Animal TKB v2 |
| v1 | `aumu Satk Rnky` | Renky Denk: Serge Animal TKB |

---

## Quick start

1. Load **Serge Animal TKB v2** as an instrument (or open the standalone app and enable
   your MIDI input under *Options → Audio/MIDI Settings*).
2. Play a note. The **default patch** is a plucked voice:

   ```
   KEY gate ─► DUSG1 trig          KEY pitch ─► NTO 1V/OCT A
   NTO var ─► VCM mid ─► VCFQ LP ─► VCA1 ─► OUT
                         DUSG1 out ─► VCA1 CV
   ```

3. **Patching:** drag from any jack to another to add a cable. Stacking several cables on
   one input sums them, like banana plugs. **Right-click a jack** to remove one of its
   cables, or all of them. **CLEAR** in the header removes every cable but keeps the knobs.
4. Pick a starting point from the **PRESET** menu in the header (also exposed as host
   programs):

   | # | Preset | What it shows |
   |---|---|---|
   | 0 | Default Voice | the patch above |
   | 1 | Fold Sweep Voice | the default voice with NTO shape and mid fold swept by a slow cycling DUSG2 |
   | 2 | Filter Pings | no oscillator: TKB gates ping the VCFQ, row B tunes each step, RVG randomises Q |
   | 3 | Slope Bongos | "Serge bongos": DUSG1 cycling at audio rate, TKB row A bends its pitch, DUSG2 strikes it |
   | 4 | Ring Fold Drone | NTO × slope-oscillator ring mod through the bottom folder, swept by RVG (no MIDI needed) |
   | 5 | Random Melody | noise → stepped (clocked by the TKB) → NTO pitch, with slewed filter cutoff |
   | 6 | Wild Animal | VCFQ BP → folder → ring mod → back into the filter, steered by slewed random voltage |

   Presets 1–6 are v2 cable translations of the six v1 demo patches.

**Tips**

- **MONITOR** (output mixer) auditions any of 42 sources directly, without patching.
- **ANALOG DRIFT** (output mixer, default 0.2) sets the amount of oscillator drift and
  component tolerance. 0 is ideal digital. Each instance has a deterministic seed.
- **Zoom:** use the zoom menu in the header, or drag the window corner. Past about the size
  of your screen, the window stops growing and the panel scrolls.
- Hover over any control or jack for a tooltip.

## Modules

| Module | Serge original | What it does in v2 |
|---|---|---|
| **NTO** | New Timbral Oscillator | Triangle-core VCO with band-limited SINE/TRI/SAW/VAR/PULSE outs, two calibrated 1V/oct inputs, through-zero linear FM, VC waveshape (sine → 'M' → pulse) and soft sync. 2× oversampled core. |
| **DUSG 1 / 2** | Dual Universal Slope Generator | Linear rise/fall slopes with a soft landing. Use them as envelope, LFO (cycle, up to ~10 kHz), portamento/slew or ASR. END/GATE is a level output. 1V/oct rate CV. |
| **SSG** | Smooth & Stepped Generator | Slew/track-and-hold and S&H with VC rates, a CYCLE pulse, and normal and **hot** (±10 V) coupler outputs. |
| **VCFQ** | Variable Q VCF | 2-pole state-variable filter with simultaneous LP/BP/HP/NOTCH outputs, ping input, a Q-compensated AGC input, and stable self-oscillation. |
| **VCM** | Wave Multipliers | **Top:** VC rounded clipper (HI) or linear VCA (LO). **Mid:** odd-harmonic wavefolder. **Bottom:** full-wave rectifier cascade (2nd → 4th → 8th) with a squared-up second output. |
| **RING** | Ring modulator | Four-quadrant transconductance multiplier with a dry → AM → ring morph and subtle, trimmable bleed. A DC carrier makes it a clean VCA. |
| **NOISE / RVG** | Noise source, Random Voltage Generator | White, pink and S&H sources. Random clock with stepped and smooth outputs. |
| **TKB** | Touch Activated Keyboard Sequencer | 4 × 16 knob field with clickable pads. Clicking a pad sets its height as pressure. KV follows the last-touched pad with glide. Vertical clock gives ABCD 64-step sequences. Random, hold, up/down and per-stage pulses. |
| **KEY** | (MIDI bridge) | MIDI note → 1V/oct KV, gate, velocity and clock. |
| **VCA 1 / 2** | Ultimate-AP-style VCA | Exponential law (~12 dB/V), max +6 dB, declicked CV. |
| **OUT** | Output mixer | Two panned inputs, monitor bus, master and analog drift. Velocity scales the final mix. |

Voltage conventions: audio ±2.5 V (black jacks), CV 0–5 V (white), gates 0/5 V with a
2.5 V threshold (red), hot outputs (grey), sync (purple). Every input is clamped to the
±12 V rails, so any feedback patch is allowed.

Full specifications: [`docs/SPEC.md`](docs/SPEC.md) lists every jack and control.
[`docs/REDESIGN_PLAN.md`](docs/REDESIGN_PLAN.md) covers why each module is built the way
it is.

## Building from source

Requirements: CMake ≥ 3.22, a C++17 compiler (Xcode on macOS, MSVC on Windows). JUCE
comes from `~/JUCE` if present, otherwise it's fetched automatically.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release                       # universal arm64+x86_64 on macOS
cmake -B build -DCMAKE_BUILD_TYPE=Release -DSATK_ARCHS=arm64     # faster native-only dev build
cmake --build build -j
```

Outputs land in `build/SergeAnimalTKB_artefacts/Release/{AU,VST3,Standalone}/`. Release
packaging (`.pkg` installers, Windows zip, GitHub Actions) is described in
[`packaging/INSTALL.md`](packaging/INSTALL.md#for-maintainers-making-a-release).

### Tests and tools

| Target | What it is |
|---|---|
| `satk_gauntlet` | Headless DSP test suite: per-module gates, determinism, feedback stress, presets, perf |
| `satk_uxtests` | Headless editor tests: resize/zoom, state round-trip, tooltips, presets (needs a logged-in GUI session) |
| `satk_snapshot <out.png> [scale]` | Renders the editor to a PNG |
| `satk_render` / `satk_measure` | Renders the sound-bar reference patches and measures them (see below) |

```sh
./build/satk_gauntlet
./build/satk_uxtests_artefacts/Release/satk_uxtests
```

## How it was verified

v2 was built under a written acceptance contract, [`gauntlet/GAUNTLET.md`](gauntlet/GAUNTLET.md):

- **Hard gates.** The build has zero warnings in our sources. Every module meets measured
  targets in `satk_gauntlet`, for example: 1V/oct tracking within 3 cents; saw aliasing
  ≤ −60 dBc in band; DUSG timing within 10%; filter slope 12 dB/oct; wavefolder harmonic
  structure; ring carrier suppression ≥ 55 dB; bit-identical renders per seed. The plugin
  also passes `auval` and pluginval 1.0.4 at strictness 10 (AU and VST3), and runs at
  ≥ 6× realtime.
- **Sound bar.** Reference patches are rendered from both v1 and v2
  (`gauntlet/renders/`) and measured against bands registered before the build
  (`gauntlet/renders/MEASUREMENTS.md`). Fresh critics then judge them in blind A/B
  comparisons against the documented hardware behaviour, with a deliberately degraded
  control in every round.
- **UI bar.** Blind critics scored editor snapshots against the hardware panels.

Every interpretation, amendment and judge round is logged in
[`gauntlet/LOG.md`](gauntlet/LOG.md).

## Repository layout

```
src/engine/    DSP engine + modules (pure C++17, no JUCE)
src/ui/        panel, widgets, cables
src/plugin/    JUCE processor/editor glue, factory presets
tests/         satk_gauntlet + satk_uxtests
tools/         snapshot, render, measure, perf
docs/          SPEC (frozen interfaces), REDESIGN_PLAN
gauntlet/      acceptance contract, research notes, renders, judge packets, LOG
packaging/     installers, release workflow docs
legacy/        v1: the original Faust plugin, its build script and demos
```

## Not modeled (by design)

Session compatibility with v1 (different parameter space; v1 stays installable). Touch-plate
capacitance, warm-up drift, and the real STS panel's PCO/DTG/UAP module complement. Host
tempo sync, MPE and polyphony are candidates for a later version.
