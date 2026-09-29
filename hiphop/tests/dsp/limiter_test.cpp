#include "stemy/dsp/limiter.h"
#include "stemy/analysis/true_peak_measure.h"
#include "test_signals.h"

#include <cmath>
#include <gtest/gtest.h>

namespace {

constexpr double kPi = 3.14159265358979323846;

double lin_to_db(double x) { return 20.0 * std::log10(std::max(x, 1e-20)); }

stemy::audio::AudioBuffer make_stereo_frames(std::size_t frames, float fill = 0.0f) {
  stemy::audio::AudioBuffer buf(frames, 2);
  for (std::size_t i = 0; i < buf.sample_count(); ++i) buf.data()[i] = fill;
  return buf;
}

}  // namespace

TEST(Limiter, DisabledIsTransparent) {
  auto buf = stemy::test::make_sine(2048, 48000, 100.0, 0.9);
  auto original = buf;
  stemy::dsp::Limiter lim;
  stemy::dsp::LimiterParams p;
  p.enabled = false;
  ASSERT_TRUE(lim.prepare(48000, p));
  ASSERT_TRUE(lim.process(buf));
  for (std::size_t i = 0; i < buf.sample_count(); ++i) {
    EXPECT_FLOAT_EQ(buf.data()[i], original.data()[i]);
  }
}

TEST(Limiter, NeverExceedsCeiling) {
  auto buf = stemy::test::make_sine(48000, 48000, 100.0, 0.99);
  stemy::dsp::Limiter lim;
  stemy::dsp::LimiterParams p;
  p.enabled = true;
  p.ceiling_dbfs = -3.0;
  p.lookahead_ms = 2.0;
  p.release_ms = 50.0;
  ASSERT_TRUE(lim.prepare(48000, p));
  ASSERT_TRUE(lim.process(buf));

  const float ceiling = static_cast<float>(std::pow(10.0, -3.0 / 20.0));
  for (std::size_t i = 100; i < buf.sample_count(); ++i) {
    EXPECT_LE(std::fabs(buf.data()[i]), ceiling + 1e-4f);
  }
}

TEST(Limiter, SilenceRemainsSilence) {
  auto buf = stemy::test::make_silence(2048, 2);
  stemy::dsp::Limiter lim;
  stemy::dsp::LimiterParams p;
  p.enabled = true;
  p.ceiling_dbfs = -1.0;
  ASSERT_TRUE(lim.prepare(48000, p));
  ASSERT_TRUE(lim.process(buf));
  for (std::size_t i = 0; i < buf.sample_count(); ++i) {
    EXPECT_FLOAT_EQ(buf.data()[i], 0.0f);
  }
}

TEST(LimiterIsp, SilenceStaysSilent) {
  auto buf = make_stereo_frames(2048, 0.0f);
  stemy::dsp::Limiter lim;
  stemy::dsp::LimiterParams p;
  p.enabled = true;
  p.ceiling_dbfs = -1.0;
  p.true_peak_aware = true;
  p.lookahead_ms = 1.0;
  ASSERT_TRUE(lim.prepare(48000, p));
  ASSERT_TRUE(lim.process(buf));
  EXPECT_DOUBLE_EQ(stemy::analysis::measure_true_peak_linear(buf, 4), 0.0);
}

TEST(LimiterIsp, SafeBelowCeilingUnchangedLevel) {
  auto buf = make_stereo_frames(4800);
  for (std::size_t n = 0; n < buf.frame_count(); ++n) {
    const float s = static_cast<float>(0.5 * std::sin(2.0 * kPi * 1000.0 * n / 48000.0));
    buf.at(n, 0) = s;
    buf.at(n, 1) = s;
  }
  stemy::dsp::Limiter lim;
  stemy::dsp::LimiterParams p;
  p.enabled = true;
  p.ceiling_dbfs = -1.0;
  p.true_peak_aware = true;
  ASSERT_TRUE(lim.prepare(48000, p));
  ASSERT_TRUE(lim.process(buf));
  const double tp = stemy::analysis::measure_true_peak_linear(buf, 4);
  EXPECT_LE(lin_to_db(tp), -1.0 + 0.05);
  EXPECT_NEAR(lin_to_db(tp), -6.0, 0.15);  // essentially untouched
  EXPECT_LT(lim.telemetry().max_gr_db, 0.05f);
}

TEST(LimiterIsp, PhaseShiftedQuarterNyquistRespectsCeiling) {
  // Sample peak ≈ −3 dBFS, continuous/ISP ≈ 0 dBFS → must engage ISP-aware GR.
  auto buf = make_stereo_frames(8192);
  const double sr = 48000.0;
  const double f = sr / 4.0;
  const double phase = kPi / 4.0;
  for (std::size_t n = 0; n < buf.frame_count(); ++n) {
    const float s =
        static_cast<float>(std::sin(2.0 * kPi * f * static_cast<double>(n) / sr + phase));
    buf.at(n, 0) = s;
    buf.at(n, 1) = s;
  }
  const double tp_before = stemy::analysis::measure_true_peak_linear(buf, 4);
  EXPECT_GT(lin_to_db(tp_before), -0.5);

  stemy::dsp::Limiter lim;
  stemy::dsp::LimiterParams p;
  p.enabled = true;
  p.ceiling_dbfs = -1.0;
  p.true_peak_aware = true;
  p.lookahead_ms = 2.0;
  p.release_ms = 50.0;
  ASSERT_TRUE(lim.prepare(48000, p));
  ASSERT_TRUE(lim.process(buf));

  const double tp = stemy::analysis::measure_true_peak_linear(buf, 4);
  EXPECT_LE(lin_to_db(tp), -1.0 + 0.08);
  EXPECT_GT(lim.telemetry().max_gr_db, 0.5f);
}

TEST(LimiterIsp, HighFrequencySineStress) {
  auto buf = make_stereo_frames(8192);
  const double sr = 48000.0;
  const double f = 0.4 * (sr * 0.5);
  const double phase = kPi / 4.0;
  for (std::size_t n = 0; n < buf.frame_count(); ++n) {
    const float s = static_cast<float>(
        0.98 * std::sin(2.0 * kPi * f * static_cast<double>(n) / sr + phase));
    buf.at(n, 0) = s;
    buf.at(n, 1) = s;
  }
  stemy::dsp::Limiter lim;
  stemy::dsp::LimiterParams p;
  p.enabled = true;
  p.ceiling_dbfs = -1.05;
  p.true_peak_aware = true;
  p.lookahead_ms = 2.0;
  ASSERT_TRUE(lim.prepare(48000, p));
  ASSERT_TRUE(lim.process(buf));
  const double tp = stemy::analysis::measure_true_peak_linear(buf, 4);
  EXPECT_LE(lin_to_db(tp), -1.05 + 0.10);
}

TEST(LimiterIsp, StereoHotterChannel) {
  auto buf = make_stereo_frames(8192);
  const double sr = 48000.0;
  const double f = sr / 4.0;
  const double phase = kPi / 4.0;
  for (std::size_t n = 0; n < buf.frame_count(); ++n) {
    const float hot =
        static_cast<float>(std::sin(2.0 * kPi * f * static_cast<double>(n) / sr + phase));
    buf.at(n, 0) = hot * 0.5f;
    buf.at(n, 1) = hot;
  }
  stemy::dsp::Limiter lim;
  stemy::dsp::LimiterParams p;
  p.enabled = true;
  p.ceiling_dbfs = -1.0;
  p.true_peak_aware = true;
  p.lookahead_ms = 2.0;
  ASSERT_TRUE(lim.prepare(48000, p));
  ASSERT_TRUE(lim.process(buf));
  const double tp = stemy::analysis::measure_true_peak_linear(buf, 4);
  EXPECT_LE(lin_to_db(tp), -1.0 + 0.08);
}

TEST(LimiterIsp, SamplePeakOnlyMissesButIspCatches) {
  // Without TP-aware, sample-peak limiter may leave ISP above ceiling.
  auto make_isp = []() {
    auto buf = make_stereo_frames(4096);
    const double sr = 48000.0;
    const double f = sr / 4.0;
    const double phase = kPi / 4.0;
    for (std::size_t n = 0; n < buf.frame_count(); ++n) {
      const float s =
          static_cast<float>(std::sin(2.0 * kPi * f * static_cast<double>(n) / sr + phase));
      buf.at(n, 0) = s;
      buf.at(n, 1) = s;
    }
    return buf;
  };

  auto buf_sample = make_isp();
  stemy::dsp::Limiter lim_s;
  stemy::dsp::LimiterParams ps;
  ps.enabled = true;
  ps.ceiling_dbfs = -1.0;
  ps.true_peak_aware = false;
  ps.lookahead_ms = 2.0;
  ASSERT_TRUE(lim_s.prepare(48000, ps));
  ASSERT_TRUE(lim_s.process(buf_sample));
  const double tp_sample_mode = stemy::analysis::measure_true_peak_linear(buf_sample, 4);

  auto buf_isp = make_isp();
  stemy::dsp::Limiter lim_t;
  stemy::dsp::LimiterParams pt = ps;
  pt.true_peak_aware = true;
  ASSERT_TRUE(lim_t.prepare(48000, pt));
  ASSERT_TRUE(lim_t.process(buf_isp));
  const double tp_isp_mode = stemy::analysis::measure_true_peak_linear(buf_isp, 4);

  EXPECT_GT(lin_to_db(tp_sample_mode), -1.0 + 0.2);  // sample-only misses ISP
  EXPECT_LE(lin_to_db(tp_isp_mode), -1.0 + 0.08);
}
