#include "stemy/fft/fft_engine.h"

#include "pffft.h"

#include <cmath>
#include <vector>

namespace stemy::fft {
namespace {

class PffftEngine final : public FftEngine {
public:
  explicit PffftEngine(std::size_t size) : size_(size) {
    setup_ = pffft_new_setup(static_cast<int>(size), PFFFT_REAL);
    work_.resize(size);
  }

  ~PffftEngine() override {
    if (setup_ != nullptr) {
      pffft_destroy_setup(setup_);
    }
  }

  Status forward_real(const float* input, float* output_packed, std::size_t size) override {
    if (setup_ == nullptr) {
      return Status::Error("PFFFT setup failed");
    }
    if (size != size_) {
      return Status::Error("FFT size mismatch");
    }
    pffft_transform_ordered(setup_, input, output_packed, work_.data(), PFFFT_FORWARD);
    return Status::Ok();
  }

  Status inverse_real(const float* input_packed, float* output, std::size_t size) override {
    if (setup_ == nullptr) {
      return Status::Error("PFFFT setup failed");
    }
    if (size != size_) {
      return Status::Error("FFT size mismatch");
    }
    pffft_transform_ordered(setup_, input_packed, output, work_.data(), PFFFT_BACKWARD);
    // pffft does not normalize; scale by 1/N for round-trip unity.
    const float scale = 1.0f / static_cast<float>(size_);
    for (std::size_t i = 0; i < size_; ++i) {
      output[i] *= scale;
    }
    return Status::Ok();
  }

  std::size_t max_size() const override { return size_; }

private:
  std::size_t size_ = 0;
  PFFFT_Setup* setup_ = nullptr;
  std::vector<float> work_;
};

bool is_supported_pffft_size(std::size_t n) {
  // pffft requires N >= 32 and N = (2^a)*(3^b)*(5^c)
  if (n < 32) {
    return false;
  }
  while (n % 2 == 0) n /= 2;
  while (n % 3 == 0) n /= 3;
  while (n % 5 == 0) n /= 5;
  return n == 1;
}

}  // namespace

std::unique_ptr<FftEngine> create_default_fft_engine(std::size_t size) {
  if (!is_supported_pffft_size(size)) {
    return nullptr;
  }
  return std::make_unique<PffftEngine>(size);
}

}  // namespace stemy::fft
