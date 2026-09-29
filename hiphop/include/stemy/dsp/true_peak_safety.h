#pragma once

#include "stemy/audio/audio_buffer.h"
#include "stemy/core/status.h"
#include "stemy/dsp/audio_safety.h"

namespace stemy::dsp {

/// Final oversampled true-peak safety stage (separate from sample-peak limiter).
/// If measured true peak exceeds ceiling, applies uniform stereo gain reduction
/// and re-measures. This is a safety control, not creative processing.
struct TruePeakSafetyParams {
  bool enabled = true;
  double ceiling_dbtp = -0.5;
  int oversample_factor = 4;
  int max_iterations = 3;
  /// Extra margin so post-scale TP stays at/under ceiling despite interp error.
  double margin_db = 0.05;
};

class TruePeakSafety {
public:
  [[nodiscard]] Status prepare(const TruePeakSafetyParams& params);
  [[nodiscard]] Status process(audio::AudioBuffer& buffer, SafetyReport& report) const;

private:
  TruePeakSafetyParams params_{};
};

}  // namespace stemy::dsp
