#pragma once

#include <cstdint>
#include <optional>

namespace stemy::audio {

/// Describes PCM stream properties. Internal processing uses float32.
struct AudioFormat {
  std::uint32_t sample_rate = 0;
  std::uint16_t channel_count = 0;
  /// Source container bit depth when known (16/24/32); nullopt for float WAV / unknown.
  std::optional<std::uint16_t> source_bit_depth;

  [[nodiscard]] bool is_valid() const {
    return sample_rate > 0 && channel_count > 0;
  }
};

/// M1 policy: stereo only. No silent upmix/downmix.
inline constexpr std::uint16_t kRequiredChannelCount = 2;
inline constexpr std::uint32_t kAllowedSampleRates[] = {
    44100, 48000, 88200, 96000};

}  // namespace stemy::audio
