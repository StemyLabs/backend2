#pragma once

#include "stemy/adaptive/parameter_range.h"
#include "stemy/adaptive/processing_decision.h"

#include <string>
#include <vector>

namespace stemy::genres::hiphop {

struct HipHopParameters {
  std::string genre_id = "hiphop";
  /// "m1-master01-baseline" keeps approved M1 behavior; "m2-*" enables character stages.
  std::string version = "m1-master01-baseline";

  bool enable_input_gain = true;
  bool enable_eq = true;
  bool enable_low_end = true;
  bool enable_transient = false;  // M2
  bool enable_dynamics = true;
  bool enable_saturation = true;
  bool enable_stereo = true;
  bool enable_limiter = true;
  bool enable_output_safety = true;

  double target_integrated_lufs = -9.0;
  double loudness_tolerance_lufs = 0.75;
  double max_lift_db = 12.0;
  double max_reduction_db = 4.0;
  double target_true_peak_dbtp = -0.5;

  bool enable_tonal_trim_on_quiet_input = true;
  double quiet_input_lufs_threshold = -14.0;
  double reference_crest_db = 10.2;
  bool prefer_tight_stereo = true;

  // --- M2 loudness operating band (−9…−8), nominal −8.5 ---
  double loudness_band_low_lufs = -9.0;
  double loudness_band_high_lufs = -8.0;
  /// Soft cap on additional lift when true-peak headroom is small (dB).
  double min_peak_headroom_db = 0.6;
  /// If input crest is below this, treat as dense (reduce punch/sat).
  double dense_crest_threshold_db = 9.0;
  /// Max allowed crest reduction from input when lifting (relative safety).
  double max_crest_reduction_db = 4.0;

  // --- M2 character stage caps / defaults (adaptive scales within) ---
  bool m2_enable_punch = false;
  bool m2_enable_saturation = false;
  bool m2_enable_hf_openness = false;
  bool m2_enable_stereo_width = false;

  double m2_punch_attack_boost_db = 1.0;
  double m2_punch_sustain_cut_db = 0.4;
  double m2_punch_mix = 0.35;
  double m2_punch_sensitivity = 0.45;
  double m2_punch_sense_hpf_hz = 120.0;

  double m2_sat_drive = 0.75;
  double m2_sat_mix = 0.10;
  int m2_sat_oversample = 4;

  double m2_hf_gain_db = 0.6;
  double m2_hf_freq_hz = 9000.0;

  double m2_stereo_width = 1.06;
  double m2_bass_mono_freq_hz = 120.0;
  double m2_wide_corr_threshold = 0.92;  // corr below → already wide enough
  double m2_high_energy_share_threshold = 0.12;  // (high+high_mid)/total

  // --- M2 refinement: adaptive low-end weight (separate from M1 quiet tonal) ---
  bool m2_enable_low_weight = false;
  double m2_low_weight_gain_db = 0.75;
  double m2_low_weight_freq_hz = 95.0;
  /// Enable weight shelf when (bass+sub_bass)/total is below this.
  double m2_bass_body_share_threshold = 0.38;
  /// Extra lift (dB above M1) for open high-crest premixes below −11 LUFS.
  double m2_open_premix_extra_db = 0.75;
  double m2_max_extra_lift_db = 1.5;

  // --- Final R0: ISP-aware limiter + extreme-sub guard ---
  bool limiter_true_peak_aware = false;
  int limiter_true_peak_oversample = 4;

  bool r0_enable_extreme_sub_guard = false;
  /// Bass-body share above which subtractive sub control begins.
  double r0_extreme_bass_threshold = 0.70;
  /// Bass-body share at which extreme-sub attenuation reaches full scale.
  double r0_extreme_bass_full_scale = 0.85;
  /// Additional low-shelf attenuation at full scale (negative dB).
  double r0_extreme_sub_max_atten_db = -1.0;
  double r0_extreme_sub_freq_hz = 60.0;
  /// Cap on combined quiet-path + extreme-sub shelf gain (dB, negative).
  double r0_extreme_sub_total_min_db = -3.0;

  // --- Final R1: stress-gated clean-loudness refinement for quiet/open ---
  bool r1_enable_clean_loudness_refinement = false;
  /// Stop searching once output reaches this LUFS or louder (e.g. −10.0).
  double r1_clean_loudness_stop_lufs = -10.0;
  double r1_clean_loudness_step_db = 0.25;
  double r1_clean_loudness_max_extra_db = 1.5;
  int r1_clean_loudness_max_steps = 6;
  bool r1_enable_extreme_sub_compensation = false;
  double r1_extreme_sub_comp_max_db = 0.5;
  double r1_stress_max_p95_gr_db = 7.5;
  double r1_stress_max_avg_gr_db = 2.0;
  double r1_stress_max_pct_gt_6db = 10.0;
  double r1_stress_max_gr_db = 12.0;
  double r1_stress_min_crest_db = 10.5;

  // --- Experimental R2B: quiet/open adaptive harmonic density (pre-limiter) ---
  /// When true, quiet/open (crest≥12) material may use elevated sat drive/mix.
  bool r2b_enable_open_density = false;
  double r2b_open_sat_drive = 0.8;
  double r2b_open_sat_mix = 0.12;
  /// Mix clamp for open-density path (R1 hard-cap remains 0.15 when disabled).
  double r2b_open_sat_mix_cap = 0.25;

  // --- Experimental R2C: quiet/open pre-limiter soft peak rounder ---
  /// When true, quiet/open material may apply soft peak rounding before the limiter.
  bool r2c_enable_soft_peak_round = false;
  /// Soft ceiling in dBFS (sample). Pre-limiter peaks after gain are often >0 dBFS.
  double r2c_soft_peak_ceiling_dbfs = 4.0;
  /// Softness / drive into the ceiling curve.
  double r2c_soft_peak_drive = 1.5;
  int r2c_soft_peak_oversample = 4;

  std::vector<adaptive::ParameterRange> ranges;

  static HipHopParameters make_provisional_defaults();  // M1 frozen defaults
  static HipHopParameters make_m2_tuning_defaults();
  static HipHopParameters make_m2_refinement_defaults();
  static HipHopParameters make_final_r0_defaults();
  static HipHopParameters make_final_r1_defaults();
  /// Experimental R2: expanded clean-loudness search only (not production).
  static HipHopParameters make_final_r2_defaults();
  /// Experimental R2B: R2 loudness search + optional open-density sat (not production).
  static HipHopParameters make_final_r2b_defaults();
  /// Experimental R2C: R2B-D character + optional soft peak rounder (not production).
  static HipHopParameters make_final_r2c_defaults();
  /// Release candidate: R2 search budget, R2B-D open density, R2C-B soft peak.
  static HipHopParameters make_final_v2_defaults();

  [[nodiscard]] bool is_m2_profile() const {
    return version.rfind("m2", 0) == 0 || version.rfind("hiphop-final", 0) == 0;
  }
  /// M2B frozen baseline (`m2b-frozen`), refinement (`m2-refinement-*`), final R0/R1/R2/R2B/R2C.
  [[nodiscard]] bool is_m2_refinement() const {
    return version.rfind("m2-refinement", 0) == 0 || version.rfind("m2b", 0) == 0 ||
           version.rfind("hiphop-final", 0) == 0;
  }
  [[nodiscard]] bool is_final_r1() const {
    return version.rfind("hiphop-final-r1", 0) == 0 ||
           version.rfind("hiphop-final-approved", 0) == 0;
  }
  [[nodiscard]] bool is_final_r2() const {
    return version.rfind("hiphop-final-r2", 0) == 0 &&
           version.rfind("hiphop-final-r2b", 0) != 0 &&
           version.rfind("hiphop-final-r2c", 0) != 0;
  }
  [[nodiscard]] bool is_final_r2b() const {
    return version.rfind("hiphop-final-r2b", 0) == 0;
  }
  [[nodiscard]] bool is_final_r2c() const {
    return version.rfind("hiphop-final-r2c", 0) == 0;
  }
  [[nodiscard]] bool is_final_v2() const {
    return version.rfind("hiphop-final-v2", 0) == 0;
  }
};

}  // namespace stemy::genres::hiphop
