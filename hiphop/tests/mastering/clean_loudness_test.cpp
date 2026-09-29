#include "stemy/mastering/clean_loudness_refinement.h"
#include "stemy/adaptive/adaptive_engine.h"
#include "stemy/config/config_loader.h"

#include <filesystem>
#include <gtest/gtest.h>

using stemy::mastering::clean_loudness_eligible;
using stemy::mastering::clean_loudness_stress_ok;
using stemy::mastering::extreme_sub_compensation_db;
using stemy::mastering::CleanLoudnessStressLimits;

namespace {

stemy::genres::hiphop::HipHopParameters r1() {
  return stemy::genres::hiphop::HipHopParameters::make_final_r1_defaults();
}

stemy::analysis::AnalysisResult quiet_open() {
  stemy::analysis::AnalysisResult a;
  a.integrated_loudness_lufs = -21.6;
  a.crest_factor_db = 19.0;
  a.true_peak_dbtp = -5.0;
  return a;
}

stemy::analysis::AnalysisResult hot_dense() {
  stemy::analysis::AnalysisResult a;
  a.integrated_loudness_lufs = -5.8;
  a.crest_factor_db = 8.5;
  a.true_peak_dbtp = 1.1;
  return a;
}

}  // namespace

TEST(CleanLoudness, OpenPremasterEligible) {
  EXPECT_TRUE(clean_loudness_eligible(quiet_open(), r1()));
}

TEST(CleanLoudness, HotDenseNotEligible) {
  EXPECT_FALSE(clean_loudness_eligible(hot_dense(), r1()));
}

TEST(CleanLoudness, HotOnlyNotEligible) {
  auto a = quiet_open();
  a.integrated_loudness_lufs = -6.0;
  a.crest_factor_db = 14.0;
  EXPECT_FALSE(clean_loudness_eligible(a, r1()));
}

TEST(CleanLoudness, DenseOnlyNotEligible) {
  auto a = quiet_open();
  a.crest_factor_db = 8.0;
  EXPECT_FALSE(clean_loudness_eligible(a, r1()));
}

TEST(CleanLoudness, DisabledFlag) {
  auto p = r1();
  p.r1_enable_clean_loudness_refinement = false;
  EXPECT_FALSE(clean_loudness_eligible(quiet_open(), p));
}

TEST(CleanLoudness, StressOkBaseline) {
  stemy::dsp::SafetyReport s;
  s.limiter_max_gr_db = 8.0f;
  s.limiter_p95_gr_while_active_db = 3.0f;
  s.limiter_avg_gr_while_active_db = 0.7f;
  s.limiter_pct_time_gr_gt_6db = 0.2f;
  stemy::analysis::AnalysisResult o;
  o.crest_factor_db = 11.5;
  o.true_peak_dbtp = -1.3;
  std::string why;
  EXPECT_TRUE(clean_loudness_stress_ok(s, o, r1(), CleanLoudnessStressLimits{}, &why));
}

TEST(CleanLoudness, StressFailsOnP95) {
  stemy::dsp::SafetyReport s;
  s.limiter_max_gr_db = 8.0f;
  s.limiter_p95_gr_while_active_db = 8.0f;
  s.limiter_avg_gr_while_active_db = 0.7f;
  stemy::analysis::AnalysisResult o;
  o.crest_factor_db = 11.5;
  o.true_peak_dbtp = -1.3;
  std::string why;
  EXPECT_FALSE(clean_loudness_stress_ok(s, o, r1(), CleanLoudnessStressLimits{}, &why));
  EXPECT_EQ(why, "p95_gr");
}

TEST(CleanLoudness, ExtremeSubCompensationBounded) {
  auto p = r1();
  stemy::adaptive::ProcessingDecision d;
  d.extreme_sub_guard_enabled = true;
  d.extreme_sub_guard_gain_db = -0.9;
  EXPECT_NEAR(extreme_sub_compensation_db(d, p), 0.45, 1e-9);
  d.extreme_sub_guard_gain_db = -1.2;
  EXPECT_NEAR(extreme_sub_compensation_db(d, p), 0.5, 1e-9);  // capped
  d.extreme_sub_guard_enabled = false;
  EXPECT_DOUBLE_EQ(extreme_sub_compensation_db(d, p), 0.0);
}

TEST(AdaptiveFinalR1, HotDenseDoesNotEnableRefinementFlagInDecide) {
  // Decision-time character must still bypass width; refinement is mastering-stage.
  stemy::analysis::AnalysisResult a = hot_dense();
  a.stereo_correlation = 0.98;
  a.stereo_width = 0.01;
  a.spectral_bands = stemy::analysis::SpectralBandEnergies{};
  a.spectral_bands->sub_bass = 0.2;
  a.spectral_bands->bass = 0.25;
  a.spectral_bands->mid = 0.3;
  a.spectral_bands->high = 0.1;
  a.spectral_bands->high_mid = 0.1;
  a.spectral_bands->low_mid = 0.05;

  auto genre = r1();
  stemy::adaptive::ProcessingDecision d;
  ASSERT_TRUE(stemy::adaptive::AdaptiveEngine().decide(a, genre, d));
  EXPECT_FALSE(d.stereo.enabled);
  EXPECT_DOUBLE_EQ(d.stereo.width, 1.0);
  EXPECT_LT(d.input_gain.gain_db, 0.0);
  EXPECT_FALSE(clean_loudness_eligible(a, genre));
}

TEST(AdaptiveFinalR1, R0ConfigDoesNotEnableCleanLoudness) {
  auto p = stemy::genres::hiphop::HipHopParameters::make_final_r0_defaults();
  EXPECT_FALSE(p.r1_enable_clean_loudness_refinement);
  EXPECT_FALSE(clean_loudness_eligible(quiet_open(), p));
}

TEST(FinalApprovedConfig, LoadsR1EquivalentCleanLoudness) {
  stemy::genres::hiphop::HipHopParameters p;
  const auto path =
      std::filesystem::path(STEMY_REPO_ROOT) / "config" / "hiphop" / "final_approved.json";
  ASSERT_TRUE(stemy::config::load_hiphop_config(path, p)) << path;
  EXPECT_EQ(p.version, "hiphop-final-approved");
  EXPECT_TRUE(p.is_final_r1());
  EXPECT_TRUE(p.r1_enable_clean_loudness_refinement);
  EXPECT_TRUE(p.limiter_true_peak_aware);
  EXPECT_TRUE(p.r0_enable_extreme_sub_guard);
  EXPECT_DOUBLE_EQ(p.target_true_peak_dbtp, -1.05);
  EXPECT_DOUBLE_EQ(p.r1_clean_loudness_stop_lufs, -10.0);
  EXPECT_DOUBLE_EQ(p.r1_clean_loudness_max_extra_db, 1.5);
  EXPECT_TRUE(clean_loudness_eligible(quiet_open(), p));
  EXPECT_FALSE(clean_loudness_eligible(hot_dense(), p));

  stemy::genres::hiphop::HipHopParameters r1_file;
  const auto r1_path =
      std::filesystem::path(STEMY_REPO_ROOT) / "config" / "hiphop" / "final_r1.json";
  ASSERT_TRUE(stemy::config::load_hiphop_config(r1_path, r1_file));
  EXPECT_EQ(p.r1_enable_clean_loudness_refinement, r1_file.r1_enable_clean_loudness_refinement);
  EXPECT_DOUBLE_EQ(p.m2_punch_mix, r1_file.m2_punch_mix);
  EXPECT_DOUBLE_EQ(p.m2_sat_mix, r1_file.m2_sat_mix);
  EXPECT_DOUBLE_EQ(p.m2_hf_gain_db, r1_file.m2_hf_gain_db);
  EXPECT_DOUBLE_EQ(p.m2_stereo_width, r1_file.m2_stereo_width);
  EXPECT_DOUBLE_EQ(p.r1_stress_max_p95_gr_db, r1_file.r1_stress_max_p95_gr_db);
}

TEST(FinalR2Config, ExpandsCleanLoudnessBudgetOnly) {
  auto r2 = stemy::genres::hiphop::HipHopParameters::make_final_r2_defaults();
  EXPECT_TRUE(r2.is_final_r2());
  EXPECT_FALSE(r2.is_final_r1());
  EXPECT_GT(r2.r1_clean_loudness_max_extra_db, 1.5);
  EXPECT_DOUBLE_EQ(r2.r1_clean_loudness_max_extra_db, 5.0);
  EXPECT_EQ(r2.r1_clean_loudness_max_steps, 20);
  EXPECT_DOUBLE_EQ(r2.r1_clean_loudness_stop_lufs, -9.0);
  // Character / TP unchanged vs R1.
  auto r1 = stemy::genres::hiphop::HipHopParameters::make_final_r1_defaults();
  EXPECT_DOUBLE_EQ(r2.target_true_peak_dbtp, r1.target_true_peak_dbtp);
  EXPECT_DOUBLE_EQ(r2.m2_punch_mix, r1.m2_punch_mix);
  EXPECT_DOUBLE_EQ(r2.m2_sat_mix, r1.m2_sat_mix);
  EXPECT_DOUBLE_EQ(r2.m2_hf_gain_db, r1.m2_hf_gain_db);
  EXPECT_DOUBLE_EQ(r2.r1_stress_max_p95_gr_db, r1.r1_stress_max_p95_gr_db);
  EXPECT_TRUE(clean_loudness_eligible(quiet_open(), r2));
  EXPECT_FALSE(clean_loudness_eligible(hot_dense(), r2));

  // Production approved must remain at R1 budget.
  stemy::genres::hiphop::HipHopParameters approved;
  const auto ap =
      std::filesystem::path(STEMY_REPO_ROOT) / "config" / "hiphop" / "final_approved.json";
  ASSERT_TRUE(stemy::config::load_hiphop_config(ap, approved));
  EXPECT_DOUBLE_EQ(approved.r1_clean_loudness_max_extra_db, 1.5);
  EXPECT_DOUBLE_EQ(approved.r1_clean_loudness_stop_lufs, -10.0);
}

TEST(FinalV2Config, PromotesR2cBWithoutChangingApproved) {
  auto v2 = stemy::genres::hiphop::HipHopParameters::make_final_v2_defaults();
  auto r1 = stemy::genres::hiphop::HipHopParameters::make_final_r1_defaults();
  EXPECT_TRUE(v2.is_final_v2());
  EXPECT_FALSE(v2.is_final_r1());
  EXPECT_FALSE(v2.is_final_r2());
  EXPECT_DOUBLE_EQ(v2.r1_clean_loudness_max_extra_db, 7.0);
  EXPECT_EQ(v2.r1_clean_loudness_max_steps, 28);
  EXPECT_DOUBLE_EQ(v2.r1_clean_loudness_stop_lufs, -9.0);
  EXPECT_TRUE(v2.r2b_enable_open_density);
  EXPECT_DOUBLE_EQ(v2.r2b_open_sat_drive, 1.5);
  EXPECT_DOUBLE_EQ(v2.r2b_open_sat_mix, 0.22);
  EXPECT_TRUE(v2.r2c_enable_soft_peak_round);
  EXPECT_DOUBLE_EQ(v2.r2c_soft_peak_ceiling_dbfs, 5.5);
  EXPECT_DOUBLE_EQ(v2.r2c_soft_peak_drive, 1.4);
  EXPECT_DOUBLE_EQ(v2.target_true_peak_dbtp, r1.target_true_peak_dbtp);
  EXPECT_DOUBLE_EQ(v2.r1_stress_max_avg_gr_db, r1.r1_stress_max_avg_gr_db);
  EXPECT_DOUBLE_EQ(v2.r1_stress_max_p95_gr_db, r1.r1_stress_max_p95_gr_db);
  EXPECT_DOUBLE_EQ(v2.r1_stress_max_pct_gt_6db, r1.r1_stress_max_pct_gt_6db);
  EXPECT_DOUBLE_EQ(v2.r1_stress_max_gr_db, r1.r1_stress_max_gr_db);
  EXPECT_DOUBLE_EQ(v2.r1_stress_min_crest_db, r1.r1_stress_min_crest_db);

  stemy::genres::hiphop::HipHopParameters loaded;
  const auto path =
      std::filesystem::path(STEMY_REPO_ROOT) / "config" / "hiphop" / "final_v2.json";
  ASSERT_TRUE(stemy::config::load_hiphop_config(path, loaded)) << path;
  EXPECT_EQ(loaded.version, "hiphop-final-v2");
  EXPECT_DOUBLE_EQ(loaded.r1_clean_loudness_max_extra_db, 7.0);
  EXPECT_DOUBLE_EQ(loaded.r2c_soft_peak_ceiling_dbfs, 5.5);
  EXPECT_DOUBLE_EQ(loaded.target_true_peak_dbtp, -1.05);

  stemy::adaptive::ProcessingDecision d_open;
  ASSERT_TRUE(stemy::adaptive::AdaptiveEngine().decide(quiet_open(), loaded, d_open));
  EXPECT_TRUE(d_open.soft_peak_round.enabled);
  EXPECT_NEAR(d_open.soft_peak_round.ceiling_dbfs, 5.5, 1e-9);
  EXPECT_NEAR(d_open.saturation.drive, 1.5, 1e-9);
  EXPECT_NEAR(d_open.saturation.mix, 0.22, 1e-9);
  EXPECT_TRUE(clean_loudness_eligible(quiet_open(), loaded));

  stemy::adaptive::ProcessingDecision d_hot;
  ASSERT_TRUE(stemy::adaptive::AdaptiveEngine().decide(hot_dense(), loaded, d_hot));
  EXPECT_FALSE(d_hot.soft_peak_round.enabled);
  EXPECT_FALSE(d_hot.saturation.enabled);
  EXPECT_FALSE(clean_loudness_eligible(hot_dense(), loaded));

  stemy::genres::hiphop::HipHopParameters approved;
  const auto ap =
      std::filesystem::path(STEMY_REPO_ROOT) / "config" / "hiphop" / "final_approved.json";
  ASSERT_TRUE(stemy::config::load_hiphop_config(ap, approved));
  EXPECT_FALSE(approved.is_final_v2());
  EXPECT_FALSE(approved.r2b_enable_open_density);
  EXPECT_FALSE(approved.r2c_enable_soft_peak_round);
  EXPECT_DOUBLE_EQ(approved.r1_clean_loudness_max_extra_db, 1.5);
  EXPECT_DOUBLE_EQ(approved.r1_clean_loudness_stop_lufs, -10.0);
  EXPECT_DOUBLE_EQ(approved.target_true_peak_dbtp, -1.05);
}
