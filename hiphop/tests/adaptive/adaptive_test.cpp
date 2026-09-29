#include "stemy/adaptive/adaptive_engine.h"
#include "stemy/analysis/analysis_result.h"

#include <gtest/gtest.h>

TEST(Adaptive, QuietInputLiftsTowardMaster01Pocket) {
  stemy::analysis::AnalysisResult analysis;
  analysis.sample_rate = 48000;
  analysis.channel_count = 2;
  analysis.integrated_loudness_lufs = -20.0;
  analysis.true_peak_dbtp = -3.0;
  analysis.crest_factor_db = 18.0;

  auto genre = stemy::genres::hiphop::HipHopParameters::make_provisional_defaults();
  stemy::adaptive::ProcessingDecision decision;
  stemy::adaptive::AdaptiveEngine engine;
  ASSERT_TRUE(engine.decide(analysis, genre, decision));

  EXPECT_GT(decision.input_gain.gain_db, 5.0);
  EXPECT_LE(decision.input_gain.gain_db, genre.max_lift_db);
  EXPECT_TRUE(decision.limiter.enabled);
  EXPECT_DOUBLE_EQ(decision.limiter.ceiling_dbfs, -0.5);
  EXPECT_DOUBLE_EQ(decision.stereo.width, 1.0);
  EXPECT_TRUE(decision.low_end.enabled);  // quiet tonal trim
  EXPECT_FALSE(decision.saturation.enabled);
  EXPECT_FALSE(decision.compressor.enabled);
}

TEST(Adaptive, HotInputDoesNotBoost) {
  stemy::analysis::AnalysisResult analysis;
  analysis.sample_rate = 48000;
  analysis.channel_count = 2;
  analysis.integrated_loudness_lufs = -6.0;
  analysis.true_peak_dbtp = -0.1;
  analysis.crest_factor_db = 8.0;

  auto genre = stemy::genres::hiphop::HipHopParameters::make_provisional_defaults();
  stemy::adaptive::ProcessingDecision decision;
  stemy::adaptive::AdaptiveEngine engine;
  ASSERT_TRUE(engine.decide(analysis, genre, decision));

  EXPECT_LE(decision.input_gain.gain_db, 0.0);
  EXPECT_GE(decision.input_gain.gain_db, -genre.max_reduction_db);
  EXPECT_FALSE(decision.low_end.enabled);  // no quiet-path tonal trim
  EXPECT_DOUBLE_EQ(decision.stereo.width, 1.0);
}

TEST(Adaptive, InPocketNearUnityGain) {
  stemy::analysis::AnalysisResult analysis;
  analysis.sample_rate = 48000;
  analysis.channel_count = 2;
  analysis.integrated_loudness_lufs = -9.0;
  analysis.true_peak_dbtp = -0.6;
  analysis.crest_factor_db = 10.2;

  auto genre = stemy::genres::hiphop::HipHopParameters::make_provisional_defaults();
  stemy::adaptive::ProcessingDecision decision;
  stemy::adaptive::AdaptiveEngine engine;
  ASSERT_TRUE(engine.decide(analysis, genre, decision));

  EXPECT_DOUBLE_EQ(decision.input_gain.gain_db, 0.0);
  EXPECT_FALSE(decision.saturation.enabled);
  EXPECT_DOUBLE_EQ(decision.stereo.width, 1.0);
}
