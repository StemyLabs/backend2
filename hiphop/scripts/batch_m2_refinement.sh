#!/usr/bin/env bash
# Generate M2 refinement WAVs + reports (does not overwrite M1 or prior M2 checkpoint).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${ROOT}/build/tools/stemy_master/stemy_master"
CFG="${ROOT}/config/hiphop/m2_refinement.json"
OUT="${ROOT}/refs/evaluation_outputs/m2_refinement"
PRE="${ROOT}/refs/stemy_premasters"
mkdir -p "${OUT}"

declare -a JOBS=(
  "00Despo - ONE.wav|Despo_ONE"
  "New York Energy premix.wav|NY_Energy"
  "OBSESSED - LADI ROCK.wav|LADI_ROCK"
  "Tef - Don't Make Me.wav|Tef_Dont_Make_Me"
  "Holiday_Hustle_Demo.wav|Holiday_Hustle_Demo"
  "Perfect_Timing_My_Time.wav|Perfect_Timing_My_Time"
)

for entry in "${JOBS[@]}"; do
  IFS='|' read -r infile tag <<< "${entry}"
  inpath="${PRE}/${infile}"
  if [[ ! -f "${inpath}" ]]; then
    echo "SKIP missing ${inpath}"
    continue
  fi
  echo "=== M2 refinement ${tag} ==="
  "${BIN}" "${inpath}" "${OUT}/${tag}_m2.wav" \
    --config "${CFG}" \
    --report-json "${OUT}/${tag}_m2_report.json" \
    --quiet
done
echo "M2 refinement outputs in ${OUT}"
