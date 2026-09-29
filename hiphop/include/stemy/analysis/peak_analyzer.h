#pragma once

#include "stemy/analysis/analyzer.h"

namespace stemy::analysis {

/// Sample-peak, RMS, and crest-factor measurement (not true-peak).
class PeakAnalyzer final : public Analyzer {
public:
  [[nodiscard]] Status analyze(const audio::AudioBuffer& buffer,
                               const audio::AudioFormat& format,
                               AnalysisResult& result) const override;
};

}  // namespace stemy::analysis
