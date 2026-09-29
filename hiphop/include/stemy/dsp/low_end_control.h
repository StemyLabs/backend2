#pragma once

#include "stemy/audio/audio_buffer.h"
#include "stemy/core/status.h"
#include "stemy/dsp/biquad.h"

#include <cstdint>

namespace stemy::dsp {

/// Dedicated low-end control (separate from general EQ).
/// Default: bypass / transparent. Conservative and fully configurable.
struct LowEndControlParams {
  bool enabled = false;
  /// High-pass to remove subsonic rumble. Disabled when enabled=false.
  bool highpass_enabled = false;
  double highpass_freq_hz = 20.0;
  double highpass_q = 0.707;
  /// Optional low-shelf trim. gain_db=0 is transparent.
  bool shelf_enabled = false;
  double shelf_freq_hz = 80.0;
  double shelf_q = 0.707;
  double shelf_gain_db = 0.0;
  /// Optional mono-below frequency for stereo bass management (0 = off).
  bool bass_mono_enabled = false;
  double bass_mono_freq_hz = 120.0;
};

class LowEndControl {
public:
  [[nodiscard]] Status prepare(std::uint32_t sample_rate, const LowEndControlParams& params);
  void reset();
  [[nodiscard]] Status process(audio::AudioBuffer& buffer) const;

private:
  LowEndControlParams params_{};
  std::uint32_t sample_rate_ = 0;
  mutable BiquadFilter hp_l_{};
  mutable BiquadFilter hp_r_{};
  mutable BiquadFilter shelf_l_{};
  mutable BiquadFilter shelf_r_{};
  mutable BiquadFilter mono_lp_l_{};
  mutable BiquadFilter mono_lp_r_{};
  mutable BiquadFilter mono_hp_l_{};
  mutable BiquadFilter mono_hp_r_{};
};

}  // namespace stemy::dsp
