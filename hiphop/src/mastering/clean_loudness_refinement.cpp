#include "stemy/mastering/clean_loudness_refinement.h"

#include <cmath>
#include <sstream>

namespace stemy::mastering {

bool clean_loudness_eligible(const analysis::AnalysisResult& input,
                             const genres::hiphop::HipHopParameters& p) {
  if (!p.r1_enable_clean_loudness_refinement) {
    return false;
  }
  if (!input.integrated_loudness_lufs) {
    return false;
  }
  const double lufs = *input.integrated_loudness_lufs;
  const double crest = input.crest_factor_db.value_or(0.0);

  // HOT: above operating band high — never refine louder.
  if (lufs > p.loudness_band_high_lufs) {
    return false;
  }
  // ALREADY-DENSE: low crest — never refine louder.
  if (crest < p.dense_crest_threshold_db) {
    return false;
  }
  // Quiet / open premaster: below quiet threshold or below −9 band.
  if (lufs >= p.quiet_input_lufs_threshold && lufs >= p.loudness_band_low_lufs) {
    return false;
  }
  // Prefer open dynamics (matches R0 open-premix spirit).
  if (crest < 12.0) {
    return false;
  }
  return true;
}

bool clean_loudness_stress_ok(const dsp::SafetyReport& safety,
                              const analysis::AnalysisResult& output,
                              const genres::hiphop::HipHopParameters& p,
                              const CleanLoudnessStressLimits& limits,
                              std::string* fail_reason) {
  auto fail = [&](const std::string& why) {
    if (fail_reason) *fail_reason = why;
    return false;
  };

  if (safety.had_nan_inf || safety.nan_inf_count > 0) {
    return fail("nan_inf");
  }
  if (safety.had_clipping || safety.clipped_sample_count > 0) {
    return fail("clipping");
  }
  if (safety.limiter_max_gr_db > static_cast<float>(limits.max_gr_db) + 1e-3f) {
    return fail("max_gr");
  }
  if (safety.limiter_p95_gr_while_active_db >
      static_cast<float>(limits.max_p95_gr_db) + 1e-3f) {
    return fail("p95_gr");
  }
  if (safety.limiter_avg_gr_while_active_db >
      static_cast<float>(limits.max_avg_gr_db) + 1e-3f) {
    return fail("avg_gr");
  }
  if (safety.limiter_pct_time_gr_gt_6db >
      static_cast<float>(limits.max_pct_gr_gt_6db) + 1e-3f) {
    return fail("pct_gt_6db");
  }
  if (output.crest_factor_db && *output.crest_factor_db + 1e-6 < limits.min_crest_db) {
    return fail("crest");
  }
  const double tp_ceil = std::min(p.target_true_peak_dbtp, -1.0);
  if (output.true_peak_dbtp && *output.true_peak_dbtp > tp_ceil + 0.02) {
    return fail("true_peak");
  }
  if (safety.true_peak_dbtp && *safety.true_peak_dbtp > tp_ceil + 0.02) {
    return fail("true_peak_safety");
  }
  // Prefer negligible emergency safety correction.
  if (safety.true_peak_safety_applied && safety.true_peak_safety_scale < 0.9886f) {
    // ~0.1 dB attenuation scale ≈ 0.9886
    return fail("tp_safety_correction");
  }
  if (fail_reason) fail_reason->clear();
  return true;
}

double extreme_sub_compensation_db(const adaptive::ProcessingDecision& d,
                                   const genres::hiphop::HipHopParameters& p) {
  if (!p.r1_enable_extreme_sub_compensation) {
    return 0.0;
  }
  if (!d.extreme_sub_guard_enabled) {
    return 0.0;
  }
  // Guard gain is negative attenuation; compensate ≤ 50% of |atten|.
  const double atten = std::min(0.0, d.extreme_sub_guard_gain_db);
  const double comp = -0.5 * atten;
  return std::min(comp, p.r1_extreme_sub_comp_max_db);
}

}  // namespace stemy::mastering
