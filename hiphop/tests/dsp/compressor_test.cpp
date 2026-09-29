#include "stemy/dsp/compressor.h"
#include "test_signals.h"

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>

TEST(Compressor, RatioOneIsTransparent) {
  auto buf = stemy::test::make_sine(4096, 48000, 200.0, 0.8);
  auto original = buf;
  stemy::dsp::Compressor c;
  stemy::dsp::CompressorParams p;
  p.enabled = true;
  p.ratio = 1.0;
  p.threshold_db = -20.0;
  ASSERT_TRUE(c.prepare(48000, p));
  ASSERT_TRUE(c.process(buf));
  for (std::size_t i = 0; i < buf.sample_count(); ++i) {
    EXPECT_NEAR(buf.data()[i], original.data()[i], 1e-6);
  }
}

TEST(Compressor, AboveThresholdReducesPeak) {
  auto buf = stemy::test::make_sine(48000, 48000, 200.0, 0.9);
  float in_peak = 0.0f;
  for (std::size_t i = 0; i < buf.sample_count(); ++i) {
    in_peak = std::max(in_peak, std::fabs(buf.data()[i]));
  }

  stemy::dsp::Compressor c;
  stemy::dsp::CompressorParams p;
  p.enabled = true;
  p.threshold_db = -20.0;
  p.ratio = 4.0;
  p.attack_ms = 1.0;
  p.release_ms = 50.0;
  ASSERT_TRUE(c.prepare(48000, p));
  ASSERT_TRUE(c.process(buf));

  float out_peak = 0.0f;
  for (std::size_t i = 1000; i < buf.sample_count(); ++i) {
    out_peak = std::max(out_peak, std::fabs(buf.data()[i]));
  }
  EXPECT_LT(out_peak, in_peak);
}
