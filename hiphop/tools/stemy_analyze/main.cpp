// Reporting-only: analyze WAV via AnalysisPipeline. Does not run DSP / does not alter audio.
#include "stemy/analysis/analysis_pipeline.h"
#include "stemy/audio/wav_io.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <string>

namespace {

nlohmann::json analysis_to_json(const stemy::analysis::AnalysisResult& a,
                                const stemy::audio::AudioFormat& fmt) {
  nlohmann::json j;
  j["duration_seconds"] = a.duration_seconds;
  j["sample_rate"] = a.sample_rate;
  j["channel_count"] = a.channel_count;
  if (fmt.source_bit_depth) j["source_bit_depth"] = *fmt.source_bit_depth;
  if (a.integrated_loudness_lufs) j["integrated_lufs"] = *a.integrated_loudness_lufs;
  if (a.short_term_loudness_lufs) j["short_term_lufs"] = *a.short_term_loudness_lufs;
  if (a.momentary_loudness_lufs) j["momentary_lufs"] = *a.momentary_loudness_lufs;
  if (a.sample_peak_dbfs) j["sample_peak_dbfs"] = *a.sample_peak_dbfs;
  if (a.true_peak_dbtp) j["true_peak_dbtp"] = *a.true_peak_dbtp;
  if (a.rms_dbfs) j["rms_dbfs"] = *a.rms_dbfs;
  if (a.crest_factor_db) j["crest_factor_db"] = *a.crest_factor_db;
  if (a.dynamic_range_db) j["dynamic_range_db"] = *a.dynamic_range_db;
  if (a.stereo_correlation) j["stereo_correlation"] = *a.stereo_correlation;
  if (a.lf_stereo_correlation) j["lf_stereo_correlation"] = *a.lf_stereo_correlation;
  if (a.lf_stereo_correlation_cutoff_hz) {
    j["lf_stereo_correlation_cutoff_hz"] = *a.lf_stereo_correlation_cutoff_hz;
  }
  if (a.stereo_width) j["stereo_width"] = *a.stereo_width;
  if (a.mid_energy) j["mid_energy"] = *a.mid_energy;
  if (a.side_energy) j["side_energy"] = *a.side_energy;
  if (a.mid_energy && a.side_energy && (*a.mid_energy + *a.side_energy) > 0.0) {
    j["side_mid_ratio"] = *a.side_energy / (*a.mid_energy + *a.side_energy);
  }
  if (a.spectral_bands) {
    const auto& b = *a.spectral_bands;
    const double tot = b.sub_bass + b.bass + b.low_mid + b.mid + b.high_mid + b.high;
    j["spectral_bands"] = {
        {"sub_bass", b.sub_bass}, {"bass", b.bass}, {"low_mid", b.low_mid},
        {"mid", b.mid}, {"high_mid", b.high_mid}, {"high", b.high},
    };
    if (tot > 0.0) {
      j["spectral_band_shares"] = {
          {"sub_bass", b.sub_bass / tot}, {"bass", b.bass / tot},
          {"low_mid", b.low_mid / tot}, {"mid", b.mid / tot},
          {"high_mid", b.high_mid / tot}, {"high", b.high / tot},
          {"bass_body", (b.sub_bass + b.bass) / tot},
          {"high_energy", (b.high_mid + b.high) / tot},
      };
    }
  }
  j["lra_supported"] = false;
  j["lf_stereo_correlation_supported"] = a.lf_stereo_correlation.has_value();
  return j;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <input.wav> [--report-json path.json]\n";
    return 2;
  }
  const std::string input = argv[1];
  std::string report_path;
  for (int i = 2; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--report-json" && i + 1 < argc) report_path = argv[++i];
  }

  stemy::audio::WavReadResult wav;
  if (auto st = stemy::audio::read_wav(input, wav); !st) {
    std::cerr << "Read failed: " << st.message() << "\n";
    return 1;
  }
  if (auto st = stemy::audio::validate_input_audio(wav.buffer, wav.format); !st) {
    std::cerr << "Validate failed: " << st.message() << "\n";
    return 1;
  }

  stemy::analysis::AnalysisResult analysis;
  if (auto st = stemy::analysis::AnalysisPipeline().run(wav.buffer, wav.format, analysis); !st) {
    std::cerr << "Analyze failed: " << st.message() << "\n";
    return 1;
  }

  nlohmann::json report;
  report["input_path"] = input;
  report["analysis"] = analysis_to_json(analysis, wav.format);

  if (!report_path.empty()) {
    std::ofstream out(report_path);
    if (!out) {
      std::cerr << "Failed to write " << report_path << "\n";
      return 1;
    }
    out << report.dump(2) << "\n";
  } else {
    std::cout << report.dump(2) << "\n";
  }
  return 0;
}
