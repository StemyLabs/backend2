// Audit-only stage probe. Does not modify production sonic defaults.
// Mirrors HipHopChain order and reports metrics after each major stage.

#include "stemy/adaptive/adaptive_engine.h"
#include "stemy/analysis/analysis_pipeline.h"
#include "stemy/audio/wav_io.h"
#include "stemy/config/config_loader.h"
#include "stemy/dsp/compressor.h"
#include "stemy/dsp/eq.h"
#include "stemy/dsp/gain_stage.h"
#include "stemy/dsp/limiter.h"
#include "stemy/dsp/low_end_control.h"
#include "stemy/dsp/saturation.h"
#include "stemy/dsp/stereo_processor.h"
#include "stemy/dsp/transient_controller.h"
#include "stemy/dsp/true_peak_safety.h"
#include "stemy/dsp/audio_safety.h"
#include "stemy/genres/hiphop/hiphop_parameters.h"

#include <nlohmann/json.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

nlohmann::json metrics_json(const stemy::analysis::AnalysisResult& a, float limiter_gr = -1.0f) {
  nlohmann::json j;
  if (a.integrated_loudness_lufs) j["integrated_lufs"] = *a.integrated_loudness_lufs;
  if (a.short_term_loudness_lufs) j["short_term_lufs"] = *a.short_term_loudness_lufs;
  if (a.sample_peak_dbfs) j["sample_peak_dbfs"] = *a.sample_peak_dbfs;
  if (a.true_peak_dbtp) j["true_peak_dbtp"] = *a.true_peak_dbtp;
  if (a.crest_factor_db) j["crest_factor_db"] = *a.crest_factor_db;
  if (a.rms_dbfs) j["rms_dbfs"] = *a.rms_dbfs;
  if (limiter_gr >= 0.0f) j["limiter_max_gr_db"] = limiter_gr;
  return j;
}

double opt(const std::optional<double>& v, double fallback = 0.0) {
  return v ? *v : fallback;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 4) {
    std::cerr << "Usage: stemy_stage_audit <input.wav> <config.json> <report.json>\n";
    return 2;
  }
  const std::filesystem::path input = argv[1];
  const std::filesystem::path config_path = argv[2];
  const std::filesystem::path report_path = argv[3];

  stemy::genres::hiphop::HipHopParameters genre;
  if (auto st = stemy::config::load_hiphop_config(config_path, genre); !st) {
    std::cerr << st.message() << "\n";
    return 1;
  }

  stemy::audio::WavReadResult wav;
  if (auto st = stemy::audio::read_wav(input, wav); !st) {
    std::cerr << st.message() << "\n";
    return 1;
  }
  auto& buf = wav.buffer;
  const auto& format = wav.format;

  stemy::analysis::AnalysisPipeline pipeline;
  stemy::analysis::AnalysisResult input_a;
  if (auto st = pipeline.run(buf, format, input_a); !st) {
    std::cerr << st.message() << "\n";
    return 1;
  }

  stemy::adaptive::ProcessingDecision decision;
  if (auto st = stemy::adaptive::AdaptiveEngine().decide(input_a, genre, decision); !st) {
    std::cerr << st.message() << "\n";
    return 1;
  }

  const double input_balance_db = stemy::dsp::measure_channel_balance_db(buf);
  const double sr = format.sample_rate;

  stemy::dsp::GainStage input_gain, output_gain;
  stemy::dsp::Eq eq, hf_eq;
  stemy::dsp::LowEndControl low_end;
  stemy::dsp::TransientController transient;
  stemy::dsp::Compressor compressor;
  stemy::dsp::Saturation saturation;
  stemy::dsp::StereoProcessor stereo;
  stemy::dsp::Limiter limiter;
  stemy::dsp::TruePeakSafety tp_safety;

  if (auto st = input_gain.prepare(decision.input_gain); !st) return 1;
  if (auto st = eq.prepare(sr, decision.eq); !st) return 1;
  if (auto st = low_end.prepare(sr, decision.low_end); !st) return 1;
  if (auto st = transient.prepare(sr, decision.transient); !st) return 1;
  if (auto st = compressor.prepare(sr, decision.compressor); !st) return 1;
  if (auto st = saturation.prepare(decision.saturation); !st) return 1;
  if (auto st = hf_eq.prepare(sr, decision.hf_eq); !st) return 1;
  if (auto st = stereo.prepare(sr, decision.stereo); !st) return 1;
  if (auto st = limiter.prepare(sr, decision.limiter); !st) return 1;
  if (auto st = output_gain.prepare(decision.output_gain); !st) return 1;
  stemy::dsp::TruePeakSafetyParams tpp;
  tpp.enabled = genre.enable_output_safety;
  tpp.ceiling_dbtp = genre.target_true_peak_dbtp;
  tpp.oversample_factor = 4;
  if (auto st = tp_safety.prepare(tpp); !st) return 1;

  nlohmann::json report;
  report["input_path"] = input.string();
  report["config_path"] = config_path.string();
  report["config_version"] = genre.version;
  report["adaptive_gain_db"] = decision.input_gain.gain_db;
  report["decision_notes"] = decision.decision_notes;
  report["decision"] = {
      {"transient_enabled", decision.transient.enabled},
      {"saturation_enabled", decision.saturation.enabled},
      {"hf_eq_enabled", decision.hf_eq.enabled},
      {"stereo_enabled", decision.stereo.enabled},
      {"stereo_width", decision.stereo.width},
      {"stereo_side_hpf_hz", decision.stereo.side_hpf_hz},
      {"limiter_enabled", decision.limiter.enabled},
      {"eq_enabled", decision.eq.enabled},
      {"low_end_enabled", decision.low_end.enabled},
  };

  auto snapshot = [&](const char* stage, double gain_change_db, float lim_gr = -1.0f) {
    stemy::analysis::AnalysisResult a;
    if (auto st = pipeline.run(buf, format, a); !st) {
      throw std::runtime_error(st.message());
    }
    nlohmann::json row = metrics_json(a, lim_gr);
    row["stage"] = stage;
    row["stage_gain_change_db"] = gain_change_db;
    row["delta_lufs_from_input"] =
        opt(a.integrated_loudness_lufs) - opt(input_a.integrated_loudness_lufs);
    report["stages"].push_back(row);
    std::cout << stage << " LUFS=" << opt(a.integrated_loudness_lufs)
              << " TP=" << opt(a.true_peak_dbtp) << " crest=" << opt(a.crest_factor_db);
    if (lim_gr >= 0.0f) std::cout << " limGR=" << lim_gr;
    std::cout << "\n";
  };

  try {
    snapshot("1_input", 0.0);

    double prev_lufs = opt(input_a.integrated_loudness_lufs);
    auto gain_delta = [&](double new_lufs) {
      const double d = new_lufs - prev_lufs;
      prev_lufs = new_lufs;
      return d;
    };

    // 2. Adaptive/input gain
    if (genre.enable_input_gain) {
      if (auto st = input_gain.process(buf); !st) throw std::runtime_error(st.message());
    }
    {
      stemy::analysis::AnalysisResult tmp;
      pipeline.run(buf, format, tmp);
      snapshot("2_input_gain", decision.input_gain.gain_db);
      prev_lufs = opt(tmp.integrated_loudness_lufs);
    }

    // 3. M1 tonal/low-end
    if (genre.enable_eq) {
      if (auto st = eq.process(buf); !st) throw std::runtime_error(st.message());
    }
    if (genre.enable_low_end) {
      if (auto st = low_end.process(buf); !st) throw std::runtime_error(st.message());
    }
    {
      stemy::analysis::AnalysisResult tmp;
      pipeline.run(buf, format, tmp);
      snapshot("3_tonal_low_end", gain_delta(opt(tmp.integrated_loudness_lufs)));
    }

    // 4. Punch
    if (genre.enable_transient) {
      if (auto st = transient.process(buf); !st) throw std::runtime_error(st.message());
    }
    {
      stemy::analysis::AnalysisResult tmp;
      pipeline.run(buf, format, tmp);
      snapshot("4_punch", gain_delta(opt(tmp.integrated_loudness_lufs)));
    }

    // compressor OFF path still executed if enable_dynamics
    if (genre.enable_dynamics) {
      if (auto st = compressor.process(buf); !st) throw std::runtime_error(st.message());
    }

    // 5. Saturation
    if (genre.enable_saturation) {
      if (auto st = saturation.process(buf); !st) throw std::runtime_error(st.message());
    }
    {
      stemy::analysis::AnalysisResult tmp;
      pipeline.run(buf, format, tmp);
      snapshot("5_saturation", gain_delta(opt(tmp.integrated_loudness_lufs)));
    }

    // 6. HF
    if (auto st = hf_eq.process(buf); !st) throw std::runtime_error(st.message());
    {
      stemy::analysis::AnalysisResult tmp;
      pipeline.run(buf, format, tmp);
      snapshot("6_hf", gain_delta(opt(tmp.integrated_loudness_lufs)));
    }

    // 7. Stereo (+ balance restore matching HipHopChain)
    if (genre.enable_stereo) {
      if (auto st = stereo.process(buf); !st) throw std::runtime_error(st.message());
    }
    {
      const double bal = stemy::dsp::measure_channel_balance_db(buf);
      const double delta = std::fabs(bal - input_balance_db);
      if (delta > 0.05 && delta <= 2.0) {
        const double corr = 0.5 * (input_balance_db - bal);
        const float gl = static_cast<float>(std::pow(10.0, corr / 20.0));
        const float gr = static_cast<float>(std::pow(10.0, -corr / 20.0));
        for (std::size_t i = 0; i < buf.frame_count(); ++i) {
          buf.at(i, 0) *= gl;
          buf.at(i, 1) *= gr;
        }
      }
    }
    {
      stemy::analysis::AnalysisResult tmp;
      pipeline.run(buf, format, tmp);
      snapshot("7_stereo", gain_delta(opt(tmp.integrated_loudness_lufs)));
    }

    // 8. Limiter
    if (genre.enable_limiter) {
      if (auto st = limiter.process(buf); !st) throw std::runtime_error(st.message());
    }
    if (auto st = output_gain.process(buf); !st) throw std::runtime_error(st.message());
    {
      stemy::analysis::AnalysisResult tmp;
      pipeline.run(buf, format, tmp);
      snapshot("8_limiter", gain_delta(opt(tmp.integrated_loudness_lufs)),
               limiter.max_gain_reduction_db());
    }

    // 9. True-peak safety
    stemy::dsp::SafetyReport safety;
    if (genre.enable_output_safety) {
      if (auto st = tp_safety.process(buf, safety); !st) throw std::runtime_error(st.message());
      if (auto st = stemy::dsp::apply_output_safety(buf, safety); !st) {
        throw std::runtime_error(st.message());
      }
    }
    {
      stemy::analysis::AnalysisResult tmp;
      pipeline.run(buf, format, tmp);
      snapshot("9_true_peak_safety", gain_delta(opt(tmp.integrated_loudness_lufs)));
      report["stages"].back()["true_peak_safety_applied"] = safety.true_peak_safety_applied;
      report["stages"].back()["true_peak_safety_scale"] = safety.true_peak_safety_scale;
    }

    // 10. Final (same as 9 after safety)
    report["stages"].push_back(report["stages"].back());
    report["stages"].back()["stage"] = "10_final_output";
  } catch (const std::exception& e) {
    std::cerr << "Audit failed: " << e.what() << "\n";
    return 1;
  }

  std::ofstream out(report_path);
  out << report.dump(2) << "\n";
  std::cout << "Wrote " << report_path << "\n";
  return 0;
}
