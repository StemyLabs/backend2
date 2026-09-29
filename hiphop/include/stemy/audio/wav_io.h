#pragma once

#include "stemy/audio/audio_buffer.h"
#include "stemy/audio/audio_format.h"
#include "stemy/core/status.h"

#include <cstdint>
#include <filesystem>
#include <string>

namespace stemy::audio {

struct WavReadResult {
  AudioBuffer buffer;
  AudioFormat format;
};

/// Read PCM/float WAV into float32 interleaved buffer.
[[nodiscard]] Status read_wav(const std::filesystem::path& path, WavReadResult& out);

/// Write float32 interleaved buffer as 32-bit float WAV (lossless for internal pipeline).
[[nodiscard]] Status write_wav(const std::filesystem::path& path,
                               const AudioBuffer& buffer,
                               const AudioFormat& format);

/// Client-review export: 24-bit PCM with triangular (TPDF) dither on float→int.
/// Does not normalize or apply gain. Preserves sample rate / channel count.
/// Deterministic when `dither_seed` is fixed.
[[nodiscard]] Status write_wav_pcm24_tpdf(const std::filesystem::path& path,
                                          const AudioBuffer& buffer,
                                          const AudioFormat& format,
                                          std::uint32_t dither_seed = 0xC11E2477u);

/// Validate M1 input policy: stereo + allowed sample rates + finite samples.
[[nodiscard]] Status validate_input_audio(const AudioBuffer& buffer,
                                          const AudioFormat& format);

}  // namespace stemy::audio
