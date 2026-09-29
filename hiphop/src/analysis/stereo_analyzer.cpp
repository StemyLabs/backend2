#include "stemy/analysis/stereo_analyzer.h"

#include <cmath>

namespace stemy::analysis {
namespace {

constexpr double kLfCorrCutoffHz = 120.0;

double pearson_corr(double sum_l, double sum_r, double sum_ll, double sum_rr, double sum_lr,
                    std::size_t n) {
  if (n == 0) return 1.0;
  const double mean_l = sum_l / static_cast<double>(n);
  const double mean_r = sum_r / static_cast<double>(n);
  const double cov = sum_lr / static_cast<double>(n) - mean_l * mean_r;
  const double var_l = sum_ll / static_cast<double>(n) - mean_l * mean_l;
  const double var_r = sum_rr / static_cast<double>(n) - mean_r * mean_r;
  const double denom = std::sqrt(std::max(0.0, var_l) * std::max(0.0, var_r));
  if (denom > 0.0) {
    return std::max(-1.0, std::min(1.0, cov / denom));
  }
  if (var_l == 0.0 && var_r == 0.0) {
    return 1.0;  // silence / identical DC
  }
  return 0.0;
}

}  // namespace

Status StereoAnalyzer::analyze(const audio::AudioBuffer& buffer,
                               const audio::AudioFormat& format,
                               AnalysisResult& result) const {
  if (buffer.channel_count() != 2 || buffer.frame_count() == 0) {
    return Status::Error("StereoAnalyzer requires non-empty stereo buffer");
  }

  double sum_l = 0.0;
  double sum_r = 0.0;
  double sum_ll = 0.0;
  double sum_rr = 0.0;
  double sum_lr = 0.0;
  double mid_e = 0.0;
  double side_e = 0.0;

  // One-pole LPF state for LF stereo correlation (<120 Hz). Audio-neutral.
  const double sr = static_cast<double>(format.sample_rate > 0 ? format.sample_rate : 48000);
  const double alpha =
      std::exp(-2.0 * 3.14159265358979323846 * kLfCorrCutoffHz / sr);
  double lpf_l = 0.0;
  double lpf_r = 0.0;
  double lf_sum_l = 0.0;
  double lf_sum_r = 0.0;
  double lf_sum_ll = 0.0;
  double lf_sum_rr = 0.0;
  double lf_sum_lr = 0.0;

  const std::size_t n = buffer.frame_count();
  for (std::size_t i = 0; i < n; ++i) {
    const double l = buffer.at(i, 0);
    const double r = buffer.at(i, 1);
    sum_l += l;
    sum_r += r;
    sum_ll += l * l;
    sum_rr += r * r;
    sum_lr += l * r;

    const double mid = 0.5 * (l + r);
    const double side = 0.5 * (l - r);
    mid_e += mid * mid;
    side_e += side * side;

    lpf_l = (1.0 - alpha) * l + alpha * lpf_l;
    lpf_r = (1.0 - alpha) * r + alpha * lpf_r;
    lf_sum_l += lpf_l;
    lf_sum_r += lpf_r;
    lf_sum_ll += lpf_l * lpf_l;
    lf_sum_rr += lpf_r * lpf_r;
    lf_sum_lr += lpf_l * lpf_r;
  }

  const double corr = pearson_corr(sum_l, sum_r, sum_ll, sum_rr, sum_lr, n);
  const double lf_corr =
      pearson_corr(lf_sum_l, lf_sum_r, lf_sum_ll, lf_sum_rr, lf_sum_lr, n);

  // Provisional width: side/(mid+side). 0 = mono energy, approaches 1 as side dominates.
  const double total = mid_e + side_e;
  const double width = (total > 0.0) ? (side_e / total) : 0.0;

  result.stereo_correlation = corr;
  result.lf_stereo_correlation = lf_corr;
  result.lf_stereo_correlation_cutoff_hz = kLfCorrCutoffHz;
  result.mid_energy = mid_e / static_cast<double>(n);
  result.side_energy = side_e / static_cast<double>(n);
  result.stereo_width = width;
  result.notes.push_back("stereo:correlation_ms_energy_provisional_width");
  result.notes.push_back("stereo:lf_correlation_one_pole_lpf_120hz");
  return Status::Ok();
}

}  // namespace stemy::analysis
