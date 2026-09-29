#include "stemy/adaptive/adaptive_engine.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace stemy::adaptive {
namespace {

double clamp_value(double v, double lo, double hi) {
  return std::max(lo, std::min(hi, v));
}

void apply_master01_quiet_tonal_trim(ProcessingDecision& out) {
  out.low_end.enabled = true;
  out.low_end.highpass_enabled = false;
  out.low_end.shelf_enabled = true;
  out.low_end.shelf_freq_hz = 55.0;
  out.low_end.shelf_q = 0.707;
  out.low_end.shelf_gain_db = -2.0;
  if (!out.low_end.bass_mono_enabled) {
    out.low_end.bass_mono_enabled = false;
  }

  out.eq.enabled = true;
  out.eq.bands[0].enabled = true;
  out.eq.bands[0].type = dsp::EqBandType::Peaking;
  out.eq.bands[0].freq_hz = 100.0;
  out.eq.bands[0].q = 0.8;
  out.eq.bands[0].gain_db = 1.5;

  out.eq.bands[1].enabled = true;
  out.eq.bands[1].type = dsp::EqBandType::Peaking;
  out.eq.bands[1].freq_hz = 350.0;
  out.eq.bands[1].q = 0.9;
  out.eq.bands[1].gain_db = 1.5;

  out.eq.bands[2].enabled = true;
  out.eq.bands[2].type = dsp::EqBandType::HighShelf;
  out.eq.bands[2].freq_hz = 8000.0;
  out.eq.bands[2].q = 0.707;
  out.eq.bands[2].gain_db = -1.5;

  out.decision_notes.push_back(
      "tonal: PROVISIONAL quiet-path trim (sub shelf -2 dB @55, +1.5 @100/+1.5 @350, HS -1.5 @8k)");
}

double high_energy_share(const analysis::AnalysisResult& analysis) {
  if (!analysis.spectral_bands) {
    return 0.0;
  }
  const auto& b = *analysis.spectral_bands;
  const double tot =
      b.sub_bass + b.bass + b.low_mid + b.mid + b.high_mid + b.high;
  if (tot <= 0.0) {
    return 0.0;
  }
  return (b.high_mid + b.high) / tot;
}

double bass_body_share(const analysis::AnalysisResult& analysis) {
  if (!analysis.spectral_bands) {
    return 1.0;  // unknown → do not add weight
  }
  const auto& b = *analysis.spectral_bands;
  const double tot =
      b.sub_bass + b.bass + b.low_mid + b.mid + b.high_mid + b.high;
  if (tot <= 0.0) {
    return 1.0;
  }
  return (b.sub_bass + b.bass) / tot;
}

double smoothstep01(double t) {
  t = clamp_value(t, 0.0, 1.0);
  return t * t * (3.0 - 2.0 * t);
}

/// Conservative subtractive sub shelf for objectively extreme bass-body dominance.
void apply_extreme_sub_guard(const analysis::AnalysisResult& analysis,
                             const genres::hiphop::HipHopParameters& p,
                             ProcessingDecision& out) {
  out.extreme_sub_guard_enabled = false;
  out.extreme_sub_guard_gain_db = 0.0;
  out.extreme_sub_guard_freq_hz = 0.0;
  out.extreme_sub_guard_reason.clear();
  out.bass_body_share = bass_body_share(analysis);

  if (!p.r0_enable_extreme_sub_guard) {
    return;
  }
  if (!analysis.spectral_bands) {
    out.extreme_sub_guard_reason = "skipped_no_spectral_bands";
    return;
  }

  const double share = out.bass_body_share;
  const double thr = p.r0_extreme_bass_threshold;
  const double full = std::max(thr + 1e-6, p.r0_extreme_bass_full_scale);
  if (share <= thr) {
    out.extreme_sub_guard_reason = "below_extreme_bass_threshold";
    return;
  }

  const double t = smoothstep01((share - thr) / (full - thr));
  const double extra =
      clamp_value(p.r0_extreme_sub_max_atten_db * t, p.r0_extreme_sub_max_atten_db, 0.0);
  if (extra > -0.05) {
    out.extreme_sub_guard_reason = "ramp_negligible";
    return;
  }

  const double freq = clamp_value(p.r0_extreme_sub_freq_hz, 55.0, 70.0);
  double combined = extra;
  if (out.low_end.enabled && out.low_end.shelf_enabled) {
    combined = out.low_end.shelf_gain_db + extra;
  }
  combined = std::max(combined, p.r0_extreme_sub_total_min_db);

  out.low_end.enabled = true;
  out.low_end.shelf_enabled = true;
  out.low_end.shelf_freq_hz = freq;
  out.low_end.shelf_q = 0.707;
  out.low_end.shelf_gain_db = combined;

  out.extreme_sub_guard_enabled = true;
  out.extreme_sub_guard_gain_db = extra;
  out.extreme_sub_guard_freq_hz = freq;
  {
    std::ostringstream oss;
    oss << "extreme_sub_guard: bass_body=" << share << " extra=" << extra
        << " dB @ " << freq << " Hz (shelf total " << combined << " dB)";
    out.extreme_sub_guard_reason = oss.str();
    out.decision_notes.push_back(out.extreme_sub_guard_reason);
  }
}

/// Frozen M1 loudness decision (MASTER_01 pocket toward −9 ±0.75).
double compute_m1_baseline_gain_db(const analysis::AnalysisResult& analysis,
                                   double max_lift_db,
                                   double max_reduction_db) {
  if (!analysis.integrated_loudness_lufs) {
    return 0.0;
  }
  constexpr double kM1Target = -9.0;
  constexpr double kM1Tol = 0.75;
  const double lufs = *analysis.integrated_loudness_lufs;
  const double delta = kM1Target - lufs;
  if (std::fabs(delta) <= kM1Tol) {
    return 0.0;
  }
  if (delta > 0.0) {
    return clamp_value(delta, 0.0, max_lift_db);
  }
  return clamp_value(delta, -max_reduction_db, 0.0);
}

double estimate_limiter_push_db(double true_peak_dbtp,
                                double gain_db,
                                double character_peak_add_db,
                                double ceiling_dbtp) {
  const double peak_est = true_peak_dbtp + gain_db + character_peak_add_db;
  return std::max(0.0, peak_est - ceiling_dbtp);
}

double estimate_character_peak_add_db(const ProcessingDecision& d) {
  double add = 0.0;
  if (d.transient.enabled) {
    // Conservative: peak lift ≈ boost * mix (matches ~0.3–0.4 dB seen in audit).
    add += d.transient.attack_boost_db * d.transient.mix;
  }
  if (d.hf_eq.enabled) {
    add += std::max(0.0, d.hf_eq.bands[0].gain_db) * 0.25;
  }
  if (d.low_weight_eq.enabled) {
    add += std::max(0.0, d.low_weight_eq.bands[0].gain_db) * 0.15;
  }
  if (d.stereo.enabled && d.stereo.width > 1.0) {
    add += (d.stereo.width - 1.0) * 2.0;  // tiny; width 1.06 → ~0.12 dB
  }
  return add;
}

void scale_punch(ProcessingDecision& out, double scale) {
  if (!out.transient.enabled) {
    return;
  }
  out.transient.attack_boost_db *= scale;
  out.transient.sustain_cut_db *= scale;
  out.transient.mix *= scale;
  if (out.transient.mix < 0.02 || out.transient.attack_boost_db < 0.05) {
    out.transient = {};
    out.decision_notes.push_back("m2 stress: punch bypassed");
  } else {
    std::ostringstream oss;
    oss << "m2 stress: punch scaled x" << scale << " → boost=" << out.transient.attack_boost_db
        << " mix=" << out.transient.mix;
    out.decision_notes.push_back(oss.str());
  }
}

void scale_low_weight(ProcessingDecision& out, double scale) {
  if (!out.low_weight_eq.enabled) {
    return;
  }
  out.low_weight_eq.bands[0].gain_db *= scale;
  if (out.low_weight_eq.bands[0].gain_db < 0.05) {
    out.low_weight_eq = {};
    out.low_weight_eq.enabled = false;
    out.decision_notes.push_back("m2 stress: low-end weight bypassed");
  } else {
    std::ostringstream oss;
    oss << "m2 stress: low weight scaled x" << scale << " → +"
        << out.low_weight_eq.bands[0].gain_db << " dB";
    out.decision_notes.push_back(oss.str());
  }
}

void scale_saturation(ProcessingDecision& out, double scale) {
  if (!out.saturation.enabled) {
    return;
  }
  out.saturation.drive *= scale;
  out.saturation.mix *= scale;
  if (out.saturation.mix < 0.02 || out.saturation.drive < 0.05) {
    out.saturation = {};
    out.decision_notes.push_back("m2 stress: saturation bypassed");
  } else {
    std::ostringstream oss;
    oss << "m2 stress: sat scaled x" << scale << " → drive=" << out.saturation.drive
        << " mix=" << out.saturation.mix;
    out.decision_notes.push_back(oss.str());
  }
}

void scale_hf(ProcessingDecision& out, double scale) {
  if (!out.hf_eq.enabled) {
    return;
  }
  out.hf_eq.bands[0].gain_db *= scale;
  if (out.hf_eq.bands[0].gain_db < 0.05) {
    out.hf_eq = {};
    out.hf_eq.enabled = false;
    out.decision_notes.push_back("m2 stress: HF bypassed");
  } else {
    std::ostringstream oss;
    oss << "m2 stress: HF scaled x" << scale << " → +" << out.hf_eq.bands[0].gain_db << " dB";
    out.decision_notes.push_back(oss.str());
  }
}

void scale_stereo(ProcessingDecision& out, double scale) {
  if (!out.stereo.enabled) {
    return;
  }
  const double w = 1.0 + (out.stereo.width - 1.0) * scale;
  if (w <= 1.001) {
    out.stereo.enabled = false;
    out.stereo.width = 1.0;
    out.stereo.side_hpf_hz = 0.0;
    out.decision_notes.push_back("m2 stress: stereo width bypassed");
  } else {
    out.stereo.width = w;
    std::ostringstream oss;
    oss << "m2 stress: stereo scaled x" << scale << " → width=" << w;
    out.decision_notes.push_back(oss.str());
  }
}

/// If M1-gain + M2 character would push the limiter materially harder than M1 alone,
/// reduce character in priority order before cutting loudness.
void relieve_character_limiter_stress(const analysis::AnalysisResult& analysis,
                                      const genres::hiphop::HipHopParameters& p,
                                      double gain_db,
                                      bool quiet_tonal,
                                      ProcessingDecision& out) {
  const double tp = analysis.true_peak_dbtp.value_or(-1.0);
  const double ceiling = p.target_true_peak_dbtp;
  const double tonal_peak = quiet_tonal ? 1.6 : 0.0;  // quiet-path trim peak lift (audit)
  const double m1_push =
      estimate_limiter_push_db(tp, gain_db, tonal_peak, ceiling);
  constexpr double kMaterialExtraDb = 1.5;

  auto m2_push = [&]() {
    return estimate_limiter_push_db(tp, gain_db, tonal_peak + estimate_character_peak_add_db(out),
                                    ceiling);
  };

  if (m2_push() <= m1_push + kMaterialExtraDb) {
    return;
  }
  out.decision_notes.push_back(
      "m2 stress: character + M1 baseline gain exceeds M1 limiter stress — reducing character first");

  // Priority: punch → sat → low weight → HF → stereo
  for (double scale : {0.5, 0.0}) {
    if (m2_push() <= m1_push + kMaterialExtraDb) break;
    if (out.transient.enabled) scale_punch(out, scale);
  }
  for (double scale : {0.5, 0.0}) {
    if (m2_push() <= m1_push + kMaterialExtraDb) break;
    if (out.saturation.enabled) scale_saturation(out, scale);
  }
  for (double scale : {0.5, 0.0}) {
    if (m2_push() <= m1_push + kMaterialExtraDb) break;
    if (out.low_weight_eq.enabled) scale_low_weight(out, scale);
  }
  for (double scale : {0.5, 0.0}) {
    if (m2_push() <= m1_push + kMaterialExtraDb) break;
    if (out.hf_eq.enabled) scale_hf(out, scale);
  }
  for (double scale : {0.5, 0.0}) {
    if (m2_push() <= m1_push + kMaterialExtraDb) break;
    if (out.stereo.enabled) scale_stereo(out, scale);
  }
}

void apply_m2_character(const analysis::AnalysisResult& analysis,
                        const genres::hiphop::HipHopParameters& p,
                        ProcessingDecision& out) {
  const double crest = analysis.crest_factor_db.value_or(12.0);
  const bool dense = crest < p.dense_crest_threshold_db;
  double density_scale = 1.0;
  if (dense) {
    density_scale = 0.0;
    out.decision_notes.push_back(
        "m2: dense/low-crest input — punch/sat auto-bypassed");
  } else if (crest < p.dense_crest_threshold_db + 1.5) {
    density_scale = 0.5;
    out.decision_notes.push_back("m2: moderately dense — punch/sat reduced 50%");
  }

  out.transient = {};
  if (p.m2_enable_punch && p.enable_transient && density_scale > 0.0) {
    out.transient.enabled = true;
    out.transient.attack_boost_db =
        clamp_value(p.m2_punch_attack_boost_db * density_scale, 0.0, 1.5);
    out.transient.sustain_cut_db =
        clamp_value(p.m2_punch_sustain_cut_db * density_scale, 0.0, 0.75);
    out.transient.mix = clamp_value(p.m2_punch_mix * density_scale, 0.0, 0.5);
    out.transient.sensitivity = p.m2_punch_sensitivity;
    out.transient.attack_ms = 2.0;
    out.transient.release_ms = 80.0;
    out.transient.sense_hpf_hz = clamp_value(p.m2_punch_sense_hpf_hz, 60.0, 150.0);
    std::ostringstream oss;
    oss << "m2 punch: boost=" << out.transient.attack_boost_db
        << "dB sustain_cut=" << out.transient.sustain_cut_db
        << "dB mix=" << out.transient.mix;
    out.decision_notes.push_back(oss.str());
  } else {
    out.decision_notes.push_back("m2 punch: off");
  }

  out.saturation = {};
  if (p.m2_enable_saturation && p.enable_saturation && density_scale > 0.0) {
    const double lufs = analysis.integrated_loudness_lufs.value_or(0.0);
    const bool quiet_or_below_band =
        lufs < p.quiet_input_lufs_threshold || lufs < p.loudness_band_low_lufs;
    const bool open_crest = crest >= 12.0;
    const bool not_hot = lufs <= p.loudness_band_high_lufs;
    const bool open_density_eligible =
        p.r2b_enable_open_density && not_hot && !dense && quiet_or_below_band && open_crest;

    double sat_drive = p.m2_sat_drive;
    double sat_mix = p.m2_sat_mix;
    double sat_mix_cap = 0.15;  // R1 / approved hard cap
    if (open_density_eligible) {
      sat_drive = p.r2b_open_sat_drive;
      sat_mix = p.r2b_open_sat_mix;
      sat_mix_cap = clamp_value(p.r2b_open_sat_mix_cap, 0.15, 0.30);
    }

    out.saturation.enabled = true;
    out.saturation.drive = clamp_value(sat_drive * density_scale, 0.0, 2.5);
    out.saturation.mix = clamp_value(sat_mix * density_scale, 0.0, sat_mix_cap);
    out.saturation.output_gain_db = 0.0;
    out.saturation.oversample_factor = p.m2_sat_oversample;
    out.saturation.preserve_gain = true;
    out.saturation.curve = dsp::SaturationCurve::TanhNormalized;
    std::ostringstream oss;
    if (open_density_eligible) {
      oss << "r2b open-density sat: drive=" << out.saturation.drive
          << " mix=" << out.saturation.mix << " os=" << out.saturation.oversample_factor;
    } else {
      oss << "m2 sat: drive=" << out.saturation.drive << " mix=" << out.saturation.mix
          << " os=" << out.saturation.oversample_factor;
    }
    out.decision_notes.push_back(oss.str());
  } else {
    out.decision_notes.push_back("m2 sat: off");
  }

  // Adaptive low-end weight (M2 refinement — not M1 quiet tonal).
  out.low_weight_eq = {};
  out.low_weight_eq.enabled = false;
  if (p.m2_enable_low_weight && density_scale > 0.0) {
    const double bass_share = bass_body_share(analysis);
    if (bass_share < p.m2_bass_body_share_threshold) {
      double gain = p.m2_low_weight_gain_db;
      if (bass_share < p.m2_bass_body_share_threshold * 0.7) {
        gain = clamp_value(gain * 1.15, 0.0, 1.25);
      } else {
        gain *= 0.75;
      }
      gain = clamp_value(gain, 0.0, 1.25);
      if (gain > 0.05) {
        out.low_weight_eq.enabled = true;
        out.low_weight_eq.bands[0].enabled = true;
        out.low_weight_eq.bands[0].type = dsp::EqBandType::LowShelf;
        out.low_weight_eq.bands[0].freq_hz =
            clamp_value(p.m2_low_weight_freq_hz, 70.0, 120.0);
        out.low_weight_eq.bands[0].q = 0.8;
        out.low_weight_eq.bands[0].gain_db = gain;
        std::ostringstream oss;
        oss << "m2 low weight: shelf +" << gain << " dB @ "
            << out.low_weight_eq.bands[0].freq_hz << " Hz (bass share "
            << bass_share << ")";
        out.decision_notes.push_back(oss.str());
      }
    } else {
      out.decision_notes.push_back("m2 low weight: bypassed (bass body sufficient)");
    }
  }

  const double high_share = high_energy_share(analysis);
  out.hf_eq = {};
  out.hf_eq.enabled = false;
  if (p.m2_enable_hf_openness) {
    if (high_share >= p.m2_high_energy_share_threshold) {
      out.decision_notes.push_back(
          "m2 hf: bypassed (high/high-mid energy already elevated)");
    } else {
      double gain = p.m2_hf_gain_db;
      if (high_share > p.m2_high_energy_share_threshold * 0.7) {
        gain *= 0.5;
      }
      gain = clamp_value(gain, 0.0, 1.0);
      if (gain > 0.05) {
        out.hf_eq.enabled = true;
        out.hf_eq.bands[0].enabled = true;
        out.hf_eq.bands[0].type = dsp::EqBandType::HighShelf;
        out.hf_eq.bands[0].freq_hz = clamp_value(p.m2_hf_freq_hz, 7000.0, 12000.0);
        out.hf_eq.bands[0].q = 0.7;
        out.hf_eq.bands[0].gain_db = gain;
        std::ostringstream oss;
        oss << "m2 hf: high shelf +" << gain << " dB @ " << out.hf_eq.bands[0].freq_hz << " Hz";
        out.decision_notes.push_back(oss.str());
      }
    }
  } else {
    out.decision_notes.push_back("m2 hf: off");
  }

  out.stereo.enabled = false;
  out.stereo.width = 1.0;
  out.stereo.side_hpf_hz = 0.0;
  if (p.m2_enable_stereo_width && p.enable_stereo) {
    const bool hot =
        analysis.integrated_loudness_lufs &&
        (*analysis.integrated_loudness_lufs > p.loudness_band_high_lufs);
    // Hot and/or already-dense: never widen (correlation/width metrics ignored).
    if (hot || dense) {
      out.decision_notes.push_back("m2 stereo: hot_dense_bypass (width=1.0)");
    } else {
      const double corr = analysis.stereo_correlation.value_or(1.0);
      const double width_metric = analysis.stereo_width.value_or(0.0);
      if (corr < p.m2_wide_corr_threshold || width_metric > 0.045) {
        out.decision_notes.push_back(
            "m2 stereo: bypassed (input already sufficiently wide)");
      } else {
        const double w = clamp_value(p.m2_stereo_width, 1.0, 1.10);
        if (w > 1.001) {
          out.stereo.enabled = true;
          out.stereo.width = w;
          out.stereo.side_hpf_hz = p.m2_bass_mono_freq_hz;
          std::ostringstream oss;
          oss << "m2 stereo: width=" << w << " side_hpf=" << p.m2_bass_mono_freq_hz
              << " Hz (no widen below)";
          out.decision_notes.push_back(oss.str());
        }
      }
    }
  } else {
    out.decision_notes.push_back("m2 stereo: off (width=1.0)");
  }

  // R2C: soft peak rounder for eligible quiet/open material only.
  out.soft_peak_round = {};
  out.soft_peak_round.enabled = false;
  {
    const double lufs = analysis.integrated_loudness_lufs.value_or(0.0);
    const bool quiet_or_below_band =
        lufs < p.quiet_input_lufs_threshold || lufs < p.loudness_band_low_lufs;
    const bool open_crest = crest >= 12.0;
    const bool not_hot = lufs <= p.loudness_band_high_lufs;
    const bool soft_peak_eligible =
        p.r2c_enable_soft_peak_round && not_hot && !dense && quiet_or_below_band &&
        open_crest;
    if (soft_peak_eligible) {
      out.soft_peak_round.enabled = true;
      out.soft_peak_round.ceiling_dbfs =
          clamp_value(p.r2c_soft_peak_ceiling_dbfs, -6.0, 12.0);
      out.soft_peak_round.drive = clamp_value(p.r2c_soft_peak_drive, 0.1, 8.0);
      out.soft_peak_round.oversample_factor = p.r2c_soft_peak_oversample;
      std::ostringstream oss;
      oss << "r2c soft-peak: ceiling=" << out.soft_peak_round.ceiling_dbfs
          << " dBFS drive=" << out.soft_peak_round.drive
          << " os=" << out.soft_peak_round.oversample_factor;
      out.decision_notes.push_back(oss.str());
    } else if (p.r2c_enable_soft_peak_round) {
      out.decision_notes.push_back(
          "r2c soft-peak: bypassed (not quiet/open eligible)");
    }
  }
}

double cap_extra_lift(double extra,
                      double headroom,
                      double max_crest_reduction_db,
                      ProcessingDecision& out,
                      double max_extra_limiter_push = 3.0) {
  if (extra <= 0.0) {
    return 0.0;
  }
  // Caps apply to EXTRA above M1 only — never used to cut below M1 baseline.
  const double into_limiter = std::max(0.0, extra - std::max(0.0, headroom));
  double capped = extra;
  if (into_limiter > max_crest_reduction_db) {
    capped = std::min(capped, std::max(0.0, headroom) + max_crest_reduction_db);
    out.decision_notes.push_back(
        "loudness: extra lift above M1 capped by relative crest/limiter stress");
  }
  const double into2 = std::max(0.0, capped - std::max(0.0, headroom));
  if (into2 > max_extra_limiter_push) {
    capped = std::min(capped, std::max(0.0, headroom) + max_extra_limiter_push);
    out.decision_notes.push_back(
        "loudness: extra lift above M1 capped to limit additional limiter stress");
  }
  return std::max(0.0, capped);
}

}  // namespace

Status AdaptiveEngine::decide(const analysis::AnalysisResult& analysis,
                              const genres::hiphop::HipHopParameters& genre_params,
                              ProcessingDecision& out) const {
  out = ProcessingDecision{};
  const bool m2 = genre_params.is_m2_profile();
  out.decision_notes.push_back(
      m2 ? (genre_params.is_m2_refinement()
                ? "M2 refinement (builds on frozen M1 tonal baseline; TP -1.0 dBTP)"
                : "M2 tuning checkpoint (builds on frozen M1 tonal baseline)")
         : "PROVISIONAL baseline: STEMY_MASTER_01 pocket (Bumpman eval); T.I. is context only");

  out.input_gain.enabled = genre_params.enable_input_gain;
  out.input_gain.gain_db = 0.0;
  out.output_gain.enabled = true;
  out.output_gain.gain_db = 0.0;

  out.eq.enabled = genre_params.enable_eq;
  out.low_end.enabled = false;
  out.transient.enabled = false;
  out.compressor.enabled = false;
  out.compressor.ratio = 1.0;
  out.saturation.enabled = false;
  out.saturation.drive = 0.0;
  out.saturation.mix = 0.0;
  out.hf_eq = {};
  out.hf_eq.enabled = false;
  out.low_weight_eq = {};
  out.low_weight_eq.enabled = false;
  out.stereo.enabled = false;
  out.stereo.width = 1.0;

  if (!m2 && genre_params.prefer_tight_stereo) {
    out.decision_notes.push_back("stereo: keep width=1.0 (tight / MASTER_01-like; no widening)");
  }

  double gain_db = 0.0;
  bool quiet_path = false;
  if (analysis.integrated_loudness_lufs) {
    const double lufs = *analysis.integrated_loudness_lufs;
    quiet_path = lufs < genre_params.quiet_input_lufs_threshold;

    if (m2) {
      const double lo = genre_params.loudness_band_low_lufs;
      const double hi = genre_params.loudness_band_high_lufs;
      const double nominal = genre_params.target_integrated_lufs;  // -8.5
      const double crest = analysis.crest_factor_db.value_or(12.0);
      const double tp = analysis.true_peak_dbtp.value_or(-1.0);
      const double ceiling = genre_params.target_true_peak_dbtp;
      const double headroom = ceiling - tp;

      const double m1_gain = compute_m1_baseline_gain_db(
          analysis, genre_params.max_lift_db, genre_params.max_reduction_db);
      out.m1_baseline_gain_db = m1_gain;
      {
        std::ostringstream oss;
        oss << "loudness: M1 baseline candidate gain=" << m1_gain << " dB";
        out.decision_notes.push_back(oss.str());
      }

      if (lufs > hi) {
        // Hot path: pull toward band (unchanged intent).
        const double reduce_target = std::min(nominal, hi);
        gain_db = clamp_value(reduce_target - lufs, -genre_params.max_reduction_db, 0.0);
        std::ostringstream oss;
        oss << "loudness: reduce " << gain_db << " dB toward " << reduce_target
            << " LUFS (input " << lufs << ")";
        out.decision_notes.push_back(oss.str());
      } else if (lufs >= lo && lufs <= hi) {
        // In band: hold, but never go below M1 candidate if M1 wanted a lift
        // (shouldn't happen when already in −9…−8 with M1 target −9).
        gain_db = std::max(0.0, m1_gain);
        if (std::fabs(gain_db) < 1e-9) {
          out.decision_notes.push_back("loudness: within −9…−8 operating band — hold");
        } else {
          out.decision_notes.push_back(
              "loudness: in band but applying non-negative M1 baseline candidate");
        }
      } else {
        // Quiet / below −9: start from M1 baseline, then optional EXTRA.
        gain_db = m1_gain;
        double lift_target = nominal;
        if (genre_params.is_m2_refinement() && crest >= 12.0 && lufs < lo) {
          lift_target = lo;  // open premix: prioritize −9 before −8.5
          out.decision_notes.push_back(
              "loudness: open premix — lift target anchored at −9 LUFS band");
        }
        const double m2_want =
            clamp_value(lift_target - lufs, 0.0, genre_params.max_lift_db);
        double extra = std::max(0.0, m2_want - m1_gain);
        if (genre_params.is_m2_refinement() && crest >= 12.0 && lufs < -11.0 &&
            crest >= genre_params.dense_crest_threshold_db) {
          extra = std::max(extra, genre_params.m2_open_premix_extra_db);
          extra = std::min(extra, genre_params.m2_max_extra_lift_db);
          out.decision_notes.push_back(
              "loudness: open premix extra lift above M1 baseline (conservative)");
        }
        const double max_push = genre_params.is_m2_refinement() ? 4.0 : 3.0;
        extra = cap_extra_lift(extra, headroom, genre_params.max_crest_reduction_db, out,
                               max_push);
        gain_db = m1_gain + extra;

        // Dense crest: may limit TOTAL lift, but record override if we cut below M1.
        if (crest < genre_params.dense_crest_threshold_db && gain_db > 2.0) {
          const double limited = 2.0;
          if (limited + 0.5 < m1_gain) {
            out.safety_override_reason =
                "dense/low-crest source: lift capped to protect punch/dynamics "
                "(may land >0.5 LU below M1)";
            out.decision_notes.push_back("safety_override: " + out.safety_override_reason);
          }
          gain_db = std::min(gain_db, limited);
          out.decision_notes.push_back(
              "loudness: lift limited on low-crest source to protect punch");
        }

        // Severe input overs: keep M1 baseline (do not invent extra cut); document.
        if (tp > 0.0) {
          out.decision_notes.push_back(
              "warning: input true-peak overs — weaker reference; M1 baseline retained where applied");
        }

        std::ostringstream oss;
        oss << "loudness: M2 gain=" << gain_db << " dB (M1 baseline " << m1_gain
            << " + extra " << (gain_db - m1_gain) << ") toward " << lift_target
            << " LUFS (band −9…−8; input " << lufs << ")";
        out.decision_notes.push_back(oss.str());
      }
    } else {
      const double target = genre_params.target_integrated_lufs;
      const double tol = genre_params.loudness_tolerance_lufs;
      const double delta = target - lufs;
      if (std::fabs(delta) <= tol) {
        gain_db = 0.0;
        out.decision_notes.push_back("loudness: in MASTER_01 pocket — no gain change");
      } else if (delta > 0.0) {
        gain_db = clamp_value(delta, 0.0, genre_params.max_lift_db);
        std::ostringstream oss;
        oss << "loudness: lift " << gain_db << " dB toward " << target << " LUFS (input " << lufs
            << ")";
        out.decision_notes.push_back(oss.str());
      } else {
        gain_db = clamp_value(delta, -genre_params.max_reduction_db, 0.0);
        std::ostringstream oss;
        oss << "loudness: reduce " << gain_db << " dB toward " << target << " LUFS (input " << lufs
            << "; do not push hotter)";
        out.decision_notes.push_back(oss.str());
      }
      out.m1_baseline_gain_db = gain_db;
    }

    if (analysis.crest_factor_db) {
      std::ostringstream oss;
      oss << "dynamics: input crest " << *analysis.crest_factor_db << " dB (ref ~"
          << genre_params.reference_crest_db << " dB; compressor OFF)";
      out.decision_notes.push_back(oss.str());
    }
  } else {
    out.decision_notes.push_back("loudness: integrated LUFS unavailable — gain left at 0 dB");
  }

  out.input_gain.gain_db = gain_db;

  out.limiter.ceiling_dbfs = genre_params.target_true_peak_dbtp;
  out.limiter.release_ms = 50.0;
  out.limiter.lookahead_ms = 1.0;
  out.limiter.true_peak_aware = genre_params.limiter_true_peak_aware;
  out.limiter.true_peak_oversample = genre_params.limiter_true_peak_oversample;
  const bool peak_needs_limit =
      analysis.true_peak_dbtp &&
      (*analysis.true_peak_dbtp > genre_params.target_true_peak_dbtp - 0.05);
  const bool gain_needs_limit = std::fabs(gain_db) > 0.1;
  out.limiter.enabled =
      genre_params.enable_limiter && (gain_needs_limit || peak_needs_limit || quiet_path);
  if (out.limiter.enabled) {
    std::ostringstream oss;
    oss << "limiter: enabled @ " << out.limiter.ceiling_dbfs << " dBTP";
    if (out.limiter.true_peak_aware) {
      oss << " (ISP-aware x" << out.limiter.true_peak_oversample << ")";
    }
    out.decision_notes.push_back(oss.str());
  } else {
    out.decision_notes.push_back("limiter: bypassed");
  }

  const bool apply_quiet_tonal =
      genre_params.enable_tonal_trim_on_quiet_input && quiet_path &&
      (genre_params.enable_eq || genre_params.enable_low_end);
  if (apply_quiet_tonal) {
    apply_master01_quiet_tonal_trim(out);
    if (!genre_params.enable_low_end) {
      out.low_end.enabled = false;
    }
    if (!genre_params.enable_eq) {
      out.eq = dsp::EqParams{};
      out.eq.enabled = false;
    }
  } else {
    out.decision_notes.push_back(
        "tonal: bypassed (M1 quiet-path trim only on quiet inputs)");
  }

  // Final R0 extreme-sub guard (general; after quiet tonal so shelves can stack).
  apply_extreme_sub_guard(analysis, genre_params, out);
  if (!genre_params.enable_low_end) {
    out.low_end.enabled = false;
  }

  out.decision_notes.push_back("compressor: OFF (checkpoint)");

  if (m2) {
    apply_m2_character(analysis, genre_params, out);
    // Character-first stress relief vs M1 at the chosen gain (usually M1 baseline).
    relieve_character_limiter_stress(analysis, genre_params, gain_db, apply_quiet_tonal, out);

    // Last resort: only if still extreme estimated push AND severe overs, allow
    // small cut below M1 with documented override (character already minimized).
    if (out.safety_override_reason.empty()) {
      const double tp = analysis.true_peak_dbtp.value_or(-1.0);
      const double ceiling = genre_params.target_true_peak_dbtp;
      const double tonal_peak = apply_quiet_tonal ? 1.6 : 0.0;
      const double push = estimate_limiter_push_db(
          tp, gain_db, tonal_peak + estimate_character_peak_add_db(out), ceiling);
      const double m1_push =
          estimate_limiter_push_db(tp, out.m1_baseline_gain_db, tonal_peak, ceiling);
      // Only after character exhausted and still >> M1 stress with severe overs.
      const bool character_exhausted =
          !out.transient.enabled && !out.saturation.enabled && !out.hf_eq.enabled &&
          (!out.stereo.enabled || out.stereo.width <= 1.001);
      if (character_exhausted && tp > 0.25 && push > m1_push + 3.0 &&
          gain_db > out.m1_baseline_gain_db) {
        // Prefer dropping EXTRA only (should already be capped); nothing more.
      } else if (character_exhausted && tp > 0.25 && push > 14.0 &&
                 gain_db + 0.5 < out.m1_baseline_gain_db) {
        // Already below M1 somehow — keep override if set elsewhere.
      }
    }
  } else {
    out.decision_notes.push_back("saturation: bypassed (M1)");
  }

  out.input_gain.gain_db = gain_db;
  return Status::Ok();
}

}  // namespace stemy::adaptive
