# Hip-Hop FINAL R0 — Engineering Candidate Report

**STOP.** First final-phase listening candidate. No Revision Round 1.

## Commits / freeze

| Item | Hash / value |
|---|---|
| `m2b-frozen` tag (immutable) | `25f352e` |
| Hardening commit | `017cf2e` — Validate true peak and harden final-phase adaptive behavior |
| Config | `config/hiphop/final_r0.json` (`hiphop-final-r0`) |
| Tests | **69/69** passed |
| M1 Despo repro | output **−9.00** LUFS (`m1-master01-baseline`) |

## TP-aware limiter architecture

Native-rate audio → 4× Kaiser-sinc ISP detector on lookahead tip (shared kernel with corrected TP meter) → instant-attack peak-hold → existing gain computer/release → gain on delayed native samples → sample brick-wall at ceiling → independent TP verify → `TruePeakSafety` emergency only.

- Control loop uses ceiling − 0.35 dB margin to absorb FIR/gain-modulation residuals.
- Client target ≈ −1.0 dBTP; R0 engineering ceiling **−1.05 dBTP**.
- No compressor / multiband / third-party limiter.

## Extreme-sub guard

- General adaptive: bass-body share ramp 0.70 → 0.85, extra low-shelf up to −1.0 dB @ ~60 Hz, total shelf floor −3.0 dB.
- Stacks with quiet-path sub shelf; additive low-weight remains off when bass already sufficient.
- Telemetry: `extreme_sub_guard_*`, `bass_body_share`.

## Comparison tables

### Bumpman

| Metric | INPUT | M2B_BASELINE | FINAL_PRECHECK | FINAL_R0 |
|---|---:|---:|---:|---:|
| LUFS-I | -21.64 | -10.03 | -10.75 | -10.84 |
| short-term LUFS | -19.52 | -8.32 | -9.04 | -9.33 |
| sample peak | -5.00 | -1.00 | -1.72 | -1.40 |
| verified true peak | -4.97 | -0.33 | -1.05 | -1.36 |
| RMS | -24.25 | -12.49 | -13.21 | -13.31 |
| crest | 19.25 | 11.49 | 11.49 | 11.91 |
| correlation | 0.845 | 0.790 | 0.790 | 0.778 |
| width metric | 0.078 | 0.105 | 0.105 | 0.111 |
| LF corr <120Hz | 0.954 | n/a | 0.921 | 0.912 |
| bass-body share | 0.541 | n/a | n/a | n/a |
| HF share | 0.133 | n/a | n/a | n/a |
| adaptive path | — | QUIET/OPEN | QUIET/OPEN | QUIET/OPEN |
| gain dB | — | 12.75 | 12.75 | 12.75 |
| M1 baseline gain | — | 12.00 | 12.00 | 12.00 |
| punch | — | ON | ON | ON |
| saturation | — | ON | ON | ON |
| low-weight additive | — | OFF | OFF | OFF |
| HF | — | OFF | OFF | OFF |
| width factor | — | 1.00 | 1.00 | 1.00 |
| extreme-sub enabled | — | n/a | n/a | OFF |
| extreme-sub gain dB | — | n/a | n/a | 0.00 |
| bass_body_share (decision) | — | n/a | n/a | 0.541 |
| ISP-aware limiter | — | no | no | ON |
| limiter max GR | — | 8.45 | 8.45 | 8.85 |
| limiter avg GR | — | 0.69 | 0.69 | 1.36 |
| limiter p95 GR | — | 3.10 | 3.10 | 5.10 |
| limiter active % | — | n/a | 93.69 | 93.69 |
| >1/>3/>6 % | — | 22.3/5.0/0.1 | 22.3/5.0/0.1 | 38.0/17.2/1.7 |
| TP safety applied | — | False | True | False |
| TP safety scale | — | 1.0000 | 0.9205 | 1.0000 |
| TP safety corr dB | — | n/a | n/a | 0.00 |
| NaN/Inf | — | 0 | 0 | 0 |
| clipped samples | — | 0 | 0 | 0 |

**R0 processing detail:**
- punch: boost=1.15 sustain_cut=0.3 mix=0.38
- sat: drive=0.8 mix=0.12
- HF: 0.0 dB @ 1000.0 Hz enabled=False
- low_end shelf: -2.0 dB @ 55.0 Hz
- extreme_sub reason: `below_extreme_bass_threshold`
- stereo: `m2 stereo: bypassed (input already sufficiently wide)`
- limiter: `limiter: enabled @ -1.05 dBTP (ISP-aware x4)`

### Antonio Slim

| Metric | INPUT | M2B_BASELINE | FINAL_PRECHECK | FINAL_R0 |
|---|---:|---:|---:|---:|
| LUFS-I | -5.80 | -8.49 | -8.50 | -8.50 |
| short-term LUFS | -4.74 | -7.43 | -7.44 | -7.44 |
| sample peak | -0.11 | -1.38 | -2.80 | -2.80 |
| verified true peak | 1.13 | -1.31 | -1.57 | -1.57 |
| RMS | -8.63 | -11.32 | -11.33 | -11.33 |
| crest | 8.52 | 9.94 | 8.52 | 8.52 |
| correlation | 0.957 | 0.954 | 0.957 | 0.957 |
| width metric | 0.022 | 0.023 | 0.022 | 0.022 |
| LF corr <120Hz | 0.988 | n/a | 0.988 | 0.988 |
| bass-body share | 0.449 | n/a | n/a | n/a |
| HF share | 0.156 | n/a | n/a | n/a |
| adaptive path | — | HOT/DENSE | HOT/DENSE | HOT/DENSE |
| gain dB | — | -2.70 | -2.70 | -2.70 |
| M1 baseline gain | — | -3.20 | -3.20 | -3.20 |
| punch | — | OFF | OFF | OFF |
| saturation | — | OFF | OFF | OFF |
| low-weight additive | — | OFF | OFF | OFF |
| HF | — | OFF | OFF | OFF |
| width factor | — | 1.06 | 1.00 | 1.00 |
| extreme-sub enabled | — | n/a | n/a | OFF |
| extreme-sub gain dB | — | n/a | n/a | 0.00 |
| bass_body_share (decision) | — | n/a | n/a | 0.449 |
| ISP-aware limiter | — | no | no | ON |
| limiter max GR | — | 0.00 | 0.00 | 0.00 |
| limiter avg GR | — | 0.00 | 0.00 | 0.00 |
| limiter p95 GR | — | 0.00 | 0.00 | 0.00 |
| limiter active % | — | n/a | 0.00 | 0.00 |
| >1/>3/>6 % | — | 0.0/0.0/0.0 | 0.0/0.0/0.0 | 0.0/0.0/0.0 |
| TP safety applied | — | False | False | False |
| TP safety scale | — | 1.0000 | 1.0000 | 1.0000 |
| TP safety corr dB | — | n/a | n/a | 0.00 |
| NaN/Inf | — | 0 | 0 | 0 |
| clipped samples | — | 0 | 0 | 0 |

**R0 processing detail:**
- punch: boost=0.0 sustain_cut=0.0 mix=0.0
- sat: drive=0.0 mix=0.0
- HF: 0.0 dB @ 1000.0 Hz enabled=False
- low_end shelf: 0.0 dB @ 80.0 Hz
- extreme_sub reason: `below_extreme_bass_threshold`
- stereo: `m2 stereo: hot_dense_bypass (width=1.0)`
- limiter: `limiter: enabled @ -1.05 dBTP (ISP-aware x4)`

### DaMind

| Metric | INPUT | M2B_BASELINE | FINAL_PRECHECK | FINAL_R0 |
|---|---:|---:|---:|---:|
| LUFS-I | -20.65 | -10.31 | -11.25 | -11.44 |
| short-term LUFS | -18.70 | -9.01 | -9.95 | -10.21 |
| sample peak | -2.94 | -1.00 | -1.94 | -1.40 |
| verified true peak | -2.94 | -0.11 | -1.05 | -1.31 |
| RMS | -21.53 | -11.42 | -12.36 | -12.85 |
| crest | 18.58 | 10.42 | 10.42 | 11.45 |
| correlation | 0.940 | 0.913 | 0.913 | 0.900 |
| width metric | 0.030 | 0.043 | 0.043 | 0.050 |
| LF corr <120Hz | 0.978 | n/a | 0.976 | 0.971 |
| bass-body share | 0.825 | n/a | n/a | n/a |
| HF share | 0.033 | n/a | n/a | n/a |
| adaptive path | — | QUIET/OPEN | QUIET/OPEN | QUIET/OPEN |
| gain dB | — | 12.40 | 12.40 | 12.40 |
| M1 baseline gain | — | 11.65 | 11.65 | 11.65 |
| punch | — | ON | ON | ON |
| saturation | — | ON | ON | ON |
| low-weight additive | — | OFF | OFF | OFF |
| HF | — | ON | ON | ON |
| width factor | — | 1.06 | 1.06 | 1.06 |
| extreme-sub enabled | — | n/a | n/a | ON |
| extreme-sub gain dB | — | n/a | n/a | -0.93 |
| bass_body_share (decision) | — | n/a | n/a | 0.825 |
| ISP-aware limiter | — | no | no | ON |
| limiter max GR | — | 10.88 | 10.88 | 10.98 |
| limiter avg GR | — | 0.89 | 0.89 | 1.74 |
| limiter p95 GR | — | 4.30 | 4.30 | 7.20 |
| limiter active % | — | n/a | 94.12 | 94.12 |
| >1/>3/>6 % | — | 25.1/8.0/2.2 | 25.1/8.0/2.2 | 39.7/21.7/8.1 |
| TP safety applied | — | False | True | False |
| TP safety scale | — | 1.0000 | 0.8975 | 1.0000 |
| TP safety corr dB | — | n/a | n/a | 0.00 |
| NaN/Inf | — | 0 | 0 | 0 |
| clipped samples | — | 0 | 0 | 0 |

**R0 processing detail:**
- punch: boost=1.15 sustain_cut=0.3 mix=0.38
- sat: drive=0.8 mix=0.12
- HF: 0.5 dB @ 9000.0 Hz enabled=True
- low_end shelf: -2.926994709451198 dB @ 60.0 Hz
- extreme_sub reason: `extreme_sub_guard: bass_body=0.825193 extra=-0.926995 dB @ 60 Hz (shelf total -2.92699 dB)`
- stereo: `m2 stereo: width=1.06 side_hpf=120 Hz (no widen below)`
- limiter: `limiter: enabled @ -1.05 dBTP (ISP-aware x4)`

## Generated WAV paths

```
refs/evaluation_outputs/hiphop_final_r0/Bumpman_Shoot_My_Shot_HIPHOP_FINAL_R0.wav
refs/evaluation_outputs/hiphop_final_r0/Bumpman_Shoot_My_Shot_HIPHOP_FINAL_R0_report.json
refs/evaluation_outputs/hiphop_final_r0/Antonio_Slim_Easy_HIPHOP_FINAL_R0.wav
refs/evaluation_outputs/hiphop_final_r0/Antonio_Slim_Easy_HIPHOP_FINAL_R0_report.json
refs/evaluation_outputs/hiphop_final_r0/DaMind_Whos_Da_Mind_HIPHOP_FINAL_R0.wav
refs/evaluation_outputs/hiphop_final_r0/DaMind_Whos_Da_Mind_HIPHOP_FINAL_R0_report.json
```

## Objective engineering concerns

1. ISP-aware limiter recovers level vs PRECHECK global safety atten on open paths; confirm by ear that punch/density are preserved.
2. Extreme-sub guard is general but only DaMind-class bass-body should trigger — verify Bumpman/Antonio stay off.
3. Residual TruePeakSafety should be ≤~0.1 dB ideally; larger corrections still mean control-loop residual.
4. Do not chase −9 LUFS if limiter stress rises.

## Listening questions for Vance (do not auto-answer)

### Bumpman
- Enough weight/punch?
- Commercial density adequate?
- Too limited or still natural?
- Warmth consistent with Bumpman V1 direction?

### Antonio
- Does width bypass preserve the finished source?
- Any need beyond level/TP normalization?

### DaMind
- Low end tighter without becoming thin?
- HF +0.5 appropriate?
- Width 1.06 appropriate?
- Punch/warmth balanced?

### Across all three
- Chain consistent while preserving source identity?
- Loudness competitive enough?
- Vocals clear/forward?
- Any grit/distortion from limiter/saturation?
