#include "stemy/audio/wav_io.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

namespace stemy::audio {
namespace {

bool is_allowed_sample_rate(std::uint32_t sr) {
  for (std::uint32_t allowed : kAllowedSampleRates) {
    if (sr == allowed) {
      return true;
    }
  }
  return false;
}

}  // namespace

Status read_wav(const std::filesystem::path& path, WavReadResult& out) {
  drwav wav;
  if (!drwav_init_file(&wav, path.string().c_str(), nullptr)) {
    return Status::Error("Failed to open WAV: " + path.string());
  }

  if (wav.channels != kRequiredChannelCount) {
    drwav_uninit(&wav);
    return Status::Error(
        "M1 accepts stereo (2-channel) input only; got channel_count=" +
        std::to_string(wav.channels));
  }

  if (!is_allowed_sample_rate(wav.sampleRate)) {
    const auto sr = wav.sampleRate;
    drwav_uninit(&wav);
    return Status::Error("Unsupported sample rate: " + std::to_string(sr) +
                         " (allowed: 44100, 48000, 88200, 96000)");
  }

  const std::size_t frames = static_cast<std::size_t>(wav.totalPCMFrameCount);
  out.buffer.resize(frames, static_cast<std::uint16_t>(wav.channels));
  out.format.sample_rate = wav.sampleRate;
  out.format.channel_count = static_cast<std::uint16_t>(wav.channels);
  if (wav.bitsPerSample > 0 && wav.translatedFormatTag != DR_WAVE_FORMAT_IEEE_FLOAT) {
    out.format.source_bit_depth = static_cast<std::uint16_t>(wav.bitsPerSample);
  }

  const drwav_uint64 frames_read =
      drwav_read_pcm_frames_f32(&wav, frames, out.buffer.data());
  drwav_uninit(&wav);

  if (frames_read != frames) {
    return Status::Error("Failed to read all PCM frames from WAV");
  }

  return out.buffer.validate_finite();
}

Status write_wav(const std::filesystem::path& path,
                 const AudioBuffer& buffer,
                 const AudioFormat& format) {
  if (buffer.channel_count() != format.channel_count) {
    return Status::Error("Buffer/format channel count mismatch");
  }
  if (format.channel_count != kRequiredChannelCount) {
    return Status::Error("M1 writes stereo only");
  }

  drwav_data_format fmt{};
  fmt.container = drwav_container_riff;
  fmt.format = DR_WAVE_FORMAT_IEEE_FLOAT;
  fmt.channels = format.channel_count;
  fmt.sampleRate = format.sample_rate;
  fmt.bitsPerSample = 32;

  drwav wav;
  if (!drwav_init_file_write(&wav, path.string().c_str(), &fmt, nullptr)) {
    return Status::Error("Failed to open WAV for write: " + path.string());
  }

  const drwav_uint64 written =
      drwav_write_pcm_frames(&wav, buffer.frame_count(), buffer.data());
  drwav_uninit(&wav);

  if (written != buffer.frame_count()) {
    return Status::Error("Failed to write all PCM frames");
  }
  return Status::Ok();
}

Status write_wav_pcm24_tpdf(const std::filesystem::path& path,
                            const AudioBuffer& buffer,
                            const AudioFormat& format,
                            std::uint32_t dither_seed) {
  if (buffer.channel_count() != format.channel_count) {
    return Status::Error("Buffer/format channel count mismatch");
  }
  if (format.channel_count != kRequiredChannelCount) {
    return Status::Error("PCM24 export writes stereo only");
  }
  if (auto st = buffer.validate_finite(); !st) {
    return st;
  }

  // Pack float → 24-bit PCM with TPDF dither (two uniform [-0.5,0.5) → triangle).
  // Scale: ±1.0 → ±2^23. Dither amplitude = 1 LSB.
  constexpr double kScale = 8388608.0;  // 2^23
  constexpr double kMax = 8388607.0;
  constexpr double kMin = -8388608.0;

  std::uint32_t rng = dither_seed ? dither_seed : 0xC11E2477u;
  auto next_u01 = [&rng]() -> double {
    // LCG (Numerical Recipes); deterministic, audio-neutral noise source.
    rng = rng * 1664525u + 1013904223u;
    return static_cast<double>(rng) * (1.0 / 4294967296.0);
  };

  const std::size_t n = buffer.sample_count();
  std::vector<std::uint8_t> packed(n * 3);
  for (std::size_t i = 0; i < n; ++i) {
    const double x = static_cast<double>(buffer.data()[i]);
    const double tpdf = (next_u01() + next_u01()) - 1.0;  // [-1, 1)
    double y = x * kScale + tpdf;
    if (y > kMax) y = kMax;
    if (y < kMin) y = kMin;
    auto v = static_cast<std::int32_t>(std::lrint(y));
    // Little-endian signed 24-bit PCM (dr_wav expects 3 bytes/sample).
    packed[i * 3 + 0] = static_cast<std::uint8_t>(v & 0xFF);
    packed[i * 3 + 1] = static_cast<std::uint8_t>((v >> 8) & 0xFF);
    packed[i * 3 + 2] = static_cast<std::uint8_t>((v >> 16) & 0xFF);
  }

  drwav_data_format fmt{};
  fmt.container = drwav_container_riff;
  fmt.format = DR_WAVE_FORMAT_PCM;
  fmt.channels = format.channel_count;
  fmt.sampleRate = format.sample_rate;
  fmt.bitsPerSample = 24;

  drwav wav;
  if (!drwav_init_file_write(&wav, path.string().c_str(), &fmt, nullptr)) {
    return Status::Error("Failed to open PCM24 WAV for write: " + path.string());
  }

  const drwav_uint64 written =
      drwav_write_pcm_frames(&wav, buffer.frame_count(), packed.data());
  drwav_uninit(&wav);

  if (written != buffer.frame_count()) {
    return Status::Error("Failed to write all PCM24 frames");
  }
  return Status::Ok();
}

Status validate_input_audio(const AudioBuffer& buffer, const AudioFormat& format) {
  if (!format.is_valid()) {
    return Status::Error("Invalid AudioFormat");
  }
  if (format.channel_count != kRequiredChannelCount) {
    return Status::Error("M1 accepts stereo (2-channel) input only");
  }
  if (buffer.channel_count() != format.channel_count) {
    return Status::Error("Buffer channel count does not match format");
  }
  if (!is_allowed_sample_rate(format.sample_rate)) {
    return Status::Error("Unsupported sample rate");
  }
  if (buffer.frame_count() == 0) {
    return Status::Error("Empty audio buffer");
  }
  return buffer.validate_finite();
}

}  // namespace stemy::audio
