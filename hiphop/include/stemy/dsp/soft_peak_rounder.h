#pragma once

#include "stemy/audio/audio_buffer.h"
#include "stemy/core/status.h"

namespace stemy::dsp {

/// Oversampled soft peak rounder: transparent below ceiling, tanh-shaped
/// approach above it. Used as optional pre-limiter crest management.
struct SoftPeakRoundParams {
  bool enabled = false;
  /// Soft ceiling in dBFS (sample). Typical pre-limiter experimental range +2…+6
  /// (absolute sample peak after adaptive gain; 0 dBFS = full-scale).
  double ceiling_dbfs = 4.0;
  /// Softness / drive into the ceiling curve (higher = harder knee).
  double drive = 1.5;
  int oversample_factor = 4;
};

class SoftPeakRounder {
public:
  [[nodiscard]] Status prepare(const SoftPeakRoundParams& params);
  [[nodiscard]] Status process(audio::AudioBuffer& buffer) const;

  [[nodiscard]] float peak_before_linear() const { return peak_before_; }
  [[nodiscard]] float peak_after_linear() const { return peak_after_; }

private:
  SoftPeakRoundParams params_{};
  mutable float peak_before_ = 0.0f;
  mutable float peak_after_ = 0.0f;
};

}  // namespace stemy::dsp
