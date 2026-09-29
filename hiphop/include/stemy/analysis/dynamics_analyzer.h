#pragma once

#include "stemy/analysis/analyzer.h"

namespace stemy::analysis {

/// Provisional dynamic-range indicators derived from peak/RMS statistics.
class DynamicsAnalyzer final : public Analyzer {
public:
  [[nodiscard]] Status analyze(const audio::AudioBuffer& buffer,
                               const audio::AudioFormat& format,
                               AnalysisResult& result) const override;
};

}  // namespace stemy::analysis
