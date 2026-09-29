#include "stemy/dsp/soft_peak_rounder.h"
#include "stemy/core/dsp_math.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace stemy::dsp {
namespace {

/// Soft-round sample peaks above `ceiling` with a tanh knee.
/// Transparent at/below ceiling; asymptotes with limited overshoot.
float soft_round_sample(float x, float ceiling_lin, float drive) {
  const float ax = std::fabs(x);
  if (ax <= ceiling_lin || ceiling_lin <= 1e-12f) {
    return x;
  }
  const float over = (ax - ceiling_lin) / ceiling_lin;
  const float shaped = std::tanh(over * drive);
  // Allow modest soft overshoot past ceiling (~15% of ceiling magnitude).
  const float max_overshoot = ceiling_lin * 0.15f;
  const float out_abs = ceiling_lin + max_overshoot * shaped;
  return std::copysign(std::min(out_abs, ax), x);
}

}  // namespace

Status SoftPeakRounder::prepare(const SoftPeakRoundParams& params) {
  // Pre-limiter material after adaptive gain may peak well above 0 dBFS.
  if (!std::isfinite(params.ceiling_dbfs) || params.ceiling_dbfs > 12.0 ||
      params.ceiling_dbfs < -24.0) {
    return Status::Error("SoftPeakRounder: invalid ceiling_dbfs");
  }
  if (!std::isfinite(params.drive) || params.drive < 0.1 || params.drive > 8.0) {
    return Status::Error("SoftPeakRounder: invalid drive");
  }
  if (params.oversample_factor != 1 && params.oversample_factor != 2 &&
      params.oversample_factor != 4 && params.oversample_factor != 8) {
    return Status::Error("SoftPeakRounder: oversample_factor must be 1,2,4, or 8");
  }
  params_ = params;
  peak_before_ = 0.0f;
  peak_after_ = 0.0f;
  return Status::Ok();
}

Status SoftPeakRounder::process(audio::AudioBuffer& buffer) const {
  peak_before_ = 0.0f;
  peak_after_ = 0.0f;
  for (std::size_t i = 0; i < buffer.sample_count(); ++i) {
    peak_before_ = std::max(peak_before_, std::fabs(buffer.data()[i]));
  }
  if (!params_.enabled) {
    peak_after_ = peak_before_;
    return Status::Ok();
  }

  const float ceiling = static_cast<float>(db_to_linear(params_.ceiling_dbfs));
  const float drive = static_cast<float>(params_.drive);
  const int os = params_.oversample_factor;

  if (os <= 1) {
    for (std::size_t i = 0; i < buffer.sample_count(); ++i) {
      buffer.data()[i] = soft_round_sample(buffer.data()[i], ceiling, drive);
    }
  } else {
    const std::size_t frames = buffer.frame_count();
    const std::uint16_t ch = buffer.channel_count();
    std::vector<float> up(frames * static_cast<std::size_t>(os));
    for (std::uint16_t c = 0; c < ch; ++c) {
      for (std::size_t i = 0; i < frames; ++i) {
        const float a = buffer.at(i, c);
        const float b = (i + 1 < frames) ? buffer.at(i + 1, c) : a;
        for (int f = 0; f < os; ++f) {
          const float t = static_cast<float>(f) / static_cast<float>(os);
          up[i * static_cast<std::size_t>(os) + static_cast<std::size_t>(f)] =
              a + (b - a) * t;
        }
      }
      for (float& s : up) {
        s = soft_round_sample(s, ceiling, drive);
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

  for (std::size_t i = 0; i < buffer.sample_count(); ++i) {
    peak_after_ = std::max(peak_after_, std::fabs(buffer.data()[i]));
  }
  return Status::Ok();
}

}  // namespace stemy::dsp
