# Hip-Hop Parameter Reference (Milestone 1)

## Configuration file

Default: `config/hiphop/default.json`  
Loaded via: `stemy::config::load_hiphop_config` or CLI `--config`.

Version string: `m1-master01-baseline`

### Stages (`stages`)

| Key | Type | Meaning |
|-----|------|---------|
| `input_gain` | bool | Enable input gain stage |
| `eq` | bool | Enable EQ stage |
| `low_end` | bool | Enable low-end control stage |
| `dynamics` | bool | Enable compressor stage (always bypassed by adaptive in M1) |
| `saturation` | bool | Enable saturation stage (always bypassed by adaptive in M1) |
| `stereo` | bool | Enable stereo stage (width forced to 1.0 in M1) |
| `limiter` | bool | Allow sample-peak limiter when adaptive enables it |
| `output_safety` | bool | NaN/clip report, channel-balance check, **true-peak safety** |

### Provisional targets (`provisional`)

| Key | Default | Unit | Meaning |
|-----|---------|------|---------|
| `target_integrated_lufs` | −9.0 | LUFS | Nominal loudness reference (not a hard exact match) |
| `loudness_tolerance_lufs` | 0.75 | LU | In-pocket band around target |
| `max_lift_db` | 12.0 | dB | Cap on loudness lift |
| `max_reduction_db` | 4.0 | dB | Cap on reduction for hot inputs |
| `target_true_peak_dbtp` | −0.5 | dBTP | Output true-peak ceiling (Gate A) |
| `enable_tonal_trim_on_quiet_input` | true | bool | Mild MASTER_01-direction EQ/low-end on quiet inputs only |
| `quiet_input_lufs_threshold` | −14.0 | LUFS | Below this → quiet path |
| `reference_crest_db` | 10.2 | dB | Contextual note only (no compressor in M1) |
| `prefer_tight_stereo` | true | bool | Keep stereo width = 1.0 |

## Adaptive decision logic

1. Measure input `AnalysisResult`.
2. Compare integrated LUFS to `target_integrated_lufs` ± tolerance.
3. **Quiet:** apply positive gain (≤ `max_lift_db`), enable sample-peak limiter, optional tonal trim.
4. **In pocket:** ~0 dB gain.
5. **Hot:** apply negative gain (≥ −`max_reduction_db`), do not boost.
6. Saturation / compressor remain **off**.
7. Stereo width remains **1.0**.
8. Final **oversampled true-peak safety** enforces `target_true_peak_dbtp`.

## Quiet-path tonal trim (approved M1 baseline)

Applied only when input LUFS < `quiet_input_lufs_threshold`:

- Low shelf −2 dB @ 55 Hz
- Peak +1.5 dB @ 100 Hz
- Peak +1.5 dB @ 350 Hz
- High shelf −1.5 dB @ 8 kHz

These are provisional magnitudes, not final commercial curves.

## ProcessingDecision fields

Produced by `AdaptiveEngine`, consumed by `HipHopChain`:

- `input_gain`, `eq`, `low_end`, `compressor`, `saturation`, `stereo`, `limiter`, `output_gain`
- `decision_notes` — human-readable audit trail

## Safety / output requirements

| Requirement | Mechanism |
|-------------|-----------|
| No NaN/Inf | Input reject + output replace/report |
| Sample-peak limiting | `Limiter` (when enabled) |
| True peak ≤ −0.5 dBTP | `TruePeakSafety` (oversampled) + mastering hard check |
| Channel balance | Relative L/R RMS change ≤ 0.25 dB vs input |
| Determinism | Same input+config → sample abs diff ≤ **1e-6** (see regression tests) |

## Not included in M1

- Compressor coloration
- Saturation / harmonic exciters
- Stereo widening
- New EQ curves beyond quiet-path trim above
- Exact LUFS matching of MASTER_01 on every title
- MP3/AAC decode in the C++ engine (WAV only)
