#pragma once

#include "stemy/audio/audio_buffer.h"
#include "stemy/core/status.h"
#include "stemy/dsp/biquad.h"

#include <cstdint>

namespace stemy::dsp {

/// Mid/side stereo width control.
/// width=1.0 → unchanged stereo image (transparent).
/// Optional side HPF keeps content below side_hpf_hz mono (no widening of lows).
struct StereoProcessorParams {
  bool enabled = false;
  double width = 1.0;  // 1.0 = original S gain
  /// If > 0, high-pass the side channel before width so bass stays mono.
  double side_hpf_hz = 0.0;
};

class StereoProcessor {
public:
  [[nodiscard]] Status prepare(std::uint32_t sample_rate, const StereoProcessorParams& params);
  /// Backward-compatible prepare when no side HPF is needed.
  [[nodiscard]] Status prepare(const StereoProcessorParams& params);
  void reset();
  [[nodiscard]] Status process(audio::AudioBuffer& buffer) const;

private:
  StereoProcessorParams params_{};
  std::uint32_t sample_rate_ = 0;
  mutable BiquadFilter side_hp_{};
};

}  // namespace stemy::dsp
