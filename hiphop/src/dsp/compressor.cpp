#include "stemy/dsp/compressor.h"
#include "stemy/core/dsp_math.h"

#include <algorithm>
#include <cmath>

namespace stemy::dsp {

Status Compressor::prepare(std::uint32_t sample_rate, const CompressorParams& params) {
  if (sample_rate == 0) {
    return Status::Error("Compressor: invalid sample rate");
  }
  if (!(params.ratio >= 1.0) || params.ratio > 20.0) {
    return Status::Error("Compressor: invalid ratio");
  }
  if (!(params.attack_ms > 0.0) || !(params.release_ms > 0.0)) {
    return Status::Error("Compressor: invalid attack/release");
  }
  if (!std::isfinite(params.threshold_db) || !std::isfinite(params.makeup_gain_db)) {
    return Status::Error("Compressor: non-finite parameters");
  }

  params_ = params;
  sample_rate_ = sample_rate;
  attack_coef_ = std::exp(-1.0 / (0.001 * params.attack_ms * sample_rate));
  release_coef_ = std::exp(-1.0 / (0.001 * params.release_ms * sample_rate));
  reset();
  return Status::Ok();
}

void Compressor::reset() { envelope_ = 0.0f; }

Status Compressor::process(audio::AudioBuffer& buffer) const {
  if (!params_.enabled || params_.ratio <= 1.0001 || buffer.channel_count() != 2) {
    return Status::Ok();  // transparent
  }

  const float makeup = static_cast<float>(db_to_linear(params_.makeup_gain_db));
  const double thresh_lin = db_to_linear(params_.threshold_db);

  for (std::size_t i = 0; i < buffer.frame_count(); ++i) {
    const float l = buffer.at(i, 0);
    const float r = buffer.at(i, 1);
    const float peak = std::max(std::fabs(l), std::fabs(r));

    if (peak > envelope_) {
      envelope_ = static_cast<float>(attack_coef_ * envelope_ + (1.0 - attack_coef_) * peak);
    } else {
      envelope_ = static_cast<float>(release_coef_ * envelope_ + (1.0 - release_coef_) * peak);
    }

    float gain = 1.0f;
    if (envelope_ > thresh_lin && envelope_ > 0.0f) {
      const double env_db = linear_to_db(envelope_);
      const double overshoot = env_db - params_.threshold_db;
      const double gr_db = overshoot - (overshoot / params_.ratio);
      gain = static_cast<float>(db_to_linear(-gr_db));
    }

    buffer.at(i, 0) = l * gain * makeup;
    buffer.at(i, 1) = r * gain * makeup;
  }
  return Status::Ok();
}

}  // namespace stemy::dsp
