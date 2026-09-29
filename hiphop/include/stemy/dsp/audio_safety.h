#pragma once

#include "stemy/audio/audio_buffer.h"
#include "stemy/core/status.h"

#include <optional>

namespace stemy::dsp {

struct SafetyReport {
  bool had_nan_inf = false;
  bool had_clipping = false;
  std::size_t nan_inf_count = 0;
  std::size_t clipped_sample_count = 0;
  float max_abs_sample = 0.0f;

  /// Oversampled true-peak after safety stage (linear / dBTP).
  std::optional<double> true_peak_linear;
  std::optional<double> true_peak_dbtp;
  bool true_peak_safety_applied = false;
  float true_peak_safety_scale = 1.0f;

  /// Channel balance: 20*log10(rms_L / rms_R). 0 = equal RMS.
  std::optional<double> channel_balance_db;
  bool channel_balance_ok = true;

  /// Pre-limiter soft peak rounder telemetry (linear sample peak).
  bool soft_peak_round_enabled = false;
  float soft_peak_before_linear = 0.0f;
  float soft_peak_after_linear = 0.0f;
  float soft_peak_reduction_db = 0.0f;

  /// Peak sample-limiter gain reduction from last chain process (dB).
  float limiter_max_gr_db = 0.0f;
  float limiter_avg_gr_while_active_db = 0.0f;
  float limiter_p95_gr_while_active_db = 0.0f;
  float limiter_pct_time_gr_gt_1db = 0.0f;
  float limiter_pct_time_gr_gt_3db = 0.0f;
  float limiter_pct_time_gr_gt_6db = 0.0f;
  /// Fraction of frames with GR > 0 (0..100). Instrumentation only.
  float limiter_pct_time_active = 0.0f;
  bool limiter_true_peak_aware = false;
  float limiter_max_detected_peak_linear = 0.0f;
};

/// Post-process safety: detect/replace NaN/Inf, flush denormals, report clipping,
/// and compute left/right RMS balance.
[[nodiscard]] Status apply_output_safety(audio::AudioBuffer& buffer,
                                         SafetyReport& report,
                                         bool replace_non_finite = true,
                                         bool flush_denormals = true,
                                         double max_abs_channel_balance_db = 1.0);

[[nodiscard]] Status validate_gain_db(double gain_db, double min_db = -24.0, double max_db = 24.0);

/// Returns absolute L/R RMS difference in dB (0 if either channel silent-equal).
[[nodiscard]] double measure_channel_balance_db(const audio::AudioBuffer& buffer);

}  // namespace stemy::dsp
