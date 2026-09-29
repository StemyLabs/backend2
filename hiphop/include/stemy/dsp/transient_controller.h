#pragma once

#include "stemy/audio/audio_buffer.h"
#include "stemy/core/status.h"

#include <cstdint>

namespace stemy::dsp {

/// Conservative stereo-linked transient / punch stage.
/// Default disabled / mix=0 → transparent.
struct TransientControllerParams {
  bool enabled = false;
  double attack_boost_db = 0.0;   // 0..1.5 for M2 checkpoint
  double sustain_cut_db = 0.0;    // 0..0.75 for M2 checkpoint
  double mix = 0.0;               // 0..0.5 for M2 checkpoint
  double sensitivity = 0.5;       // 0.2..1.0
  double attack_ms = 2.0;
  double release_ms = 80.0;
  double sense_hpf_hz = 120.0;
};

class TransientController {
public:
  [[nodiscard]] Status prepare(std::uint32_t sample_rate, const TransientControllerParams& params);
  void reset();
  [[nodiscard]] Status process(audio::AudioBuffer& buffer) const;

private:
  TransientControllerParams params_{};
  std::uint32_t sample_rate_ = 0;
  double attack_coef_ = 0.0;
  double release_coef_ = 0.0;
  // One-pole HPF for sense path (stereo-linked uses mono max abs after HPF).
  mutable double hpf_z_ = 0.0;
  mutable double env_fast_ = 0.0;
  mutable double env_slow_ = 0.0;
  double hpf_a_ = 0.0;
};

}  // namespace stemy::dsp
