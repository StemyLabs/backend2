#include "stemy/genres/hiphop/hiphop_parameters.h"

namespace stemy::genres::hiphop {

HipHopParameters HipHopParameters::make_provisional_defaults() {
  HipHopParameters p;
  p.version = "m1-master01-baseline";
  p.enable_transient = false;
  p.m2_enable_punch = false;
  p.m2_enable_saturation = false;
  p.m2_enable_hf_openness = false;
  p.m2_enable_stereo_width = false;
  p.target_integrated_lufs = -9.0;
  p.loudness_tolerance_lufs = 0.75;
  p.prefer_tight_stereo = true;
  p.ranges = {
      {"input_gain_db", -12.0, 12.0, 0.0, true, "dB"},
      {"target_integrated_lufs", -14.0, -6.0, -9.0, true, "LUFS"},
      {"target_true_peak_dbtp", -2.0, 0.0, -0.5, true, "dBTP"},
  };
  return p;
}

HipHopParameters HipHopParameters::make_m2_tuning_defaults() {
  HipHopParameters p = make_provisional_defaults();
  p.version = "m2-tuning-checkpoint";
  p.enable_transient = true;
  p.prefer_tight_stereo = false;  // allow controlled width when M2 enables it

  p.target_integrated_lufs = -8.5;
  p.loudness_band_low_lufs = -9.0;
  p.loudness_band_high_lufs = -8.0;
  p.loudness_tolerance_lufs = 0.0;  // unused for band logic; keep field

  p.m2_enable_punch = true;
  p.m2_enable_saturation = true;
  p.m2_enable_hf_openness = true;
  p.m2_enable_stereo_width = true;

  p.m2_punch_attack_boost_db = 1.0;
  p.m2_punch_sustain_cut_db = 0.4;
  p.m2_punch_mix = 0.35;
  p.m2_punch_sensitivity = 0.45;

  p.m2_sat_drive = 0.75;
  p.m2_sat_mix = 0.10;
  p.m2_sat_oversample = 4;

  p.m2_hf_gain_db = 0.6;
  p.m2_hf_freq_hz = 9000.0;

  p.m2_stereo_width = 1.06;
  p.m2_bass_mono_freq_hz = 120.0;

  p.ranges = {
      {"target_integrated_lufs", -9.0, -8.0, -8.5, true, "LUFS"},
      {"m2_punch_mix", 0.0, 0.5, 0.35, true, "linear"},
      {"m2_sat_mix", 0.0, 0.15, 0.10, true, "linear"},
      {"m2_hf_gain_db", 0.0, 1.0, 0.6, true, "dB"},
      {"m2_stereo_width", 1.0, 1.10, 1.06, true, "linear"},
  };
  return p;
}

HipHopParameters HipHopParameters::make_m2_refinement_defaults() {
  HipHopParameters p = make_m2_tuning_defaults();
  p.version = "m2-refinement-v2";

  p.target_true_peak_dbtp = -1.0;

  // Moderate punch (kick-focused), less sustain cut.
  p.m2_punch_attack_boost_db = 1.15;
  p.m2_punch_sustain_cut_db = 0.30;
  p.m2_punch_mix = 0.38;
  p.m2_punch_sensitivity = 0.42;
  p.m2_punch_sense_hpf_hz = 90.0;

  // Subtle analog warmth.
  p.m2_sat_drive = 0.80;
  p.m2_sat_mix = 0.12;

  // HF slightly conservative.
  p.m2_hf_gain_db = 0.5;

  // Adaptive low-end weight.
  p.m2_enable_low_weight = true;
  p.m2_low_weight_gain_db = 0.75;
  p.m2_low_weight_freq_hz = 95.0;
  p.m2_bass_body_share_threshold = 0.38;

  // Open premix loudness (NY Energy class).
  p.m2_open_premix_extra_db = 0.75;
  p.m2_max_extra_lift_db = 1.5;

  p.ranges = {
      {"target_integrated_lufs", -9.0, -8.0, -8.5, true, "LUFS"},
      {"target_true_peak_dbtp", -2.0, -0.5, -1.0, true, "dBTP"},
      {"m2_punch_mix", 0.0, 0.5, 0.38, true, "linear"},
      {"m2_sat_mix", 0.0, 0.15, 0.12, true, "linear"},
      {"m2_hf_gain_db", 0.0, 1.0, 0.5, true, "dB"},
      {"m2_stereo_width", 1.0, 1.10, 1.06, true, "linear"},
      {"m2_low_weight_gain_db", 0.0, 1.25, 0.75, true, "dB"},
  };
  return p;
}

HipHopParameters HipHopParameters::make_final_r0_defaults() {
  HipHopParameters p = make_m2_refinement_defaults();
  p.version = "hiphop-final-r0";
  // Engineering margin under client −1.0 dBTP target.
  p.target_true_peak_dbtp = -1.05;
  p.limiter_true_peak_aware = true;
  p.limiter_true_peak_oversample = 4;
  p.r0_enable_extreme_sub_guard = true;
  p.r0_extreme_bass_threshold = 0.70;
  p.r0_extreme_bass_full_scale = 0.85;
  p.r0_extreme_sub_max_atten_db = -1.0;
  p.r0_extreme_sub_freq_hz = 60.0;
  p.r0_extreme_sub_total_min_db = -3.0;
  return p;
}

HipHopParameters HipHopParameters::make_final_r1_defaults() {
  HipHopParameters p = make_final_r0_defaults();
  p.version = "hiphop-final-r1";
  p.r1_enable_clean_loudness_refinement = true;
  p.r1_clean_loudness_stop_lufs = -10.0;
  p.r1_clean_loudness_step_db = 0.25;
  p.r1_clean_loudness_max_extra_db = 1.5;
  p.r1_clean_loudness_max_steps = 6;
  p.r1_enable_extreme_sub_compensation = true;
  p.r1_extreme_sub_comp_max_db = 0.5;
  p.r1_stress_max_p95_gr_db = 7.5;
  p.r1_stress_max_avg_gr_db = 2.0;
  p.r1_stress_max_pct_gt_6db = 10.0;
  p.r1_stress_max_gr_db = 12.0;
  p.r1_stress_min_crest_db = 10.5;
  return p;
}

HipHopParameters HipHopParameters::make_final_r2_defaults() {
  // Character / TP / stress gates identical to R1. Only expand clean-loudness
  // search budget so quiet/open material can find the natural stress stop.
  HipHopParameters p = make_final_r1_defaults();
  p.version = "hiphop-final-r2";
  // Explore through the −10…−9 region if stress allows (not a hard mandate).
  p.r1_clean_loudness_stop_lufs = -9.0;
  p.r1_clean_loudness_step_db = 0.25;
  p.r1_clean_loudness_max_extra_db = 5.0;  // was 1.5 in R1/approved
  p.r1_clean_loudness_max_steps = 20;     // 20 × 0.25 = 5.0 dB
  return p;
}

HipHopParameters HipHopParameters::make_final_r2b_defaults() {
  // R2 loudness search + optional quiet/open harmonic density (sat).
  // Density amounts come from JSON per candidate; defaults keep R1 sat.
  HipHopParameters p = make_final_r2_defaults();
  p.version = "hiphop-final-r2b";
  p.r2b_enable_open_density = false;
  p.r2b_open_sat_drive = 0.8;
  p.r2b_open_sat_mix = 0.12;
  p.r2b_open_sat_mix_cap = 0.25;
  return p;
}

HipHopParameters HipHopParameters::make_final_r2c_defaults() {
  // R2B-D character (open density) + optional soft peak rounder for quiet/open.
  // Soft-peak amounts come from JSON; stress gates / TP unchanged.
  HipHopParameters p = make_final_r2b_defaults();
  p.version = "hiphop-final-r2c";
  p.r2b_enable_open_density = true;
  p.r2b_open_sat_drive = 1.5;
  p.r2b_open_sat_mix = 0.22;
  p.r2b_open_sat_mix_cap = 0.25;
  // Expand search so efficiency gains can explore toward −10…−9.5 if stress allows.
  p.r1_clean_loudness_max_extra_db = 7.0;
  p.r1_clean_loudness_max_steps = 28;
  p.r1_clean_loudness_stop_lufs = -9.0;
  p.r2c_enable_soft_peak_round = false;
  p.r2c_soft_peak_ceiling_dbfs = 4.0;
  p.r2c_soft_peak_drive = 1.5;
  p.r2c_soft_peak_oversample = 4;
  return p;
}

HipHopParameters HipHopParameters::make_final_v2_defaults() {
  // R2C-B operating point as a generalized candidate. Stress gates and TP match R1.
  // Search cap is 7.0 dB so the accepted +5.25 dB quiet/open step remains reachable.
  HipHopParameters p = make_final_r2c_defaults();
  p.version = "hiphop-final-v2";
  p.r1_clean_loudness_stop_lufs = -9.0;
  p.r1_clean_loudness_step_db = 0.25;
  p.r1_clean_loudness_max_extra_db = 7.0;
  p.r1_clean_loudness_max_steps = 28;
  p.r2b_enable_open_density = true;
  p.r2b_open_sat_drive = 1.5;
  p.r2b_open_sat_mix = 0.22;
  p.r2b_open_sat_mix_cap = 0.25;
  p.r2c_enable_soft_peak_round = true;
  p.r2c_soft_peak_ceiling_dbfs = 5.5;
  p.r2c_soft_peak_drive = 1.4;
  p.r2c_soft_peak_oversample = 4;
  return p;
}

}  // namespace stemy::genres::hiphop
