# Analysis metrics (Milestone 1)

Analyzers **measure only**. They do not choose mastering parameters.

## `AnalysisResult` fields and units

| Field | Unit | Notes |
|-------|------|-------|
| `sample_rate` | Hz | |
| `channel_count` | count | M1 expects 2 |
| `duration_seconds` | s | |
| `integrated_loudness_lufs` | LUFS | BS.1770 K-weighted, gated |
| `short_term_loudness_lufs` | LUFS | ~3 s window max (practical aggregate) |
| `momentary_loudness_lufs` | LUFS | ~400 ms block max |
| `sample_peak_linear` | linear | 1.0 = 0 dBFS |
| `sample_peak_dbfs` | dBFS | |
| `true_peak_linear` | linear | oversampled |
| `true_peak_dbtp` | dBTP | |
| `rms_linear` | linear | interleaved RMS |
| `rms_dbfs` | dBFS | |
| `crest_factor_linear` | ratio | peak/RMS |
| `crest_factor_db` | dB | |
| `dynamic_range_db` | dB | **provisional** peak−RMS |
| `spectral_bands.*` | linear power | fixed Hz bands via FFT |
| `stereo_width` | 0..1 | **provisional** side/(mid+side) |
| `mid_energy` / `side_energy` | linear power | |
| `stereo_correlation` | −1..1 | Pearson L/R |

Unset metrics use `std::optional`.

## Analyzers

- `PeakAnalyzer` — sample peak, RMS, crest
- `TruePeakAnalyzer` — oversampled true peak (modular; separate from sample peak)
- `LoudnessAnalyzer` — BS.1770-style K-weighting + absolute/relative gating
- `DynamicsAnalyzer` — provisional DR indicator
- `SpectralAnalyzer` — band energies via internal FFT abstraction
- `StereoAnalyzer` — correlation, M/S energy, provisional width
- `AnalysisPipeline` — runs the suite

## Thresholds

No commercial loudness/spectral **targets** are assumed in M1.
Adaptive rules currently remain transparent and record notes for later tuning.
