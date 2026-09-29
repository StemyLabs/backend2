#include "stemy/analysis/loudness_analyzer.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace stemy::analysis {
namespace {

constexpr double kPi = 3.14159265358979323846;

struct Biquad {
  double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
  double z1 = 0, z2 = 0;

  float process(float x) {
    const double y = b0 * x + z1;
    z1 = b1 * x - a1 * y + z2;
    z2 = b2 * x - a2 * y;
    return static_cast<float>(y);
  }
};

/// BS.1770-4 K-weighting stage 1 (pre-filter / high shelf).
Biquad make_prefilter(double fs) {
  const double f0 = 1681.974450955533;
  const double G = 3.999843853973347;
  const double Q = 0.7071752369554196;
  const double K = std::tan(kPi * f0 / fs);
  const double Vh = std::pow(10.0, G / 20.0);
  const double Vb = std::pow(Vh, 0.4996666474386978);
  const double a0 = 1.0 + K / Q + K * K;

  Biquad b;
  b.b0 = (Vh + Vb * K / Q + K * K) / a0;
  b.b1 = 2.0 * (K * K - Vh) / a0;
  b.b2 = (Vh - Vb * K / Q + K * K) / a0;
  b.a1 = 2.0 * (K * K - 1.0) / a0;
  b.a2 = (1.0 - K / Q + K * K) / a0;
  return b;
}

/// BS.1770-4 K-weighting stage 2 (RLB high-pass).
Biquad make_rlb(double fs) {
  const double f0 = 38.13547087602444;
  const double Q = 0.5003270373238773;
  const double K = std::tan(kPi * f0 / fs);
  const double a0 = 1.0 + K / Q + K * K;

  Biquad b;
  b.b0 = 1.0 / a0;
  b.b1 = -2.0 / a0;
  b.b2 = 1.0 / a0;
  b.a1 = 2.0 * (K * K - 1.0) / a0;
  b.a2 = (1.0 - K / Q + K * K) / a0;
  return b;
}

double mean_square_block(const std::vector<float>& left,
                         const std::vector<float>& right,
                         std::size_t start,
                         std::size_t length) {
  double sum = 0.0;
  for (std::size_t i = 0; i < length; ++i) {
    const double l = left[start + i];
    const double r = right[start + i];
    sum += l * l + r * r;  // stereo channel weights = 1.0
  }
  return sum / static_cast<double>(length);
}

double loudness_from_ms(double mean_square) {
  if (!(mean_square > 0.0)) {
    return -70.0;
  }
  return -0.691 + 10.0 * std::log10(mean_square);
}

}  // namespace

Status LoudnessAnalyzer::analyze(const audio::AudioBuffer& buffer,
                                 const audio::AudioFormat& format,
                                 AnalysisResult& result) const {
  if (buffer.channel_count() != 2) {
    return Status::Error("LoudnessAnalyzer requires stereo");
  }
  if (buffer.frame_count() == 0) {
    return Status::Error("LoudnessAnalyzer: empty buffer");
  }

  const double fs = static_cast<double>(format.sample_rate);
  const std::size_t frames = buffer.frame_count();

  Biquad pre_l = make_prefilter(fs);
  Biquad pre_r = make_prefilter(fs);
  Biquad rlb_l = make_rlb(fs);
  Biquad rlb_r = make_rlb(fs);

  std::vector<float> left(frames);
  std::vector<float> right(frames);
  for (std::size_t i = 0; i < frames; ++i) {
    left[i] = rlb_l.process(pre_l.process(buffer.at(i, 0)));
    right[i] = rlb_r.process(pre_r.process(buffer.at(i, 1)));
  }

  // 400 ms gating blocks with 75% overlap (BS.1770).
  const std::size_t block_len = static_cast<std::size_t>(std::round(0.4 * fs));
  const std::size_t hop = block_len / 4;
  if (block_len < 1 || hop < 1) {
    return Status::Error("LoudnessAnalyzer: sample rate too low for gating blocks");
  }

  std::vector<double> block_ms;
  for (std::size_t start = 0; start + block_len <= frames; start += hop) {
    block_ms.push_back(mean_square_block(left, right, start, block_len));
  }

  if (block_ms.empty()) {
    // Shorter than one block: use whole-file mean square.
    const double ms = mean_square_block(left, right, 0, frames);
    result.integrated_loudness_lufs = loudness_from_ms(ms);
    result.momentary_loudness_lufs = result.integrated_loudness_lufs;
    result.short_term_loudness_lufs = result.integrated_loudness_lufs;
    result.notes.push_back("loudness:short_file_ungated");
    return Status::Ok();
  }

  // Absolute gate: -70 LUFS.
  constexpr double kAbsGateLufs = -70.0;
  std::vector<double> above_abs;
  above_abs.reserve(block_ms.size());
  for (double ms : block_ms) {
    if (loudness_from_ms(ms) > kAbsGateLufs) {
      above_abs.push_back(ms);
    }
  }

  double integrated = -70.0;
  if (!above_abs.empty()) {
    double sum = 0.0;
    for (double ms : above_abs) {
      sum += ms;
    }
    const double relative_threshold =
        loudness_from_ms(sum / static_cast<double>(above_abs.size())) - 10.0;

    double gated_sum = 0.0;
    std::size_t gated_count = 0;
    for (double ms : above_abs) {
      if (loudness_from_ms(ms) > relative_threshold) {
        gated_sum += ms;
        ++gated_count;
      }
    }
    if (gated_count > 0) {
      integrated = loudness_from_ms(gated_sum / static_cast<double>(gated_count));
    }
  }

  // Momentary: max of 400 ms ungated block loudness (common practical metric).
  double mom = -70.0;
  for (double ms : block_ms) {
    mom = std::max(mom, loudness_from_ms(ms));
  }

  // Short-term: 3 s windows.
  const std::size_t st_len = static_cast<std::size_t>(std::round(3.0 * fs));
  double st = -70.0;
  if (frames >= st_len) {
    const std::size_t st_hop = std::max<std::size_t>(1, st_len / 10);
    for (std::size_t start = 0; start + st_len <= frames; start += st_hop) {
      st = std::max(st, loudness_from_ms(mean_square_block(left, right, start, st_len)));
    }
  } else {
    st = loudness_from_ms(mean_square_block(left, right, 0, frames));
  }

  result.integrated_loudness_lufs = integrated;
  result.momentary_loudness_lufs = mom;
  result.short_term_loudness_lufs = st;
  result.notes.push_back("loudness:bs1770_kweight_gated");
  return Status::Ok();
}

}  // namespace stemy::analysis
