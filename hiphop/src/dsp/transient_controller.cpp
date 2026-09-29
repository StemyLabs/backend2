#include "stemy/dsp/transient_controller.h"
#include "stemy/core/dsp_math.h"

#include <algorithm>
#include <cmath>

namespace stemy::dsp {
namespace {

double clamp_value(double v, double lo, double hi) {
  return std::max(lo, std::min(hi, v));
}

}  // namespace

Status TransientController::prepare(std::uint32_t sample_rate,
                                    const TransientControllerParams& params) {
  if (sample_rate == 0) {
    return Status::Error("TransientController: invalid sample rate");
  }
  if (!std::isfinite(params.attack_boost_db) || params.attack_boost_db < 0.0 ||
      params.attack_boost_db > 6.0) {
    return Status::Error("TransientController: invalid attack_boost_db");
  }
  if (!std::isfinite(params.sustain_cut_db) || params.sustain_cut_db < 0.0 ||
      params.sustain_cut_db > 6.0) {
    return Status::Error("TransientController: invalid sustain_cut_db");
  }
  if (!std::isfinite(params.mix) || params.mix < 0.0 || params.mix > 1.0) {
    return Status::Error("TransientController: invalid mix");
  }
  if (!(params.sensitivity > 0.0) || params.sensitivity > 2.0) {
    return Status::Error("TransientController: invalid sensitivity");
  }
  if (!(params.attack_ms > 0.0) || !(params.release_ms > 0.0)) {
    return Status::Error("TransientController: invalid times");
  }
  if (!(params.sense_hpf_hz > 0.0) || params.sense_hpf_hz >= 0.45 * sample_rate) {
    return Status::Error("TransientController: invalid sense_hpf_hz");
  }

  params_ = params;
  sample_rate_ = sample_rate;
  attack_coef_ = std::exp(-1.0 / (0.001 * params.attack_ms * sample_rate));
  release_coef_ = std::exp(-1.0 / (0.001 * params.release_ms * sample_rate));
  const double x = std::exp(-2.0 * 3.14159265358979323846 * params.sense_hpf_hz / sample_rate);
  hpf_a_ = x;
  reset();
  return Status::Ok();
}

void TransientController::reset() {
  hpf_z_ = 0.0;
  env_fast_ = 0.0;
  env_slow_ = 0.0;
}

Status TransientController::process(audio::AudioBuffer& buffer) const {
  if (!params_.enabled || params_.mix <= 0.0 || buffer.channel_count() != 2) {
    return Status::Ok();
  }
  if (params_.attack_boost_db <= 0.0 && params_.sustain_cut_db <= 0.0) {
    return Status::Ok();
  }

  const float mix = static_cast<float>(params_.mix);
  const double boost_lin = db_to_linear(params_.attack_boost_db);
  const double cut_lin = db_to_linear(-params_.sustain_cut_db);
  const double sens = params_.sensitivity;

  for (std::size_t i = 0; i < buffer.frame_count(); ++i) {
    float l = buffer.at(i, 0);
    float r = buffer.at(i, 1);

    const double mono = 0.5 * (static_cast<double>(l) + static_cast<double>(r));
    const double hp = mono - hpf_z_;
    hpf_z_ = mono * (1.0 - hpf_a_) + hpf_z_ * hpf_a_;
    const double det = std::fabs(hp) * sens;

    if (det > env_fast_) {
      env_fast_ = attack_coef_ * env_fast_ + (1.0 - attack_coef_) * det;
    } else {
      env_fast_ = release_coef_ * env_fast_ + (1.0 - release_coef_) * det;
    }
    env_slow_ = release_coef_ * env_slow_ + (1.0 - release_coef_) * env_fast_;

    const double ratio = (env_slow_ > 1e-9) ? (env_fast_ / env_slow_) : 1.0;
    double transient_amt = clamp_value(ratio - 1.0, 0.0, 1.0);
    if (env_fast_ < 1e-4) {
      transient_amt = 0.0;
    }

    const double gain =
        1.0 + transient_amt * (boost_lin - 1.0) + (1.0 - transient_amt) * (cut_lin - 1.0);
    const float g = static_cast<float>(gain);
    buffer.at(i, 0) = l * (1.0f - mix) + (l * g) * mix;
    buffer.at(i, 1) = r * (1.0f - mix) + (r * g) * mix;
  }
  return Status::Ok();
}

}  // namespace stemy::dsp
