#include "stemy/adaptive/adaptive_engine.h"
#include "stemy/genres/hiphop/hiphop_chain.h"
#include "test_signals.h"

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>

TEST(HipHopChain, InPocketDecisionNearIdentity) {
  auto buf = stemy::test::make_sine(8192, 48000, 440.0, 0.4);
  auto original = buf;
  auto fmt = stemy::test::stereo_format(48000);

  // Amplitude 0.4 ≈ -8 dBTP-ish sample peak; set analysis as already in pocket.
  stemy::analysis::AnalysisResult analysis;
  analysis.sample_rate = fmt.sample_rate;
  analysis.channel_count = 2;
  analysis.integrated_loudness_lufs = -9.0;
  analysis.true_peak_dbtp = -8.0;
  analysis.crest_factor_db = 10.0;

  auto genre = stemy::genres::hiphop::HipHopParameters::make_provisional_defaults();
  stemy::adaptive::ProcessingDecision decision;
  stemy::adaptive::AdaptiveEngine engine;
  ASSERT_TRUE(engine.decide(analysis, genre, decision));
  EXPECT_DOUBLE_EQ(decision.input_gain.gain_db, 0.0);
  EXPECT_FALSE(decision.limiter.enabled);

  stemy::genres::hiphop::HipHopChain chain;
  ASSERT_TRUE(chain.prepare(fmt.sample_rate, genre, decision));
  ASSERT_TRUE(chain.process(buf));

  double max_diff = 0.0;
  for (std::size_t i = 0; i < buf.sample_count(); ++i) {
    max_diff = std::max(max_diff,
                        static_cast<double>(std::fabs(buf.data()[i] - original.data()[i])));
  }
  EXPECT_LT(max_diff, 1e-5);
}

TEST(HipHopChain, QuietPathAppliesGainAndLimiter) {
  auto buf = stemy::test::make_sine(8192, 48000, 440.0, 0.2);
  auto fmt = stemy::test::stereo_format(48000);

  stemy::analysis::AnalysisResult analysis;
  analysis.sample_rate = fmt.sample_rate;
  analysis.channel_count = 2;
  analysis.integrated_loudness_lufs = -18.0;
  analysis.true_peak_dbtp = -14.0;
  analysis.crest_factor_db = 16.0;

  auto genre = stemy::genres::hiphop::HipHopParameters::make_provisional_defaults();
  stemy::adaptive::ProcessingDecision decision;
  stemy::adaptive::AdaptiveEngine engine;
  ASSERT_TRUE(engine.decide(analysis, genre, decision));
  EXPECT_GT(decision.input_gain.gain_db, 0.0);
  EXPECT_TRUE(decision.limiter.enabled);

  stemy::genres::hiphop::HipHopChain chain;
  ASSERT_TRUE(chain.prepare(fmt.sample_rate, genre, decision));
  ASSERT_TRUE(chain.process(buf));
  EXPECT_TRUE(buf.validate_finite());
}
