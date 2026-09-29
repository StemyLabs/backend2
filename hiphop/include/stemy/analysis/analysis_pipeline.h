#pragma once

#include "stemy/analysis/analysis_result.h"
#include "stemy/audio/audio_buffer.h"
#include "stemy/audio/audio_format.h"
#include "stemy/core/status.h"

namespace stemy::analysis {

/// Runs the standard analyzer suite and returns a populated AnalysisResult.
class AnalysisPipeline {
public:
  [[nodiscard]] Status run(const audio::AudioBuffer& buffer,
                           const audio::AudioFormat& format,
                           AnalysisResult& out) const;
};

}  // namespace stemy::analysis
