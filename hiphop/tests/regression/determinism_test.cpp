#include "stemy/mastering/mastering_engine.h"
#include "test_signals.h"

#include <cmath>
#include <gtest/gtest.h>

/// Determinism tolerance for float DSP round-trips (documented for M1 Gate A).
constexpr double kDeterminismAbsTol = 1e-6;

TEST(Regression, SameInputSameConfigDeterministic) {
  auto a = stemy::test::make_sine(48000, 48000, 220.0, 0.3);
  auto b = a;
  auto fmt = stemy::test::stereo_format(48000);
  auto genre = stemy::genres::hiphop::HipHopParameters::make_provisional_defaults();

  stemy::mastering::MasteringEngine engine;
  stemy::mastering::MasteringStats stats_a;
  stemy::mastering::MasteringStats stats_b;
  ASSERT_TRUE(engine.process_buffer(a, fmt, genre, stats_a));
  ASSERT_TRUE(engine.process_buffer(b, fmt, genre, stats_b));

  ASSERT_EQ(a.sample_count(), b.sample_count());
  double max_diff = 0.0;
  for (std::size_t i = 0; i < a.sample_count(); ++i) {
    max_diff = std::max(max_diff, static_cast<double>(std::fabs(a.data()[i] - b.data()[i])));
  }
  EXPECT_LE(max_diff, kDeterminismAbsTol);

  ASSERT_TRUE(stats_a.output_analysis.true_peak_dbtp);
  ASSERT_TRUE(stats_b.output_analysis.true_peak_dbtp);
  EXPECT_NEAR(*stats_a.output_analysis.true_peak_dbtp, *stats_b.output_analysis.true_peak_dbtp,
              1e-4);
  EXPECT_LE(*stats_a.output_analysis.true_peak_dbtp, genre.target_true_peak_dbtp + 0.01);
}

TEST(Regression, OutputTruePeakRespectsCeiling) {
  // Hot sine near FS — engine must deliver TP <= -0.5 dBTP.
  auto buf = stemy::test::make_sine(48000, 48000, 100.0, 0.95);
  auto fmt = stemy::test::stereo_format(48000);
  auto genre = stemy::genres::hiphop::HipHopParameters::make_provisional_defaults();
  stemy::mastering::MasteringEngine engine;
  stemy::mastering::MasteringStats stats;
  ASSERT_TRUE(engine.process_buffer(buf, fmt, genre, stats));
  ASSERT_TRUE(stats.output_analysis.true_peak_dbtp);
  EXPECT_LE(*stats.output_analysis.true_peak_dbtp, genre.target_true_peak_dbtp + 0.01);
}
