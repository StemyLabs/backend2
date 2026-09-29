#pragma once

#include "stemy/audio/audio_buffer.h"
#include "stemy/audio/audio_format.h"

#include <cmath>
#include <cstdint>

namespace stemy::test {

inline constexpr double kPi = 3.14159265358979323846;

inline audio::AudioFormat stereo_format(std::uint32_t sr = 48000) {
  audio::AudioFormat f;
  f.sample_rate = sr;
  f.channel_count = 2;
  return f;
}

inline audio::AudioBuffer make_silence(std::size_t frames, std::uint16_t ch = 2) {
  return audio::AudioBuffer(frames, ch);
}

inline audio::AudioBuffer make_impulse(std::size_t frames, std::uint16_t ch = 2) {
  audio::AudioBuffer b(frames, ch);
  if (frames > 0) {
    for (std::uint16_t c = 0; c < ch; ++c) {
      b.at(0, c) = 1.0f;
    }
  }
  return b;
}

inline audio::AudioBuffer make_sine(std::size_t frames,
                                    std::uint32_t sample_rate,
                                    double freq_hz,
                                    double amplitude = 0.5,
                                    std::uint16_t ch = 2) {
  audio::AudioBuffer b(frames, ch);
  for (std::size_t i = 0; i < frames; ++i) {
    const float s = static_cast<float>(
        amplitude * std::sin(2.0 * kPi * freq_hz * static_cast<double>(i) /
                             static_cast<double>(sample_rate)));
    for (std::uint16_t c = 0; c < ch; ++c) {
      b.at(i, c) = s;
    }
  }
  return b;
}

inline audio::AudioBuffer make_stereo_sine(std::size_t frames,
                                           std::uint32_t sample_rate,
                                           double freq_l,
                                           double freq_r,
                                           double amplitude = 0.5) {
  audio::AudioBuffer b(frames, 2);
  for (std::size_t i = 0; i < frames; ++i) {
    b.at(i, 0) = static_cast<float>(
        amplitude * std::sin(2.0 * kPi * freq_l * static_cast<double>(i) /
                             static_cast<double>(sample_rate)));
    b.at(i, 1) = static_cast<float>(
        amplitude * std::sin(2.0 * kPi * freq_r * static_cast<double>(i) /
                             static_cast<double>(sample_rate)));
  }
  return b;
}

inline audio::AudioBuffer make_broadband(std::size_t frames, std::uint32_t seed = 1) {
  audio::AudioBuffer b(frames, 2);
  std::uint32_t s = seed;
  for (std::size_t i = 0; i < frames; ++i) {
    // Deterministic LCG noise in [-0.5, 0.5]
    s = s * 1664525u + 1013904223u;
    const float n = (static_cast<float>(s >> 8) / static_cast<float>(1u << 24)) - 0.5f;
    b.at(i, 0) = n;
    b.at(i, 1) = -n * 0.7f;
  }
  return b;
}

}  // namespace stemy::test
