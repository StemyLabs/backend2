#pragma once

#include <cmath>
#include <limits>

namespace stemy {

inline constexpr float kDenormalThreshold = 1.0e-15f;
inline constexpr float kMaxAbsGainLinear = 16.0f;  // +24 dB safety clamp for gain params

[[nodiscard]] inline bool is_finite_sample(float x) {
  return std::isfinite(x);
}

[[nodiscard]] inline float flush_denormal(float x) {
  return (std::fabs(x) < kDenormalThreshold) ? 0.0f : x;
}

[[nodiscard]] inline double linear_to_db(double linear, double floor_db = -120.0) {
  if (!(linear > 0.0) || !std::isfinite(linear)) {
    return floor_db;
  }
  return 20.0 * std::log10(linear);
}

[[nodiscard]] inline double db_to_linear(double db) {
  return std::pow(10.0, db / 20.0);
}

}  // namespace stemy
