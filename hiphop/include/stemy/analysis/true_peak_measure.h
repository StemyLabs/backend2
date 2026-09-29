#pragma once

#include "stemy/audio/audio_buffer.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace stemy::analysis {

/// Oversampled true-peak measurement (ITU-R BS.1770-style).
/// Method: zero-stuff ×N + Kaiser-windowed sinc low-pass (not linear interpolation).
/// Shared by TruePeakAnalyzer and TruePeakSafety.
[[nodiscard]] double measure_true_peak_linear(const audio::AudioBuffer& buffer,
                                              int oversample_factor = 4);

/// Sample peak only (no oversampling) — for tests comparing against true peak.
[[nodiscard]] double measure_sample_peak_linear(const audio::AudioBuffer& buffer);

/// Streaming inter-sample peak detector using the same 4× (or 2/8) Kaiser-sinc kernel.
/// Used by the ISP-aware limiter for lookahead peak detection.
class TruePeakIspDetector {
public:
  explicit TruePeakIspDetector(int oversample_factor = 4);

  void reset();
  void set_oversample_factor(int oversample_factor);

  /// Push one mono sample; returns max |sample| and |reconstructed| over L phases.
  [[nodiscard]] double push_mono(float x);

  /// Push one stereo frame; returns max ISP/sample peak across both channels.
  [[nodiscard]] double push_stereo(float left, float right);

  [[nodiscard]] int oversample_factor() const { return L_; }

private:
  void ensure_kernel();
  [[nodiscard]] double emit_peak(std::vector<double>& hist);

  int L_ = 4;
  int n_poly_ = 0;
  const std::vector<double>* h_ = nullptr;  // owned by process-wide cache
  std::vector<double> hist_l_;
  std::vector<double> hist_r_;
};

}  // namespace stemy::analysis
