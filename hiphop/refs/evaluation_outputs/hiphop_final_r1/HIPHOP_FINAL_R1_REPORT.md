# Hip-Hop FINAL R1 — Revision Round 1 Report

**STOP.** Structured R1 only. No Revision Round 2.

## Freeze

| Item | Value |
|---|---|
| R0 freeze tag | `hiphop-final-r0` → `ba3ab04` |
| `m2b-frozen` | `25f352e` (untouched) |
| R0 config | `config/hiphop/final_r0.json` (untouched) |
| R1 config | `config/hiphop/final_r1.json` (`hiphop-final-r1`) |
| Tests | **79/79** |
| M1 Despo | −9.00 LUFS |

## R1 change summary

Stress-gated clean-loudness refinement for quiet/open premixes only (+0.25 dB steps, max +1.5 dB). Stops at −10 LUFS-I or stress limits. Extreme-sub compensation ≤50% of |atten|. HOT/DENSE excluded. Character/ISP/TP architecture unchanged from R0.

### Bumpman

| Metric | R0 | R1 |
|---|---:|---:|
| LUFS-I | -10.84 | -10.54 |
| true peak | -1.36 | -1.26 |
| sample peak | -1.40 | -1.40 |
| crest | 11.91 | 11.57 |
| max GR | 8.85 | 9.83 |
| avg GR | 1.36 | 1.91 |
| p95 GR | 5.10 | 6.00 |
| >1/>3/>6 % | 37.98/17.16/1.74 | 49.19/25.75/4.57 |
| active % | 93.69 | 93.69 |
| width factor | 1.00 | 1.00 |
| corr | 0.78 | 0.77 |
| LF corr | 0.91 | 0.91 |
| bass-body out | n/a | n/a |
| input gain | 12.75 | 13.75 |
| TP safety scale | 1.00 | 1.00 |

**R1 clean-loudness telemetry**

- enabled: True
- extra gain dB: 1.0
- extreme-sub comp dB: 0.0
- candidates tested: 6
- stop reason: `stress_limited:avg_gr`
- target reached: False
- stress limited: True
- extreme_sub guard: False gain=0.0

### Antonio Slim

| Metric | R0 | R1 |
|---|---:|---:|
| LUFS-I | -8.50 | -8.50 |
| true peak | -1.57 | -1.57 |
| sample peak | -2.80 | -2.80 |
| crest | 8.52 | 8.52 |
| max GR | 0.00 | 0.00 |
| avg GR | 0.00 | 0.00 |
| p95 GR | 0.00 | 0.00 |
| >1/>3/>6 % | 0.00/0.00/0.00 | 0.00/0.00/0.00 |
| active % | 0.00 | 0.00 |
| width factor | 1.00 | 1.00 |
| corr | 0.96 | 0.96 |
| LF corr | 0.99 | 0.99 |
| bass-body out | n/a | n/a |
| input gain | -2.70 | -2.70 |
| TP safety scale | 1.00 | 1.00 |

**R1 clean-loudness telemetry**

- enabled: False
- extra gain dB: 0.0
- extreme-sub comp dB: 0.0
- candidates tested: 0
- stop reason: `not_eligible_hot_dense_or_closed`
- target reached: False
- stress limited: False
- extreme_sub guard: False gain=0.0

### DaMind

| Metric | R0 | R1 |
|---|---:|---:|
| LUFS-I | -11.44 | -11.44 |
| true peak | -1.31 | -1.31 |
| sample peak | -1.40 | -1.40 |
| crest | 11.45 | 11.45 |
| max GR | 10.98 | 10.98 |
| avg GR | 1.74 | 1.74 |
| p95 GR | 7.20 | 7.20 |
| >1/>3/>6 % | 39.68/21.73/8.10 | 39.68/21.73/8.10 |
| active % | 94.12 | 94.12 |
| width factor | 1.06 | 1.06 |
| corr | 0.90 | 0.90 |
| LF corr | 0.97 | 0.97 |
| bass-body out | n/a | n/a |
| input gain | 12.40 | 12.40 |
| TP safety scale | 1.00 | 1.00 |

**R1 clean-loudness telemetry**

- enabled: True
- extra gain dB: 0.0
- extreme-sub comp dB: 0.0
- candidates tested: 1
- stop reason: `stress_limited_at_base:p95_gr`
- target reached: False
- stress limited: True
- extreme_sub guard: True gain=-0.9269947094511978

## WAV paths

```
refs/evaluation_outputs/hiphop_final_r1/Bumpman_Shoot_My_Shot_HIPHOP_FINAL_R1.wav
refs/evaluation_outputs/hiphop_final_r1/Antonio_Slim_Easy_HIPHOP_FINAL_R1.wav
refs/evaluation_outputs/hiphop_final_r1/DaMind_Whos_Da_Mind_HIPHOP_FINAL_R1.wav
```

## Objective concerns

- Loudness remains subordinate to stress gates; tracks may stay below −10 if clean gain is unavailable.
- Antonio must remain essentially identical to R0.

## Short summary for Vance

R1 only tries small clean loudness lifts on open premixes (Bumpman/DaMind). Antonio is unchanged by design. Please listen for punch/clarity vs the modest level change.

## Accepted extra gain

| Track | Extra dB | Stop | Target (−10) | Notes |
|---|---:|---|---|---|
| Bumpman | **+1.00** | stress_limited (avg GR) | not fully (landed −10.54) | In/near client −10…−9 preference; +1 dB clean lift |
| DaMind | **+0.00** | stress_limited at compensation attempt (p95) | no | Extreme-sub comp +0.45 rejected; R0 gain retained |
| Antonio | **+0.00** | not_eligible_hot_dense | n/a | Client-approved; unchanged |

## Limiter stress R0 → R1

| Track | max GR R0→R1 | avg GR R0→R1 | p95 GR R0→R1 |
|---|---|---|---|
| Bumpman | 8.85 → 9.83 | 1.36 → 1.91 | 5.10 → 6.00 |
| DaMind | 10.98 → 10.98 | 1.74 → 1.74 | 7.20 → 7.20 |
| Antonio | 0 → 0 | 0 → 0 | 0 → 0 |

## Verified TP

All R1 outputs TP ≤ −1.0 dBTP (Bumpman −1.26, Antonio −1.57, DaMind −1.31). Safety scale 1.0.
