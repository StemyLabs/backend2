#include "stemy/dsp/biquad.h"

#include <cmath>

namespace stemy::dsp {
namespace {
constexpr double kPi = 3.14159265358979323846;
}

void BiquadFilter::set_coeffs(const BiquadCoeffs& c) { c_ = c; }

void BiquadFilter::reset() {
  z1_ = 0.0;
  z2_ = 0.0;
}

float BiquadFilter::process(float x) {
  const double y = c_.b0 * x + z1_;
  z1_ = c_.b1 * x - c_.a1 * y + z2_;
  z2_ = c_.b2 * x - c_.a2 * y;
  return static_cast<float>(y);
}

BiquadCoeffs BiquadFilter::peaking(double sample_rate, double freq_hz, double q, double gain_db) {
  const double A = std::pow(10.0, gain_db / 40.0);
  const double w0 = 2.0 * kPi * freq_hz / sample_rate;
  const double alpha = std::sin(w0) / (2.0 * q);
  const double cosw = std::cos(w0);

  BiquadCoeffs c;
  const double a0 = 1.0 + alpha / A;
  c.b0 = (1.0 + alpha * A) / a0;
  c.b1 = (-2.0 * cosw) / a0;
  c.b2 = (1.0 - alpha * A) / a0;
  c.a1 = (-2.0 * cosw) / a0;
  c.a2 = (1.0 - alpha / A) / a0;
  return c;
}

BiquadCoeffs BiquadFilter::lowshelf(double sample_rate, double freq_hz, double q, double gain_db) {
  const double A = std::pow(10.0, gain_db / 40.0);
  const double w0 = 2.0 * kPi * freq_hz / sample_rate;
  const double alpha = std::sin(w0) / (2.0 * q);
  const double cosw = std::cos(w0);
  const double two_sqrtA_alpha = 2.0 * std::sqrt(A) * alpha;

  BiquadCoeffs c;
  const double a0 = (A + 1.0) + (A - 1.0) * cosw + two_sqrtA_alpha;
  c.b0 = (A * ((A + 1.0) - (A - 1.0) * cosw + two_sqrtA_alpha)) / a0;
  c.b1 = (2.0 * A * ((A - 1.0) - (A + 1.0) * cosw)) / a0;
  c.b2 = (A * ((A + 1.0) - (A - 1.0) * cosw - two_sqrtA_alpha)) / a0;
  c.a1 = (-2.0 * ((A - 1.0) + (A + 1.0) * cosw)) / a0;
  c.a2 = ((A + 1.0) + (A - 1.0) * cosw - two_sqrtA_alpha) / a0;
  return c;
}

BiquadCoeffs BiquadFilter::highshelf(double sample_rate, double freq_hz, double q, double gain_db) {
  const double A = std::pow(10.0, gain_db / 40.0);
  const double w0 = 2.0 * kPi * freq_hz / sample_rate;
  const double alpha = std::sin(w0) / (2.0 * q);
  const double cosw = std::cos(w0);
  const double two_sqrtA_alpha = 2.0 * std::sqrt(A) * alpha;

  BiquadCoeffs c;
  const double a0 = (A + 1.0) - (A - 1.0) * cosw + two_sqrtA_alpha;
  c.b0 = (A * ((A + 1.0) + (A - 1.0) * cosw + two_sqrtA_alpha)) / a0;
  c.b1 = (-2.0 * A * ((A - 1.0) + (A + 1.0) * cosw)) / a0;
  c.b2 = (A * ((A + 1.0) + (A - 1.0) * cosw - two_sqrtA_alpha)) / a0;
  c.a1 = (2.0 * ((A - 1.0) - (A + 1.0) * cosw)) / a0;
  c.a2 = ((A + 1.0) - (A - 1.0) * cosw - two_sqrtA_alpha) / a0;
  return c;
}

BiquadCoeffs BiquadFilter::highpass(double sample_rate, double freq_hz, double q) {
  const double w0 = 2.0 * kPi * freq_hz / sample_rate;
  const double alpha = std::sin(w0) / (2.0 * q);
  const double cosw = std::cos(w0);

  BiquadCoeffs c;
  const double a0 = 1.0 + alpha;
  c.b0 = ((1.0 + cosw) / 2.0) / a0;
  c.b1 = (-(1.0 + cosw)) / a0;
  c.b2 = ((1.0 + cosw) / 2.0) / a0;
  c.a1 = (-2.0 * cosw) / a0;
  c.a2 = (1.0 - alpha) / a0;
  return c;
}

BiquadCoeffs BiquadFilter::lowpass(double sample_rate, double freq_hz, double q) {
  const double w0 = 2.0 * kPi * freq_hz / sample_rate;
  const double alpha = std::sin(w0) / (2.0 * q);
  const double cosw = std::cos(w0);

  BiquadCoeffs c;
  const double a0 = 1.0 + alpha;
  c.b0 = ((1.0 - cosw) / 2.0) / a0;
  c.b1 = (1.0 - cosw) / a0;
  c.b2 = ((1.0 - cosw) / 2.0) / a0;
  c.a1 = (-2.0 * cosw) / a0;
  c.a2 = (1.0 - alpha) / a0;
  return c;
}

}  // namespace stemy::dsp
