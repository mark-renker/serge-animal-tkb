#!/bin/bash
# Serge Animal TKB v2 — assemble a blind sound-bar critic packet.
#   tools/make_sound_packet.sh <round> [<piece>=<control stem>]
#   e.g. tools/make_sound_packet.sh sound-r2 ring=control/ring_bipolar_leaky
# Copies each v1/v2 render pair (+ spectrograms) into gauntlet/judge/<round>/
# under neutral names <piece>_A / <piece>_B with a per-piece random A/B
# assignment, adds the negative control (if given) as <piece>_C, and writes
# the key to gauntlet/judge/KEY-<round>.md (OUTSIDE the packet folder).
# PACKET.md is authored separately; its A/B/C measurement table comes from
#   satk_measure gauntlet/renders --key gauntlet/judge/KEY-<round>.md
# (sound-r4 used vcm_bot=control/vcm_bot_single_abs, one naive abs() rectifier;
# sound-r3 used dusg_env=control/dusg_env_rc, the RC-envelope control;
# sound-r1 used nto_saw=control/nto_saw_1975_naive_zoh12k and the unipolar
# ring_sines pair as the ring piece; since round 2 the T09 ring piece is the
# bipolar-modulator pair, amended 2026-11-05 with Mark's approval.)
set -euo pipefail
ROUND="${1:?round name, e.g. sound-r2}"
CONTROL="${2:-}"
CPIECE="${CONTROL%%=*}"; CSTEM="${CONTROL#*=}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
R="$ROOT/gauntlet/renders"
OUT="$ROOT/gauntlet/judge/$ROUND"
KEY="$ROOT/gauntlet/judge/KEY-$ROUND.md"
mkdir -p "$OUT"

# piece name -> render stem (identical relative path under v1/ and v2/)
PIECES="nto_saw:nto_saw_1975 nto_shape:nto_shape_sweep vcm_mid_fold:vcm_mid_foldsweep
vcm_bot:vcm_bot_full vcfq_ping:vcfq_ping dusg_cycle:dusg_cycle dusg_end:supplementary/dusg_end_duty
dusg_env:dusg_env_shape dusg_env_direct:dusg_env_shape_monitor_direct
ring:supplementary/ring_bipolar ring_unipolar:ring_sines default_voice:default_note60"

{
  echo "# KEY — blind sound-bar packet \`$ROUND\` (DO NOT give this file to the critic)"
  echo
  echo "Generated $(date -u +%Y-%m-%dT%H:%MZ) by tools/make_sound_packet.sh; A/B drawn"
  echo "per piece from bash \$RANDOM. Machine-readable lines (read by satk_measure --key):"
  echo
  echo '```'
} > "$KEY"

for pair in $PIECES; do
  piece="${pair%%:*}"; stem="${pair##*:}"
  if (( RANDOM % 2 )); then a=v1; b=v2; else a=v2; b=v1; fi
  cp "$R/$a/$stem.wav" "$OUT/${piece}_A.wav"
  cp "$R/$b/$stem.wav" "$OUT/${piece}_B.wav"
  cp "$R/spectrograms/$a/$stem.png" "$OUT/${piece}_A.png"
  cp "$R/spectrograms/$b/$stem.png" "$OUT/${piece}_B.png"
  extra=""
  if [[ -n "$CONTROL" && "$piece" == "$CPIECE" ]]; then
    cp "$R/$CSTEM.wav" "$OUT/${piece}_C.wav"
    cp "$R/spectrograms/$CSTEM.png" "$OUT/${piece}_C.png"
    extra=" C=control"
  fi
  echo "key: $piece A=$a B=$b$extra   ($stem.wav${extra:+; C = $CSTEM.wav})" >> "$KEY"
done
echo '```' >> "$KEY"
echo "packet -> $OUT"
echo "key    -> $KEY"
