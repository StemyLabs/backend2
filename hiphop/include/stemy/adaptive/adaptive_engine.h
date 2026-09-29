#pragma once

#include "stemy/adaptive/parameter_range.h"
#include "stemy/adaptive/processing_decision.h"
#include "stemy/analysis/analysis_result.h"
#include "stemy/core/status.h"
#include "stemy/genres/hiphop/hiphop_parameters.h"

namespace stemy::adaptive {

/// Maps AnalysisResult + genre configuration → ProcessingDecision.
/// Does not process audio. Does not invent commercial targets.
class AdaptiveEngine {
public:
  [[nodiscard]] Status decide(const analysis::AnalysisResult& analysis,
                              const genres::hiphop::HipHopParameters& genre_params,
                              ProcessingDecision& out) const;
};

}  // namespace stemy::adaptive
