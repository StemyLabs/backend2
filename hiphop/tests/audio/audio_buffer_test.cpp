#include "stemy/audio/audio_buffer.h"

#include <gtest/gtest.h>
#include <limits>

TEST(AudioBuffer, ResizeAndClear) {
  stemy::audio::AudioBuffer b(100, 2);
  EXPECT_EQ(b.frame_count(), 100u);
  EXPECT_EQ(b.channel_count(), 2u);
  EXPECT_EQ(b.sample_count(), 200u);
  b.at(0, 0) = 0.5f;
  b.clear();
  EXPECT_FLOAT_EQ(b.at(0, 0), 0.0f);
}

TEST(AudioBuffer, DetectsNonFinite) {
  stemy::audio::AudioBuffer b(4, 2);
  b.at(1, 0) = std::numeric_limits<float>::quiet_NaN();
  EXPECT_FALSE(b.validate_finite());
}

TEST(AudioBuffer, FlushDenormals) {
  stemy::audio::AudioBuffer b(2, 2);
  b.at(0, 0) = 1e-40f;
  b.flush_denormals();
  EXPECT_FLOAT_EQ(b.at(0, 0), 0.0f);
}
