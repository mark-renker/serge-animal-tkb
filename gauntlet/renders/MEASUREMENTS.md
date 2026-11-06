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
still 200/200. Re-generated 2026-11-04 on branch `vcm-sweep-fix` after the
VCM-mid knee fix. **Re-generated 2026-11-05 on branch `sound-r2`** (round-2
fixes for the sound-r1 blind critic, LOG.md "Sound bar round 2"): VCM BOT
octave cascade, RING linear core + bleed law, VCM MID knee/level
compensation + purer NTO SINE; plus measurement-bug fixes and the T09 band
amendment below. Gauntlet 210/210. **Re-generated 2026-11-06 on branch
`sound-r3`** (round-3 fixes for the sound-r2 blind critic, LOG.md "Sound bar
round 3"): VCM BOT rectifier cusps, VCM/RING 2-pole knob smoothing (FOLD
clicks), NTO VAR exact-sine start, VCFQ linear-below-5 V integrators (ping
tuning), VCA linear-below-5 V output stage (envelope bend + carrier H3); new
rows (T05d on the cycle ramps, click, carrier H3, VAR minimum THD), the ping
ring-frequency window moved past the pulse, and a new negative control
(`control/dusg_env_rc.wav`). Gauntlet 225/226 (the one failure is the known
preset-4 level, not in scope). The table is the tool's output verbatim.

**T09 band amendment (2026-11-05, approved by Mark):** the ring suppression
band is measured on the **bipolar (AC-coupled) modulator** render
(`supplementary/ring_bipolar.wav`), which now exists for **both** versions
(v1's rendered by `legacy/render_gauntlet.cpp`, same Faust toolchain; the ten
original v1 files re-render byte-identically). The v1 unipolar `ring_sines`
patch stays as listening material only (`ring_unipolar` rows, informational).

**Measurement-bug fixes (2026-11-05; no band moved):**
1. `vcm_bot`: f0 was refined at the nominal 220 Hz, where a full-depth BOT
   has no energy (H1 ≈ −110 dB), so the harmonic bank ran off-frequency and
   leakage read as "H1 −38.4 dB". f0 now comes from the strongest of
   H1..H16 (÷ its order); window moved to the steady state 1.0–4.5 s (past
   any onset and the bus DC blocker). Old v2 file re-measured with the fix:
   H1 −96, strongest **H10**, H8 −42 dB (what the critic heard).
2. `default_voice`: the 0.25–1.25 s RMS window was ~75 % silence for a
   ~0.2 s pluck; now RMS over the sounding span (first..last sample above 1 %
   of the file peak), plus the span length and the peak.
3. `dusg_cycle`: the old "T05e (proxy)" row read the MAIN output, not END.
   Relabelled as what it is (T05c info: slope symmetry), and T05e is measured
   on END itself in the new `dusg_end` pair (`supplementary/dusg_end_duty.wav`,
   both versions).

## Method per quantity

| piece | quantity | how (file → numbers) |
|---|---|---|
| nto_saw | T03 alias floors | 65536 samples from 0.5 s; f0 by interpolated BH peak; worst bin > 20 Hz from every harmonic k·f0 (DC incl.), in 0–6 kHz and 0–24 kHz, dB re the fundamental (T03 says ±20 Hz; the v1 manifest used ±25 Hz, hence −26.5 here vs its −27.0) |
| nto_shape | T04 centroid (extra) | per 1 s step, window 0.2–0.95 s, harmonics 1..100 at the refined f0, amplitude-weighted centroid (t_vcm_ring::centroid) |
| vcm_mid_fold | T08 odd count / odd:even / centroid | same per-step harmonic bank; step 8 (FOLD max) for count (≥ −40 dB re H1 AND re strongest, as the gate test) and odd:even energy |
| vcm_mid_fold | T08 transfer folding (proxy) | the gate's static DC-ramp cannot be run on a file; instead waveform extrema per period at max fold (slopeReversals, 2 % hysteresis): a sine through a curve with r reversals shows 2 + 2r extrema per period → r |
| vcm_mid_fold | min-fold THD, per-step RMS (info) | step 1 harmonic bank: sqrt(ΣH2..H40²)/H1; RMS of each step window in dBFS and its max−min span |
| vcm_bot | T08 even dominance | steady state 1.0–4.5 s; f0 = strongest interpolated peak among H1..H16 of 220 Hz ÷ its order (round-2 fix); harmonics 1..100, even:odd energy; info: H1..H16 re strongest, strongest order, worst non-harmonic re the strongest harmonic |
| vcfq_ping | T07 decay vs Q | per 2 s Q step, each ping = first sample > 10 % of the step peak + 0.45 s window, `decayTime` (peak → −40 dB), median of the step's pings; info ring frequency = zero crossings of the first Q 35 ping **5–45 ms after the onset** (round 3; r1/r2 started at the pulse, inside its forced part) |
| dusg_cycle | T05c (info) | fraction of rising samples of the MAIN OUT over 1–5 s (rise = fall dialed → 0.50): slope symmetry, not END |
| dusg_cycle | T05d (round 3) | every ramp between consecutive turning points of the 200 Hz triangle (1–5 s, ~1600 ramps): least-squares line through the points in 10–90 % of that ramp's swing, max deviation in % of the swing; worst rise / worst fall (the same line fit as `dusg_env`; the bus's 10 Hz DC blocker bends both versions identically) |
| dusg_end | T05e | `supplementary/dusg_end_duty.wav` (both versions): END → monitor, 5 rise/(rise+fall) ratios × 1.5 s; END edges as large sample steps through the DC-blocked monitor, duty = mean (fall edge − rise edge)/period |
| dusg_env | T05d linearity | carrier envelope = max \|x\| per 1 ms block; each of 3 transients: least-squares line through the 10–90 % FS points of the rise, max deviation in % FS (the gate's "mid 80 %"); info (round 3): carrier H3 re H1 over the 50 ms up to each rise peak (harmonic bank at 1 kHz), worst of 3 |
| vcm_mid_fold | click (info, round 3) | 4–12 kHz energy in 512-sample Hann windows around each 1 s FOLD step (max over ±256-sample offsets) vs the same band 0.3 s before/after; worst excess over the 7 steps (`t_vcm_ring::stepExcessDb`, same code as the new gate check) |
| nto_shape | T04 (info, round 3) | THD H2..H40 of step 1 (minimum shape) from the same harmonic bank |
| ring | T09 suppression | `supplementary/ring_bipolar.wav` (both versions; amended band patch): 131072-sample BH spectrum from 0.5 s; modulator f from the lower-sideband peak; weaker sideband vs the Y (~80 Hz) component and vs the X (300 Hz) component; info: Y leak and X ± 2·fY products re the strongest component 30 Hz–2 kHz |
| ring_unipolar | info only | same analysis on the unipolar `ring_sines.wav` (listening material) |
| default_voice | info | RMS over the sounding span (first..last |x| > 1 % of the file peak), peak, tail peak 3.0–4.5 s |
| supplementary | T09 morph | per-step BH spectra (X vs sidebands); v2 only (v1 has no morph) |

Spectrograms (`spectrograms/{v1,v2,control}/`, plus `{v1,v2}/supplementary/`):
`/opt/homebrew/bin/ffmpeg -i <wav> -lavfi
"showspectrumpic=s=1280x640:mode=combined:color=intensity:scale=log:fscale=lin:drange=120:win_func=hann:legend=1" <png>`
— identical settings for every file (linear frequency axis, dBFS colour scale, 120 dB range).

## Table (v1 | v2 | negative control)

IN/OUT = the pre-registered band; `—` = informational row (no band, or the
band is specified under different conditions). Three negative controls exist:
`nto_saw` C = round-1 control (`control/nto_saw_1975_naive_zoh12k.wav`),
`ring` C = round-2 control (`control/ring_bipolar_leaky.wav`: bare X·Y with
−20 dB of both inputs leaking through, ~20 dB carrier rejection),
`dusg_env` C = round-3 control (`control/dusg_env_rc.wav`: the same VCA
audition patch with the slope generator replaced by an RC capacitor-charge
envelope — exponential rise/fall instead of linear slopes).

| piece | gate | quantity | band | v1 | v1 | v2 | v2 | control | control |
|---|---|---|---|---|---|---|---|---|---|
| nto_saw | T03 | worst non-harmonic, 0–6 kHz (harmonics ±20 Hz excluded) | ≤ −60 dBc | -26.5 dBc @ 4550 Hz | **OUT** | -78.6 dBc @ 2575 Hz | IN | -12.7 dBc @ 4100 Hz | **OUT** |
| nto_saw | T03 | worst non-harmonic, 0–24 kHz | ≤ −30 dBc | -21.7 dBc @ 22325 Hz | **OUT** | -52.8 dBc @ 22925 Hz | IN | -12.4 dBc @ 8050 Hz | **OUT** |
| nto_saw | info | f0 (BH-FFT peak, interpolated) | 1975 Hz | 1974.998 Hz | — | 1974.998 Hz | — | 1974.999 Hz | — |
| nto_shape | T04 (extra) | harmonic centroid per shape step 1..8 (Hz) | strictly increasing | 223, 331, 460, 521, 472, 388, 341, 321 | **OUT** | 236, 331, 413, 450, 523, 680, 986, 1794 | IN | | |
| nto_shape | T04 (info) | THD at minimum shape (step 1, H2..H40) | ≤ 2 % | 0.36 % | — | 0.23 % | — | | |
| vcm_mid_fold | T08 | odd harmonics ≥ −40 dBc at max fold (count) | ≥ 5 (re H1 and re max) | 15 re H1 / 8 re max | IN | 36 re H1 / 11 re max | IN | | |
| vcm_mid_fold | T08 | odd:even energy at max fold | ≥ 12 dB | 71.7 dB | IN | 73.2 dB | IN | | |
| vcm_mid_fold | T08 | harmonic centroid per fold step 1..8 (Hz) | strictly increasing | 229, 239, 262, 321, 502, 689, 1154, 1646 | IN | 240, 247, 265, 314, 465, 888, 1289, 1993 | IN | | |
| vcm_mid_fold | T08 (proxy) | transfer slope reversals at max fold (from waveform extrema) | ≥ 2 | 6.0 | IN | 6.0 | IN | | |
| vcm_mid_fold | info | worst non-harmonic at max fold, 220 Hz (T08's AA band is specified at 1 kHz) | (−50 dBc @ 1 kHz) | -86.3 dBc | — | -67.5 dBc | — | | |
| vcm_mid_fold | info | THD at minimum fold (step 1, H2..H40) | — | 1.82 % | — | 0.81 % | — | | |
| vcm_mid_fold | info | RMS per fold step 1..8 (dBFS) | — | -20.6, -17.7, -15.5, -14.8, -17.0, -15.6, -17.1, -16.8 (span 5.8 dB) | — | -18.4, -18.2, -17.5, -16.0, -15.0, -16.2, -15.9, -17.6 (span 3.4 dB) | — | | |
| vcm_mid_fold | info | click at the FOLD steps: worst 4–12 kHz excess over steady state (7 steps) | — | +0.3 dB | — | -0.0 dB | — | | |
| vcm_bot | T08 | even:odd energy at full depth (steady state 1.0–4.5 s) | ≥ +12 dB | -85.3 dB | **OUT** | +95.6 dB | IN | | |
| vcm_bot | info | H1..H16 dB re strongest | — | -10, -122, -7, -101, -4, -120, -2, -105, -6, -97, -10, -97, -2, -94, -19, -89 | — | -123, -67, -123, -25, -119, -33, -99, 0, -99, -35, -115, -32, -130, -51, -117, -27 | — | | |
| vcm_bot | info | strongest harmonic | H8 (3 octave stages) | H17 | — | H8 | — | | |
| vcm_bot | info | worst non-harmonic re strongest harmonic, 0–24 kHz | — | -36.2 dB @ 18300 Hz | — | -82.1 dB @ 23800 Hz | — | | |
| vcfq_ping | T07 | ping decay peak→−40 dB, median per Q step (Q 2/6/15/35) | strictly increasing | 3.8, 10.7, 27.5, 64.4 ms | IN | 4.0, 10.8, 27.5, 64.4 ms | IN | | |
| vcfq_ping | info | mean ping peak per Q step (FS) | — | 0.107, 0.110, 0.111, 0.112 | — | 0.060, 0.089, 0.107, 0.114 | — | | |
| vcfq_ping | info | ring frequency, first Q=35 ping (zero crossings, 5–45 ms after onset) | fc = 800 Hz | 799.8 Hz | — | 799.9 Hz | — | | |
| dusg_cycle | T05c (info) | rising fraction of the MAIN OUT cycle, rise = fall dialed (slope symmetry; not END) | 0.50 ± 0.01 | 0.500 | IN | 0.500 | IN | | |
| dusg_cycle | info | cycle frequency | 200 Hz | 200.00 Hz | — | 200.00 Hz | — | | |
| dusg_cycle | T05d | cycle ramps: mid-80 % deviation from a line, worst rise / fall over all ramps 1–5 s | ≤ 3 % FS | rise 3.18 / fall 3.18 % FS (1598 ramps) | **OUT** | rise 0.84 / fall 0.84 % FS (1598 ramps) | IN | | |
| dusg_end | T05e | END duty at rise/(rise+fall) = .05/.25/.50/.75/.95 | tracks ratio ±0.01 | 0.200, 0.200, 0.200, 0.200, 0.200 (worst err 0.750) | **OUT** | 0.050, 0.250, 0.500, 0.750, 0.950 (worst err 0.000) | IN | | |
| dusg_env | T05d | mid-80 % rise deviation from a line (worst of 3) | ≤ 3 % FS | 0.28 % FS | IN | 0.31 % FS | IN | 15.68 % FS | **OUT** |
| dusg_env | info | mid-80 % fall deviation from a line (worst of 3) | — | 0.26 % FS | — | 0.30 % FS | — | 16.09 % FS | — |
| dusg_env | T05a (info) | rise / fall time (10–90 % ÷ 0.8), dialed 0.5 / 1.0 s | ±10 % | 0.497 s / 0.996 s | — | 0.494 s / 0.989 s | — | 0.282 s / 0.599 s | — |
| dusg_env | info | 1 kHz carrier H3 re H1, 50 ms up to each rise peak (worst of 3) | — | -49.6 dBc | — | -43.8 dBc | — | -43.6 dBc | — |
| dusg_env_direct | info | slow envelope straight through the monitor bus | — | peak 0.0019, rms 0.00110 | — | peak 0.0072, rms 0.00414 | — | | |
| ring | T09 | suppression of Y (modulator, ~80 Hz) re weaker sideband (bipolar modulator) | 55–85 dB | 116.9 dB | **OUT** | 72.3 dB | IN | 14.4 dB | **OUT** |
| ring | T09 | suppression of X (300 Hz sine) re weaker sideband (bipolar modulator) | 55–85 dB | 71.1 dB | IN | 59.3 dB | IN | 0.9 dB | **OUT** |
| ring | info | modulator frequency (from sideband spacing) | ~80 Hz | 79.9 Hz | — | 79.9 Hz | — | 79.9 Hz | — |
| ring | info | modulator (~80 Hz) leak re strongest component | — | -117.0 dB | — | -72.5 dB | — | -14.5 dB | — |
| ring | info | X ± 2·fY products (≈140 / 460 Hz) re strongest | — | -40.1 / -39.6 dB | — | -50.8 / -50.5 dB | — | -48.1 / -47.7 dB | — |
| ring_unipolar | info | Y suppression re weaker sideband (unipolar 0..5 V modulator: AM by construction) | (listening only) | 111.6 dB | — | 72.3 dB | — | | |
| ring_unipolar | info | X suppression re weaker sideband (unipolar modulator) | (listening only) | -10.4 dB | — | -8.1 dB | — | | |
| ring_unipolar | info | modulator frequency | ~80 Hz | 79.9 Hz | — | 79.9 Hz | — | | |
| ring_unipolar | info | modulator (~80 Hz) leak re strongest component | — | -122.0 dB | — | -80.4 dB | — | | |
| ring_unipolar | info | X ± 2·fY products (≈140 / 460 Hz) re strongest | — | -23.1 / -22.5 dB | — | -48.9 / -48.6 dB | — | | |
| default_voice | info | RMS over the sounding span (|x| > 1 % of peak), L | — | 0.1548 over 0.247 s | — | 0.1726 over 0.177 s | — | | |
| default_voice | info | peak (L) | — | 0.4066 | — | 0.6552 | — | | |
| default_voice | info | peak 3.0–4.5 s (tail) | — | 0.00000 | — | 0.00000 | — | | |

| file (v2 only) | gate | quantity | band | v2 | v2 |
|---|---|---|---|---|---|
| ring_morph_sweep.wav | T09 | morph 9 steps: X falls, sidebands rise | strictly monotonic | X -8, -10, -13, -19, -86 / SB -125, -33, -26, -23, -20 dBFS (steps 0,2,4,6,8) | IN |

## Findings

**Pre-registered bands — scorecard (round 3; v2 IN everywhere):**

| band | v1 | v2 | discriminates? |
|---|---|---|---|
| T03 NTO alias floor 0–6 kHz ≤ −60 dBc | OUT (−26.5) | IN (−78.6) | yes |
| T03 NTO alias floor 0–24 kHz ≤ −30 dBc | OUT (−21.7) | IN (−52.8) | yes |
| T04 (extra) NTO shape centroid rising | OUT | IN | yes |
| T08 MID ≥ 5 odd harmonics ≥ −40 dBc | IN (15 / 8) | IN (32 / 13) | no — v1's ideal sin() folder also passes |
| T08 MID odd:even ≥ 12 dB | IN (71.7) | IN (73.0) | no |
| T08 MID centroid strictly rising over 8 fold steps | IN | IN (241 → 1995 Hz) | no |
| T08 MID transfer folds ≥ 2 (proxy) | IN (6.0) | IN (6.0) | no |
| T08 BOT even:odd ≥ +12 dB | OUT (−85.3) | IN (+95.0) | yes |
| T07 ping decay monotonic in Q | IN | IN | no |
| T05d mid-80 % rise ≤ 3 % FS (dusg_env) | IN (0.28 %) | IN (0.31 %; r2 0.86 %) | no (the RC control is OUT, 15.7 %) |
| T05d cycle ramps ≤ 3 % FS (dusg_cycle, new r3) | OUT (3.18 %) | IN (0.84 %) | **yes** |
| T05e END duty tracks rise/(rise+fall) (now on END) | OUT (0.200 at every ratio: v1's END is a pulse) | IN (worst err 0.000) | **yes** (new pair) |
| T09 Y suppression 55–85 dB (bipolar pair) | OUT (116.9, above band) | IN (72.3) | yes (upper "bleed is a feature" edge) |
| T09 X suppression 55–85 dB (bipolar pair) | IN (71.1) | IN (59.2) | no |

**Round-3 changes visible in the table (v2, r2 → r3; LOG.md "Sound bar round 3"):**
- VCM BOT (finding 1): the stages are now square-law / pre-warped
  precision-rectifier blends — real cusps with symmetric crossings. H8 still
  dominant; the series is dense and slowly falling: H16 −27, H4/H12 −25/−32,
  H6/H10 −33/−35 dB re H8 (the 6/10 pair is the feed's small H3; an ideal
  sine gives −286), and content to the top of the band (r2: H16 −112, nothing
  above ~6 kHz). Ideal sine full depth: H16 −26, H24 −25, H32 −37, H48 −42,
  H64 −46, H96 −52 dB. Even:odd +95.6 dB. Alias floor: −102 dB re strongest
  over 0–20 kHz (ADAA2 on the tabulated kernel; r2's polynomial −119); the
  full-band row's −82.1 dB @ 23.8 kHz is cusp content above Nyquist leaking
  through the halfband's transition band.
- VCM MID (finding 2b): no clicks at the FOLD steps — worst 4–12 kHz excess
  +21.8 dB (r2 file) → −0.0 dB (v1 +0.3). Minimum-fold THD 1.28 → 0.81 %
  (the feed VCA is now linear at 2 V; the folder knee itself is unchanged —
  a W 0.6/0.7 rescan lowered it further but cut the real-sine centroid margin
  at low feed levels to 1.001). The 220 Hz max-fold info alias row moved
  −72.9 → −67.5 dBc re H1 (feed purity changed; the 1 kHz T08 gate test is
  unchanged at −56.9 dBc).
- NTO VAR (finding 2a): minimum shape is the exact sine (jack THD 1.55 % →
  0.000 %; in the file 0.23 %, the output bus limiter's own H3); centroid
  236, 331, 413, 450, 523, 680, 986, 1794 Hz (strictly rising).
- VCFQ ping (finding 3): Q 35 ring 795 Hz (r2, old window) / 797.3 Hz (r2
  file, new window) → **799.9 Hz** (theory fc·√(1−1/4Q²) = 799.92). Decays
  4.0, 10.8, 27.5, 64.4 ms (v1 3.8, 10.7, 27.5, 64.4). Peak-vs-Q law kept
  (interpretation, LOG.md).
- DUSG env (finding 4): rise deviation 0.86 → **0.31 %** FS (v1 0.28 %),
  fall 0.86 → 0.30 %; carrier H3 at the top of the rise −39 (critic) →
  −43.8 dBc in the file (v1 −49.6). At the VCA jack it is now −51.6 dBc =
  the NTO SINE's own documented residual (−52.0); the remaining ~8 dB in the
  file is the OUT bus `fastTanh` limiter (≈ −47.7 dBc at this 0.22 FS level;
  `ModulesOut.h` is outside the round's allowed files).
- DUSG cycle (finding 5): T05d on the cycle ramps: v1 3.18 % OUT, v2 0.84 %
  IN (new discriminating band).

**Round-2 changes visible in the table (v2, r1 → r2):**
- VCM BOT (finding 1): strongest harmonic H10 → **H8**; H8 −42 → 0 dB; H4
  −1 → −58, H6 −1 → −24, H10 0 → −24, H12 −2 → −53 dB; even:odd +85 → +95 dB
  (re-measured r1 file with the fixed f0); worst non-harmonic −61 → −119 dB re
  strongest. The ±24 dB H6/H10 pair is the cascade's ×8 phase sensitivity to
  the feed's own small H3 (NTO SINE + VCA soft-sat); an ideal sine gives
  H8 alone (others ≤ −71 dB). No fade-in: the r1 v2 file had none either
  (peak 0.221 in its first 50 ms); the ~150 ms fade-in is in v1's render
  (0.158 → 0.224). The −35.8/−36.2 dBc grid at 18.3 kHz is v1's file too.
- RING (finding 2): on the unipolar `ring_sines` listening patch the ~80 Hz
  leak went −72.2 → −80.4 dB re strongest and the X ± 2·fY products −32.2 →
  −48.9 dB; at the RING jack the products are −33 → −105 dB — the remaining
  −49 dB in the file is the shared OUT-bus soft limiter (`fastTanh`, not in
  scope). In the bipolar pair the residual −50.8 dB products come from the
  patch's AC coupler (VCFQ input stage soft clip on the offset triangle; the
  same coupler gives v1 −40 dB), not from RING.
- VCM MID (finding 3): min-fold THD in the render 2.77 % → 1.28 % (ideal
  2.5 V sine: 4.61 % → 0.85 %); centroid still strictly rising (241, 248,
  265, 311, 457, 870, 1252, 1995 Hz); per-step RMS span 6.6 → 3.6 dB
  (±1.8 dB). The info alias row at 220 Hz is −72.9 dBc (r1 −80.3).

**Bands that do not discriminate (v1 lands IN):** T08 MID odd count, odd:even,
centroid sweep and transfer folding; T07 ping law; T05d linearity on the
slow envelope (the cycle-ramp T05d row does discriminate); T09 X
suppression on the bipolar pair. v1's ideal `sin()` folder, linear ramps, SVF
ping and ideal multiplier already satisfy them; the critic is the tiebreaker
on those dimensions.

**Not measurable from the files as specified**
- **T09 morph monotonicity**: v1 has no morph control; v2-only supplementary.
- **T08 static transfer**: the gate's DC ramp cannot be recovered from a
  sine render; the extrema count is a proxy.
- **T05d on the monitor bus**: neither version's monitor passes a 0.5 s
  envelope (both OUT buses DC-block; v2 at 10 Hz). `dusg_env_shape.wav` (DUSG →
  VCA CV on a 1 kHz carrier) is the evidence for both; the `_monitor_direct`
  files only document the limitation.

**Level matching**: all pairs within ±1 dB RMS (`v2/MANIFEST.md`), except the
near-silent `_monitor_direct` evidence pair and default_voice (each version's
untouched default; sounding-span RMS v2 0.1726 vs v1 0.1548, +0.9 dB; peak
0.655 vs 0.407 — v2's pluck is shorter and has a higher crest factor; r2 was
0.1616 / 0.582: the round-3 VCA output stage no longer soft-compresses the
pluck's peaks, which reach ~5.6 V at the VCA).
