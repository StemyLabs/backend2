# M1 vs M2 Objective Comparison (Tuning Checkpoint)

Objective metrics only. Client listening determines subjective preference.

## Proposed combined M2 settings

| Stage | Setting |
|---|---|
| Loudness | band −9…−8 LUFS-I, nominal −8.5; lift capped by relative crest/limiter stress (≤4 dB into limiter from headroom) |
| TP ceiling | −0.5 dBTP (unchanged) |
| Compressor | OFF |
| Punch | boost 1.0 dB, sustain_cut 0.4 dB, mix 0.35, sens 0.45; auto-bypass if crest < 9 dB |
| Saturation | drive 0.75, mix 0.10, OS×4, gain-preserve |
| HF openness | +0.6 dB HS @ 9 kHz (auto-reduce/bypass if highs elevated) |
| Stereo | width 1.06, side HPF @ 120 Hz (no widen below); bypass if already wide |
| M1 quiet-path tonal | unchanged |

## Per-track output metrics

| Track | M1 LUFS-I | M2 LUFS-I | M1 ST | M2 ST | M1 TP | M2 TP | M1 Crest | M2 Crest | Limiter max GR | Corr M1→M2 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| Despo_ONE | -9.00 | -8.47 | -7.86 | -7.33 | -3.17 | -1.75 | 7.93 | 8.84 | 0.00 | 0.980→0.980 |
| NY_Energy | -11.15 | -11.78 | -9.09 | -9.78 | -0.50 | -0.50 | 11.83 | 12.45 | 5.66 | 0.939→0.941 |
| LADI_ROCK | -10.25 | -13.01 | -8.37 | -10.65 | -0.50 | -0.50 | 12.59 | 15.46 | 3.73 | 0.965→0.965 |
| Tef_Dont_Make_Me | -9.00 | -8.49 | -7.78 | -7.27 | -3.52 | -2.62 | 7.77 | 8.17 | 0.00 | 0.999→1.000 |
| Holiday_Hustle_Demo | -10.30 | -13.67 | -8.47 | -11.41 | -0.50 | -0.50 | 13.06 | 16.26 | 5.48 | 0.916→0.922 |
| Perfect_Timing_My_Time | -9.17 | -8.74 | -8.50 | -8.10 | -0.50 | -0.50 | 10.70 | 10.27 | 0.83 | 0.918→0.917 |

## Stage activity (M2 decision)

| Track | Gain dB | Punch | Sat | HF | Stereo width |
|---|---:|---|---|---|---|
| Despo_ONE | -2.15 | off | off | on +0.30 @9000Hz | 1.06 |
| NY_Energy | 4.14 | on boost=1.00 cut=0.40 mix=0.35 | on drive=0.75 mix=0.10 | on +0.60 @9000Hz | 1.06 |
| LADI_ROCK | 5.58 | on boost=1.00 cut=0.40 mix=0.35 | on drive=0.75 mix=0.10 | off | 1.06 |
| Tef_Dont_Make_Me | -2.91 | off | off | on +0.30 @9000Hz | 1.06 |
| Holiday_Hustle_Demo | 4.00 | on boost=1.00 cut=0.40 mix=0.35 | on drive=0.75 mix=0.10 | off | 1.06 |
| Perfect_Timing_My_Time | 0.64 | on boost=1.00 cut=0.40 mix=0.35 | on drive=0.75 mix=0.10 | on +0.60 @9000Hz | 1.00 (off) |

## Spectral band deltas (M2 vs M1 output, dB energy)

| Track | sub | bass | low_mid | mid | high_mid | high |
|---|---:|---:|---:|---:|---:|---:|
| Despo_ONE | 0.50 | 0.50 | 0.50 | 0.51 | 0.51 | 0.66 |
| NY_Energy | -0.72 | -0.38 | -0.89 | -0.86 | -0.87 | -0.94 |
| LADI_ROCK | -3.35 | -2.73 | -3.26 | -2.60 | -2.57 | -2.79 |
| Tef_Dont_Make_Me | 0.50 | 0.50 | 0.50 | 0.50 | 0.51 | 0.63 |
| Holiday_Hustle_Demo | -3.58 | -2.36 | -3.40 | -3.78 | -3.99 | -4.10 |
| Perfect_Timing_My_Time | 0.48 | 0.38 | 0.50 | 0.51 | 0.40 | 0.49 |

## Safety

- **Despo_ONE**: nan_inf=0, channel_balance_ok=True, TP=-1.75 dBTP
- **NY_Energy**: nan_inf=0, channel_balance_ok=True, TP=-0.50 dBTP
- **LADI_ROCK**: nan_inf=0, channel_balance_ok=True, TP=-0.50 dBTP
- **Tef_Dont_Make_Me**: nan_inf=0, channel_balance_ok=True, TP=-2.62 dBTP
- **Holiday_Hustle_Demo**: nan_inf=0, channel_balance_ok=True, TP=-0.50 dBTP
- **Perfect_Timing_My_Time**: nan_inf=0, channel_balance_ok=True, TP=-0.50 dBTP

## Notes
- M1 frozen: `config/hiphop/default.json` (`m1-master01-baseline`). Outputs in `refs/evaluation_outputs/m1/` untouched.
- M2 deliverable: `refs/evaluation_outputs/m2/*_m2.wav` + reports.
- Outputs below −9 LUFS-I are intentional when lift would exceed relative crest/limiter stress budget.
- No claim that M2 is subjectively better — client A/B decides.
