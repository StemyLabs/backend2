#include "stemy/dsp/true_peak_safety.h"
#include "stemy/analysis/true_peak_measure.h"
#include "stemy/core/dsp_math.h"

#include <cmath>

namespace stemy::dsp {

Status TruePeakSafety::prepare(const TruePeakSafetyParams& params) {
  if (!std::isfinite(params.ceiling_dbtp) || params.ceiling_dbtp > 0.0 ||
      params.ceiling_dbtp < -24.0) {
    return Status::Error("TruePeakSafety: ceiling out of range");
  }
  if (params.oversample_factor < 2 || params.max_iterations < 1) {
    return Status::Error("TruePeakSafety: invalid oversample/iterations");
  }
  params_ = params;
  return Status::Ok();
}

Status TruePeakSafety::process(audio::AudioBuffer& buffer, SafetyReport& report) const {
  if (!params_.enabled || buffer.channel_count() != 2) {
    return Status::Ok();
  }

  const double ceiling_lin = db_to_linear(params_.ceiling_dbtp);
  const double target_lin = db_to_linear(params_.ceiling_dbtp - params_.margin_db);

  for (int iter = 0; iter < params_.max_iterations; ++iter) {
    const double tp = analysis::measure_true_peak_linear(buffer, params_.oversample_factor);
    report.true_peak_linear = tp;
    report.true_peak_dbtp = linear_to_db(tp);

    if (!(tp > ceiling_lin)) {
      report.true_peak_safety_applied = report.true_peak_safety_applied || (iter > 0);
      return Status::Ok();
    }

    if (!(tp > 0.0)) {
      return Status::Ok();
    }

    const float scale = static_cast<float>(target_lin / tp);
    for (std::size_t i = 0; i < buffer.sample_count(); ++i) {
      buffer.data()[i] *= scale;
    }
    report.true_peak_safety_applied = true;
    report.true_peak_safety_scale *= scale;
  }

  const double tp_final =
      analysis::measure_true_peak_linear(buffer, params_.oversample_factor);
  report.true_peak_linear = tp_final;
  report.true_peak_dbtp = linear_to_db(tp_final);
  if (tp_final > ceiling_lin + 1e-6) {
    return Status::Error("TruePeakSafety: unable to bring true peak under ceiling");
  }
  return Status::Ok();
}

}  // namespace stemy::dsp
