#include "stemy/dsp/stereo_processor.h"
#include "test_signals.h"

#include <gtest/gtest.h>

TEST(Stereo, WidthOneTransparent) {
  auto buf = stemy::test::make_stereo_sine(2048, 48000, 440.0, 660.0, 0.4);
  auto original = buf;
  stemy::dsp::StereoProcessor sp;
  stemy::dsp::StereoProcessorParams p;
  p.enabled = true;
  p.width = 1.0;
  ASSERT_TRUE(sp.prepare(48000, p));
  ASSERT_TRUE(sp.process(buf));
  for (std::size_t i = 0; i < buf.sample_count(); ++i) {
    EXPECT_NEAR(buf.data()[i], original.data()[i], 1e-6);
  }
}

TEST(Stereo, WidthZeroCollapsesToMono) {
  auto buf = stemy::test::make_stereo_sine(2048, 48000, 440.0, 660.0, 0.4);
  stemy::dsp::StereoProcessor sp;
  stemy::dsp::StereoProcessorParams p;
  p.enabled = true;
  p.width = 0.0;
  ASSERT_TRUE(sp.prepare(48000, p));
  ASSERT_TRUE(sp.process(buf));
  for (std::size_t i = 0; i < buf.frame_count(); ++i) {
    EXPECT_NEAR(buf.at(i, 0), buf.at(i, 1), 1e-5);
  }
}

TEST(Stereo, SideHpfKeepsNearMonoBassEnergy) {
  // Correlated low sine should stay near original level with side HPF + mild width.
  auto buf = stemy::test::make_stereo_sine(48000, 48000, 60.0, 60.0, 0.5);
  double in_energy = 0.0;
  for (std::size_t i = 0; i < buf.sample_count(); ++i) {
    in_energy += static_cast<double>(buf.data()[i]) * buf.data()[i];
  }
  stemy::dsp::StereoProcessor sp;
  stemy::dsp::StereoProcessorParams p;
  p.enabled = true;
  p.width = 1.10;
  p.side_hpf_hz = 120.0;
  ASSERT_TRUE(sp.prepare(48000, p));
  ASSERT_TRUE(sp.process(buf));
  double out_energy = 0.0;
  for (std::size_t i = 0; i < buf.sample_count(); ++i) {
    out_energy += static_cast<double>(buf.data()[i]) * buf.data()[i];
  }
  EXPECT_NEAR(out_energy, in_energy, in_energy * 0.02);
}
