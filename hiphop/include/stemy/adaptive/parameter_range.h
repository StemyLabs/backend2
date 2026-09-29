#pragma once

#include <string>

namespace stemy::adaptive {

/// Inclusive numeric range for a tunable parameter. Values are provisional until tuned.
struct ParameterRange {
  std::string name;
  double min_value = 0.0;
  double max_value = 0.0;
  double default_value = 0.0;
  bool provisional = true;
  std::string unit;  // e.g. "dB", "ms", "ratio", "linear"
};

[[nodiscard]] inline double clamp_to_range(double value, const ParameterRange& range) {
  if (value < range.min_value) return range.min_value;
  if (value > range.max_value) return range.max_value;
  return value;
}

}  // namespace stemy::adaptive
