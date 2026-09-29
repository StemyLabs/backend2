#include "stemy/audio/audio_buffer.h"
#include "stemy/core/dsp_math.h"

#include <algorithm>
#include <cmath>

namespace stemy::audio {

AudioBuffer::AudioBuffer(std::size_t frame_count, std::uint16_t channel_count) {
  resize(frame_count, channel_count);
}

void AudioBuffer::resize(std::size_t frame_count, std::uint16_t channel_count) {
  frame_count_ = frame_count;
  channel_count_ = channel_count;
  samples_.assign(static_cast<std::size_t>(frame_count) * channel_count, 0.0f);
}

void AudioBuffer::clear() {
  std::fill(samples_.begin(), samples_.end(), 0.0f);
}

float& AudioBuffer::at(std::size_t frame, std::uint16_t channel) {
  return samples_[frame * channel_count_ + channel];
}

float AudioBuffer::at(std::size_t frame, std::uint16_t channel) const {
  return samples_[frame * channel_count_ + channel];
}

Status AudioBuffer::validate_finite() const {
  for (float s : samples_) {
    if (!std::isfinite(s)) {
      return Status::Error("AudioBuffer contains NaN or Inf samples");
    }
  }
  return Status::Ok();
}

void AudioBuffer::flush_denormals() {
  for (float& s : samples_) {
    s = flush_denormal(s);
  }
}

bool AudioBuffer::has_clipped_samples(float threshold) const {
  for (float s : samples_) {
    if (std::fabs(s) > threshold) {
      return true;
    }
  }
  return false;
}

}  // namespace stemy::audio
