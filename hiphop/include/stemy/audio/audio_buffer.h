#pragma once

#include "stemy/audio/audio_format.h"
#include "stemy/core/status.h"

#include <cstddef>
#include <vector>

namespace stemy::audio {

/// Interleaved float32 audio buffer (LRLR...).
/// Channels are stored interleaved for simple WAV round-trips; DSP stages may
/// deinterleave locally when helpful.
class AudioBuffer {
public:
  AudioBuffer() = default;
  AudioBuffer(std::size_t frame_count, std::uint16_t channel_count);

  void resize(std::size_t frame_count, std::uint16_t channel_count);
  void clear();

  [[nodiscard]] std::size_t frame_count() const { return frame_count_; }
  [[nodiscard]] std::uint16_t channel_count() const { return channel_count_; }
  [[nodiscard]] std::size_t sample_count() const { return samples_.size(); }
  [[nodiscard]] bool empty() const { return samples_.empty(); }

  [[nodiscard]] float* data() { return samples_.data(); }
  [[nodiscard]] const float* data() const { return samples_.data(); }

  [[nodiscard]] float& at(std::size_t frame, std::uint16_t channel);
  [[nodiscard]] float at(std::size_t frame, std::uint16_t channel) const;

  /// Returns Error if any sample is NaN/Inf.
  [[nodiscard]] Status validate_finite() const;

  /// Flush denormals toward zero in-place (optional safety pass).
  void flush_denormals();

  /// Returns true if any |sample| > 1.0.
  [[nodiscard]] bool has_clipped_samples(float threshold = 1.0f) const;

private:
  std::size_t frame_count_ = 0;
  std::uint16_t channel_count_ = 0;
  std::vector<float> samples_;
};

}  // namespace stemy::audio
