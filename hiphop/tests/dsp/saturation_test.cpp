#include "stemy/dsp/saturation.h"
#include "test_signals.h"

#include <gtest/gtest.h>

TEST(Saturation, ZeroDriveTransparent) {
  auto buf = stemy::test::make_sine(1024, 48000, 440.0, 0.5);
  auto original = buf;
  stemy::dsp::Saturation sat;
  stemy::dsp::SaturationParams p;
  p.enabled = true;
  p.drive = 0.0;
  p.mix = 1.0;
  ASSERT_TRUE(sat.prepare(p));
  ASSERT_TRUE(sat.process(buf));
  for (std::size_t i = 0; i < buf.sample_count(); ++i) {
    EXPECT_FLOAT_EQ(buf.data()[i], original.data()[i]);
  }
}

TEST(Saturation, OutputRemainsFinite) {
  auto buf = stemy::test::make_sine(2048, 48000, 440.0, 0.9);
  stemy::dsp::Saturation sat;
  stemy::dsp::SaturationParams p;
  p.enabled = true;
  p.drive = 2.0;
  p.mix = 1.0;
  ASSERT_TRUE(sat.prepare(p));
  ASSERT_TRUE(sat.process(buf));
  EXPECT_TRUE(buf.validate_finite());
}
