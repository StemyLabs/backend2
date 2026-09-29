#include "stemy/config/config_loader.h"

#include <fstream>
#include <nlohmann/json.hpp>

namespace stemy::config {

Status load_hiphop_config(const std::filesystem::path& path,
                          genres::hiphop::HipHopParameters& out) {
  std::ifstream in(path);
  if (!in) {
    return Status::Error("Failed to open config: " + path.string());
  }

  nlohmann::json j;
  try {
    in >> j;
  } catch (const std::exception& e) {
    return Status::Error(std::string("JSON parse error: ") + e.what());
  }

  std::string version = "m1-master01-baseline";
  if (j.contains("version")) {
    version = j.at("version").get<std::string>();
  }
  // v2 candidate before R0's "hiphop-final" prefix, and before experimental R2*.
  if (version.rfind("hiphop-final-v2", 0) == 0) {
    out = genres::hiphop::HipHopParameters::make_final_v2_defaults();
  } else if (version.rfind("hiphop-final-r2c", 0) == 0) {
    out = genres::hiphop::HipHopParameters::make_final_r2c_defaults();
  } else if (version.rfind("hiphop-final-r2b", 0) == 0) {
    out = genres::hiphop::HipHopParameters::make_final_r2b_defaults();
  } else if (version.rfind("hiphop-final-r2", 0) == 0) {
    out = genres::hiphop::HipHopParameters::make_final_r2_defaults();
  } else if (version.rfind("hiphop-final-r1", 0) == 0 ||
             version.rfind("hiphop-final-approved", 0) == 0) {
    // Production alias hiphop-final-approved == R1 sonic state (client approval).
    out = genres::hiphop::HipHopParameters::make_final_r1_defaults();
  } else if (version.rfind("hiphop-final", 0) == 0) {
    out = genres::hiphop::HipHopParameters::make_final_r0_defaults();
  } else if (version.rfind("m2-refinement", 0) == 0 || version.rfind("m2b", 0) == 0) {
    out = genres::hiphop::HipHopParameters::make_m2_refinement_defaults();
  } else if (version.rfind("m2", 0) == 0) {
    out = genres::hiphop::HipHopParameters::make_m2_tuning_defaults();
  } else {
    out = genres::hiphop::HipHopParameters::make_provisional_defaults();
  }
  out.version = version;

  try {
    if (j.contains("genre_id")) out.genre_id = j.at("genre_id").get<std::string>();

    if (j.contains("stages")) {
      const auto& s = j.at("stages");
      if (s.contains("input_gain")) out.enable_input_gain = s.at("input_gain").get<bool>();
      if (s.contains("eq")) out.enable_eq = s.at("eq").get<bool>();
      if (s.contains("low_end")) out.enable_low_end = s.at("low_end").get<bool>();
      if (s.contains("transient")) out.enable_transient = s.at("transient").get<bool>();
      if (s.contains("dynamics")) out.enable_dynamics = s.at("dynamics").get<bool>();
      if (s.contains("saturation")) out.enable_saturation = s.at("saturation").get<bool>();
      if (s.contains("stereo")) out.enable_stereo = s.at("stereo").get<bool>();
      if (s.contains("limiter")) out.enable_limiter = s.at("limiter").get<bool>();
      if (s.contains("output_safety")) out.enable_output_safety = s.at("output_safety").get<bool>();
    }

    if (j.contains("provisional")) {
      const auto& p = j.at("provisional");
      auto getd = [&](const char* k, double& dst) {
        if (p.contains(k)) dst = p.at(k).get<double>();
      };
      auto getb = [&](const char* k, bool& dst) {
        if (p.contains(k)) dst = p.at(k).get<bool>();
      };
      auto geti = [&](const char* k, int& dst) {
        if (p.contains(k)) dst = p.at(k).get<int>();
      };

      getd("target_integrated_lufs", out.target_integrated_lufs);
      getd("loudness_tolerance_lufs", out.loudness_tolerance_lufs);
      getd("max_lift_db", out.max_lift_db);
      getd("max_reduction_db", out.max_reduction_db);
      getd("target_true_peak_dbtp", out.target_true_peak_dbtp);
      if (p.contains("limiter_ceiling_dbfs")) {
        out.target_true_peak_dbtp = p.at("limiter_ceiling_dbfs").get<double>();
      }
      getb("enable_tonal_trim_on_quiet_input", out.enable_tonal_trim_on_quiet_input);
      getd("quiet_input_lufs_threshold", out.quiet_input_lufs_threshold);
      getd("reference_crest_db", out.reference_crest_db);
      getb("prefer_tight_stereo", out.prefer_tight_stereo);

      getd("loudness_band_low_lufs", out.loudness_band_low_lufs);
      getd("loudness_band_high_lufs", out.loudness_band_high_lufs);
      getd("min_peak_headroom_db", out.min_peak_headroom_db);
      getd("dense_crest_threshold_db", out.dense_crest_threshold_db);
      getd("max_crest_reduction_db", out.max_crest_reduction_db);

      getb("m2_enable_punch", out.m2_enable_punch);
      getb("m2_enable_saturation", out.m2_enable_saturation);
      getb("m2_enable_hf_openness", out.m2_enable_hf_openness);
      getb("m2_enable_stereo_width", out.m2_enable_stereo_width);

      getd("m2_punch_attack_boost_db", out.m2_punch_attack_boost_db);
      getd("m2_punch_sustain_cut_db", out.m2_punch_sustain_cut_db);
      getd("m2_punch_mix", out.m2_punch_mix);
      getd("m2_punch_sensitivity", out.m2_punch_sensitivity);
      getd("m2_sat_drive", out.m2_sat_drive);
      getd("m2_sat_mix", out.m2_sat_mix);
      geti("m2_sat_oversample", out.m2_sat_oversample);
      getd("m2_hf_gain_db", out.m2_hf_gain_db);
      getd("m2_hf_freq_hz", out.m2_hf_freq_hz);
      getd("m2_stereo_width", out.m2_stereo_width);
      getd("m2_bass_mono_freq_hz", out.m2_bass_mono_freq_hz);
      getd("m2_wide_corr_threshold", out.m2_wide_corr_threshold);
      getd("m2_high_energy_share_threshold", out.m2_high_energy_share_threshold);

      getb("m2_enable_low_weight", out.m2_enable_low_weight);
      getd("m2_low_weight_gain_db", out.m2_low_weight_gain_db);
      getd("m2_low_weight_freq_hz", out.m2_low_weight_freq_hz);
      getd("m2_bass_body_share_threshold", out.m2_bass_body_share_threshold);
      getd("m2_punch_sense_hpf_hz", out.m2_punch_sense_hpf_hz);
      getd("m2_open_premix_extra_db", out.m2_open_premix_extra_db);
      getd("m2_max_extra_lift_db", out.m2_max_extra_lift_db);

      getb("limiter_true_peak_aware", out.limiter_true_peak_aware);
      geti("limiter_true_peak_oversample", out.limiter_true_peak_oversample);
      getb("r0_enable_extreme_sub_guard", out.r0_enable_extreme_sub_guard);
      getd("r0_extreme_bass_threshold", out.r0_extreme_bass_threshold);
      getd("r0_extreme_bass_full_scale", out.r0_extreme_bass_full_scale);
      getd("r0_extreme_sub_max_atten_db", out.r0_extreme_sub_max_atten_db);
      getd("r0_extreme_sub_freq_hz", out.r0_extreme_sub_freq_hz);
      getd("r0_extreme_sub_total_min_db", out.r0_extreme_sub_total_min_db);

      getb("r1_enable_clean_loudness_refinement", out.r1_enable_clean_loudness_refinement);
      getd("r1_clean_loudness_stop_lufs", out.r1_clean_loudness_stop_lufs);
      getd("r1_clean_loudness_step_db", out.r1_clean_loudness_step_db);
      getd("r1_clean_loudness_max_extra_db", out.r1_clean_loudness_max_extra_db);
      geti("r1_clean_loudness_max_steps", out.r1_clean_loudness_max_steps);
      getb("r1_enable_extreme_sub_compensation", out.r1_enable_extreme_sub_compensation);
      getd("r1_extreme_sub_comp_max_db", out.r1_extreme_sub_comp_max_db);
      getd("r1_stress_max_p95_gr_db", out.r1_stress_max_p95_gr_db);
      getd("r1_stress_max_avg_gr_db", out.r1_stress_max_avg_gr_db);
      getd("r1_stress_max_pct_gt_6db", out.r1_stress_max_pct_gt_6db);
      getd("r1_stress_max_gr_db", out.r1_stress_max_gr_db);
      getd("r1_stress_min_crest_db", out.r1_stress_min_crest_db);

      getb("r2b_enable_open_density", out.r2b_enable_open_density);
      getd("r2b_open_sat_drive", out.r2b_open_sat_drive);
      getd("r2b_open_sat_mix", out.r2b_open_sat_mix);
      getd("r2b_open_sat_mix_cap", out.r2b_open_sat_mix_cap);

      getb("r2c_enable_soft_peak_round", out.r2c_enable_soft_peak_round);
      getd("r2c_soft_peak_ceiling_dbfs", out.r2c_soft_peak_ceiling_dbfs);
      getd("r2c_soft_peak_drive", out.r2c_soft_peak_drive);
      geti("r2c_soft_peak_oversample", out.r2c_soft_peak_oversample);
    }
  } catch (const std::exception& e) {
    return Status::Error(std::string("Config field error: ") + e.what());
  }

  return Status::Ok();
}

}  // namespace stemy::config
