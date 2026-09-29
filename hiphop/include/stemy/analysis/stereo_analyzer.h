#pragma once

#include "stemy/analysis/analyzer.h"

namespace stemy::analysis {

/// Stereo correlation, mid/side energy, and provisional width.
class StereoAnalyzer final : public Analyzer {
public:
  [[nodiscard]] Status analyze(const audio::AudioBuffer& buffer,
                               const audio::AudioFormat& format,
                               AnalysisResult& result) const override;
};

}  // namespace stemy::analysis
