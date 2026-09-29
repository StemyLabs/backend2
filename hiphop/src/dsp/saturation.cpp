#include "stemy/dsp/saturation.h"
#include "stemy/core/dsp_math.h"

#include <cmath>
#include <vector>

namespace stemy::dsp {
namespace {

float apply_curve(float x, float drive, SaturationCurve curve) {
  switch (curve) {
    case SaturationCurve::TanhNormalized: {
      const float d = 1.0f + drive;
      const float n = std::tanh(d);
      return (n > 0.0f) ? (std::tanh(x * d) / n) : x;
    }
  }
  return x;
}

}  // namespace

Status Saturation::prepare(const SaturationParams& params) {
  if (!std::isfinite(params.drive) || params.drive < 0.0 || params.drive > 10.0) {
    return Status::Error("Saturation: invalid drive");
  }
  if (!std::isfinite(params.mix) || params.mix < 0.0 || params.mix > 1.0) {
    return Status::Error("Saturation: invalid mix");
  }
  if (!std::isfinite(params.output_gain_db) ||
      params.output_gain_db < -24.0 || params.output_gain_db > 24.0) {
    return Status::Error("Saturation: invalid output gain");
  }
  if (params.oversample_factor != 1 && params.oversample_factor != 2 &&
      params.oversample_factor != 4 && params.oversample_factor != 8) {
    return Status::Error("Saturation: oversample_factor must be 1,2,4, or 8");
  }
  params_ = params;
  return Status::Ok();
}

Status Saturation::process(audio::AudioBuffer& buffer) const {
  if (!params_.enabled || params_.mix <= 0.0 || params_.drive <= 0.0) {
    return Status::Ok();
  }

  const float mix = static_cast<float>(params_.mix);
  const float drive = static_cast<float>(params_.drive);
  const float out_g = static_cast<float>(db_to_linear(params_.output_gain_db));
  const int os = params_.oversample_factor;

  // Copy dry for mix / optional gain match.
  std::vector<float> dry(buffer.data(), buffer.data() + buffer.sample_count());
  double dry_sum_sq = 0.0;
  for (float s : dry) {
    dry_sum_sq += static_cast<double>(s) * s;
  }

  if (os <= 1) {
    for (std::size_t i = 0; i < buffer.sample_count(); ++i) {
      buffer.data()[i] = apply_curve(buffer.data()[i], drive, params_.curve);
    }
  } else {
    // Per-channel linear upsample → nonlinearity → box downsample (anti-alias control for M2).
    const std::size_t frames = buffer.frame_count();
    const std::uint16_t ch = buffer.channel_count();
    std::vector<float> up(frames * static_cast<std::size_t>(os));
    for (std::uint16_t c = 0; c < ch; ++c) {
      for (std::size_t i = 0; i < frames; ++i) {
        const float a = buffer.at(i, c);
        const float b = (i + 1 < frames) ? buffer.at(i + 1, c) : a;
        for (int f = 0; f < os; ++f) {
          const float t = static_cast<float>(f) / static_cast<float>(os);
          up[i * static_cast<std::size_t>(os) + static_cast<std::size_t>(f)] = a + (b - a) * t;
        }
      }
      for (float& s : up) {
        s = apply_curve(s, drive, params_.curve);
      }
      for (std::size_t i = 0; i < frames; ++i) {
        double acc = 0.0;
        for (int f = 0; f < os; ++f) {
          acc += up[i * static_cast<std::size_t>(os) + static_cast<std::size_t>(f)];
        }
        buffer.at(i, c) = static_cast<float>(acc / static_cast<double>(os));
      }
    }
  }

  double wet_sum_sq = 0.0;
  for (std::size_t i = 0; i < buffer.sample_count(); ++i) {
    wet_sum_sq += static_cast<double>(buffer.data()[i]) * buffer.data()[i];
  }
  float gain_match = 1.0f;
  if (params_.preserve_gain && wet_sum_sq > 1e-20 && dry_sum_sq > 1e-20) {
    gain_match = static_cast<float>(std::sqrt(dry_sum_sq / wet_sum_sq));
  }

  for (std::size_t i = 0; i < buffer.sample_count(); ++i) {
    const float d = dry[i];
    const float w = buffer.data()[i] * gain_match;
    buffer.data()[i] = (d * (1.0f - mix) + w * mix) * out_g;
  }
  return Status::Ok();
}

}  // namespace stemy::dsp
