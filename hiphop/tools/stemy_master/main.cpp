#include "stemy/config/config_loader.h"
#include "stemy/mastering/mastering_engine.h"

#include <cstdlib>
#include <cmath>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

namespace {

void print_usage(const char* argv0) {
  std::cerr << "Usage: " << argv0
            << " <input.wav> <output.wav> [--config path.json] [--report-json path.json] [--quiet]\n";
}

void print_analysis(const char* label, const stemy::analysis::AnalysisResult& a) {
  std::cout << label << ":\n";
  std::cout << "  duration_s=" << a.duration_seconds
            << " sr=" << a.sample_rate
            << " ch=" << a.channel_count << "\n";
  if (a.integrated_loudness_lufs) {
    std::cout << "  integrated_lufs=" << *a.integrated_loudness_lufs << "\n";
  }
  if (a.short_term_loudness_lufs) {
    std::cout << "  short_term_lufs=" << *a.short_term_loudness_lufs << "\n";
  }
  if (a.sample_peak_dbfs) {
    std::cout << "  sample_peak_dbfs=" << *a.sample_peak_dbfs << "\n";
  }
  if (a.true_peak_dbtp) {
    std::cout << "  true_peak_dbtp=" << *a.true_peak_dbtp << "\n";
  }
  if (a.rms_dbfs) {
    std::cout << "  rms_dbfs=" << *a.rms_dbfs << "\n";
  }
  if (a.crest_factor_db) {
    std::cout << "  crest_factor_db=" << *a.crest_factor_db << "\n";
  }
  if (a.stereo_correlation) {
    std::cout << "  stereo_correlation=" << *a.stereo_correlation << "\n";
  }
  if (a.lf_stereo_correlation) {
    std::cout << "  lf_stereo_correlation=" << *a.lf_stereo_correlation;
    if (a.lf_stereo_correlation_cutoff_hz) {
      std::cout << " (<" << *a.lf_stereo_correlation_cutoff_hz << " Hz)";
    }
    std::cout << "\n";
  }
  if (a.stereo_width) {
    std::cout << "  stereo_width=" << *a.stereo_width << "\n";
  }
  if (a.spectral_bands) {
    const auto& b = *a.spectral_bands;
    std::cout << "  spectral: sub=" << b.sub_bass << " bass=" << b.bass
              << " low_mid=" << b.low_mid << " mid=" << b.mid
              << " high_mid=" << b.high_mid << " high=" << b.high << "\n";
  }
}

nlohmann::json analysis_to_json(const stemy::analysis::AnalysisResult& a) {
  nlohmann::json j;
  j["duration_seconds"] = a.duration_seconds;
  j["sample_rate"] = a.sample_rate;
  j["channel_count"] = a.channel_count;
  if (a.integrated_loudness_lufs) j["integrated_lufs"] = *a.integrated_loudness_lufs;
  if (a.short_term_loudness_lufs) j["short_term_lufs"] = *a.short_term_loudness_lufs;
  if (a.sample_peak_dbfs) j["sample_peak_dbfs"] = *a.sample_peak_dbfs;
  if (a.true_peak_dbtp) j["true_peak_dbtp"] = *a.true_peak_dbtp;
  if (a.rms_dbfs) j["rms_dbfs"] = *a.rms_dbfs;
  if (a.crest_factor_db) j["crest_factor_db"] = *a.crest_factor_db;
  if (a.stereo_correlation) j["stereo_correlation"] = *a.stereo_correlation;
  if (a.lf_stereo_correlation) j["lf_stereo_correlation"] = *a.lf_stereo_correlation;
  if (a.lf_stereo_correlation_cutoff_hz) {
    j["lf_stereo_correlation_cutoff_hz"] = *a.lf_stereo_correlation_cutoff_hz;
  }
  if (a.stereo_width) j["stereo_width"] = *a.stereo_width;
  if (a.mid_energy) j["mid_energy"] = *a.mid_energy;
  if (a.side_energy) j["side_energy"] = *a.side_energy;
  if (a.spectral_bands) {
    j["spectral_bands"] = {
        {"sub_bass", a.spectral_bands->sub_bass},
        {"bass", a.spectral_bands->bass},
        {"low_mid", a.spectral_bands->low_mid},
        {"mid", a.spectral_bands->mid},
        {"high_mid", a.spectral_bands->high_mid},
        {"high", a.spectral_bands->high},
    };
  }
  return j;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) {
    print_usage(argv[0]);
    return 2;
  }

  const std::string input = argv[1];
  const std::string output = argv[2];
  std::string config_path;
  std::string report_json_path;
  bool quiet = false;

  for (int i = 3; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--config" && i + 1 < argc) {
      config_path = argv[++i];
    } else if (arg == "--report-json" && i + 1 < argc) {
      report_json_path = argv[++i];
    } else if (arg == "--quiet") {
      quiet = true;
    } else if (arg == "--help" || arg == "-h") {
      print_usage(argv[0]);
      return 0;
    } else {
      std::cerr << "Unknown argument: " << arg << "\n";
      print_usage(argv[0]);
      return 2;
    }
  }

  stemy::mastering::MasteringRequest request;
  request.input_path = input;
  request.output_path = output;
  request.genre_params = stemy::genres::hiphop::HipHopParameters::make_provisional_defaults();

  if (!config_path.empty()) {
    if (auto st = stemy::config::load_hiphop_config(config_path, request.genre_params); !st) {
      std::cerr << "Config error: " << st.message() << "\n";
      return 1;
    }
    if (!quiet) {
      std::cout << "Loaded config: " << config_path << "\n";
    }
  }

  if (!quiet) {
    std::cout << "STEMY master (M1 Hip-Hop, MASTER_01 provisional baseline)\n";
    std::cout << "Input:  " << input << "\n";
    std::cout << "Output: " << output << "\n";
  }

  stemy::mastering::MasteringEngine engine;
  stemy::mastering::MasteringStats stats;
  if (auto st = engine.process_file(request, stats); !st) {
    std::cerr << "Processing failed: " << st.message() << "\n";
    return 1;
  }

  if (!report_json_path.empty()) {
    nlohmann::json report;
    report["input_path"] = input;
    report["output_path"] = output;
    report["config_path"] = config_path;
    report["config_version"] = request.genre_params.version;
    report["input_analysis"] = analysis_to_json(stats.input_analysis);
    report["output_analysis"] = analysis_to_json(stats.output_analysis);
    report["processing"] = {
        {"input_gain_db", stats.decision.input_gain.gain_db},
        {"limiter_enabled", stats.decision.limiter.enabled},
        {"limiter_ceiling_dbfs", stats.decision.limiter.ceiling_dbfs},
        {"limiter_true_peak_aware", stats.decision.limiter.true_peak_aware},
        {"limiter_true_peak_oversample", stats.decision.limiter.true_peak_oversample},
        {"eq_enabled", stats.decision.eq.enabled},
        {"low_end_enabled", stats.decision.low_end.enabled},
        {"low_end_shelf_enabled", stats.decision.low_end.shelf_enabled},
        {"low_end_shelf_gain_db", stats.decision.low_end.shelf_gain_db},
        {"low_end_shelf_freq_hz", stats.decision.low_end.shelf_freq_hz},
        {"transient_enabled", stats.decision.transient.enabled},
        {"transient_attack_boost_db", stats.decision.transient.attack_boost_db},
        {"transient_sustain_cut_db", stats.decision.transient.sustain_cut_db},
        {"transient_mix", stats.decision.transient.mix},
        {"saturation_enabled", stats.decision.saturation.enabled},
        {"saturation_drive", stats.decision.saturation.drive},
        {"saturation_mix", stats.decision.saturation.mix},
        {"soft_peak_round_enabled", stats.decision.soft_peak_round.enabled},
        {"soft_peak_ceiling_dbfs", stats.decision.soft_peak_round.ceiling_dbfs},
        {"soft_peak_drive", stats.decision.soft_peak_round.drive},
        {"soft_peak_oversample", stats.decision.soft_peak_round.oversample_factor},
        {"hf_eq_enabled", stats.decision.hf_eq.enabled},
        {"hf_gain_db", stats.decision.hf_eq.bands[0].gain_db},
        {"hf_freq_hz", stats.decision.hf_eq.bands[0].freq_hz},
        {"low_weight_eq_enabled", stats.decision.low_weight_eq.enabled},
        {"low_weight_gain_db", stats.decision.low_weight_eq.bands[0].gain_db},
        {"low_weight_freq_hz", stats.decision.low_weight_eq.bands[0].freq_hz},
        {"compressor_enabled", stats.decision.compressor.enabled},
        {"stereo_enabled", stats.decision.stereo.enabled},
        {"stereo_width", stats.decision.stereo.width},
        {"m1_baseline_gain_db", stats.decision.m1_baseline_gain_db},
        {"safety_override_reason", stats.decision.safety_override_reason},
        {"extreme_sub_guard_enabled", stats.decision.extreme_sub_guard_enabled},
        {"bass_body_share", stats.decision.bass_body_share},
        {"extreme_sub_guard_gain_db", stats.decision.extreme_sub_guard_gain_db},
        {"extreme_sub_guard_freq_hz", stats.decision.extreme_sub_guard_freq_hz},
        {"extreme_sub_guard_reason", stats.decision.extreme_sub_guard_reason},
        {"clean_loudness_refinement_enabled", stats.decision.clean_loudness_refinement_enabled},
        {"clean_loudness_extra_gain_db", stats.decision.clean_loudness_extra_gain_db},
        {"clean_loudness_extreme_sub_comp_db", stats.decision.clean_loudness_extreme_sub_comp_db},
        {"clean_loudness_candidates_tested", stats.decision.clean_loudness_candidates_tested},
        {"clean_loudness_stop_reason", stats.decision.clean_loudness_stop_reason},
        {"clean_loudness_target_reached", stats.decision.clean_loudness_target_reached},
        {"clean_loudness_stress_limited", stats.decision.clean_loudness_stress_limited},
        {"decision_notes", stats.decision.decision_notes},
    };
    {
      nlohmann::json cand = nlohmann::json::array();
      for (const auto& c : stats.decision.clean_loudness_candidate_log) {
        cand.push_back({
            {"extra_gain_db", c.extra_gain_db},
            {"total_input_gain_db", c.total_input_gain_db},
            {"integrated_lufs", c.integrated_lufs},
            {"true_peak_dbtp", c.true_peak_dbtp},
            {"crest_db", c.crest_db},
            {"max_gr_db", c.max_gr_db},
            {"avg_gr_db", c.avg_gr_db},
            {"p95_gr_db", c.p95_gr_db},
            {"pct_gt_3db", c.pct_gt_3db},
            {"pct_gt_6db", c.pct_gt_6db},
            {"accepted", c.accepted},
            {"reject_reason", c.reject_reason},
        });
      }
      report["processing"]["clean_loudness_candidate_log"] = cand;
    }
    nlohmann::json safety = nlohmann::json::object();
    safety["nan_inf_count"] = stats.safety.nan_inf_count;
    safety["clipped_sample_count"] = stats.safety.clipped_sample_count;
    safety["max_abs_sample"] = stats.safety.max_abs_sample;
    safety["true_peak_safety_applied"] = stats.safety.true_peak_safety_applied;
    safety["true_peak_safety_scale"] = stats.safety.true_peak_safety_scale;
    if (stats.safety.true_peak_safety_scale > 0.0f) {
      safety["true_peak_safety_correction_db"] =
          20.0 * std::log10(static_cast<double>(stats.safety.true_peak_safety_scale));
    } else {
      safety["true_peak_safety_correction_db"] = 0.0;
    }
    safety["channel_balance_ok"] = stats.safety.channel_balance_ok;
    safety["limiter_max_gr_db"] = stats.safety.limiter_max_gr_db;
    safety["limiter_avg_gr_while_active_db"] = stats.safety.limiter_avg_gr_while_active_db;
    safety["limiter_p95_gr_while_active_db"] = stats.safety.limiter_p95_gr_while_active_db;
    safety["limiter_pct_time_gr_gt_1db"] = stats.safety.limiter_pct_time_gr_gt_1db;
    safety["limiter_pct_time_gr_gt_3db"] = stats.safety.limiter_pct_time_gr_gt_3db;
    safety["limiter_pct_time_gr_gt_6db"] = stats.safety.limiter_pct_time_gr_gt_6db;
    safety["limiter_pct_time_active"] = stats.safety.limiter_pct_time_active;
    safety["limiter_true_peak_aware"] = stats.safety.limiter_true_peak_aware;
    safety["limiter_max_detected_peak_linear"] = stats.safety.limiter_max_detected_peak_linear;
    safety["soft_peak_round_enabled"] = stats.safety.soft_peak_round_enabled;
    safety["soft_peak_before_linear"] = stats.safety.soft_peak_before_linear;
    safety["soft_peak_after_linear"] = stats.safety.soft_peak_after_linear;
    safety["soft_peak_reduction_db"] = stats.safety.soft_peak_reduction_db;
    if (stats.safety.true_peak_dbtp) {
      safety["true_peak_dbtp"] = *stats.safety.true_peak_dbtp;
    }
    if (stats.safety.channel_balance_db) {
      safety["channel_balance_db"] = *stats.safety.channel_balance_db;
    }
    report["safety"] = safety;
    std::ofstream out(report_json_path);
    if (!out) {
      std::cerr << "Failed to write report JSON: " << report_json_path << "\n";
      return 1;
    }
    out << report.dump(2) << "\n";
  }

  if (!quiet) {
    print_analysis("Input analysis", stats.input_analysis);
    print_analysis("Output analysis", stats.output_analysis);
    std::cout << "Safety: nan_inf=" << stats.safety.nan_inf_count
              << " clipped_samples=" << stats.safety.clipped_sample_count
              << " max_abs=" << stats.safety.max_abs_sample;
    if (stats.safety.true_peak_dbtp) {
      std::cout << " tp_dbtp=" << *stats.safety.true_peak_dbtp;
    }
    if (stats.safety.channel_balance_db) {
      std::cout << " balance_db=" << *stats.safety.channel_balance_db;
    }
    std::cout << "\n";
    std::cout << "Decision notes:\n";
    for (const auto& n : stats.decision.decision_notes) {
      std::cout << "  - " << n << "\n";
    }
    std::cout << "Done.\n";
  }

  return 0;
}
