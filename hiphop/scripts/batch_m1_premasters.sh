#!/usr/bin/env bash
# Batch-process M1 premaster WAVs and emit per-track + aggregate JSON reports.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${ROOT}/build/tools/stemy_master/stemy_master"
CFG="${ROOT}/config/hiphop/default.json"
OUT_DIR="${ROOT}/refs/evaluation_outputs/m1"
mkdir -p "${OUT_DIR}"

if [[ ! -x "${BIN}" ]]; then
  echo "Missing binary: ${BIN}" >&2
  exit 1
fi

declare -a JOBS=(
  "00Despo - ONE.wav|Despo_ONE"
  "New York Energy premix.wav|NY_Energy"
  "OBSESSED - LADI ROCK.wav|LADI_ROCK"
  "Tef - Don't Make Me.wav|Tef_Dont_Make_Me"
)

AGG="${OUT_DIR}/m1_premaster_metrics.json"
echo '[' > "${AGG}.partial"
first=1

for entry in "${JOBS[@]}"; do
  IFS='|' read -r infile tag <<< "${entry}"
  inpath="${ROOT}/refs/stemy_premasters/${infile}"
  outwav="${OUT_DIR}/${tag}_m1.wav"
  outjson="${OUT_DIR}/${tag}_m1_report.json"
  echo "=== Processing ${infile} → ${tag}_m1.wav ==="
  "${BIN}" "${inpath}" "${outwav}" --config "${CFG}" --report-json "${outjson}" --quiet
  if [[ ${first} -eq 0 ]]; then
    echo ',' >> "${AGG}.partial"
  fi
  first=0
  cat "${outjson}" >> "${AGG}.partial"
done

echo ']' >> "${AGG}.partial"
mv "${AGG}.partial" "${AGG}"
echo "Wrote aggregate report: ${AGG}"
