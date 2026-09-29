# M2B Final-Phase Baseline Report (unchanged chain)

**STOP:** No final-phase sonic retune performed.

## Freeze status

| Item | Value |
|---|---|
| Branch | `hiphop-final-phase` |
| Config | `config/hiphop/m2b_frozen.json` |
| Config version | `m2b-frozen` |
| Sonic policy | identical to `m2-refinement-v2` / `make_m2_refinement_defaults()` |
| Freeze doc | `docs/m2b_freeze.md` |
| Pre-change tests | **49/49 passed** |
| Post-process tests | **49/49 passed** |
| M1 Despo repro LUFS Δ | **0.0** |
| LRA | not supported in current analyzers |
| LF stereo correlation | not supported (overall corr/width/mid-side only) |

## 1. Input analysis

| Track | SR | Bit | Ch | Dur (s) | LUFS-I | ST | Peak | TP | RMS | Crest | Corr | Width | Bass-body share | High share |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| TEST 01 / Bumpman | 48000 | 24 | 2 | 219.44 | -21.64 | -19.52 | -5.00 | -5.00 | -24.25 | 19.25 | 0.845 | 0.078 | 0.541 | 0.133 |
| TEST 02 / Antonio Slim | 44100 | 16 | 2 | 216.89 | -5.80 | -4.74 | -0.11 | -0.11 | -8.63 | 8.52 | 0.957 | 0.022 | 0.449 | 0.156 |
| TEST 03 / DaMind | 48000 | 24 | 2 | 195.92 | -20.65 | -18.70 | -2.94 | -2.94 | -21.53 | 18.58 | 0.940 | 0.030 | 0.825 | 0.033 |

### Expected-condition verification

| Track | Expected | Measured | Confirmed? |
|---|---|---|---|
| Bumpman | quiet ~−21…−22 LUFS, crest ~19, headroom, wider stereo, mono-compatible LF | −21.64 LUFS, crest 19.25, TP −5.0, corr 0.845 / width 0.078, bass-body 0.54 | **Yes** (LF “strongly mono” not separately measured; overall corr still high) |
| Antonio Slim | hot ~−6 LUFS, TP possibly >0, crest ~8–9, low LRA | −5.80 LUFS, crest 8.52, TP **−0.11** (not over), tight width 0.022 | **Mostly yes**; TP overs not present on this file; LRA N/A |
| DaMind | quiet ~−20…−21, crest ~18–19, strong sub/bass | −20.65 LUFS, crest 18.58, bass-body **0.825** | **Yes** |

## 2. M2B output analysis

| Track | LUFS-I | ST | TP | Crest | Corr | Width | nan/clip |
|---|---:|---:|---:|---:|---:|---:|---|
| TEST 01 / Bumpman | -10.03 | -8.32 | -1.00 | 11.49 | 0.790 | 0.105 | nan=0 clip=0 |
| TEST 02 / Antonio Slim | -8.49 | -7.43 | -1.38 | 9.94 | 0.954 | 0.023 | nan=0 clip=0 |
| TEST 03 / DaMind | -10.31 | -9.01 | -1.00 | 10.42 | 0.913 | 0.043 | nan=0 clip=0 |

## 3. Input → M2B deltas

| Track | ΔLUFS | ΔCrest | ΔTP | ΔCorr | ΔWidth | Bass-body share in→out |
|---|---:|---:|---:|---:|---:|---|
| TEST 01 / Bumpman | 11.61 | -7.77 | 4.00 | -0.055 | 0.027 | 0.541 → 0.495 |
| TEST 02 / Antonio Slim | -2.69 | 1.42 | -1.27 | -0.003 | 0.001 | 0.449 → 0.449 |
| TEST 03 / DaMind | 10.34 | -8.17 | 1.94 | -0.027 | 0.013 | 0.825 → 0.725 |

## 4. Adaptive decisions (cross-track — must differ)

| Track | Path | Gain dB | M1 base | Extra/Reduce | Quiet tonal | Punch | Sat | Low weight | HF | Width |
|---|---|---:|---:|---|---|---|---|---|---|---|
| TEST 01 / Bumpman | QUIET / OPEN | 12.75 | 12.00 | 0.75 | on | on 1.15/0.30/0.38 | on d=0.80 m=0.12 | off | off | 1.00 bypass |
| TEST 02 / Antonio Slim | HOT / ALREADY-DENSE | -2.70 | -3.20 | 0.50 | off | off | off | off | off | 1.06 |
| TEST 03 / DaMind | QUIET / OPEN | 12.40 | 11.65 | 0.75 | on | on 1.15/0.30/0.38 | on d=0.80 m=0.12 | off | +0.50@9000 | 1.06 |

**Adaptive differentiation:** settings are **not** identical across tracks (gain polarity differs; punch/sat on for open, off for dense; low-weight correctly off on DaMind; stereo bypass only on Bumpman).

## 5. Limiter telemetry

| Track | Ceiling | Pre-lim gain | Max GR | Avg active | P95 active | %t>1 | %t>3 | %t>6 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| TEST 01 / Bumpman | -1.00 | 12.75 | 8.45 | 0.69 | 3.10 | 22.27 | 5.04 | 0.12 |
| TEST 02 / Antonio Slim | -1.00 | -2.70 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 |
| TEST 03 / DaMind | -1.00 | 12.40 | 10.88 | 0.89 | 4.30 | 25.05 | 8.03 | 2.22 |

## 6. Per-track technical assessment

### TEST 01 — Bumpman
- **Correct:** QUIET/OPEN path; large lift (+12.75 = M1 12 + 0.75 extra); punch/sat on; HF bypassed (high energy already up); **stereo bypassed** (already wide); **low-weight bypassed** (bass-body 0.54 ≥ threshold); TP held at −1.0; output −10.03 LUFS (below −9) with limiter max GR 8.45 / p95 3.1 — stress prevented forcing −9.
- **Investigate:** Crest 19.3→11.5 (−7.8 dB) is substantial densification; %t>1 dB ≈22%. Final phase should judge if this preserves punch subjectively vs MASTER_01 reference.
- **Not a failure:** Landing under −9 is consistent with limiter-stress policy.

### TEST 02 — Antonio Slim
- **Correct:** HOT/DENSE behavior; **negative** gain (−2.70 dB toward −8.5); punch/sat **auto-bypassed** (crest 8.52 < 9); HF bypassed; no additive low weight; TP −1.38 (safe); limiter GR **0** (attenuation alone enough); crest slightly **increased** 8.52→9.94 (less crushed).
- **Problematic / investigate:** **Stereo width still applied (1.06)** on an already-commercial dense master because corr is high (0.957) and width metric is low — current rule treats “tight” as “needs widen.” For hot/dense path, width should likely auto-bypass. Flag as adaptive-policy refinement (not implemented).
- **Expectation delta:** Input TP was −0.11, not >0 as previously estimated.

### TEST 03 — DaMind
- **Correct:** QUIET/OPEN loudness path (+12.40 dB); **low-weight OFF** despite quiet input — bass-body share 0.825 correctly blocked additive warmth (loudness ≠ bass need); punch/sat on; HF +0.5 (high share only 0.033); TP −1.0; output −10.31 LUFS with heavy limiter (max GR 10.9, %t>3 ≈8%).
- **Investigate:** Quiet-path tonal trim still applies M1 sub shelf −2 @55 while mix is already sub-dominant — objective bass-body share stays very high at output (~0.72). Whether −2 @55 is enough / too much is a Vance listening question. Limiter stress similar to Bumpman — may need final-phase lift/backoff tuning to get closer to −9 without more crush.
- **Validated requirement:** loudness lift and bass enhancement are **not coupled**.

## 7. Regression

- Full suite: **49/49 passed** (before and after baseline processing).
- M1 frozen config reproducibility: Despo LUFS delta **0.0**.
- No DSP parameter changes during this audit; only freeze alias config + analyze-only tool + reporting.

## 8. Generated paths

```
refs/New Audios/HIPHOP_M2B_Test_0{1,2,3}_*.wav
refs/evaluation_outputs/m2b_final_baseline/input_analysis/*_input.json
refs/evaluation_outputs/m2b_final_baseline/m2b_outputs/*_M2B_BASELINE.wav
refs/evaluation_outputs/m2b_final_baseline/m2b_outputs/*_M2B_BASELINE_report.json
config/hiphop/m2b_frozen.json
docs/m2b_freeze.md
```

## 9. Recommended final-phase changes (DO NOT IMPLEMENT YET)

### A. Objective engineering fixes
1. Hot/dense path: force stereo width bypass (and optionally skip side HPF path) when crest < dense threshold or when reducing gain.
2. Add LF stereo correlation metric if mono-compatibility of sub must be proven (currently unsupported).
3. Consider reporting limiter “active time %” explicitly (frames_active/total) alongside GR>1/3/6 histograms.

### B. Adaptive-policy refinements
1. Open-premix lift vs limiter stress: Bumpman/DaMind land ~−10 LUFS with max GR 8–11 dB — review whether extra 0.75 dB above M1 is useful or should be stress-gated by predicted GR.
2. Quiet-path tonal on sub-heavy sources: keep low-weight off (works) but decide if M1 −2 @55 shelf should scale with bass-body share.
3. NY-class open premix vs these two opens: same policy applied; validate commercial −9…−8 target only where GR telemetry stays mild.

### C. Subjective questions for Vance
1. Bumpman at −10.0 LUFS with crest ~11.5 — acceptable commercial density vs MASTER_01, or too limited?
2. Antonio Slim: is subtle width 1.06 on a finished-sounding master desirable, or should dense masters stay width=1.0?
3. DaMind: does the retained sub weight + HF +0.5 feel balanced, or still too sub-forward after lift?
4. Punch/sat levels on open paths — enough impact/warmth without grit on vocals?

---

**STOP CONDITION MET.** Awaiting review before any final Hip-Hop candidate retune.
