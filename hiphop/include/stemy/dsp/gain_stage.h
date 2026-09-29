#pragma once

#include "stemy/audio/audio_buffer.h"
#include "stemy/core/status.h"

namespace stemy::dsp {

struct GainStageParams {
  bool enabled = true;
  double gain_db = 0.0;  // 0 dB = transparent
};

class GainStage {
public:
  [[nodiscard]] Status prepare(const GainStageParams& params);
  [[nodiscard]] Status process(audio::AudioBuffer& buffer) const;

private:
  GainStageParams params_{};
  float gain_linear_ = 1.0f;
};

}  // namespace stemy::dsp
