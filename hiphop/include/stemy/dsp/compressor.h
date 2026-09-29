#pragma once

#include "stemy/audio/audio_buffer.h"
#include "stemy/core/status.h"

#include <cstdint>

namespace stemy::dsp {

/// Feed-forward peak compressor. ratio=1 or disabled → transparent.
struct CompressorParams {
  bool enabled = false;
  double threshold_db = 0.0;
  double ratio = 1.0;          // 1:1 = no compression
  double attack_ms = 10.0;
  double release_ms = 100.0;
  double makeup_gain_db = 0.0; // 0 = no makeup
  double knee_db = 0.0;        // hard knee
};

class Compressor {
public:
  [[nodiscard]] Status prepare(std::uint32_t sample_rate, const CompressorParams& params);
  void reset();
  [[nodiscard]] Status process(audio::AudioBuffer& buffer) const;

private:
  CompressorParams params_{};
  std::uint32_t sample_rate_ = 0;
  mutable float envelope_ = 0.0f;
  double attack_coef_ = 0.0;
  double release_coef_ = 0.0;
};

}  // namespace stemy::dsp
