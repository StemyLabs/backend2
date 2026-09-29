#pragma once

#include "stemy/adaptive/processing_decision.h"
#include "stemy/analysis/analysis_result.h"
#include "stemy/dsp/audio_safety.h"
#include "stemy/genres/hiphop/hiphop_parameters.h"

#include <string>

namespace stemy::mastering {

struct CleanLoudnessStressLimits {
  double max_p95_gr_db = 7.5;
  double max_avg_gr_db = 2.0;
  double max_pct_gr_gt_6db = 10.0;
  double max_gr_db = 12.0;
  double min_crest_db = 10.5;
};

/// Quiet/open premaster eligibility for R1 clean-loudness refinement.
/// Explicitly excludes HOT and ALREADY-DENSE inputs.
[[nodiscard]] bool clean_loudness_eligible(const analysis::AnalysisResult& input,
                                           const genres::hiphop::HipHopParameters& p);

/// True when measured limiter/dynamics/TP stress is within R1 guardrails.
[[nodiscard]] bool clean_loudness_stress_ok(const dsp::SafetyReport& safety,
                                            const analysis::AnalysisResult& output,
                                            const genres::hiphop::HipHopParameters& p,
                                            const CleanLoudnessStressLimits& limits,
                                            std::string* fail_reason);

/// Bounded extreme-sub loudness compensation (≤ 50% of |extra atten|).
[[nodiscard]] double extreme_sub_compensation_db(const adaptive::ProcessingDecision& d,
                                                 const genres::hiphop::HipHopParameters& p);

}  // namespace stemy::mastering
