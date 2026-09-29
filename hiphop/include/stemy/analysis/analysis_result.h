#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace stemy::analysis {

/// Band energy summary (linear power or dB as documented per field).
struct SpectralBandEnergies {
  /// Approximate bands (Hz): sub <60, bass 60–250, low-mid 250–500,
  /// mid 500–2k, high-mid 2k–6k, high >6k.
  /// Values are mean power spectral density contributions in linear power units
  /// (not yet normalized to a commercial target).
  double sub_bass = 0.0;
  double bass = 0.0;
  double low_mid = 0.0;
  double mid = 0.0;
  double high_mid = 0.0;
  double high = 0.0;
};

/// Aggregate analysis metrics. Units are documented per field.
/// Fields use std::optional when a metric may not have been computed.
struct AnalysisResult {
  std::uint32_t sample_rate = 0;
  std::uint16_t channel_count = 0;
  double duration_seconds = 0.0;

  /// ITU-R BS.1770 integrated loudness (LUFS).
  std::optional<double> integrated_loudness_lufs;
  /// Short-term loudness (LUFS), typically 3 s window; mean of valid blocks if aggregated.
  std::optional<double> short_term_loudness_lufs;
  /// Momentary loudness (LUFS), typically 400 ms; optional aggregate.
  std::optional<double> momentary_loudness_lufs;

  /// Sample peak as linear absolute amplitude (1.0 = 0 dBFS digital full scale).
  std::optional<double> sample_peak_linear;
  /// Sample peak in dBFS.
  std::optional<double> sample_peak_dbfs;

  /// True-peak (oversampled) as linear absolute amplitude.
  std::optional<double> true_peak_linear;
  /// True-peak in dBTP.
  std::optional<double> true_peak_dbtp;

  /// RMS of interleaved samples (linear).
  std::optional<double> rms_linear;
  /// RMS in dBFS.
  std::optional<double> rms_dbfs;

  /// Crest factor = sample_peak_linear / rms_linear (linear ratio).
  std::optional<double> crest_factor_linear;
  /// Crest factor in dB = 20*log10(crest_factor_linear).
  std::optional<double> crest_factor_db;

  /// Provisional dynamic-range indicators (not yet commercial "DR" standards).
  std::optional<double> dynamic_range_db;

  std::optional<SpectralBandEnergies> spectral_bands;

  /// Stereo width indicator in [0, ~1+]; 0 = mono, higher = wider. Provisional definition.
  std::optional<double> stereo_width;
  /// Mid-channel energy (linear power).
  std::optional<double> mid_energy;
  /// Side-channel energy (linear power).
  std::optional<double> side_energy;
  /// Pearson correlation between L and R in [-1, 1].
  std::optional<double> stereo_correlation;
  /// Pearson correlation of one-pole LPF'd L/R (<~120 Hz). Instrumentation only.
  std::optional<double> lf_stereo_correlation;
  /// Cutoff used for lf_stereo_correlation (Hz).
  std::optional<double> lf_stereo_correlation_cutoff_hz;

  /// Free-form notes from analyzers (debug / provenance).
  std::vector<std::string> notes;
};

}  // namespace stemy::analysis
