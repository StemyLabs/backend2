#pragma once

#include "stemy/audio/audio_buffer.h"
#include "stemy/core/status.h"
#include "stemy/dsp/biquad.h"

#include <array>
#include <cstdint>

namespace stemy::dsp {

enum class EqBandType { Peaking, LowShelf, HighShelf, HighPass, LowPass };

struct EqBandParams {
  bool enabled = false;
  EqBandType type = EqBandType::Peaking;
  double freq_hz = 1000.0;
  double q = 0.707;
  double gain_db = 0.0;  // 0 dB = transparent for peak/shelf
};

/// Multi-band parametric EQ. Default: all bands disabled → pass-through.
struct EqParams {
  bool enabled = true;
  static constexpr std::size_t kMaxBands = 8;
  std::array<EqBandParams, kMaxBands> bands{};
};

class Eq {
public:
  [[nodiscard]] Status prepare(std::uint32_t sample_rate, const EqParams& params);
  void reset();
  [[nodiscard]] Status process(audio::AudioBuffer& buffer) const;

private:
  EqParams params_{};
  std::uint32_t sample_rate_ = 0;
  // Per channel, per band
  mutable std::array<std::array<BiquadFilter, EqParams::kMaxBands>, 2> filters_{};
};

}  // namespace stemy::dsp
