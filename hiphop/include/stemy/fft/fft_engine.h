#pragma once

#include "stemy/core/status.h"

#include <cstddef>
#include <memory>
#include <vector>

namespace stemy::fft {

/// Internal FFT abstraction. Implementation currently uses pffft and can be swapped later.
class FftEngine {
public:
  virtual ~FftEngine() = default;

  /// Real forward FFT. size must be a supported power-of-two (implementation-defined).
  /// Output layout is implementation-defined but consistent for spectral analyzers.
  [[nodiscard]] virtual Status forward_real(const float* input,
                                            float* output_packed,
                                            std::size_t size) = 0;

  [[nodiscard]] virtual Status inverse_real(const float* input_packed,
                                            float* output,
                                            std::size_t size) = 0;

  [[nodiscard]] virtual std::size_t max_size() const = 0;
};

[[nodiscard]] std::unique_ptr<FftEngine> create_default_fft_engine(std::size_t size);

}  // namespace stemy::fft
