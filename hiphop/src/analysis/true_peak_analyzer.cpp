#include "stemy/analysis/true_peak_analyzer.h"
#include "stemy/analysis/true_peak_measure.h"
#include "stemy/core/dsp_math.h"

namespace stemy::analysis {

TruePeakAnalyzer::TruePeakAnalyzer(int oversample_factor)
    : oversample_factor_(oversample_factor < 2 ? 4 : oversample_factor) {}

Status TruePeakAnalyzer::analyze(const audio::AudioBuffer& buffer,
                                 const audio::AudioFormat& format,
                                 AnalysisResult& result) const {
  if (buffer.channel_count() != 2) {
    return Status::Error("TruePeakAnalyzer requires stereo buffer");
  }
  const double peak = measure_true_peak_linear(buffer, oversample_factor_);
  result.true_peak_linear = peak;
  result.true_peak_dbtp = linear_to_db(peak);
  result.notes.push_back("true_peak:oversample_x" + std::to_string(oversample_factor_) +
                         ":kaiser_sinc_lpf_bs1770_style");
  (void)format;
  return Status::Ok();
}

}  // namespace stemy::analysis
