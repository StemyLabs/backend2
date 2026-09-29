# Corrected M2 vs M1 (adaptive floor)

M1 frozen/reproducible. No additional sonic retune.

## Loudness / dynamics

| Track | M1 LUFS | M2 LUFS | ΔLU | M1 gain† | M2 gain | M1 baseline | M1 crest | M2 crest | M2 TP |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Despo_ONE | -9.00 | -8.47 | 0.53 | -2.65 | -2.15 | -2.65 | 7.93 | 8.84 | -1.75 |
| NY_Energy | -11.15 | -10.65 | 0.50 | 5.56 | 6.06 | 5.56 | 11.83 | 11.37 | -0.50 |
| LADI_ROCK | -10.25 | -9.85 | 0.40 | 9.82 | 10.32 | 9.82 | 12.59 | 12.16 | -0.50 |
| Tef_Dont_Make_Me | -9.00 | -8.49 | 0.51 | -3.41 | -2.91 | -3.41 | 7.77 | 8.17 | -2.62 |
| Holiday_Hustle_Demo | -10.30 | -9.86 | 0.44 | 8.97 | 9.47 | 8.97 | 13.06 | 12.61 | -0.50 |
| Perfect_Timing_My_Time | -9.17 | -8.74 | 0.43 | 0.00 | 0.64 | 0.00 | 10.70 | 10.27 | -0.50 |

† From frozen M1 report.

## Limiter telemetry (M2)

| Track | max GR | avg GR (active) | p95 GR (active) | %t >1 dB | %t >3 dB | %t >6 dB |
|---|---:|---:|---:|---:|---:|---:|
| Despo_ONE | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 |
| NY_Energy | 7.53 | 0.80 | 4.70 | 21.78 | 9.81 | 2.61 |
| LADI_ROCK | 8.34 | 0.66 | 3.30 | 22.80 | 5.99 | 0.10 |
| Tef_Dont_Make_Me | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 |
| Holiday_Hustle_Demo | 10.83 | 0.66 | 3.40 | 20.66 | 6.13 | 0.69 |
| Perfect_Timing_My_Time | 0.83 | 0.09 | 0.50 | 0.00 | 0.00 | 0.00 |

## Character / overrides

| Track | Punch | Sat | HF | Stereo | safety_override_reason |
|---|---|---|---|---|---|
| Despo_ONE | off | off | on | on 1.06 | — |
| NY_Energy | on | on | on | on 1.06 | — |
| LADI_ROCK | on | on | off | on 1.06 | — |
| Tef_Dont_Make_Me | off | off | on | on 1.06 | — |
| Holiday_Hustle_Demo | on | on | off | on 1.06 | — |
| Perfect_Timing_My_Time | on | on | on | off | — |

## Regression guard (quiet/in-pocket: M2 < M1−0.5 LU)

No quiet/in-pocket track is >0.5 LU quieter than M1 without override.

## Safety overrides recorded

None.

## Decision notes (loudness/stress)
- **Despo_ONE**:
  - loudness: M1 baseline candidate gain=-2.64535 dB
  - loudness: reduce -2.14535 dB toward -8.5 LUFS (input -6.35465)
- **NY_Energy**:
  - loudness: M1 baseline candidate gain=5.56221 dB
  - loudness: M2 gain=6.06221 dB (M1 baseline 5.56221 + extra 0.5) toward -8.5 LUFS (band −9…−8; input -14.5622)
- **LADI_ROCK**:
  - loudness: M1 baseline candidate gain=9.81658 dB
  - loudness: M2 gain=10.3166 dB (M1 baseline 9.81658 + extra 0.5) toward -8.5 LUFS (band −9…−8; input -18.8166)
- **Tef_Dont_Make_Me**:
  - loudness: M1 baseline candidate gain=-3.40924 dB
  - loudness: reduce -2.90924 dB toward -8.5 LUFS (input -5.59076)
- **Holiday_Hustle_Demo**:
  - loudness: M1 baseline candidate gain=8.96888 dB
  - warning: input true-peak overs — weaker reference; M1 baseline retained where applied
  - loudness: M2 gain=9.46888 dB (M1 baseline 8.96888 + extra 0.5) toward -8.5 LUFS (band −9…−8; input -17.9689)
- **Perfect_Timing_My_Time**:
  - loudness: M1 baseline candidate gain=0 dB
  - loudness: M2 gain=0.639283 dB (M1 baseline 0 + extra 0.639283) toward -8.5 LUFS (band −9…−8; input -9.13928)
