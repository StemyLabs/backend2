#!/usr/bin/env bash
# Generate combined M2 WAVs + reports for client A/B (does not overwrite M1).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${ROOT}/build/tools/stemy_master/stemy_master"
CFG="${ROOT}/config/hiphop/m2_tuning.json"
OUT="${ROOT}/refs/evaluation_outputs/m2"
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
  echo "=== M2 ${tag} ==="
  "${BIN}" "${inpath}" "${OUT}/${tag}_m2.wav" \
    --config "${CFG}" \
    --report-json "${OUT}/${tag}_m2_report.json" \
    --quiet
done
echo "M2 outputs in ${OUT}"
