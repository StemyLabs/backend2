#include "stemy/dsp/true_peak_safety.h"
#include "stemy/analysis/true_peak_measure.h"
#include "stemy/core/dsp_math.h"
#include "test_signals.h"

#include <gtest/gtest.h>

TEST(TruePeakSafety, ReducesHotSignalUnderCeiling) {
  auto buf = stemy::test::make_sine(48000, 48000, 1000.0, 0.99);
  const double before = stemy::analysis::measure_true_peak_linear(buf, 4);
  ASSERT_GT(stemy::linear_to_db(before), -0.5);

  stemy::dsp::TruePeakSafety safety;
  stemy::dsp::TruePeakSafetyParams p;
  p.ceiling_dbtp = -0.5;
  ASSERT_TRUE(safety.prepare(p));
  stemy::dsp::SafetyReport report;
  ASSERT_TRUE(safety.process(buf, report));
  ASSERT_TRUE(report.true_peak_dbtp);
  EXPECT_LE(*report.true_peak_dbtp, -0.5 + 0.01);
  EXPECT_TRUE(report.true_peak_safety_applied);
}

TEST(TruePeakSafety, LeavesSafeSignalUnchanged) {
  auto buf = stemy::test::make_sine(4096, 48000, 440.0, 0.2);
  auto original = buf;
  stemy::dsp::TruePeakSafety safety;
  stemy::dsp::TruePeakSafetyParams p;
  p.ceiling_dbtp = -0.5;
  ASSERT_TRUE(safety.prepare(p));
  stemy::dsp::SafetyReport report;
  ASSERT_TRUE(safety.process(buf, report));
  EXPECT_FALSE(report.true_peak_safety_applied);
  for (std::size_t i = 0; i < buf.sample_count(); ++i) {
    EXPECT_FLOAT_EQ(buf.data()[i], original.data()[i]);
  }
}
