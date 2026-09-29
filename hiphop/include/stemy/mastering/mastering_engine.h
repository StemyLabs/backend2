#pragma once

#include "stemy/adaptive/processing_decision.h"
#include "stemy/analysis/analysis_result.h"
#include "stemy/audio/audio_buffer.h"
#include "stemy/audio/audio_format.h"
#include "stemy/core/status.h"
#include "stemy/dsp/audio_safety.h"
#include "stemy/genres/hiphop/hiphop_parameters.h"

#include <filesystem>
#include <optional>
#include <string>

namespace stemy::mastering {

struct MasteringStats {
  analysis::AnalysisResult input_analysis;
  analysis::AnalysisResult output_analysis;
  adaptive::ProcessingDecision decision;
  dsp::SafetyReport safety;
};

struct MasteringRequest {
  std::filesystem::path input_path;
  std::filesystem::path output_path;
  genres::hiphop::HipHopParameters genre_params =
      genres::hiphop::HipHopParameters::make_provisional_defaults();
  bool write_output = true;
};

/// High-level orchestrator: validate → analyze → decide → process → analyze → write.
class MasteringEngine {
public:
  [[nodiscard]] Status process_file(const MasteringRequest& request, MasteringStats& stats) const;

  [[nodiscard]] Status process_buffer(audio::AudioBuffer& buffer,
                                      const audio::AudioFormat& format,
                                      const genres::hiphop::HipHopParameters& genre_params,
                                      MasteringStats& stats) const;
};

}  // namespace stemy::mastering
