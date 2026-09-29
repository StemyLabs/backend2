# M2B Final Precheck — Engineering Hardening Report

**STOP after this pass.** No Final Hip-Hop R0 subjective retune.

## 1. Freeze

| Item | Value |
|---|---|
| Tag | `m2b-frozen` (annotated) |
| Commit | `25f352e` — Freeze M2B as immutable final-phase baseline |
| Config | `config/hiphop/m2b_frozen.json` (`version`: `m2b-frozen`) |
| Loader | `m2b*` / `m2-refinement*` → `make_m2_refinement_defaults()` |
| Tests at freeze | 49/49 (pre-hardening); post-hardening **60/60** |
| M1 Despo repro | input −6.35 → output **−9.00** LUFS, gain −2.645, config `m1-master01-baseline` |

## 2. True-peak implementation audit

**Bug confirmed:** prior `measure_true_peak_linear` used **linear interpolation** upsampling. Linear interp cannot exceed sample magnitudes → reported TP ≡ sample peak on all three client inputs.

| Item | Detail |
|---|---|
| Location | `src/analysis/true_peak_measure.cpp` (shared by `TruePeakAnalyzer` + `TruePeakSafety`) |
| Old method | linear interpolation (`linear_interp_provisional`) |
| New method | zero-stuff ×N + Kaiser-windowed sinc LPF (BS.1770-style) |
| Oversample factor | default **4×** (also 2× / 8× selectable) |
| Filter | Kaiser β=8.5, ~96-tap sinc LPF, polyphase streaming; DC gain preserved |
| Independence | `measure_sample_peak_linear` is separate; TP always ≥ sample peak |
| Reporting | analysis pipeline + safety report both use corrected meter |

**Engineering note:** fixing the shared meter also makes `TruePeakSafety` enforce −1.0 dBTP correctly. That is meter correctness, not a limiter-policy change. Frozen M2B_BASELINE WAVs were rendered under the broken meter and can exceed −1.0 when remeasured.

## 3. Corrected TP measurements

### Inputs

| Track | sample peak | old TP | corrected TP | Δ |
|---|---:|---:|---:|---:|
| Bumpman | -5.00 | -5.00 | -4.97 | +0.02 |
| Antonio Slim | -0.11 | -0.11 | 1.13 | +1.24 |
| DaMind | -2.94 | -2.94 | -2.94 | +0.00 |

### Frozen M2B_BASELINE output WAVs (remeasured; **no re-render**)

| Track | sample peak | old reported TP | corrected TP | Δ | vs −1.0 ceiling |
|---|---:|---:|---:|---:|---|
| Bumpman | -1.00 | -1.00 | -0.33 | +0.67 | **EXCEEDS** |
| Antonio Slim | -1.38 | -1.38 | -1.31 | +0.07 | OK |
| DaMind | -1.00 | -1.00 | -0.11 | +0.89 | **EXCEEDS** |

> **Separate ceiling finding (no limiter policy change yet):** Bumpman and DaMind frozen M2B outputs measure **−0.33** and **−0.11 dBTP** with the corrected meter — above the −1.0 contract. Antonio’s frozen M2B output remains under ceiling (−1.31).

## 4. Hot/dense width bypass

- Guard in `apply_m2_character`: if `LUFS > band_high` **or** `crest < dense_crest_threshold` → width=1.0, note `hot_dense_bypass`.
- Quiet/open width rules unchanged.
- Regression tests added and passing.

## 5. Telemetry (audio-neutral)

- Limiter active-time % (`limiter_pct_time_active`).
- LF stereo correlation: one-pole LPF @ **120 Hz** (`lf_stereo_correlation`).

## 6. Three-track comparison

### Bumpman

| Metric | INPUT | M2B_BASELINE | FINAL_PRECHECK |
|---|---:|---:|---:|
| LUFS-I | -21.64 | -10.03 | -10.75 |
| sample peak | -5.00 | -1.00 | -1.72 |
| true peak (verified) | -4.97 | -0.33 | -1.05 |
| crest | 19.25 | 11.49 | 11.49 |
| correlation | 0.845 | 0.790 | 0.790 |
| width metric | 0.078 | 0.105 | 0.105 |
| LF corr <120Hz | 0.954 | n/a | 0.921 |
| adaptive path | — | QUIET/OPEN | QUIET/OPEN |
| gain dB | — | 12.75 | 12.75 |
| punch | — | ON | ON |
| saturation | — | ON | ON |
| low-end weight | — | OFF | OFF |
| HF | — | OFF | OFF |
| width factor | — | 1.00 | 1.00 |
| limiter max GR | — | 8.45 | 8.45 |
| limiter avg GR | — | 0.69 | 0.69 |
| limiter p95 GR | — | 3.10 | 3.10 |
| limiter active % | — | n/a | 93.69 |
| >1 / >3 / >6 dB % | — | 22.27/5.04/0.12 | 22.27/5.04/0.12 |
| TP safety applied | — | False | True |
| TP safety scale | — | 1.0000 | 0.9205 |

Stereo decision: `m2 stereo: bypassed (input already sufficiently wide)`

### Antonio Slim

| Metric | INPUT | M2B_BASELINE | FINAL_PRECHECK |
|---|---:|---:|---:|
| LUFS-I | -5.80 | -8.49 | -8.50 |
| sample peak | -0.11 | -1.38 | -2.80 |
| true peak (verified) | 1.13 | -1.31 | -1.57 |
| crest | 8.52 | 9.94 | 8.52 |
| correlation | 0.957 | 0.954 | 0.957 |
| width metric | 0.022 | 0.023 | 0.022 |
| LF corr <120Hz | 0.988 | n/a | 0.988 |
| adaptive path | — | HOT/DENSE | HOT/DENSE |
| gain dB | — | -2.70 | -2.70 |
| punch | — | OFF | OFF |
| saturation | — | OFF | OFF |
| low-end weight | — | OFF | OFF |
| HF | — | OFF | OFF |
| width factor | — | 1.06 | 1.00 |
| limiter max GR | — | 0.00 | 0.00 |
| limiter avg GR | — | 0.00 | 0.00 |
| limiter p95 GR | — | 0.00 | 0.00 |
| limiter active % | — | n/a | 0.00 |
| >1 / >3 / >6 dB % | — | 0.00/0.00/0.00 | 0.00/0.00/0.00 |
| TP safety applied | — | False | False |
| TP safety scale | — | 1.0000 | 1.0000 |

Stereo decision: `m2 stereo: hot_dense_bypass (width=1.0)`

### DaMind

| Metric | INPUT | M2B_BASELINE | FINAL_PRECHECK |
|---|---:|---:|---:|
| LUFS-I | -20.65 | -10.31 | -11.25 |
| sample peak | -2.94 | -1.00 | -1.94 |
| true peak (verified) | -2.94 | -0.11 | -1.05 |
| crest | 18.58 | 10.42 | 10.42 |
| correlation | 0.940 | 0.913 | 0.913 |
| width metric | 0.030 | 0.043 | 0.043 |
| LF corr <120Hz | 0.978 | n/a | 0.976 |
| adaptive path | — | QUIET/OPEN | QUIET/OPEN |
| gain dB | — | 12.40 | 12.40 |
| punch | — | ON | ON |
| saturation | — | ON | ON |
| low-end weight | — | OFF | OFF |
| HF | — | ON | ON |
| width factor | — | 1.06 | 1.06 |
| limiter max GR | — | 10.88 | 10.88 |
| limiter avg GR | — | 0.89 | 0.89 |
| limiter p95 GR | — | 4.30 | 4.30 |
| limiter active % | — | n/a | 94.12 |
| >1 / >3 / >6 dB % | — | 25.05/8.03/2.22 | 25.05/8.03/2.22 |
| TP safety applied | — | False | True |
| TP safety scale | — | 1.0000 | 0.8975 |

Stereo decision: `m2 stereo: width=1.06 side_hpf=120 Hz (no widen below)`

## 7. Generated files

```
refs/evaluation_outputs/m2b_final_precheck/precheck_outputs/Bumpman_Shoot_My_Shot_FINAL_PRECHECK.wav
refs/evaluation_outputs/m2b_final_precheck/precheck_outputs/Bumpman_Shoot_My_Shot_FINAL_PRECHECK_report.json
refs/evaluation_outputs/m2b_final_precheck/precheck_outputs/Antonio_Slim_Easy_FINAL_PRECHECK.wav
refs/evaluation_outputs/m2b_final_precheck/precheck_outputs/Antonio_Slim_Easy_FINAL_PRECHECK_report.json
refs/evaluation_outputs/m2b_final_precheck/precheck_outputs/DaMind_Whos_Da_Mind_FINAL_PRECHECK.wav
refs/evaluation_outputs/m2b_final_precheck/precheck_outputs/DaMind_Whos_Da_Mind_FINAL_PRECHECK_report.json
refs/evaluation_outputs/m2b_final_precheck/input_reanalysis/*_input.json
refs/evaluation_outputs/m2b_final_precheck/tp_audit/*_M2B_BASELINE_corrected_tp.json
refs/evaluation_outputs/m2b_final_precheck/FINAL_PRECHECK_REPORT.md
```

## 8. Remaining objective issues

1. Frozen M2B WAVs exceed −1.0 dBTP under corrected meter (Bumpman −0.33, DaMind −0.11). Limiter policy unchanged; PRECHECK enforces via corrected safety meter.
2. Antonio input corrected TP **+1.13 dBTP**; PRECHECK output −1.57 with width bypass.
3. Bumpman/DaMind still ~−10…−10.8 LUFS with heavy limiter GR — loudness not increased.

## 9. Subjective questions for Vance

1. Bumpman/DaMind: is −10-ish LUFS acceptable, or is more loudness worth more limiter stress?
2. After corrected TP safety pull-down (~0.7 LU quieter on Bumpman/DaMind PRECHECK), too soft vs frozen M2B WAVs?
3. Antonio width=1.00: any missing width vs baseline 1.06?
4. DaMind width 1.06 + HF +0.5: keep for R0 or trim?
5. Prioritize ISP enforcement architecture (safety vs lookahead limiter) before character tweaks?
