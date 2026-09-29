#pragma once

#include "stemy/dsp/compressor.h"
#include "stemy/dsp/eq.h"
#include "stemy/dsp/gain_stage.h"
#include "stemy/dsp/limiter.h"
#include "stemy/dsp/low_end_control.h"
#include "stemy/dsp/saturation.h"
#include "stemy/dsp/soft_peak_rounder.h"
#include "stemy/dsp/stereo_processor.h"
#include "stemy/dsp/transient_controller.h"

#include <string>
#include <vector>

namespace stemy::adaptive {

struct ProcessingDecision {
  dsp::GainStageParams input_gain{};
  dsp::EqParams eq{};
  dsp::LowEndControlParams low_end{};
  dsp::TransientControllerParams transient{};
  dsp::CompressorParams compressor{};
  dsp::SaturationParams saturation{};
  /// M2 HF openness (runs after saturation per approved order).
  dsp::EqParams hf_eq{};
  /// M2 refinement: adaptive low-end weight (after M1 tonal, before punch).
  dsp::EqParams low_weight_eq{};
  dsp::StereoProcessorParams stereo{};
  /// Optional pre-limiter soft peak rounder (R2C quiet/open crest management).
  dsp::SoftPeakRoundParams soft_peak_round{};
  dsp::LimiterParams limiter{};
  dsp::GainStageParams output_gain{};

  /// Frozen M1-equivalent gain used as M2 baseline candidate (dB).
  double m1_baseline_gain_db = 0.0;
  /// Non-empty when M2 intentionally allows >0.5 LU quieter than M1 path.
  std::string safety_override_reason;

  /// Final R0 extreme-sub guard telemetry (audio path via low_end shelf).
  bool extreme_sub_guard_enabled = false;
  double bass_body_share = 0.0;
  double extreme_sub_guard_gain_db = 0.0;
  double extreme_sub_guard_freq_hz = 0.0;
  std::string extreme_sub_guard_reason;

  /// Final R1/R2 clean-loudness refinement telemetry.
  bool clean_loudness_refinement_enabled = false;
  double clean_loudness_extra_gain_db = 0.0;
  double clean_loudness_extreme_sub_comp_db = 0.0;
  int clean_loudness_candidates_tested = 0;
  std::string clean_loudness_stop_reason;
  bool clean_loudness_target_reached = false;
  bool clean_loudness_stress_limited = false;

  /// Per-candidate search history (deterministic offline evaluation).
  struct CleanLoudnessCandidate {
    double extra_gain_db = 0.0;
    double total_input_gain_db = 0.0;
    double integrated_lufs = 0.0;
    double true_peak_dbtp = 0.0;
    double crest_db = 0.0;
    double max_gr_db = 0.0;
    double avg_gr_db = 0.0;
    double p95_gr_db = 0.0;
    double pct_gt_3db = 0.0;
    double pct_gt_6db = 0.0;
    bool accepted = false;
    std::string reject_reason;  // empty when accepted
  };
  std::vector<CleanLoudnessCandidate> clean_loudness_candidate_log;

  std::vector<std::string> decision_notes;
};

}  // namespace stemy::adaptive
