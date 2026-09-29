#include "stemy/dsp/transient_controller.h"
#include "test_signals.h"

#include <gtest/gtest.h>

TEST(Transient, DisabledIsTransparent) {
  auto buf = stemy::test::make_sine(2048, 48000, 100.0, 0.5);
  auto original = buf;
  stemy::dsp::TransientController tc;
  stemy::dsp::TransientControllerParams p;
  p.enabled = false;
  p.mix = 0.5;
  p.attack_boost_db = 1.0;
  ASSERT_TRUE(tc.prepare(48000, p));
  ASSERT_TRUE(tc.process(buf));
  for (std::size_t i = 0; i < buf.sample_count(); ++i) {
    EXPECT_FLOAT_EQ(buf.data()[i], original.data()[i]);
  }
}

TEST(Transient, ZeroMixTransparent) {
  auto buf = stemy::test::make_impulse(2048, 2);
  auto original = buf;
  stemy::dsp::TransientController tc;
  stemy::dsp::TransientControllerParams p;
  p.enabled = true;
  p.mix = 0.0;
  p.attack_boost_db = 1.5;
  ASSERT_TRUE(tc.prepare(48000, p));
  ASSERT_TRUE(tc.process(buf));
  for (std::size_t i = 0; i < buf.sample_count(); ++i) {
    EXPECT_FLOAT_EQ(buf.data()[i], original.data()[i]);
  }
}

TEST(Transient, RemainsFinite) {
  auto buf = stemy::test::make_broadband(8192);
  stemy::dsp::TransientController tc;
  stemy::dsp::TransientControllerParams p;
  p.enabled = true;
  p.mix = 0.5;
  p.attack_boost_db = 1.5;
  p.sustain_cut_db = 0.75;
  ASSERT_TRUE(tc.prepare(48000, p));
  ASSERT_TRUE(tc.process(buf));
  EXPECT_TRUE(buf.validate_finite());
}
