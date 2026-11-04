# Sound-bar measurements — v1 vs v2 (phase 4 evidence)

Pre-registered bands (GAUNTLET.md "Sound bar" + hard gates T03/T05/T07/T08/T09)
measured on **both** the v1 reference WAVs (`v1/`) and the v2 renders (`v2/`)
by the **same code**: `tools/MeasureSound.cpp` → target **`satk_measure`**
(`./build/satk_measure gauntlet/renders`). It reads the 16-bit WAVs (left
channel, int16/32000) and reuses the gauntlet's own analysis primitives —
`t_nto::spectrum` (4-term Blackman-Harris FFT), `worstNonHarmonic`,
`peakNear`, `zcFreq`; `t_vcm_ring::harmonics` (Goertzel bank on whole periods),
`centroid`, `oddEvenEnergy`, `slopeReversals`, `ampSpectrum`;
`t_vcfq::decayTime`. Nothing was tuned after seeing results; the band verdicts
below are exactly what the tool printed.

Generated 2026-11-04 on branch `sound-bar-evidence` (base 89c647b). Gauntlet
still 200/200.

## Method per quantity

| piece | quantity | how (file → numbers) |
|---|---|---|
| nto_saw | T03 alias floors | 65536 samples from 0.5 s; f0 by interpolated BH peak; worst bin > 20 Hz from every harmonic k·f0 (DC incl.), in 0–6 kHz and 0–24 kHz, dB re the fundamental (T03 says ±20 Hz; the v1 manifest used ±25 Hz, hence −26.5 here vs its −27.0) |
| nto_shape | T04 centroid (extra) | per 1 s step, window 0.2–0.95 s, harmonics 1..100 at the refined f0, amplitude-weighted centroid (t_vcm_ring::centroid) |
| vcm_mid_fold | T08 odd count / odd:even / centroid | same per-step harmonic bank; step 8 (FOLD max) for count (≥ −40 dB re H1 AND re strongest, as the gate test) and odd:even energy |
| vcm_mid_fold | T08 transfer folding (proxy) | the gate's static DC-ramp cannot be run on a file; instead waveform extrema per period at max fold (slopeReversals, 2 % hysteresis): a sine through a curve with r reversals shows 2 + 2r extrema per period → r |
| vcm_bot | T08 even dominance | harmonics 1..100 over 0.5–4.5 s, even:odd energy |
| vcfq_ping | T07 decay vs Q | per 2 s Q step, each ping = first sample > 10 % of the step peak + 0.45 s window, `decayTime` (peak → −40 dB), median of the step's pings |
| dusg_cycle | T05e (proxy) | fraction of rising samples over 1–5 s (rise = fall dialed → 0.50) |
| dusg_env | T05d linearity | carrier envelope = max \|x\| per 1 ms block; each of 3 transients: least-squares line through the 10–90 % FS points of the rise, max deviation in % FS (the gate's "mid 80 %") |
| ring | T09 suppression | 131072-sample BH spectrum from 0.5 s; modulator f from the lower-sideband peak; weaker sideband vs the Y (~80 Hz) component and vs the X (300 Hz) component |
| supplementary | T09 morph, T05e duty | per-step BH spectra (X vs sidebands); END edges as large sample steps through the DC-blocked monitor |

Spectrograms (`spectrograms/{v1,v2,control}/`, plus `v2/supplementary/`):
`/opt/homebrew/bin/ffmpeg -i <wav> -lavfi
"showspectrumpic=s=1280x640:mode=combined:color=intensity:scale=log:fscale=lin:drange=120:win_func=hann:legend=1" <png>`
— identical settings for every file (linear frequency axis, dBFS colour scale, 120 dB range).

## Table (v1 | v2 | negative control)

IN/OUT = the pre-registered band; `—` = informational row (no band, or the
band is specified under different conditions).

| piece | gate | quantity | band | v1 | v1 | v2 | v2 | control | control |
|---|---|---|---|---|---|---|---|---|---|
| nto_saw | T03 | worst non-harmonic, 0–6 kHz (harmonics ±20 Hz excluded) | ≤ −60 dBc | -26.5 dBc @ 4550 Hz | **OUT** | -78.6 dBc @ 2575 Hz | IN | -12.7 dBc @ 4100 Hz | **OUT** |
| nto_saw | T03 | worst non-harmonic, 0–24 kHz | ≤ −30 dBc | -21.7 dBc @ 22325 Hz | **OUT** | -52.8 dBc @ 22925 Hz | IN | -12.4 dBc @ 8050 Hz | **OUT** |
| nto_saw | info | f0 (BH-FFT peak, interpolated) | 1975 Hz | 1974.998 Hz | — | 1974.998 Hz | — | 1974.999 Hz | — |
| nto_shape | T04 (extra) | harmonic centroid per shape step 1..8 (Hz) | strictly increasing | 223, 331, 460, 521, 472, 388, 341, 321 | **OUT** | 270, 295, 386, 429, 505, 661, 963, 1794 | IN | | |
| vcm_mid_fold | T08 | odd harmonics ≥ −40 dBc at max fold (count) | ≥ 5 (re H1 and re max) | 15 re H1 / 8 re max | IN | 30 re H1 / 9 re max | IN | | |
| vcm_mid_fold | T08 | odd:even energy at max fold | ≥ 12 dB | 71.7 dB | IN | 73.4 dB | IN | | |
| vcm_mid_fold | T08 | harmonic centroid per fold step 1..8 (Hz) | strictly increasing | 229, 239, 262, 321, 502, 689, 1154, 1646 | IN | 265, 256, 275, 322, 451, 825, 1217, 1899 | **OUT** | | |
| vcm_mid_fold | T08 (proxy) | transfer slope reversals at max fold (from waveform extrema) | ≥ 2 | 6.0 | IN | 6.0 | IN | | |
| vcm_mid_fold | info | worst non-harmonic at max fold, 220 Hz (T08's AA band is specified at 1 kHz) | (−50 dBc @ 1 kHz) | -86.3 dBc | — | -73.7 dBc | — | | |
| vcm_bot | T08 | even:odd energy at full depth | ≥ +12 dB | -73.0 dB | **OUT** | +36.0 dB | IN | | |
| vcm_bot | info | H1..H8 dB re strongest | — | -9.5, -92.0, -6.5, -98.7, -3.3, -81.0, -2.1, -84.8 | — | -38.4, -0.8, -40.4, 0.0, -61.1, -3.1, -36.5, -37.7 | — | | |
| vcfq_ping | T07 | ping decay peak→−40 dB, median per Q step (Q 2/6/15/35) | strictly increasing | 3.8, 10.7, 27.5, 64.4 ms | IN | 4.0, 10.8, 27.1, 64.1 ms | IN | | |
| vcfq_ping | info | mean ping peak per Q step (FS) | — | 0.107, 0.110, 0.111, 0.112 | — | 0.059, 0.089, 0.106, 0.114 | — | | |
| vcfq_ping | info | ring frequency, first Q=35 ping (zero crossings) | fc = 800 Hz | 800 Hz | — | 795 Hz | — | | |
| dusg_cycle | T05e (proxy) | rising fraction of the cycle, rise = fall dialed (only END-duty evidence in the pair set) | 0.50 ± 0.01 | 0.500 | IN | 0.500 | IN | | |
| dusg_cycle | info | cycle frequency | 200 Hz | 200.00 Hz | — | 200.00 Hz | — | | |
| dusg_env | T05d | mid-80 % rise deviation from a line (worst of 3) | ≤ 3 % FS | 0.28 % FS | IN | 0.85 % FS | IN | | |
| dusg_env | info | mid-80 % fall deviation from a line (worst of 3) | — | 0.26 % FS | — | 0.86 % FS | — | | |
| dusg_env | T05a (info) | rise / fall time (10–90 % ÷ 0.8), dialed 0.5 / 1.0 s | ±10 % | 0.497 s / 0.996 s | — | 0.492 s / 0.985 s | — | | |
| dusg_env_direct | info | slow envelope straight through the monitor bus | — | peak 0.0019, rms 0.00110 | — | peak 0.0072, rms 0.00414 | — | | |
| ring | T09 | suppression of Y (modulator, ~80 Hz) re weaker sideband | 55–85 dB | 111.6 dB | **OUT** | 63.6 dB | IN | | |
| ring | T09 | suppression of X (300 Hz sine) re weaker sideband | 55–85 dB | -10.4 dB | **OUT** | -8.6 dB | **OUT** | | |
| ring | info | modulator frequency (from sideband spacing) | ~80 Hz | 79.9 Hz | — | 79.9 Hz | — | | |
| default_voice | info | RMS 0.25–1.25 s (L) | — | 0.0770 | — | 0.0694 | — | | |
| default_voice | info | peak 3.0–4.5 s (tail) | — | 0.00000 | — | 0.00000 | — | | |

### v2-only supplementary (no v1 counterpart file)

| file (v2 only) | gate | quantity | band | v2 | v2 |
|---|---|---|---|---|---|
| ring_bipolar.wav | T09 | suppression of Y re weaker sideband (bipolar modulator) | 55–85 dB | 64.7 dB | IN |
| ring_bipolar.wav | T09 | suppression of X re weaker sideband (bipolar modulator) | 55–85 dB | 73.7 dB | IN |
| ring_bipolar.wav | info | modulator frequency | ~80 Hz | 79.9 Hz | — |
| ring_morph_sweep.wav | T09 | morph 9 steps: X falls, sidebands rise | strictly monotonic | X -16, -18, -22, -28, -98 / SB -126, -31, -25, -21, -19 dBFS (steps 0,2,4,6,8) | IN |
| dusg_end_duty.wav | T05e | END duty at rise/(rise+fall) = .05/.25/.50/.75/.95 | tracks ratio ±0.01 | 0.050, 0.250, 0.500, 0.750, 0.950 (worst err 0.000) | IN |

## Findings

**Pre-registered bands — scorecard (v2 IN / v1 OUT = band discriminates as intended):**

| band | v1 | v2 | discriminates? |
|---|---|---|---|
| T03 NTO alias floor 0–6 kHz ≤ −60 dBc | OUT (−26.5) | IN (−78.6) | yes |
| T03 NTO alias floor 0–24 kHz ≤ −30 dBc | OUT (−21.7) | IN (−52.8) | yes |
| T08 MID ≥ 5 odd harmonics ≥ −40 dBc | IN (15 / 8) | IN (30 / 9) | **no** — v1's ideal sin() folder also passes |
| T08 MID odd:even ≥ 12 dB | IN (71.7) | IN (73.4) | **no** — both symmetric folders |
| T08 MID centroid strictly rising over 8 fold steps | IN | **OUT** (265 → 256 Hz dip at step 2) | inverted — **v2 misses** |
| T08 MID transfer folds ≥ 2 (proxy) | IN (6.0) | IN (6.0) | **no** |
| T08 BOT even:odd ≥ +12 dB | OUT (−73.0) | IN (+36.0) | yes |
| T07 ping decay monotonic in Q | IN | IN | **no** — v1 already obeys the law (3.8/10.7/27.5/64.4 vs 4.0/10.8/27.1/64.1 ms) |
| T05d mid-80 % rise ≤ 3 % FS | IN (0.28 %) | IN (0.85 %) | **no** — v1's ramps are perfectly linear |
| T05e END duty (proxy, single 50 % point) | IN | IN | **no**, and only a proxy (see below) |
| T09 Y suppression 55–85 dB (manifest patch) | OUT (111.6, above band) | IN (63.6) | yes (via the upper "bleed is a feature" edge) |
| T09 X suppression 55–85 dB (manifest patch) | OUT (−10.4) | **OUT** (−8.6) | no — the patch is AM by construction (see below) |

**v2 misses**

1. **T08 MID centroid monotonicity — OUT.** Per-step centroid 265, 256, 275, 322,
   451, 825, 1217, 1899 Hz: a 9 Hz dip from FOLD step 1 to 2. Diagnosed (scratch
   probes, not committed): the NTO SINE is not a pure sine (H3 −36.9 dB, H5
   −46.3 dB, THD 1.55 % — inside T04's 2 %), and at minimum fold those residual
   harmonics dominate an amplitude-weighted centroid; the folder's gentle
   low-drive curvature then partly cancels them at step 2. Jack-level centroids
   of the same patch: VCA-0.8 feed 255, 245, 263 … (dip); NTO SINE direct at
   2.5 V 249, 255, 288 … (monotonic, barely); an ideal 2.0 V sine 220, 237,
   270 … (monotonic, as the gate test). So the module itself is monotonic, but
   the manifest patch (real NTO sine at 0.8 depth via a VCA) is not. Reported
   as OUT; nothing was retuned. The v1 manifest's own (different) centroid
   method put v1's dip at step 1 (308 → 295 Hz); with the shared method v1 is
   strictly rising — this quantity is sensitive to method at low fold.
2. **T09 X suppression on the manifest patch — OUT for both.** The manifest's
   modulator (DUSG2 cycling, 0..5 V) carries 2.5 V DC; v2's RING Y is
   DC-coupled by design (T09: "DC carrier → clean VCA/AM"), so the 300 Hz input
   passes as AM. The band presumes a bipolar modulator. With the DC stripped at
   patch level (`v2/supplementary/ring_bipolar.wav`, DUSG2 → VCFQ HP 20 Hz →
   RING Y) v2 lands IN on both (Y 64.7 dB, X 73.7 dB), and the morph sweep is
   monotonic. v1 has no counterpart for either.

**Bands that do not discriminate (v1 lands IN):** T08 MID odd count, odd:even
and transfer folding; T07 ping law; T05d linearity; the T05e proxy. These were
pre-registered as v1-out-of-band in GAUNTLET.md's wording, but v1's ideal
`sin()` folder, its linear ramps and its SVF ping already satisfy them. The
VCM-mid difference GAUNTLET.md anticipated is anti-aliasing (T08 −50 dBc @
1 kHz), which no v1 file exercises; at the 220 Hz fold sweep v1 is actually
*cleaner* (−86.3 dBc vs v2 −73.7 dBc), because v1's smooth `sin()` folder at
0.8 depth stays band-limited while v2's 6-turnaround rounded cascade is far
richer. The critic is the tiebreaker on those dimensions.

**Not measurable from the files as specified**

- **T05e END duty law**: no file in the v1 set taps END, and the only cycling
  render has rise = fall, so the 5–95 % duty law cannot be measured on the pair.
  The row above is a proxy (rising fraction of OUT at one ratio). v2 alone
  (`supplementary/dusg_end_duty.wav`, END → monitor) tracks 0.05/0.25/0.50/0.75/0.95
  with zero error at 0.001 resolution.
- **T09 morph monotonicity**: v1 has no morph control; v2-only supplementary.
- **T08 static transfer**: the gate's DC ramp cannot be recovered from a
  sine render; the extrema count is a proxy.
- **T05d on the monitor bus**: neither version's monitor passes a 0.5 s
  envelope (both OUT buses DC-block; v2 at 10 Hz). `dusg_env_shape.wav` (DUSG →
  VCA CV on a 1 kHz carrier) is the evidence for both; the `_monitor_direct`
  files only document the limitation (v2 peak 0.0072 / v1 0.0019).

**Level matching**: all pairs within ±1 dB RMS (`v2/MANIFEST.md`), except the
near-silent `_monitor_direct` evidence pair and default_voice's peak (v2 0.588
vs v1 0.407, note-window RMS within 0.9 dB; v2 is the untouched default, not level-trimmed).
