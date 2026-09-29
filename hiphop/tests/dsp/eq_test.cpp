#include "stemy/dsp/eq.h"
#include "test_signals.h"

#include <gtest/gtest.h>

TEST(Eq, DisabledBandsAreTransparent) {
  auto buf = stemy::test::make_sine(2048, 48000, 1000.0, 0.5);
  auto original = buf;
  stemy::dsp::Eq eq;
  stemy::dsp::EqParams p;
  p.enabled = true;  // no bands enabled
  ASSERT_TRUE(eq.prepare(48000, p));
  ASSERT_TRUE(eq.process(buf));
  for (std::size_t i = 0; i < buf.sample_count(); ++i) {
    EXPECT_FLOAT_EQ(buf.data()[i], original.data()[i]);
  }
}

TEST(Eq, RejectsInvalidGain) {
  stemy::dsp::Eq eq;
  stemy::dsp::EqParams p;
  p.bands[0].enabled = true;
  p.bands[0].gain_db = 100.0;
  EXPECT_FALSE(eq.prepare(48000, p));
}
