#include "stemy/analysis/analysis_pipeline.h"
#include "stemy/analysis/dynamics_analyzer.h"
#include "stemy/analysis/loudness_analyzer.h"
#include "stemy/analysis/peak_analyzer.h"
#include "stemy/analysis/spectral_analyzer.h"
#include "stemy/analysis/stereo_analyzer.h"
#include "stemy/analysis/true_peak_analyzer.h"

namespace stemy::analysis {

Status AnalysisPipeline::run(const audio::AudioBuffer& buffer,
                             const audio::AudioFormat& format,
                             AnalysisResult& out) const {
  out = AnalysisResult{};
  out.sample_rate = format.sample_rate;
  out.channel_count = format.channel_count;
  out.duration_seconds =
      static_cast<double>(buffer.frame_count()) / static_cast<double>(format.sample_rate);

  const PeakAnalyzer peak;
  const TruePeakAnalyzer true_peak;
  const LoudnessAnalyzer loudness;
  const DynamicsAnalyzer dynamics;
  const SpectralAnalyzer spectral;
  const StereoAnalyzer stereo;

  if (auto st = peak.analyze(buffer, format, out); !st) return st;
  if (auto st = true_peak.analyze(buffer, format, out); !st) return st;
  if (auto st = loudness.analyze(buffer, format, out); !st) return st;
  if (auto st = dynamics.analyze(buffer, format, out); !st) return st;
  if (auto st = spectral.analyze(buffer, format, out); !st) return st;
  if (auto st = stereo.analyze(buffer, format, out); !st) return st;
  return Status::Ok();
}

}  // namespace stemy::analysis
