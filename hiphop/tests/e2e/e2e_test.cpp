#include "stemy/audio/wav_io.h"
#include "stemy/mastering/mastering_engine.h"
#include "test_signals.h"

#include <filesystem>
#include <gtest/gtest.h>

TEST(E2E, ProcessSineWavRoundTrip) {
  const auto dir = std::filesystem::temp_directory_path() / "stemy_e2e";
  std::filesystem::create_directories(dir);
  const auto in_path = dir / "in.wav";
  const auto out_path = dir / "out.wav";

  auto buf = stemy::test::make_sine(48000, 48000, 440.0, 0.3);
  auto fmt = stemy::test::stereo_format(48000);
  ASSERT_TRUE(stemy::audio::write_wav(in_path, buf, fmt));

  stemy::mastering::MasteringEngine engine;
  stemy::mastering::MasteringRequest req;
  req.input_path = in_path;
  req.output_path = out_path;
  stemy::mastering::MasteringStats stats;
  ASSERT_TRUE(engine.process_file(req, stats));

  stemy::audio::WavReadResult out;
  ASSERT_TRUE(stemy::audio::read_wav(out_path, out));
  EXPECT_EQ(out.format.channel_count, 2);
  EXPECT_EQ(out.format.sample_rate, 48000u);
  EXPECT_TRUE(out.buffer.validate_finite());
  EXPECT_FALSE(stats.safety.had_nan_inf);
  ASSERT_TRUE(stats.output_analysis.sample_peak_linear);
  EXPECT_LE(*stats.output_analysis.sample_peak_linear, 1.0 + 1e-5);
}
