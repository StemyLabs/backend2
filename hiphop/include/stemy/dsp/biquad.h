#pragma once

#include "stemy/audio/audio_buffer.h"
#include "stemy/core/status.h"

namespace stemy::dsp {

/// Shared biquad (Direct Form I). Coefficients are explicit; no genre knowledge.
struct BiquadCoeffs {
  double b0 = 1.0;
  double b1 = 0.0;
  double b2 = 0.0;
  double a1 = 0.0;
  double a2 = 0.0;
};

class BiquadFilter {
public:
  void set_coeffs(const BiquadCoeffs& c);
  void reset();
  [[nodiscard]] float process(float x);

  /// Design helpers (Audio EQ Cookbook). Gain_db = 0 → unity (transparent).
  static BiquadCoeffs peaking(double sample_rate, double freq_hz, double q, double gain_db);
  static BiquadCoeffs lowshelf(double sample_rate, double freq_hz, double q, double gain_db);
  static BiquadCoeffs highshelf(double sample_rate, double freq_hz, double q, double gain_db);
  static BiquadCoeffs highpass(double sample_rate, double freq_hz, double q);
  static BiquadCoeffs lowpass(double sample_rate, double freq_hz, double q);

private:
  BiquadCoeffs c_{};
  double z1_ = 0.0;
  double z2_ = 0.0;
};

}  // namespace stemy::dsp
