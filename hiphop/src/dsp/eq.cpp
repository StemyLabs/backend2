#include "stemy/dsp/eq.h"

#include <cmath>

namespace stemy::dsp {
namespace {

Status validate_band(const EqBandParams& b, std::uint32_t sample_rate) {
  if (!b.enabled) {
    return Status::Ok();
  }
  if (!(b.freq_hz > 0.0) || b.freq_hz >= 0.49 * static_cast<double>(sample_rate)) {
    return Status::Error("EQ band frequency out of range");
  }
  if (!(b.q > 0.0) || b.q > 50.0) {
    return Status::Error("EQ Q out of range");
  }
  if (!std::isfinite(b.gain_db) || b.gain_db < -24.0 || b.gain_db > 24.0) {
    return Status::Error("EQ gain out of range");
  }
  return Status::Ok();
}

BiquadCoeffs coeffs_for(const EqBandParams& b, std::uint32_t sr) {
  switch (b.type) {
    case EqBandType::Peaking:
      return BiquadFilter::peaking(sr, b.freq_hz, b.q, b.gain_db);
    case EqBandType::LowShelf:
      return BiquadFilter::lowshelf(sr, b.freq_hz, b.q, b.gain_db);
    case EqBandType::HighShelf:
      return BiquadFilter::highshelf(sr, b.freq_hz, b.q, b.gain_db);
    case EqBandType::HighPass:
      return BiquadFilter::highpass(sr, b.freq_hz, b.q);
    case EqBandType::LowPass:
      return BiquadFilter::lowpass(sr, b.freq_hz, b.q);
  }
  return {};
}

}  // namespace

Status Eq::prepare(std::uint32_t sample_rate, const EqParams& params) {
  if (sample_rate == 0) {
    return Status::Error("Eq: invalid sample rate");
  }
  for (const auto& band : params.bands) {
    if (auto st = validate_band(band, sample_rate); !st) {
      return st;
    }
  }
  params_ = params;
  sample_rate_ = sample_rate;
  reset();

  for (std::size_t ch = 0; ch < 2; ++ch) {
    for (std::size_t i = 0; i < EqParams::kMaxBands; ++i) {
      if (params_.bands[i].enabled) {
        filters_[ch][i].set_coeffs(coeffs_for(params_.bands[i], sample_rate_));
      }
    }
  }
  return Status::Ok();
}

void Eq::reset() {
  for (auto& ch : filters_) {
    for (auto& f : ch) {
      f.reset();
    }
  }
}

Status Eq::process(audio::AudioBuffer& buffer) const {
  if (!params_.enabled || buffer.channel_count() != 2) {
    return Status::Ok();
  }

  bool any = false;
  for (const auto& b : params_.bands) {
    if (b.enabled) {
      any = true;
      break;
    }
  }
  if (!any) {
    return Status::Ok();  // transparent
  }

  for (std::size_t i = 0; i < buffer.frame_count(); ++i) {
    float l = buffer.at(i, 0);
    float r = buffer.at(i, 1);
    for (std::size_t b = 0; b < EqParams::kMaxBands; ++b) {
      if (!params_.bands[b].enabled) {
        continue;
      }
      l = filters_[0][b].process(l);
      r = filters_[1][b].process(r);
    }
    buffer.at(i, 0) = l;
    buffer.at(i, 1) = r;
  }
  return Status::Ok();
}

}  // namespace stemy::dsp
