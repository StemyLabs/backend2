#include "stemy/analysis/spectral_analyzer.h"
#include "stemy/fft/fft_engine.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace stemy::analysis {
namespace {

void accumulate_band(SpectralBandEnergies& bands, double freq_hz, double power) {
  if (freq_hz < 60.0) {
    bands.sub_bass += power;
  } else if (freq_hz < 250.0) {
    bands.bass += power;
  } else if (freq_hz < 500.0) {
    bands.low_mid += power;
  } else if (freq_hz < 2000.0) {
    bands.mid += power;
  } else if (freq_hz < 6000.0) {
    bands.high_mid += power;
  } else {
    bands.high += power;
  }
}

}  // namespace

Status SpectralAnalyzer::analyze(const audio::AudioBuffer& buffer,
                                 const audio::AudioFormat& format,
                                 AnalysisResult& result) const {
  if (buffer.channel_count() != 2 || buffer.frame_count() == 0) {
    return Status::Error("SpectralAnalyzer requires non-empty stereo buffer");
  }

  constexpr std::size_t kFftSize = 2048;
  auto fft = fft::create_default_fft_engine(kFftSize);
  if (!fft) {
    return Status::Error("SpectralAnalyzer: failed to create FFT engine");
  }

  std::vector<float> mono(kFftSize, 0.0f);
  std::vector<float> spectrum(kFftSize, 0.0f);
  SpectralBandEnergies bands{};

  const std::size_t hop = kFftSize / 2;
  std::size_t windows = 0;

  for (std::size_t start = 0; start + kFftSize <= buffer.frame_count(); start += hop) {
    for (std::size_t i = 0; i < kFftSize; ++i) {
      const float w = 0.5f * (1.0f - std::cos(2.0f * 3.14159265358979323846f *
                                              static_cast<float>(i) /
                                              static_cast<float>(kFftSize - 1)));
      const float l = buffer.at(start + i, 0);
      const float r = buffer.at(start + i, 1);
      mono[i] = 0.5f * (l + r) * w;
    }

    auto st = fft->forward_real(mono.data(), spectrum.data(), kFftSize);
    if (!st) {
      return st;
    }

    const double bin_hz = static_cast<double>(format.sample_rate) / static_cast<double>(kFftSize);
    for (std::size_t k = 1; k < kFftSize / 2; ++k) {
      const float re = spectrum[2 * k];
      const float im = spectrum[2 * k + 1];
      const double power = static_cast<double>(re) * re + static_cast<double>(im) * im;
      accumulate_band(bands, bin_hz * static_cast<double>(k), power);
    }
    ++windows;
  }

  if (windows == 0) {
    const std::size_t n = std::min(buffer.frame_count(), kFftSize);
    std::fill(mono.begin(), mono.end(), 0.0f);
    for (std::size_t i = 0; i < n; ++i) {
      mono[i] = 0.5f * (buffer.at(i, 0) + buffer.at(i, 1));
    }
    auto st = fft->forward_real(mono.data(), spectrum.data(), kFftSize);
    if (!st) {
      return st;
    }
    const double bin_hz = static_cast<double>(format.sample_rate) / static_cast<double>(kFftSize);
    for (std::size_t k = 1; k < kFftSize / 2; ++k) {
      const float re = spectrum[2 * k];
      const float im = spectrum[2 * k + 1];
      const double power = static_cast<double>(re) * re + static_cast<double>(im) * im;
      accumulate_band(bands, bin_hz * static_cast<double>(k), power);
    }
    windows = 1;
  }

  const double inv = 1.0 / static_cast<double>(windows);
  bands.sub_bass *= inv;
  bands.bass *= inv;
  bands.low_mid *= inv;
  bands.mid *= inv;
  bands.high_mid *= inv;
  bands.high *= inv;

  result.spectral_bands = bands;
  result.notes.push_back("spectral:fft2048_band_power_mean");
  return Status::Ok();
}

}  // namespace stemy::analysis
