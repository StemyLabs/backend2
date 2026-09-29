#include "stemy/analysis/dynamics_analyzer.h"
#include "stemy/core/dsp_math.h"

#include <cmath>

namespace stemy::analysis {

Status DynamicsAnalyzer::analyze(const audio::AudioBuffer& buffer,
                                 const audio::AudioFormat& format,
                                 AnalysisResult& result) const {
  (void)format;
  if (buffer.empty()) {
    return Status::Error("DynamicsAnalyzer: empty buffer");
  }

  // Provisional DR indicator: peak_dbfs - rms_dbfs (same as crest factor in dB).
  // Not a commercial "DR" meter standard — marked provisional for later replacement.
  if (result.sample_peak_dbfs && result.rms_dbfs) {
    result.dynamic_range_db = *result.sample_peak_dbfs - *result.rms_dbfs;
  } else {
    double peak = 0.0;
    double sum_sq = 0.0;
    const std::size_t n = buffer.sample_count();
    for (std::size_t i = 0; i < n; ++i) {
      const double s = buffer.data()[i];
      peak = std::max(peak, std::fabs(s));
      sum_sq += s * s;
    }
    const double rms = std::sqrt(sum_sq / static_cast<double>(n));
    result.dynamic_range_db = linear_to_db(peak) - linear_to_db(rms);
  }
  result.notes.push_back("dynamics:provisional_peak_minus_rms");
  return Status::Ok();
}

}  // namespace stemy::analysis
