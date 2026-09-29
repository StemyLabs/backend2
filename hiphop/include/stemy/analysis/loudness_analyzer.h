#pragma once

#include "stemy/analysis/analyzer.h"

namespace stemy::analysis {

/// ITU-R BS.1770-4 style integrated / short-term / momentary loudness
/// with K-weighting and absolute/relative gating.
class LoudnessAnalyzer final : public Analyzer {
public:
  [[nodiscard]] Status analyze(const audio::AudioBuffer& buffer,
                               const audio::AudioFormat& format,
                               AnalysisResult& result) const override;
};

}  // namespace stemy::analysis
