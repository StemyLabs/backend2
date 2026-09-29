# M2 Loudness Regression Audit (read-only)

No sonic stages were changed for this audit. Stage probe: `tools/stemy_stage_audit`.

## Limiter GR meaning

`limiter_max_gr_db` is the **maximum instantaneous** gain reduction in dB over the rendered buffer (`max` of `-20*log10(gain_)` while `gain_ < 1`). It is **not** average GR.

## Adaptive gain: M1 vs M2

| Track | In LUFS | In TP | In Crest | M1 gain | M2 gain | Δgain | M1 out | M2 out | Δout |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| NY_Energy | -14.56 | -0.64 | 14.88 | 5.56 | 4.14 | -1.42 | -11.15 | -11.78 | -0.62 |
| LADI_ROCK | -18.82 | -2.08 | 19.84 | 9.82 | 5.58 | -4.23 | -10.25 | -13.01 | -2.76 |
| Holiday_Hustle_Demo | -17.97 | 0.68 | 22.00 | 8.97 | 4.00 | -4.97 | -10.30 | -13.67 | -3.37 |
| Perfect_Timing_My_Time | -9.14 | -0.12 | 11.05 | 0.00 | 0.64 | 0.64 | -9.17 | -8.74 | 0.43 |

## Stage tables — NY_Energy

### M1 (adaptive gain 5.56 dB)

| Stage | Stage Δ LUFS* | LUFS-I | TP | Sample peak | Crest | Limiter max GR | Δ from input |
|---|---:|---:|---:|---:|---:|---:|---:|
| 1_input | 0.00 | -14.56 | -0.64 | -0.64 | 14.88 | — | 0.00 |
| 2_input_gain | 5.56 | -9.00 | 4.92 | 4.92 | 14.88 | — | 5.56 |
| 3_tonal_low_end | 0.77 | -8.23 | 6.53 | 6.53 | 16.04 | — | 6.33 |
| 4_punch | 0.00 | -8.23 | 6.53 | 6.53 | 16.04 | — | 6.33 |
| 5_saturation | 0.00 | -8.23 | 6.53 | 6.53 | 16.04 | — | 6.33 |
| 6_hf | 0.00 | -8.23 | 6.53 | 6.53 | 16.04 | — | 6.33 |
| 7_stereo | 0.00 | -8.23 | 6.53 | 6.53 | 16.04 | — | 6.33 |
| 8_limiter | -2.92 | -11.15 | -0.50 | -0.50 | 11.83 | 7.03 | 3.41 |
| 9_true_peak_safety | 0.00 | -11.15 | -0.50 | -0.50 | 11.83 | — | 3.41 |

*For stage 2, Stage Δ is the adaptive gain decision (dB), not measured LUFS delta.

### M2 (adaptive gain 4.14 dB)

| Stage | Stage Δ LUFS* | LUFS-I | TP | Sample peak | Crest | Limiter max GR | Δ from input |
|---|---:|---:|---:|---:|---:|---:|---:|
| 1_input | 0.00 | -14.56 | -0.64 | -0.64 | 14.88 | — | 0.00 |
| 2_input_gain | 4.14 | -10.42 | 3.50 | 3.50 | 14.88 | — | 4.14 |
| 3_tonal_low_end | 0.77 | -9.65 | 5.11 | 5.11 | 16.04 | — | 4.91 |
| 4_punch | 0.16 | -9.49 | 5.47 | 5.47 | 16.27 | — | 5.07 |
| 5_saturation | -0.06 | -9.56 | 4.97 | 4.97 | 15.81 | — | 5.01 |
| 6_hf | 0.00 | -9.55 | 5.00 | 5.00 | 15.83 | — | 5.01 |
| 7_stereo | 0.00 | -9.55 | 5.16 | 5.16 | 15.99 | — | 5.02 |
| 8_limiter | -2.23 | -11.78 | -0.50 | -0.50 | 12.45 | 5.66 | 2.79 |
| 9_true_peak_safety | 0.00 | -11.78 | -0.50 | -0.50 | 12.45 | — | 2.79 |

*For stage 2, Stage Δ is the adaptive gain decision (dB), not measured LUFS delta.

## Stage tables — LADI_ROCK

### M1 (adaptive gain 9.82 dB)

| Stage | Stage Δ LUFS* | LUFS-I | TP | Sample peak | Crest | Limiter max GR | Δ from input |
|---|---:|---:|---:|---:|---:|---:|---:|
| 1_input | 0.00 | -18.82 | -2.08 | -2.08 | 19.84 | — | 0.00 |
| 2_input_gain | 9.82 | -9.00 | 7.73 | 7.73 | 19.84 | — | 9.82 |
| 3_tonal_low_end | 0.34 | -8.66 | 7.85 | 7.85 | 19.51 | — | 10.15 |
| 4_punch | 0.00 | -8.66 | 7.85 | 7.85 | 19.51 | — | 10.15 |
| 5_saturation | 0.00 | -8.66 | 7.85 | 7.85 | 19.51 | — | 10.15 |
| 6_hf | 0.00 | -8.66 | 7.85 | 7.85 | 19.51 | — | 10.15 |
| 7_stereo | 0.00 | -8.66 | 7.85 | 7.85 | 19.51 | — | 10.15 |
| 8_limiter | -1.59 | -10.25 | -0.50 | -0.50 | 12.59 | 8.35 | 8.57 |
| 9_true_peak_safety | 0.00 | -10.25 | -0.50 | -0.50 | 12.59 | — | 8.57 |

*For stage 2, Stage Δ is the adaptive gain decision (dB), not measured LUFS delta.

### M2 (adaptive gain 5.58 dB)

| Stage | Stage Δ LUFS* | LUFS-I | TP | Sample peak | Crest | Limiter max GR | Δ from input |
|---|---:|---:|---:|---:|---:|---:|---:|
| 1_input | 0.00 | -18.82 | -2.08 | -2.08 | 19.84 | — | 0.00 |
| 2_input_gain | 5.58 | -13.23 | 3.50 | 3.50 | 19.84 | — | 5.58 |
| 3_tonal_low_end | 0.34 | -12.89 | 3.62 | 3.62 | 19.51 | — | 5.92 |
| 4_punch | 0.06 | -12.83 | 3.82 | 3.82 | 19.65 | — | 5.98 |
| 5_saturation | -0.04 | -12.88 | 3.30 | 3.30 | 19.14 | — | 5.94 |
| 6_hf | 0.00 | -12.88 | 3.30 | 3.30 | 19.14 | — | 5.94 |
| 7_stereo | 0.01 | -12.87 | 3.23 | 3.23 | 19.07 | — | 5.95 |
| 8_limiter | -0.14 | -13.01 | -0.50 | -0.50 | 15.46 | 3.73 | 5.80 |
| 9_true_peak_safety | 0.00 | -13.01 | -0.50 | -0.50 | 15.46 | — | 5.80 |

*For stage 2, Stage Δ is the adaptive gain decision (dB), not measured LUFS delta.

## Stage tables — Holiday_Hustle_Demo

### M1 (adaptive gain 8.97 dB)

| Stage | Stage Δ LUFS* | LUFS-I | TP | Sample peak | Crest | Limiter max GR | Δ from input |
|---|---:|---:|---:|---:|---:|---:|---:|
| 1_input | 0.00 | -17.97 | 0.68 | 0.68 | 22.00 | — | 0.00 |
| 2_input_gain | 8.97 | -9.00 | 9.65 | 9.65 | 22.00 | — | 8.97 |
| 3_tonal_low_end | 0.47 | -8.53 | 10.16 | 10.16 | 21.77 | — | 9.44 |
| 4_punch | 0.00 | -8.53 | 10.16 | 10.16 | 21.77 | — | 9.44 |
| 5_saturation | 0.00 | -8.53 | 10.16 | 10.16 | 21.77 | — | 9.44 |
| 6_hf | 0.00 | -8.53 | 10.16 | 10.16 | 21.77 | — | 9.44 |
| 7_stereo | 0.00 | -8.53 | 10.16 | 10.16 | 21.77 | — | 9.44 |
| 8_limiter | -1.77 | -10.30 | -0.50 | -0.50 | 13.06 | 10.66 | 7.67 |
| 9_true_peak_safety | 0.00 | -10.30 | -0.50 | -0.50 | 13.06 | — | 7.67 |

*For stage 2, Stage Δ is the adaptive gain decision (dB), not measured LUFS delta.

### M2 (adaptive gain 4.00 dB)

| Stage | Stage Δ LUFS* | LUFS-I | TP | Sample peak | Crest | Limiter max GR | Δ from input |
|---|---:|---:|---:|---:|---:|---:|---:|
| 1_input | 0.00 | -17.97 | 0.68 | 0.68 | 22.00 | — | 0.00 |
| 2_input_gain | 4.00 | -13.97 | 4.68 | 4.68 | 22.00 | — | 4.00 |
| 3_tonal_low_end | 0.47 | -13.49 | 5.19 | 5.19 | 21.77 | — | 4.47 |
| 4_punch | 0.09 | -13.40 | 5.55 | 5.55 | 22.01 | — | 4.57 |
| 5_saturation | -0.03 | -13.43 | 4.97 | 4.97 | 21.45 | — | 4.54 |
| 6_hf | 0.00 | -13.43 | 4.97 | 4.97 | 21.45 | — | 4.54 |
| 7_stereo | 0.02 | -13.41 | 4.98 | 4.98 | 21.44 | — | 4.56 |
| 8_limiter | -0.26 | -13.67 | -0.50 | -0.50 | 16.26 | 5.48 | 4.30 |
| 9_true_peak_safety | 0.00 | -13.67 | -0.50 | -0.50 | 16.26 | — | 4.30 |

*For stage 2, Stage Δ is the adaptive gain decision (dB), not measured LUFS delta.

## Stage tables — Perfect_Timing_My_Time

### M1 (adaptive gain 0.00 dB)

| Stage | Stage Δ LUFS* | LUFS-I | TP | Sample peak | Crest | Limiter max GR | Δ from input |
|---|---:|---:|---:|---:|---:|---:|---:|
| 1_input | 0.00 | -9.14 | -0.12 | -0.12 | 11.05 | — | 0.00 |
| 2_input_gain | 0.00 | -9.14 | -0.12 | -0.12 | 11.05 | — | 0.00 |
| 3_tonal_low_end | 0.00 | -9.14 | -0.12 | -0.12 | 11.05 | — | 0.00 |
| 4_punch | 0.00 | -9.14 | -0.12 | -0.12 | 11.05 | — | 0.00 |
| 5_saturation | 0.00 | -9.14 | -0.12 | -0.12 | 11.05 | — | 0.00 |
| 6_hf | 0.00 | -9.14 | -0.12 | -0.12 | 11.05 | — | 0.00 |
| 7_stereo | 0.00 | -9.14 | -0.12 | -0.12 | 11.05 | — | 0.00 |
| 8_limiter | -0.03 | -9.17 | -0.50 | -0.50 | 10.70 | 0.38 | -0.03 |
| 9_true_peak_safety | 0.00 | -9.17 | -0.50 | -0.50 | 10.70 | — | -0.03 |

*For stage 2, Stage Δ is the adaptive gain decision (dB), not measured LUFS delta.

### M2 (adaptive gain 0.64 dB)

| Stage | Stage Δ LUFS* | LUFS-I | TP | Sample peak | Crest | Limiter max GR | Δ from input |
|---|---:|---:|---:|---:|---:|---:|---:|
| 1_input | 0.00 | -9.14 | -0.12 | -0.12 | 11.05 | — | 0.00 |
| 2_input_gain | 0.64 | -8.50 | 0.52 | 0.52 | 11.05 | — | 0.64 |
| 3_tonal_low_end | 0.00 | -8.50 | 0.52 | 0.52 | 11.05 | — | 0.64 |
| 4_punch | -0.06 | -8.56 | 0.65 | 0.65 | 11.24 | — | 0.58 |
| 5_saturation | -0.03 | -8.59 | 0.33 | 0.33 | 10.92 | — | 0.55 |
| 6_hf | 0.02 | -8.57 | 0.33 | 0.33 | 10.92 | — | 0.57 |
| 7_stereo | 0.00 | -8.57 | 0.33 | 0.33 | 10.92 | — | 0.57 |
| 8_limiter | -0.17 | -8.74 | -0.50 | -0.50 | 10.27 | 0.83 | 0.40 |
| 9_true_peak_safety | 0.00 | -8.74 | -0.50 | -0.50 | 10.27 | — | 0.40 |

*For stage 2, Stage Δ is the adaptive gain decision (dB), not measured LUFS delta.
