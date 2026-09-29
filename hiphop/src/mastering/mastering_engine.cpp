#include "stemy/mastering/mastering_engine.h"
#include "stemy/adaptive/adaptive_engine.h"
#include "stemy/analysis/analysis_pipeline.h"
#include "stemy/audio/wav_io.h"
#include "stemy/genres/hiphop/hiphop_chain.h"
#include "stemy/mastering/clean_loudness_refinement.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>
#include <vector>

namespace stemy::mastering {
namespace {

void copy_buffer(const audio::AudioBuffer& src, audio::AudioBuffer& dst) {
  dst.resize(src.frame_count(), src.channel_count());
  if (src.sample_count() > 0) {
    std::memcpy(dst.data(), src.data(), src.sample_count() * sizeof(float));
  }
}

Status render_candidate(const audio::AudioBuffer& dry,
                        const audio::AudioFormat& format,
                        const genres::hiphop::HipHopParameters& genre_params,
                        adaptive::ProcessingDecision decision,
                        analysis::AnalysisPipeline& pipeline,
                        audio::AudioBuffer& out_buf,
                        dsp::SafetyReport& safety,
                        analysis::AnalysisResult& out_analysis) {
  copy_buffer(dry, out_buf);
  genres::hiphop::HipHopChain chain;
  if (auto st = chain.prepare(format.sample_rate, genre_params, decision); !st) {
    return st;
  }
  if (auto st = chain.process(out_buf, &safety); !st) {
    return st;
  }
  return pipeline.run(out_buf, format, out_analysis);
}

void log_clean_candidate(adaptive::ProcessingDecision& d,
                         double extra_gain_db,
                         double total_gain_db,
                         const dsp::SafetyReport& safety,
                         const analysis::AnalysisResult& out,
                         bool accepted,
                         const std::string& reject_reason) {
  adaptive::ProcessingDecision::CleanLoudnessCandidate c;
  c.extra_gain_db = extra_gain_db;
  c.total_input_gain_db = total_gain_db;
  c.integrated_lufs = out.integrated_loudness_lufs.value_or(0.0);
  c.true_peak_dbtp = out.true_peak_dbtp.value_or(0.0);
  c.crest_db = out.crest_factor_db.value_or(0.0);
  c.max_gr_db = static_cast<double>(safety.limiter_max_gr_db);
  c.avg_gr_db = static_cast<double>(safety.limiter_avg_gr_while_active_db);
  c.p95_gr_db = static_cast<double>(safety.limiter_p95_gr_while_active_db);
  c.pct_gt_3db = static_cast<double>(safety.limiter_pct_time_gr_gt_3db);
  c.pct_gt_6db = static_cast<double>(safety.limiter_pct_time_gr_gt_6db);
  c.accepted = accepted;
  c.reject_reason = reject_reason;
  d.clean_loudness_candidate_log.push_back(c);
}

}  // namespace

Status MasteringEngine::process_buffer(audio::AudioBuffer& buffer,
                                       const audio::AudioFormat& format,
                                       const genres::hiphop::HipHopParameters& genre_params,
                                       MasteringStats& stats) const {
  if (auto st = audio::validate_input_audio(buffer, format); !st) {
    return st;
  }

  analysis::AnalysisPipeline pipeline;
  if (auto st = pipeline.run(buffer, format, stats.input_analysis); !st) {
    return st;
  }

  adaptive::AdaptiveEngine adaptive;
  if (auto st = adaptive.decide(stats.input_analysis, genre_params, stats.decision); !st) {
    return st;
  }

  // Keep dry copy for deterministic candidate renders (R1/R2 clean-loudness).
  audio::AudioBuffer dry;
  copy_buffer(buffer, dry);

  const double r0_gain = stats.decision.input_gain.gain_db;
  stats.decision.clean_loudness_refinement_enabled = false;
  stats.decision.clean_loudness_extra_gain_db = 0.0;
  stats.decision.clean_loudness_extreme_sub_comp_db = 0.0;
  stats.decision.clean_loudness_candidates_tested = 0;
  stats.decision.clean_loudness_stop_reason.clear();
  stats.decision.clean_loudness_target_reached = false;
  stats.decision.clean_loudness_stress_limited = false;
  stats.decision.clean_loudness_candidate_log.clear();

  CleanLoudnessStressLimits limits;
  limits.max_p95_gr_db = genre_params.r1_stress_max_p95_gr_db;
  limits.max_avg_gr_db = genre_params.r1_stress_max_avg_gr_db;
  limits.max_pct_gr_gt_6db = genre_params.r1_stress_max_pct_gt_6db;
  limits.max_gr_db = genre_params.r1_stress_max_gr_db;
  limits.min_crest_db = genre_params.r1_stress_min_crest_db;

  const bool eligible = clean_loudness_eligible(stats.input_analysis, genre_params);
  if (!genre_params.r1_enable_clean_loudness_refinement) {
    stats.decision.clean_loudness_stop_reason = "disabled";
  } else if (!eligible) {
    stats.decision.clean_loudness_stop_reason = "not_eligible_hot_dense_or_closed";
  } else {
    stats.decision.clean_loudness_refinement_enabled = true;

    double comp = extreme_sub_compensation_db(stats.decision, genre_params);
    comp = std::min(comp, genre_params.r1_clean_loudness_max_extra_db);
    stats.decision.clean_loudness_extreme_sub_comp_db = comp;

    double accepted_extra = comp;
    stats.decision.input_gain.gain_db = r0_gain + accepted_extra;

    audio::AudioBuffer trial;
    dsp::SafetyReport safety;
    analysis::AnalysisResult out_an;
    if (auto st = render_candidate(dry, format, genre_params, stats.decision, pipeline, trial,
                                   safety, out_an);
        !st) {
      return st;
    }
    stats.decision.clean_loudness_candidates_tested = 1;

    std::string stress_fail;
    bool ok = clean_loudness_stress_ok(safety, out_an, genre_params, limits, &stress_fail);
    if (!ok) {
      log_clean_candidate(stats.decision, accepted_extra, r0_gain + accepted_extra, safety, out_an,
                          false, stress_fail);
      // Compensation alone too stressful — fall back to pure R0 gain.
      accepted_extra = 0.0;
      stats.decision.clean_loudness_extreme_sub_comp_db = 0.0;
      stats.decision.clean_loudness_stress_limited = true;
      stats.decision.clean_loudness_stop_reason = "stress_limited_at_base:" + stress_fail;
    } else {
      log_clean_candidate(stats.decision, accepted_extra, r0_gain + accepted_extra, safety, out_an,
                          true, "");
      const double stop_lufs = genre_params.r1_clean_loudness_stop_lufs;
      auto loud_enough = [&](const analysis::AnalysisResult& a) {
        return a.integrated_loudness_lufs && *a.integrated_loudness_lufs >= stop_lufs - 1e-9;
      };

      if (loud_enough(out_an)) {
        stats.decision.clean_loudness_target_reached = true;
        stats.decision.clean_loudness_stop_reason = "already_at_or_above_stop_lufs";
      } else {
        const double step = genre_params.r1_clean_loudness_step_db;
        const double max_extra = genre_params.r1_clean_loudness_max_extra_db;
        const int max_steps = genre_params.r1_clean_loudness_max_steps;
        int steps = 0;
        while (steps < max_steps && accepted_extra + step <= max_extra + 1e-9) {
          const double trial_extra = accepted_extra + step;
          adaptive::ProcessingDecision trial_dec = stats.decision;
          trial_dec.input_gain.gain_db = r0_gain + trial_extra;
          dsp::SafetyReport trial_safety;
          analysis::AnalysisResult trial_out;
          if (auto st = render_candidate(dry, format, genre_params, trial_dec, pipeline, trial,
                                         trial_safety, trial_out);
              !st) {
            log_clean_candidate(stats.decision, trial_extra, r0_gain + trial_extra, trial_safety,
                                trial_out, false, "gain_prepare");
            stats.decision.clean_loudness_stress_limited = true;
            stats.decision.clean_loudness_stop_reason = "stress_limited:gain_prepare";
            break;
          }
          ++stats.decision.clean_loudness_candidates_tested;
          ++steps;

          if (!clean_loudness_stress_ok(trial_safety, trial_out, genre_params, limits,
                                        &stress_fail)) {
            log_clean_candidate(stats.decision, trial_extra, r0_gain + trial_extra, trial_safety,
                                trial_out, false, stress_fail);
            stats.decision.clean_loudness_stress_limited = true;
            stats.decision.clean_loudness_stop_reason = "stress_limited:" + stress_fail;
            break;
          }
          log_clean_candidate(stats.decision, trial_extra, r0_gain + trial_extra, trial_safety,
                              trial_out, true, "");
          accepted_extra = trial_extra;
          safety = trial_safety;
          out_an = trial_out;
          if (loud_enough(trial_out)) {
            stats.decision.clean_loudness_target_reached = true;
            stats.decision.clean_loudness_stop_reason = "target_reached";
            break;
          }
          if (accepted_extra + 1e-9 >= max_extra) {
            stats.decision.clean_loudness_stop_reason = "max_extra_reached";
            break;
          }
        }
        if (stats.decision.clean_loudness_stop_reason.empty()) {
          stats.decision.clean_loudness_stop_reason = "max_steps_reached";
        }
      }
    }

    stats.decision.clean_loudness_extra_gain_db = accepted_extra;
    stats.decision.input_gain.gain_db = r0_gain + accepted_extra;
    {
      std::ostringstream oss;
      oss << "r1 clean_loudness: extra=" << accepted_extra
          << " dB (comp=" << stats.decision.clean_loudness_extreme_sub_comp_db
          << ") candidates=" << stats.decision.clean_loudness_candidates_tested
          << " stop=" << stats.decision.clean_loudness_stop_reason;
      stats.decision.decision_notes.push_back(oss.str());
    }
  }

  // Final deterministic render at accepted gain.
  genres::hiphop::HipHopChain chain;
  if (auto st = chain.prepare(format.sample_rate, genre_params, stats.decision); !st) {
    return st;
  }
  copy_buffer(dry, buffer);
  if (auto st = chain.process(buffer, &stats.safety); !st) {
    return st;
  }

  if (auto st = pipeline.run(buffer, format, stats.output_analysis); !st) {
    return st;
  }

  // Hard Gate A requirement: measured output true peak <= configured ceiling.
  if (stats.output_analysis.true_peak_dbtp) {
    const double tp = *stats.output_analysis.true_peak_dbtp;
    const double ceiling = genre_params.target_true_peak_dbtp;
    constexpr double kEpsDb = 0.01;  // numerical tolerance
    if (tp > ceiling + kEpsDb) {
      std::ostringstream oss;
      oss << "Output true peak " << tp << " dBTP exceeds ceiling " << ceiling << " dBTP";
      return Status::Error(oss.str());
    }
  }
  return Status::Ok();
}

Status MasteringEngine::process_file(const MasteringRequest& request,
                                     MasteringStats& stats) const {
  audio::WavReadResult wav;
  if (auto st = audio::read_wav(request.input_path, wav); !st) {
    return st;
  }

  if (auto st = process_buffer(wav.buffer, wav.format, request.genre_params, stats); !st) {
    return st;
  }

  if (request.write_output) {
    if (auto st = audio::write_wav(request.output_path, wav.buffer, wav.format); !st) {
      return st;
    }
  }
  return Status::Ok();
}

}  // namespace stemy::mastering
