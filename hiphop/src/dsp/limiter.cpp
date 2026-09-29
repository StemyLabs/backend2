#include "stemy/dsp/limiter.h"
#include "stemy/core/dsp_math.h"

#include <algorithm>
#include <cmath>

namespace stemy::dsp {
namespace {

constexpr int kHistBins = 240;

float percentile_from_hist(const std::array<std::uint64_t, kHistBins>& hist,
                           std::uint64_t active_count,
                           double pct) {
  if (active_count == 0) {
    return 0.0f;
  }
  const double target = pct * static_cast<double>(active_count);
  std::uint64_t acc = 0;
  for (int i = 0; i < kHistBins; ++i) {
    acc += hist[static_cast<std::size_t>(i)];
    if (static_cast<double>(acc) >= target) {
      return static_cast<float>((i + 1) * 0.1);  // bin upper edge
    }
  }
  return 24.0f;
}

}  // namespace

Status Limiter::prepare(std::uint32_t sample_rate, const LimiterParams& params) {
  if (sample_rate == 0) {
    return Status::Error("Limiter: invalid sample rate");
  }
  if (!std::isfinite(params.ceiling_dbfs) || params.ceiling_dbfs > 0.0 ||
      params.ceiling_dbfs < -24.0) {
    return Status::Error("Limiter: ceiling out of range");
  }
  if (!(params.release_ms > 0.0) || !(params.lookahead_ms >= 0.0)) {
    return Status::Error("Limiter: invalid time constants");
  }
  if (params.true_peak_oversample != 2 && params.true_peak_oversample != 4 &&
      params.true_peak_oversample != 8) {
    return Status::Error("Limiter: true_peak_oversample must be 2, 4, or 8");
  }

  params_ = params;
  sample_rate_ = sample_rate;
  ceiling_linear_ = static_cast<float>(db_to_linear(params.ceiling_dbfs));
  // ISP control margin: gain modulation across FIR neighborhood (~0.35 dB).
  const double control_db =
      params.true_peak_aware ? (params.ceiling_dbfs - 0.35) : params.ceiling_dbfs;
  control_ceiling_linear_ = static_cast<float>(db_to_linear(control_db));
  release_coef_ = std::exp(-1.0 / (0.001 * params.release_ms * sample_rate));
  // Peak-hold releases slower than gain so ISP holes do not reopen.
  isp_hold_release_coef_ = std::exp(-1.0 / (0.001 * params.release_ms * 2.5 * sample_rate));
  lookahead_samples_ =
      static_cast<std::size_t>(std::round(0.001 * params.lookahead_ms * sample_rate));
  if (lookahead_samples_ < 1) {
    lookahead_samples_ = 1;
  }
  isp_detector_.set_oversample_factor(params_.true_peak_oversample);
  reset();
  return Status::Ok();
}

void Limiter::reset() {
  delay_l_.assign(lookahead_samples_, 0.0f);
  delay_r_.assign(lookahead_samples_, 0.0f);
  delay_pos_ = 0;
  gain_ = 1.0f;
  isp_peak_hold_ = 0.0f;
  telemetry_ = {};
  gr_hist_.fill(0);
  isp_detector_.reset();
}

Status Limiter::process(audio::AudioBuffer& buffer) const {
  if (!params_.enabled || buffer.channel_count() != 2) {
    telemetry_ = {};
    return Status::Ok();
  }
  if (delay_l_.size() != lookahead_samples_) {
    const_cast<Limiter*>(this)->reset();
  }
  telemetry_ = {};
  telemetry_.true_peak_aware = params_.true_peak_aware;
  gr_hist_.fill(0);
  isp_detector_.reset();
  double gr_sum_active = 0.0;
  std::uint64_t gt1 = 0;
  std::uint64_t gt3 = 0;
  std::uint64_t gt6 = 0;

  const std::uint64_t n = buffer.frame_count();
  telemetry_.frames_total = n;

  for (std::size_t i = 0; i < buffer.frame_count(); ++i) {
    const float in_l = buffer.at(i, 0);
    const float in_r = buffer.at(i, 1);

    const float delayed_l = delay_l_[delay_pos_];
    const float delayed_r = delay_r_[delay_pos_];
    delay_l_[delay_pos_] = in_l;
    delay_r_[delay_pos_] = in_r;
    delay_pos_ = (delay_pos_ + 1) % lookahead_samples_;

    float peak = std::max(std::fabs(in_l), std::fabs(in_r));
    if (params_.true_peak_aware) {
      peak = static_cast<float>(
          std::max(static_cast<double>(peak), isp_detector_.push_stereo(in_l, in_r)));
      // Instant-attack peak hold so release does not reopen ISP holes.
      if (peak > isp_peak_hold_) {
        isp_peak_hold_ = peak;
      } else {
        isp_peak_hold_ = static_cast<float>(isp_hold_release_coef_ * isp_peak_hold_ +
                                            (1.0 - isp_hold_release_coef_) * peak);
      }
      peak = isp_peak_hold_;
    }
    telemetry_.max_detected_peak_linear =
        std::max(telemetry_.max_detected_peak_linear, peak);

    float target = 1.0f;
    const float lim_ceiling =
        params_.true_peak_aware ? control_ceiling_linear_ : ceiling_linear_;
    if (peak > lim_ceiling && peak > 0.0f) {
      target = lim_ceiling / peak;
    }

    if (target < gain_) {
      gain_ = target;
    } else {
      gain_ = static_cast<float>(release_coef_ * gain_ + (1.0 - release_coef_) * target);
    }

    float gr = 0.0f;
    if (gain_ > 0.0f && gain_ < 1.0f) {
      gr = -20.0f * std::log10(gain_);
      telemetry_.max_gr_db = std::max(telemetry_.max_gr_db, gr);
      ++telemetry_.frames_active;
      gr_sum_active += static_cast<double>(gr);
      int bin = static_cast<int>(gr / 0.1f);
      if (bin < 0) bin = 0;
      if (bin >= kHistBins) bin = kHistBins - 1;
      ++gr_hist_[static_cast<std::size_t>(bin)];
      if (gr > 1.0f) ++gt1;
      if (gr > 3.0f) ++gt3;
      if (gr > 6.0f) ++gt6;
    }

    float out_l = delayed_l * gain_;
    float out_r = delayed_r * gain_;

    // Sample-domain brick-wall (native rate). ISP overs are handled by TP-aware
    // detection above; TruePeakSafety remains an emergency fallback.
    const float out_peak = std::max(std::fabs(out_l), std::fabs(out_r));
    if (out_peak > ceiling_linear_ && out_peak > 0.0f) {
      const float s = ceiling_linear_ / out_peak;
      out_l *= s;
      out_r *= s;
    }

    buffer.at(i, 0) = out_l;
    buffer.at(i, 1) = out_r;
  }

  if (telemetry_.frames_active > 0) {
    telemetry_.avg_gr_while_active_db =
        static_cast<float>(gr_sum_active / static_cast<double>(telemetry_.frames_active));
    telemetry_.p95_gr_while_active_db = percentile_from_hist(gr_hist_, telemetry_.frames_active, 0.95);
  }
  if (n > 0) {
    const double inv = 100.0 / static_cast<double>(n);
    telemetry_.pct_time_gr_gt_1db = static_cast<float>(gt1 * inv);
    telemetry_.pct_time_gr_gt_3db = static_cast<float>(gt3 * inv);
    telemetry_.pct_time_gr_gt_6db = static_cast<float>(gt6 * inv);
  }
  return Status::Ok();
}

}  // namespace stemy::dsp
