# M2 Refinement vs M1 and Previous M2

M1 reproducibility (Despo): LUFS delta **0.000000** (frozen baseline intact).
Config: `config/hiphop/m2_refinement.json` (`m2-refinement-v2`). Previous M2: `refs/evaluation_outputs/m2/`.

## Loudness

| Track | M1 | M2 prev | M2 refinement | Δ ref vs M1 | Δ ref vs prev | Ref TP | Ref crest |
|---|---:|---:|---:|---:|---:|---:|---:|
| Despo_ONE | -9.00 | -8.47 | -8.47 | 0.53 | -0.00 | -1.76 | 8.84 |
| NY_Energy | -11.15 | -10.65 | -10.66 | 0.49 | -0.01 | -1.00 | 10.93 |
| LADI_ROCK | -10.25 | -9.85 | -9.90 | 0.35 | -0.04 | -1.00 | 11.62 |
| Tef_Dont_Make_Me | -9.00 | -8.49 | -8.49 | 0.51 | -0.00 | -2.63 | 8.17 |
| Holiday_Hustle_Demo | -10.30 | -9.86 | -9.93 | 0.37 | -0.07 | -1.00 | 12.16 |
| Perfect_Timing_My_Time | -9.17 | -8.74 | -8.91 | 0.26 | -0.17 | -1.00 | 9.92 |

## Gain / character (refinement)

| Track | Gain | M1 base | Low weight | Punch | Sat |
|---|---:|---:|---|---|---|
| Despo_ONE | -2.15 | -2.65 | off | off | off |
| NY_Energy | 6.31 | 5.56 | off | on | on |
| LADI_ROCK | 10.57 | 9.82 | +0.56 dB | on | on |
| Tef_Dont_Make_Me | -2.91 | -3.41 | off | off | off |
| Holiday_Hustle_Demo | 9.72 | 8.97 | off | on | on |
| Perfect_Timing_My_Time | 0.64 | 0.00 | off | on | on |

## Limiter telemetry (refinement)

| Track | max GR | avg active | p95 active | %t >3 dB |
|---|---:|---:|---:|---:|
| Despo_ONE | 0.00 | 0.00 | 0.00 | 0.00 |
| NY_Energy | 8.25 | 0.94 | 5.30 | 11.49 |
| LADI_ROCK | 9.00 | 0.86 | 3.90 | 9.41 |
| Tef_Dont_Make_Me | 0.00 | 0.00 | 0.00 | 0.00 |
| Holiday_Hustle_Demo | 11.51 | 0.83 | 3.90 | 8.02 |
| Perfect_Timing_My_Time | 1.34 | 0.21 | 0.90 | 0.00 |

## Regression guard (>0.5 LU below M1 without override)

All quiet/in-pocket tracks within guard.

## Safety overrides

None.

## Checklist vs expected

See `expected_changes_checklist.md` for pre-implementation targets.

