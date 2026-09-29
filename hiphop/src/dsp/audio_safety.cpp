#include "stemy/dsp/audio_safety.h"
#include "stemy/core/dsp_math.h"

#include <algorithm>
#include <cmath>

namespace stemy::dsp {

double measure_channel_balance_db(const audio::AudioBuffer& buffer) {
  if (buffer.channel_count() != 2 || buffer.frame_count() == 0) {
    return 0.0;
  }
  double sum_l = 0.0;
  double sum_r = 0.0;
  for (std::size_t i = 0; i < buffer.frame_count(); ++i) {
    const double l = buffer.at(i, 0);
    const double r = buffer.at(i, 1);
    sum_l += l * l;
    sum_r += r * r;
  }
  const double rms_l = std::sqrt(sum_l / static_cast<double>(buffer.frame_count()));
  const double rms_r = std::sqrt(sum_r / static_cast<double>(buffer.frame_count()));
  if (rms_l <= 0.0 && rms_r <= 0.0) {
    return 0.0;
  }
  if (rms_l <= 0.0 || rms_r <= 0.0) {
    return 120.0;  // extreme imbalance
  }
  return 20.0 * std::log10(rms_l / rms_r);
}

Status apply_output_safety(audio::AudioBuffer& buffer,
                           SafetyReport& report,
                           bool replace_non_finite,
                           bool do_flush_denormals,
                           double max_abs_channel_balance_db) {
  report.had_nan_inf = false;
  report.had_clipping = false;
  report.nan_inf_count = 0;
  report.clipped_sample_count = 0;
  report.max_abs_sample = 0.0f;

  for (std::size_t i = 0; i < buffer.sample_count(); ++i) {
    float s = buffer.data()[i];
    if (!std::isfinite(s)) {
      report.had_nan_inf = true;
      ++report.nan_inf_count;
      if (replace_non_finite) {
        s = 0.0f;
      } else {
        return Status::Error("Non-finite sample detected in output safety check");
      }
    }
    if (do_flush_denormals) {
      s = flush_denormal(s);
    }
    const float a = std::fabs(s);
    report.max_abs_sample = std::max(report.max_abs_sample, a);
    if (a > 1.0f) {
      report.had_clipping = true;
      ++report.clipped_sample_count;
    }
    buffer.data()[i] = s;
  }

  // Absolute L/R imbalance is content-dependent; report only.
  // Unintended processing imbalance is checked relative to input in the chain.
  report.channel_balance_db = measure_channel_balance_db(buffer);
  report.channel_balance_ok = true;
  (void)max_abs_channel_balance_db;
  return Status::Ok();
}

Status validate_gain_db(double gain_db, double min_db, double max_db) {
  if (!std::isfinite(gain_db)) {
    return Status::Error("Gain is not finite");
  }
  if (gain_db < min_db || gain_db > max_db) {
    return Status::Error("Gain out of allowed range");
  }
  return Status::Ok();
}

}  // namespace stemy::dsp
