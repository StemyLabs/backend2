#include "stemy/audio/wav_io.h"
#include "stemy/dsp/audio_safety.h"
#include "test_signals.h"

#include <gtest/gtest.h>

TEST(Validation, RejectsMono) {
  auto buf = stemy::test::make_silence(100, 1);
  auto fmt = stemy::test::stereo_format();
  fmt.channel_count = 1;
  EXPECT_FALSE(stemy::audio::validate_input_audio(buf, fmt));
}

TEST(Validation, RejectsBadSampleRate) {
  auto buf = stemy::test::make_silence(100, 2);
  auto fmt = stemy::test::stereo_format(12345);
  EXPECT_FALSE(stemy::audio::validate_input_audio(buf, fmt));
}

TEST(Validation, AcceptsStereo48k) {
  auto buf = stemy::test::make_silence(100, 2);
  auto fmt = stemy::test::stereo_format(48000);
  EXPECT_TRUE(stemy::audio::validate_input_audio(buf, fmt));
}

TEST(Validation, GainRange) {
  EXPECT_TRUE(stemy::dsp::validate_gain_db(0.0));
  EXPECT_FALSE(stemy::dsp::validate_gain_db(100.0));
}
