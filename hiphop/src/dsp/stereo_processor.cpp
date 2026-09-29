#include "stemy/dsp/stereo_processor.h"

#include <cmath>

namespace stemy::dsp {

Status StereoProcessor::prepare(const StereoProcessorParams& params) {
  return prepare(sample_rate_ > 0 ? sample_rate_ : 48000, params);
}

Status StereoProcessor::prepare(std::uint32_t sample_rate, const StereoProcessorParams& params) {
  if (sample_rate == 0) {
    return Status::Error("StereoProcessor: invalid sample rate");
  }
  if (!std::isfinite(params.width) || params.width < 0.0 || params.width > 2.0) {
    return Status::Error("StereoProcessor: width out of allowed range [0, 2]");
  }
  if (!std::isfinite(params.side_hpf_hz) || params.side_hpf_hz < 0.0) {
    return Status::Error("StereoProcessor: invalid side_hpf_hz");
  }
  if (params.side_hpf_hz > 0.0 && params.side_hpf_hz >= 0.45 * sample_rate) {
    return Status::Error("StereoProcessor: side_hpf_hz too high for sample rate");
  }
  params_ = params;
  sample_rate_ = sample_rate;
  reset();
  if (params_.side_hpf_hz > 0.0) {
    side_hp_.set_coeffs(BiquadFilter::highpass(sample_rate_, params_.side_hpf_hz, 0.707));
  }
  return Status::Ok();
}

void StereoProcessor::reset() {
  side_hp_.reset();
}

Status StereoProcessor::process(audio::AudioBuffer& buffer) const {
  if (!params_.enabled || buffer.channel_count() != 2) {
    return Status::Ok();
  }
  const bool apply_width = std::abs(params_.width - 1.0) >= 1e-12;
  const bool apply_side_hp = params_.side_hpf_hz > 0.0;
  if (!apply_width && !apply_side_hp) {
    return Status::Ok();
  }

  const float w = static_cast<float>(params_.width);
  for (std::size_t i = 0; i < buffer.frame_count(); ++i) {
    const float l = buffer.at(i, 0);
    const float r = buffer.at(i, 1);
    const float mid = 0.5f * (l + r);
    float side = 0.5f * (l - r);
    if (apply_side_hp) {
      side = side_hp_.process(side);
    }
    if (apply_width) {
      side *= w;
    }
    buffer.at(i, 0) = mid + side;
    buffer.at(i, 1) = mid - side;
  }
  return Status::Ok();
}

}  // namespace stemy::dsp
