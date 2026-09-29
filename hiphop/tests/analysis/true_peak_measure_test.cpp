#include "stemy/analysis/true_peak_measure.h"
#include "stemy/audio/audio_buffer.h"

#include <cmath>
#include <gtest/gtest.h>

namespace {

constexpr double kPi = 3.14159265358979323846;

double lin_to_db(double x) {
  return 20.0 * std::log10(std::max(x, 1e-20));
}

stemy::audio::AudioBuffer make_stereo(std::size_t frames, float fill = 0.0f) {
  stemy::audio::AudioBuffer buf(frames, 2);
  for (std::size_t i = 0; i < buf.sample_count(); ++i) {
    buf.data()[i] = fill;
  }
  return buf;
}

}  // namespace

TEST(TruePeakMeasure, SilenceIsZero) {
  auto buf = make_stereo(2048, 0.0f);
  EXPECT_DOUBLE_EQ(stemy::analysis::measure_sample_peak_linear(buf), 0.0);
  EXPECT_DOUBLE_EQ(stemy::analysis::measure_true_peak_linear(buf, 4), 0.0);
}

TEST(TruePeakMeasure, SafeBelowCeilingWaveform) {
  auto buf = make_stereo(4800);
  const double amp = 0.5;  // −6 dBFS sample peak
  for (std::size_t n = 0; n < buf.frame_count(); ++n) {
    const float s = static_cast<float>(amp * std::sin(2.0 * kPi * 1000.0 * n / 48000.0));
    buf.at(n, 0) = s;
    buf.at(n, 1) = s;
  }
  const double sp = stemy::analysis::measure_sample_peak_linear(buf);
  const double tp = stemy::analysis::measure_true_peak_linear(buf, 4);
  EXPECT_NEAR(lin_to_db(sp), -6.0, 0.05);
  EXPECT_LE(tp, 0.55);                 // comfortably under 0 dBFS
  EXPECT_GE(tp, sp * 0.98);            // TP >= sample peak (within tiny numerical slack)
  EXPECT_LE(lin_to_db(tp), -5.5);
}

TEST(TruePeakMeasure, IntersamplePeakExceedsSamplePeak) {
  // Classic ISP: fs/4 sine at π/4 phase → sample crest ≈ 0.707, continuous crest = 1.
  auto buf = make_stereo(4096, 0.0f);
  const double sr = 48000.0;
  const double f = sr / 4.0;
  const double phase = kPi / 4.0;
  for (std::size_t n = 0; n < buf.frame_count(); ++n) {
    const float s =
        static_cast<float>(std::sin(2.0 * kPi * f * static_cast<double>(n) / sr + phase));
    buf.at(n, 0) = s;
    buf.at(n, 1) = s;
  }
  const double sp = stemy::analysis::measure_sample_peak_linear(buf);
  const double tp = stemy::analysis::measure_true_peak_linear(buf, 4);
  EXPECT_NEAR(sp, std::sqrt(0.5), 1e-3);
  EXPECT_GT(tp, sp + 0.15) << "corrected TP must detect inter-sample overshoot";
  EXPECT_GT(tp, 0.95);
  EXPECT_GT(lin_to_db(tp) - lin_to_db(sp), 2.0);
}

TEST(TruePeakMeasure, HighFrequencySineMissesSampleCrest) {
  // Near-Nyquist stress: 0.4 * Nyquist, phase so discrete samples undersample crest.
  auto buf = make_stereo(8192, 0.0f);
  const double sr = 48000.0;
  const double f = 0.4 * (sr * 0.5);
  const double phase = kPi / 4.0;
  const double amp = 0.95;
  for (std::size_t n = 0; n < buf.frame_count(); ++n) {
    const float s = static_cast<float>(
        amp * std::sin(2.0 * kPi * f * static_cast<double>(n) / sr + phase));
    buf.at(n, 0) = s;
    buf.at(n, 1) = s;
  }
  const double sp = stemy::analysis::measure_sample_peak_linear(buf);
  const double tp = stemy::analysis::measure_true_peak_linear(buf, 4);
  EXPECT_LT(sp, amp - 0.005);
  EXPECT_GT(tp, sp);
  EXPECT_GT(tp, 0.97);
}

TEST(TruePeakMeasure, StereoUsesHotterChannel) {
  auto buf = make_stereo(4096, 0.0f);
  const double sr = 48000.0;
  const double f = sr / 4.0;
  const double phase = kPi / 4.0;
  for (std::size_t n = 0; n < buf.frame_count(); ++n) {
    const float hot =
        static_cast<float>(std::sin(2.0 * kPi * f * static_cast<double>(n) / sr + phase));
    buf.at(n, 0) = hot * 0.5f;
    buf.at(n, 1) = hot;
  }
  const double sp = stemy::analysis::measure_sample_peak_linear(buf);
  const double tp = stemy::analysis::measure_true_peak_linear(buf, 4);
  EXPECT_NEAR(sp, std::sqrt(0.5), 1e-3);
  EXPECT_GT(tp, 0.95);
}

TEST(TruePeakMeasure, TpNeverBelowSamplePeak) {
  auto buf = make_stereo(1024);
  for (std::size_t n = 0; n < buf.frame_count(); ++n) {
    const float s = static_cast<float>(0.8 * std::sin(2.0 * kPi * 440.0 * n / 48000.0));
    buf.at(n, 0) = s;
    buf.at(n, 1) = static_cast<float>(-0.7 * std::sin(2.0 * kPi * 880.0 * n / 48000.0));
  }
  const double sp = stemy::analysis::measure_sample_peak_linear(buf);
  const double tp = stemy::analysis::measure_true_peak_linear(buf, 4);
  EXPECT_GE(tp, sp - 1e-9);
}

TEST(TruePeakMeasure, FactorTwoAndEightRun) {
  auto buf = make_stereo(2048, 0.0f);
  const double sr = 48000.0;
  const double f = sr / 4.0;
  const double phase = kPi / 4.0;
  for (std::size_t n = 0; n < buf.frame_count(); ++n) {
    const float s =
        static_cast<float>(std::sin(2.0 * kPi * f * static_cast<double>(n) / sr + phase));
    buf.at(n, 0) = s;
    buf.at(n, 1) = s;
  }
  const double tp2 = stemy::analysis::measure_true_peak_linear(buf, 2);
  const double tp8 = stemy::analysis::measure_true_peak_linear(buf, 8);
  EXPECT_GT(tp2, 0.95);
  EXPECT_GT(tp8, 0.95);
}
