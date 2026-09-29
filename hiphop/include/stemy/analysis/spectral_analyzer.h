#pragma once

#include "stemy/analysis/analyzer.h"

namespace stemy::analysis {

/// Broadband spectral energy by fixed frequency bands via internal FFT.
class SpectralAnalyzer final : public Analyzer {
public:
  [[nodiscard]] Status analyze(const audio::AudioBuffer& buffer,
                               const audio::AudioFormat& format,
                               AnalysisResult& result) const override;
};

}  // namespace stemy::analysis
