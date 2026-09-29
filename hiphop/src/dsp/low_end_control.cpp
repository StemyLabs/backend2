#include "stemy/dsp/low_end_control.h"

#include <cmath>

namespace stemy::dsp {

Status LowEndControl::prepare(std::uint32_t sample_rate, const LowEndControlParams& params) {
  if (sample_rate == 0) {
    return Status::Error("LowEndControl: invalid sample rate");
  }
  if (params.highpass_enabled) {
    if (!(params.highpass_freq_hz > 0.0) ||
        params.highpass_freq_hz >= 0.49 * sample_rate) {
      return Status::Error("LowEndControl: invalid highpass frequency");
    }
  }
  if (params.shelf_enabled) {
    if (!std::isfinite(params.shelf_gain_db) ||
        params.shelf_gain_db < -24.0 || params.shelf_gain_db > 24.0) {
      return Status::Error("LowEndControl: invalid shelf gain");
    }
  }
  params_ = params;
  sample_rate_ = sample_rate;
  reset();

  if (params_.enabled && params_.highpass_enabled) {
    const auto c = BiquadFilter::highpass(sample_rate_, params_.highpass_freq_hz, params_.highpass_q);
    hp_l_.set_coeffs(c);
    hp_r_.set_coeffs(c);
  }
  if (params_.enabled && params_.shelf_enabled) {
    const auto c = BiquadFilter::lowshelf(sample_rate_, params_.shelf_freq_hz, params_.shelf_q,
                                          params_.shelf_gain_db);
    shelf_l_.set_coeffs(c);
    shelf_r_.set_coeffs(c);
  }
  if (params_.enabled && params_.bass_mono_enabled) {
    const auto lp = BiquadFilter::lowpass(sample_rate_, params_.bass_mono_freq_hz, 0.707);
    const auto hp = BiquadFilter::highpass(sample_rate_, params_.bass_mono_freq_hz, 0.707);
    mono_lp_l_.set_coeffs(lp);
    mono_lp_r_.set_coeffs(lp);
    mono_hp_l_.set_coeffs(hp);
    mono_hp_r_.set_coeffs(hp);
  }
  return Status::Ok();
}

void LowEndControl::reset() {
  hp_l_.reset();
  hp_r_.reset();
  shelf_l_.reset();
  shelf_r_.reset();
  mono_lp_l_.reset();
  mono_lp_r_.reset();
  mono_hp_l_.reset();
  mono_hp_r_.reset();
}

Status LowEndControl::process(audio::AudioBuffer& buffer) const {
  if (!params_.enabled || buffer.channel_count() != 2) {
    return Status::Ok();
  }

  for (std::size_t i = 0; i < buffer.frame_count(); ++i) {
    float l = buffer.at(i, 0);
    float r = buffer.at(i, 1);

    if (params_.highpass_enabled) {
      l = hp_l_.process(l);
      r = hp_r_.process(r);
    }
    if (params_.shelf_enabled && params_.shelf_gain_db != 0.0) {
      l = shelf_l_.process(l);
      r = shelf_r_.process(r);
    }
    if (params_.bass_mono_enabled) {
      const float l_low = mono_lp_l_.process(l);
      const float r_low = mono_lp_r_.process(r);
      const float l_high = mono_hp_l_.process(l);
      const float r_high = mono_hp_r_.process(r);
      const float mono_low = 0.5f * (l_low + r_low);
      l = mono_low + l_high;
      r = mono_low + r_high;
    }

    buffer.at(i, 0) = l;
    buffer.at(i, 1) = r;
  }
  return Status::Ok();
}

}  // namespace stemy::dsp
