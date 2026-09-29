#include "stemy/adaptive/adaptive_engine.h"
#include "stemy/analysis/analysis_result.h"

#include <gtest/gtest.h>
#include <string>

TEST(AdaptiveM2, QuietPathEnablesCharacterWithinCaps) {
  stemy::analysis::AnalysisResult a;
  a.sample_rate = 48000;
  a.channel_count = 2;
  a.integrated_loudness_lufs = -16.0;
  a.true_peak_dbtp = -3.0;
  a.crest_factor_db = 14.0;
  a.stereo_correlation = 0.97;
  a.stereo_width = 0.02;
  a.spectral_bands = stemy::analysis::SpectralBandEnergies{};
  a.spectral_bands->mid = 10.0;
  a.spectral_bands->high = 0.5;
  a.spectral_bands->high_mid = 0.5;

  auto genre = stemy::genres::hiphop::HipHopParameters::make_m2_tuning_defaults();
  stemy::adaptive::ProcessingDecision d;
  ASSERT_TRUE(stemy::adaptive::AdaptiveEngine().decide(a, genre, d));
  EXPECT_FALSE(d.compressor.enabled);
  EXPECT_TRUE(d.transient.enabled);
  EXPECT_LE(d.transient.attack_boost_db, 1.5);
  EXPECT_LE(d.transient.mix, 0.5);
  EXPECT_TRUE(d.saturation.enabled);
  EXPECT_LE(d.saturation.mix, 0.15);
  EXPECT_LE(d.stereo.width, 1.10);
}

TEST(AdaptiveM2, DenseCrestDisablesPunchAndSat) {
  stemy::analysis::AnalysisResult a;
  a.integrated_loudness_lufs = -9.2;
  a.true_peak_dbtp = -0.6;
  a.crest_factor_db = 7.5;
  a.stereo_correlation = 0.98;
  a.stereo_width = 0.01;

  auto genre = stemy::genres::hiphop::HipHopParameters::make_m2_tuning_defaults();
  stemy::adaptive::ProcessingDecision d;
  ASSERT_TRUE(stemy::adaptive::AdaptiveEngine().decide(a, genre, d));
  EXPECT_FALSE(d.transient.enabled);
  EXPECT_FALSE(d.saturation.enabled);
  EXPECT_FALSE(d.stereo.enabled);
  EXPECT_DOUBLE_EQ(d.stereo.width, 1.0);
}

TEST(AdaptiveM2Refinement, HotDenseForcesWidthBypass) {
  // Antonio-like: hot LUFS + dense crest + high corr / narrow width metric.
  stemy::analysis::AnalysisResult a;
  a.integrated_loudness_lufs = -5.8;
  a.true_peak_dbtp = -0.11;
  a.crest_factor_db = 8.5;
  a.stereo_correlation = 0.98;
  a.stereo_width = 0.01;
  a.spectral_bands = stemy::analysis::SpectralBandEnergies{};
  a.spectral_bands->mid = 10.0;
  a.spectral_bands->high = 1.0;
  a.spectral_bands->high_mid = 1.0;

  auto genre = stemy::genres::hiphop::HipHopParameters::make_m2_refinement_defaults();
  stemy::adaptive::ProcessingDecision d;
  ASSERT_TRUE(stemy::adaptive::AdaptiveEngine().decide(a, genre, d));
  EXPECT_LT(d.input_gain.gain_db, 0.0);
  EXPECT_FALSE(d.transient.enabled);
  EXPECT_FALSE(d.saturation.enabled);
  EXPECT_FALSE(d.stereo.enabled);
  EXPECT_DOUBLE_EQ(d.stereo.width, 1.0);
  bool saw_bypass = false;
  for (const auto& n : d.decision_notes) {
    if (n.find("hot_dense_bypass") != std::string::npos) saw_bypass = true;
  }
  EXPECT_TRUE(saw_bypass);
}

TEST(AdaptiveM2Refinement, QuietOpenNarrowStillGetsWidth) {
  stemy::analysis::AnalysisResult a;
  a.integrated_loudness_lufs = -18.0;
  a.true_peak_dbtp = -3.0;
  a.crest_factor_db = 16.0;
  a.stereo_correlation = 0.97;
  a.stereo_width = 0.02;
  a.spectral_bands = stemy::analysis::SpectralBandEnergies{};
  a.spectral_bands->mid = 10.0;
  a.spectral_bands->high = 0.5;
  a.spectral_bands->high_mid = 0.5;
  a.spectral_bands->sub_bass = 100.0;
  a.spectral_bands->bass = 400.0;

  auto genre = stemy::genres::hiphop::HipHopParameters::make_m2_refinement_defaults();
  stemy::adaptive::ProcessingDecision d;
  ASSERT_TRUE(stemy::adaptive::AdaptiveEngine().decide(a, genre, d));
  EXPECT_TRUE(d.stereo.enabled);
  EXPECT_NEAR(d.stereo.width, 1.06, 1e-9);
}

TEST(AdaptiveM2Refinement, QuietAlreadyWideBypassesWidth) {
  stemy::analysis::AnalysisResult a;
  a.integrated_loudness_lufs = -18.0;
  a.true_peak_dbtp = -3.0;
  a.crest_factor_db = 16.0;
  a.stereo_correlation = 0.80;  // below wide-corr threshold
  a.stereo_width = 0.08;
  a.spectral_bands = stemy::analysis::SpectralBandEnergies{};
  a.spectral_bands->mid = 10.0;

  auto genre = stemy::genres::hiphop::HipHopParameters::make_m2_refinement_defaults();
  stemy::adaptive::ProcessingDecision d;
  ASSERT_TRUE(stemy::adaptive::AdaptiveEngine().decide(a, genre, d));
  EXPECT_FALSE(d.stereo.enabled);
  EXPECT_DOUBLE_EQ(d.stereo.width, 1.0);
}

TEST(AdaptiveM2Refinement, HotOnlyStillBypassesWidth) {
  // Hot but not dense: still force width bypass.
  stemy::analysis::AnalysisResult a;
  a.integrated_loudness_lufs = -6.0;
  a.true_peak_dbtp = -0.5;
  a.crest_factor_db = 12.0;
  a.stereo_correlation = 0.98;
  a.stereo_width = 0.01;

  auto genre = stemy::genres::hiphop::HipHopParameters::make_m2_refinement_defaults();
  stemy::adaptive::ProcessingDecision d;
  ASSERT_TRUE(stemy::adaptive::AdaptiveEngine().decide(a, genre, d));
  EXPECT_FALSE(d.stereo.enabled);
  EXPECT_DOUBLE_EQ(d.stereo.width, 1.0);
}

TEST(AdaptiveFinalR0, ExtremeSubTriggersOnDaMindLikeBass) {
  stemy::analysis::AnalysisResult a;
  a.integrated_loudness_lufs = -20.65;
  a.true_peak_dbtp = -2.94;
  a.crest_factor_db = 18.5;
  a.stereo_correlation = 0.94;
  a.stereo_width = 0.03;
  a.spectral_bands = stemy::analysis::SpectralBandEnergies{};
  // bass_body ≈ 0.825
  a.spectral_bands->sub_bass = 0.50;
  a.spectral_bands->bass = 0.325;
  a.spectral_bands->low_mid = 0.05;
  a.spectral_bands->mid = 0.05;
  a.spectral_bands->high_mid = 0.04;
  a.spectral_bands->high = 0.035;

  auto genre = stemy::genres::hiphop::HipHopParameters::make_final_r0_defaults();
  stemy::adaptive::ProcessingDecision d;
  ASSERT_TRUE(stemy::adaptive::AdaptiveEngine().decide(a, genre, d));
  EXPECT_TRUE(d.extreme_sub_guard_enabled);
  EXPECT_LT(d.extreme_sub_guard_gain_db, -0.2);
  EXPECT_GE(d.extreme_sub_guard_gain_db, -1.0);
  EXPECT_TRUE(d.low_end.enabled);
  EXPECT_TRUE(d.low_end.shelf_enabled);
  EXPECT_GE(d.low_end.shelf_gain_db, -3.0);
  EXPECT_TRUE(d.limiter.true_peak_aware);
  EXPECT_DOUBLE_EQ(genre.target_true_peak_dbtp, -1.05);
}

TEST(AdaptiveFinalR0, ExtremeSubSkipsBumpmanLikeBass) {
  stemy::analysis::AnalysisResult a;
  a.integrated_loudness_lufs = -21.6;
  a.true_peak_dbtp = -5.0;
  a.crest_factor_db = 19.0;
  a.stereo_correlation = 0.84;
  a.stereo_width = 0.08;
  a.spectral_bands = stemy::analysis::SpectralBandEnergies{};
  // bass_body ≈ 0.54
  a.spectral_bands->sub_bass = 0.30;
  a.spectral_bands->bass = 0.24;
  a.spectral_bands->low_mid = 0.11;
  a.spectral_bands->mid = 0.22;
  a.spectral_bands->high_mid = 0.09;
  a.spectral_bands->high = 0.04;

  auto genre = stemy::genres::hiphop::HipHopParameters::make_final_r0_defaults();
  stemy::adaptive::ProcessingDecision d;
  ASSERT_TRUE(stemy::adaptive::AdaptiveEngine().decide(a, genre, d));
  EXPECT_FALSE(d.extreme_sub_guard_enabled);
  EXPECT_NEAR(d.bass_body_share, 0.54, 0.02);
}

TEST(AdaptiveFinalR0, ExtremeSubSkipsAntonioLikeBass) {
  stemy::analysis::AnalysisResult a;
  a.integrated_loudness_lufs = -5.8;
  a.true_peak_dbtp = 1.13;
  a.crest_factor_db = 8.5;
  a.stereo_correlation = 0.96;
  a.stereo_width = 0.02;
  a.spectral_bands = stemy::analysis::SpectralBandEnergies{};
  a.spectral_bands->sub_bass = 0.20;
  a.spectral_bands->bass = 0.25;
  a.spectral_bands->low_mid = 0.15;
  a.spectral_bands->mid = 0.25;
  a.spectral_bands->high_mid = 0.10;
  a.spectral_bands->high = 0.05;

  auto genre = stemy::genres::hiphop::HipHopParameters::make_final_r0_defaults();
  stemy::adaptive::ProcessingDecision d;
  ASSERT_TRUE(stemy::adaptive::AdaptiveEngine().decide(a, genre, d));
  EXPECT_FALSE(d.extreme_sub_guard_enabled);
  EXPECT_FALSE(d.stereo.enabled);
  EXPECT_DOUBLE_EQ(d.stereo.width, 1.0);
  EXPECT_LT(d.input_gain.gain_db, 0.0);
}

TEST(AdaptiveM2, QuietNearCeilingStillLiftsWithCrestCap) {
  stemy::analysis::AnalysisResult a;
  a.integrated_loudness_lufs = -14.5;
  a.true_peak_dbtp = -0.6;
  a.crest_factor_db = 14.0;
  a.stereo_correlation = 0.97;
  a.stereo_width = 0.02;

  auto genre = stemy::genres::hiphop::HipHopParameters::make_m2_tuning_defaults();
  stemy::adaptive::ProcessingDecision d;
  ASSERT_TRUE(stemy::adaptive::AdaptiveEngine().decide(a, genre, d));
  // M1 baseline toward −9 is 5.5 dB; M2 must not undercut that.
  EXPECT_NEAR(d.m1_baseline_gain_db, 5.5, 0.01);
  EXPECT_GE(d.input_gain.gain_db, d.m1_baseline_gain_db - 1e-9);
}

TEST(AdaptiveM2, QuietPathUsesM1GainAsFloor) {
  stemy::analysis::AnalysisResult a;
  a.integrated_loudness_lufs = -18.8;
  a.true_peak_dbtp = -2.0;
  a.crest_factor_db = 19.0;
  a.stereo_correlation = 0.97;
  a.stereo_width = 0.02;

  auto genre = stemy::genres::hiphop::HipHopParameters::make_m2_tuning_defaults();
  stemy::adaptive::ProcessingDecision d;
  ASSERT_TRUE(stemy::adaptive::AdaptiveEngine().decide(a, genre, d));
  EXPECT_NEAR(d.m1_baseline_gain_db, 9.8, 0.01);
  EXPECT_GE(d.input_gain.gain_db, d.m1_baseline_gain_db - 1e-9);
  EXPECT_TRUE(d.safety_override_reason.empty());
}

TEST(AdaptiveM2, LoudnessBandHold) {
  stemy::analysis::AnalysisResult a;
  a.integrated_loudness_lufs = -8.7;
  a.true_peak_dbtp = -1.0;
  a.crest_factor_db = 11.0;
  a.stereo_correlation = 0.96;
  a.stereo_width = 0.02;

  auto genre = stemy::genres::hiphop::HipHopParameters::make_m2_tuning_defaults();
  stemy::adaptive::ProcessingDecision d;
  ASSERT_TRUE(stemy::adaptive::AdaptiveEngine().decide(a, genre, d));
  EXPECT_DOUBLE_EQ(d.input_gain.gain_db, 0.0);
}

TEST(AdaptiveM1, StillTransparentCharacter) {
  stemy::analysis::AnalysisResult a;
  a.integrated_loudness_lufs = -9.0;
  a.true_peak_dbtp = -0.6;
  a.crest_factor_db = 10.2;

  auto genre = stemy::genres::hiphop::HipHopParameters::make_provisional_defaults();
  stemy::adaptive::ProcessingDecision d;
  ASSERT_TRUE(stemy::adaptive::AdaptiveEngine().decide(a, genre, d));
  EXPECT_FALSE(d.transient.enabled);
  EXPECT_FALSE(d.saturation.enabled);
  EXPECT_FALSE(d.hf_eq.enabled);
  EXPECT_FALSE(d.low_weight_eq.enabled);
  EXPECT_DOUBLE_EQ(d.stereo.width, 1.0);
}

TEST(AdaptiveM2Refinement, SparseBassEnablesLowWeight) {
  stemy::analysis::AnalysisResult a;
  a.integrated_loudness_lufs = -18.0;
  a.true_peak_dbtp = -2.0;
  a.crest_factor_db = 18.0;
  a.stereo_correlation = 0.96;
  a.stereo_width = 0.02;
  a.spectral_bands = stemy::analysis::SpectralBandEnergies{};
  a.spectral_bands->sub_bass = 100.0;
  a.spectral_bands->bass = 200.0;
  a.spectral_bands->low_mid = 800.0;
  a.spectral_bands->mid = 1500.0;
  a.spectral_bands->high_mid = 400.0;
  a.spectral_bands->high = 300.0;

  auto genre = stemy::genres::hiphop::HipHopParameters::make_m2_refinement_defaults();
  stemy::adaptive::ProcessingDecision d;
  ASSERT_TRUE(stemy::adaptive::AdaptiveEngine().decide(a, genre, d));
  EXPECT_TRUE(d.low_weight_eq.enabled);
  EXPECT_GE(d.low_weight_eq.bands[0].gain_db, 0.05);
  EXPECT_DOUBLE_EQ(genre.target_true_peak_dbtp, -1.0);
}

TEST(AdaptiveM2Refinement, OpenPremixGetsExtraAboveM1) {
  stemy::analysis::AnalysisResult a;
  a.integrated_loudness_lufs = -14.5;
  a.true_peak_dbtp = -0.6;
  a.crest_factor_db = 14.5;
  a.stereo_correlation = 0.97;
  a.stereo_width = 0.02;

  auto genre = stemy::genres::hiphop::HipHopParameters::make_m2_refinement_defaults();
  stemy::adaptive::ProcessingDecision d;
  ASSERT_TRUE(stemy::adaptive::AdaptiveEngine().decide(a, genre, d));
  EXPECT_NEAR(d.m1_baseline_gain_db, 5.5, 0.01);
  EXPECT_GT(d.input_gain.gain_db, d.m1_baseline_gain_db + 0.5);
}
