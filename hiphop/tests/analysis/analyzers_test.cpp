#include "stemy/analysis/analysis_pipeline.h"
#include "stemy/analysis/loudness_analyzer.h"
#include "stemy/analysis/peak_analyzer.h"
#include "stemy/analysis/stereo_analyzer.h"
#include "test_signals.h"

#include <gtest/gtest.h>

TEST(Analyzers, SilencePeakNearZero) {
  auto buf = stemy::test::make_silence(48000, 2);
  auto fmt = stemy::test::stereo_format(48000);
  stemy::analysis::AnalysisResult r;
  stemy::analysis::PeakAnalyzer peak;
  ASSERT_TRUE(peak.analyze(buf, fmt, r));
  ASSERT_TRUE(r.sample_peak_linear);
  EXPECT_NEAR(*r.sample_peak_linear, 0.0, 1e-12);
}

TEST(Analyzers, SinePeakNearAmplitude) {
  constexpr double amp = 0.5;
  auto buf = stemy::test::make_sine(48000, 48000, 1000.0, amp);
  auto fmt = stemy::test::stereo_format(48000);
  stemy::analysis::AnalysisResult r;
  stemy::analysis::PeakAnalyzer peak;
  ASSERT_TRUE(peak.analyze(buf, fmt, r));
  ASSERT_TRUE(r.sample_peak_linear);
  EXPECT_NEAR(*r.sample_peak_linear, amp, 0.02);
  ASSERT_TRUE(r.crest_factor_db);
  EXPECT_GT(*r.crest_factor_db, 0.0);
}

TEST(Analyzers, MonoCorrelationNearOne) {
  auto buf = stemy::test::make_sine(8000, 48000, 440.0, 0.4);
  auto fmt = stemy::test::stereo_format(48000);
  stemy::analysis::AnalysisResult r;
  stemy::analysis::StereoAnalyzer stereo;
  ASSERT_TRUE(stereo.analyze(buf, fmt, r));
  ASSERT_TRUE(r.stereo_correlation);
  EXPECT_NEAR(*r.stereo_correlation, 1.0, 1e-5);
}

TEST(Analyzers, PipelineFillsCoreFields) {
  auto buf = stemy::test::make_sine(48000, 48000, 100.0, 0.25);  // low-frequency sine
  auto fmt = stemy::test::stereo_format(48000);
  stemy::analysis::AnalysisResult r;
  stemy::analysis::AnalysisPipeline pipe;
  ASSERT_TRUE(pipe.run(buf, fmt, r));
  EXPECT_TRUE(r.integrated_loudness_lufs.has_value());
  EXPECT_TRUE(r.true_peak_dbtp.has_value());
  EXPECT_TRUE(r.spectral_bands.has_value());
  EXPECT_TRUE(r.stereo_width.has_value());
}
