#pragma once

#include "stemy/analysis/analysis_result.h"
#include "stemy/audio/audio_buffer.h"
#include "stemy/audio/audio_format.h"
#include "stemy/core/status.h"

namespace stemy::analysis {

/// Analyzer interface: measurement only (no processing decisions).
class Analyzer {
public:
  virtual ~Analyzer() = default;

  /// Merge metrics into `result` (does not clear unrelated fields).
  [[nodiscard]] virtual Status analyze(const audio::AudioBuffer& buffer,
                                       const audio::AudioFormat& format,
                                       AnalysisResult& result) const = 0;
};

}  // namespace stemy::analysis
