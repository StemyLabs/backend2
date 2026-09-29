#include <gtest/gtest.h>

#include "stemy/analysis/stereo_analyzer.h"
#include "test_signals.h"

#include <cmath>

TEST(StereoAnalyzerTest, MonoIdenticalChannelsCorrelate) {
  auto format = stemy::test::stereo_format();
  auto buffer = stemy::test::make_sine(4800, 48000, 440.0, 0.4);
  stemy::analysis::AnalysisResult result;
  stemy::analysis::StereoAnalyzer analyzer;
  ASSERT_TRUE(analyzer.analyze(buffer, format, result).ok());
  ASSERT_TRUE(result.stereo_correlation.has_value());
  EXPECT_NEAR(*result.stereo_correlation, 1.0, 1e-4);
  ASSERT_TRUE(result.side_energy.has_value());
  EXPECT_NEAR(*result.side_energy, 0.0, 1e-6);
}

TEST(StereoAnalyzerTest, IndependentChannelsLowerCorrelation) {
  auto format = stemy::test::stereo_format();
  auto buffer = stemy::test::make_stereo_sine(48000, 48000, 440.0, 660.0, 0.4);
  stemy::analysis::AnalysisResult result;
  stemy::analysis::StereoAnalyzer analyzer;
  ASSERT_TRUE(analyzer.analyze(buffer, format, result).ok());
  ASSERT_TRUE(result.stereo_correlation.has_value());
  EXPECT_LT(std::fabs(*result.stereo_correlation), 0.2);
}
