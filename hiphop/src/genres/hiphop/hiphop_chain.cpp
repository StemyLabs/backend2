#include "stemy/genres/hiphop/hiphop_chain.h"

#include <cmath>

namespace stemy::genres::hiphop {

Status HipHopChain::prepare(std::uint32_t sample_rate,
                            const HipHopParameters& genre_params,
                            const adaptive::ProcessingDecision& decision) {
  if (sample_rate == 0) {
    return Status::Error("HipHopChain: invalid sample rate");
  }
  genre_ = genre_params;
  decision_ = decision;
  sample_rate_ = sample_rate;

  if (auto st = input_gain_.prepare(decision_.input_gain); !st) return st;
  if (auto st = eq_.prepare(sample_rate_, decision_.eq); !st) return st;
  if (auto st = low_end_.prepare(sample_rate_, decision_.low_end); !st) return st;
  if (auto st = low_weight_eq_.prepare(sample_rate_, decision_.low_weight_eq); !st) return st;
  if (auto st = transient_.prepare(sample_rate_, decision_.transient); !st) return st;
  if (auto st = compressor_.prepare(sample_rate_, decision_.compressor); !st) return st;
  if (auto st = saturation_.prepare(decision_.saturation); !st) return st;
  if (auto st = hf_eq_.prepare(sample_rate_, decision_.hf_eq); !st) return st;
  if (auto st = stereo_.prepare(sample_rate_, decision_.stereo); !st) return st;
  if (auto st = soft_peak_rounder_.prepare(decision_.soft_peak_round); !st) return st;
  if (auto st = limiter_.prepare(sample_rate_, decision_.limiter); !st) return st;
  if (auto st = output_gain_.prepare(decision_.output_gain); !st) return st;

  dsp::TruePeakSafetyParams tp;
  tp.enabled = genre_.enable_output_safety;
  tp.ceiling_dbtp = genre_.target_true_peak_dbtp;
  tp.oversample_factor = 4;
  if (auto st = true_peak_safety_.prepare(tp); !st) return st;
  return Status::Ok();
}

Status HipHopChain::process(audio::AudioBuffer& buffer,
                            dsp::SafetyReport* safety_report) const {
  if (buffer.channel_count() != 2) {
    return Status::Error("HipHopChain requires stereo buffer");
  }

  const double input_balance_db = dsp::measure_channel_balance_db(buffer);

  // 1. Input / adaptive gain
  if (genre_.enable_input_gain) {
    if (auto st = input_gain_.process(buffer); !st) return st;
  }
  // 2. Existing M1 tonal / low-end
  if (genre_.enable_eq) {
    if (auto st = eq_.process(buffer); !st) return st;
  }
  if (genre_.enable_low_end) {
    if (auto st = low_end_.process(buffer); !st) return st;
  }
  // M2 adaptive low-end weight (after M1 tonal, before punch).
  if (auto st = low_weight_eq_.process(buffer); !st) return st;
  // 3. Transient / punch
  if (genre_.enable_transient) {
    if (auto st = transient_.process(buffer); !st) return st;
  }
  // 4. Compressor (OFF in M1/M2 checkpoint decisions)
  if (genre_.enable_dynamics) {
    if (auto st = compressor_.process(buffer); !st) return st;
  }
  // 5. Saturation / warmth
  if (genre_.enable_saturation) {
    if (auto st = saturation_.process(buffer); !st) return st;
  }
  // HF openness (after sat)
  if (auto st = hf_eq_.process(buffer); !st) return st;
  // Stereo enhancement
  if (genre_.enable_stereo) {
    if (auto st = stereo_.process(buffer); !st) return st;
  }

  // Preserve input L/R balance after M/S width / bass-mono (measured RMS).
  {
    const double bal = dsp::measure_channel_balance_db(buffer);
    const double delta = std::fabs(bal - input_balance_db);
    if (delta > 0.05 && delta <= 2.0) {
      const double corr = 0.5 * (input_balance_db - bal);
      const float gl = static_cast<float>(std::pow(10.0, corr / 20.0));
      const float gr = static_cast<float>(std::pow(10.0, -corr / 20.0));
      for (std::size_t i = 0; i < buffer.frame_count(); ++i) {
        buffer.at(i, 0) *= gl;
        buffer.at(i, 1) *= gr;
      }
    }
  }

  // 6b. Optional soft peak rounder (pre-limiter crest management)
  if (auto st = soft_peak_rounder_.process(buffer); !st) return st;

  // 7. Limiter
  if (genre_.enable_limiter) {
    if (auto st = limiter_.process(buffer); !st) return st;
  }
  if (auto st = output_gain_.process(buffer); !st) return st;

  dsp::SafetyReport local;
  dsp::SafetyReport& report = safety_report ? *safety_report : local;
  report.soft_peak_round_enabled = decision_.soft_peak_round.enabled;
  report.soft_peak_before_linear = soft_peak_rounder_.peak_before_linear();
  report.soft_peak_after_linear = soft_peak_rounder_.peak_after_linear();
  if (report.soft_peak_before_linear > 1e-12f &&
      report.soft_peak_after_linear > 0.0f) {
    report.soft_peak_reduction_db =
        20.0f * std::log10(report.soft_peak_before_linear /
                           std::max(report.soft_peak_after_linear, 1e-12f));
  } else {
    report.soft_peak_reduction_db = 0.0f;
  }
  report.limiter_max_gr_db = limiter_.max_gain_reduction_db();
  {
    const auto& t = limiter_.telemetry();
    report.limiter_avg_gr_while_active_db = t.avg_gr_while_active_db;
    report.limiter_p95_gr_while_active_db = t.p95_gr_while_active_db;
    report.limiter_pct_time_gr_gt_1db = t.pct_time_gr_gt_1db;
    report.limiter_pct_time_gr_gt_3db = t.pct_time_gr_gt_3db;
    report.limiter_pct_time_gr_gt_6db = t.pct_time_gr_gt_6db;
    if (t.frames_total > 0) {
      report.limiter_pct_time_active =
          100.0f * static_cast<float>(t.frames_active) /
          static_cast<float>(t.frames_total);
    } else {
      report.limiter_pct_time_active = 0.0f;
    }
    report.limiter_true_peak_aware = t.true_peak_aware;
    report.limiter_max_detected_peak_linear = t.max_detected_peak_linear;
  }

  // 8. True-peak safety
  if (genre_.enable_output_safety) {
    if (auto st = true_peak_safety_.process(buffer, report); !st) return st;
  }

  if (genre_.enable_output_safety) {
    if (auto st = dsp::apply_output_safety(buffer, report); !st) return st;
    const double out_bal = report.channel_balance_db.value_or(0.0);
    const double delta = std::fabs(out_bal - input_balance_db);
    if (delta > 0.25) {
      report.channel_balance_ok = false;
      return Status::Error("Unexpected channel balance change introduced by processing");
    }
  }
  return Status::Ok();
}

}  // namespace stemy::genres::hiphop
