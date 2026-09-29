#pragma once

#include "stemy/adaptive/processing_decision.h"
#include "stemy/audio/audio_buffer.h"
#include "stemy/core/status.h"
#include "stemy/dsp/audio_safety.h"
#include "stemy/dsp/compressor.h"
#include "stemy/dsp/eq.h"
#include "stemy/dsp/gain_stage.h"
#include "stemy/dsp/limiter.h"
#include "stemy/dsp/low_end_control.h"
#include "stemy/dsp/saturation.h"
#include "stemy/dsp/soft_peak_rounder.h"
#include "stemy/dsp/stereo_processor.h"
#include "stemy/dsp/transient_controller.h"
#include "stemy/dsp/true_peak_safety.h"
#include "stemy/genres/hiphop/hiphop_parameters.h"

#include <cstdint>

namespace stemy::genres::hiphop {

class HipHopChain {
public:
  [[nodiscard]] Status prepare(std::uint32_t sample_rate,
                               const HipHopParameters& genre_params,
                               const adaptive::ProcessingDecision& decision);

  [[nodiscard]] Status process(audio::AudioBuffer& buffer,
                               dsp::SafetyReport* safety_report = nullptr) const;

private:
  HipHopParameters genre_{};
  adaptive::ProcessingDecision decision_{};
  std::uint32_t sample_rate_ = 0;

  dsp::GainStage input_gain_{};
  dsp::Eq eq_{};
  dsp::LowEndControl low_end_{};
  dsp::TransientController transient_{};
  dsp::Compressor compressor_{};
  dsp::Saturation saturation_{};
  dsp::Eq hf_eq_{};
  dsp::Eq low_weight_eq_{};
  dsp::StereoProcessor stereo_{};
  dsp::SoftPeakRounder soft_peak_rounder_{};
  dsp::Limiter limiter_{};
  dsp::GainStage output_gain_{};
  dsp::TruePeakSafety true_peak_safety_{};
};

}  // namespace stemy::genres::hiphop
