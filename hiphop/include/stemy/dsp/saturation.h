#pragma once

#include "stemy/audio/audio_buffer.h"
#include "stemy/core/status.h"

#include <cstdint>
#include <string>

namespace stemy::dsp {

enum class SaturationCurve {
  TanhNormalized = 0,  // default modular soft clip
};

/// Harmonic saturation with optional oversampling.
/// Default: drive=0, mix=0 → transparent.
struct SaturationParams {
  bool enabled = false;
  double drive = 0.0;
  double mix = 0.0;
  double output_gain_db = 0.0;
  int oversample_factor = 4;  // 1 = off; 2/4/8 supported
  SaturationCurve curve = SaturationCurve::TanhNormalized;
  /// If true, scale wet so RMS ≈ dry (approximate through-gain).
  bool preserve_gain = true;
};

class Saturation {
public:
  [[nodiscard]] Status prepare(const SaturationParams& params);
  [[nodiscard]] Status process(audio::AudioBuffer& buffer) const;

private:
  SaturationParams params_{};
};

}  // namespace stemy::dsp
