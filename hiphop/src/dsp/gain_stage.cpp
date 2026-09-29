#include "stemy/dsp/gain_stage.h"
#include "stemy/core/dsp_math.h"
#include "stemy/dsp/audio_safety.h"

#include <cmath>

namespace stemy::dsp {

Status GainStage::prepare(const GainStageParams& params) {
  if (auto st = validate_gain_db(params.gain_db); !st) {
    return st;
  }
  params_ = params;
  gain_linear_ = static_cast<float>(db_to_linear(params.gain_db));
  if (gain_linear_ > kMaxAbsGainLinear) {
    return Status::Error("GainStage: excessive gain");
  }
  return Status::Ok();
}

Status GainStage::process(audio::AudioBuffer& buffer) const {
  if (!params_.enabled || std::abs(params_.gain_db) < 1e-12) {
    return Status::Ok();
  }
  for (std::size_t i = 0; i < buffer.sample_count(); ++i) {
    buffer.data()[i] *= gain_linear_;
  }
  return Status::Ok();
}

}  // namespace stemy::dsp
