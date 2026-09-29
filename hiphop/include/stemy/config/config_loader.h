#pragma once

#include "stemy/core/status.h"
#include "stemy/genres/hiphop/hiphop_parameters.h"

#include <filesystem>

namespace stemy::config {

/// Load Hip-Hop parameters from JSON. Kept out of real-time DSP paths.
[[nodiscard]] Status load_hiphop_config(const std::filesystem::path& path,
                                        genres::hiphop::HipHopParameters& out);

}  // namespace stemy::config
