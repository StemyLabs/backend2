#include "stemy/dsp/stereo_processor.h"
#include "test_signals.h"

#include <cmath>
#include <gtest/gtest.h>

TEST(MonoCompat, WidthWithBassMonoKeepsFiniteAndCorrSane) {
  auto buf = stemy::test::make_stereo_sine(48000, 48000, 80.0, 1000.0, 0.4);
  stemy::dsp::StereoProcessor sp;
  stemy::dsp::StereoProcessorParams spp;
  spp.enabled = true;
  spp.width = 1.10;
  spp.side_hpf_hz = 120.0;
  ASSERT_TRUE(sp.prepare(48000, spp));
  ASSERT_TRUE(sp.process(buf));
  EXPECT_TRUE(buf.validate_finite());

  // Mono fold-down remains finite.
  double peak = 0.0;
  for (std::size_t i = 0; i < buf.frame_count(); ++i) {
    const double m = 0.5 * (buf.at(i, 0) + buf.at(i, 1));
    peak = std::max(peak, std::fabs(m));
  }
  EXPECT_LT(peak, 1.5);
}
