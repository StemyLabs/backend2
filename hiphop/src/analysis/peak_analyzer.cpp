#include "stemy/analysis/peak_analyzer.h"
#include "stemy/core/dsp_math.h"

#include <cmath>

namespace stemy::analysis {

Status PeakAnalyzer::analyze(const audio::AudioBuffer& buffer,
                             const audio::AudioFormat& format,
                             AnalysisResult& result) const {
  if (buffer.empty()) {
    return Status::Error("PeakAnalyzer: empty buffer");
  }

  double peak = 0.0;
  double sum_sq = 0.0;
  const std::size_t n = buffer.sample_count();
  const float* data = buffer.data();

  for (std::size_t i = 0; i < n; ++i) {
    const double a = std::fabs(static_cast<double>(data[i]));
    peak = std::max(peak, a);
    sum_sq += static_cast<double>(data[i]) * static_cast<double>(data[i]);
  }

  const double rms = std::sqrt(sum_sq / static_cast<double>(n));
  result.sample_rate = format.sample_rate;
  result.channel_count = format.channel_count;
  result.duration_seconds =
      static_cast<double>(buffer.frame_count()) / static_cast<double>(format.sample_rate);

  result.sample_peak_linear = peak;
  result.sample_peak_dbfs = linear_to_db(peak);
  result.rms_linear = rms;
  result.rms_dbfs = linear_to_db(rms);

  if (rms > 0.0) {
    result.crest_factor_linear = peak / rms;
    result.crest_factor_db = linear_to_db(peak / rms);
  } else {
    result.crest_factor_linear = 0.0;
    result.crest_factor_db = 0.0;
  }

  return Status::Ok();
}

}  // namespace stemy::analysis
