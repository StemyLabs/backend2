#pragma once

#include "stemy/analysis/analyzer.h"

namespace stemy::analysis {

/// Oversampled true-peak measurement (ITU-R BS.1770 / BS.1771 style).
/// Kept separate from sample-peak measurement.
class TruePeakAnalyzer final : public Analyzer {
public:
  explicit TruePeakAnalyzer(int oversample_factor = 4);

  [[nodiscard]] Status analyze(const audio::AudioBuffer& buffer,
                               const audio::AudioFormat& format,
                               AnalysisResult& result) const override;

private:
  int oversample_factor_;
};

}  // namespace stemy::analysis
