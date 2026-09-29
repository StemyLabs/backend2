#pragma once

#include "stemy/audio/audio_buffer.h"
#include "stemy/analysis/true_peak_measure.h"
#include "stemy/core/status.h"

#include <array>
#include <cstdint>
#include <vector>

namespace stemy::dsp {

/// Lookahead peak limiter. When disabled, pass-through.
struct LimiterParams {
  bool enabled = false;
  double ceiling_dbfs = -1.0;
  double release_ms = 50.0;
  double lookahead_ms = 1.0;
  /// When true, lookahead peak detection uses reconstructed inter-sample peaks
  /// (same Kaiser-sinc ×N kernel as the corrected true-peak meter).
  bool true_peak_aware = false;
  int true_peak_oversample = 4;
};

/// Telemetry from the last process() call.
struct LimiterTelemetry {
  float max_gr_db = 0.0f;
  /// Mean GR over frames where GR > 0 (limiter actively reducing).
  float avg_gr_while_active_db = 0.0f;
  /// 95th percentile of GR among active frames.
  float p95_gr_while_active_db = 0.0f;
  float pct_time_gr_gt_1db = 0.0f;
  float pct_time_gr_gt_3db = 0.0f;
  float pct_time_gr_gt_6db = 0.0f;
  std::uint64_t frames_total = 0;
  std::uint64_t frames_active = 0;
  bool true_peak_aware = false;
  float max_detected_peak_linear = 0.0f;
};

class Limiter {
public:
  [[nodiscard]] Status prepare(std::uint32_t sample_rate, const LimiterParams& params);
  void reset();
  [[nodiscard]] Status process(audio::AudioBuffer& buffer) const;

  [[nodiscard]] float max_gain_reduction_db() const { return telemetry_.max_gr_db; }
  [[nodiscard]] const LimiterTelemetry& telemetry() const { return telemetry_; }

private:
  LimiterParams params_{};
  std::uint32_t sample_rate_ = 0;
  std::size_t lookahead_samples_ = 0;
  mutable std::vector<float> delay_l_;
  mutable std::vector<float> delay_r_;
  mutable std::size_t delay_pos_ = 0;
  mutable float gain_ = 1.0f;
  mutable LimiterTelemetry telemetry_{};
  /// 0.1 dB bins from 0..24 dB for active-GR percentile.
  static constexpr int kHistBins = 240;
  mutable std::array<std::uint64_t, kHistBins> gr_hist_{};
  mutable analysis::TruePeakIspDetector isp_detector_{4};
  mutable float isp_peak_hold_ = 0.0f;
  double release_coef_ = 0.0;
  double isp_hold_release_coef_ = 0.0;
  float ceiling_linear_ = 1.0f;
  /// Slightly tighter ceiling for ISP control loop (native brick-wall stays at ceiling_linear_).
  float control_ceiling_linear_ = 1.0f;
};

}  // namespace stemy::dsp
