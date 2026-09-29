#include "stemy/analysis/true_peak_measure.h"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <vector>

namespace stemy::analysis {
namespace {

constexpr double kPi = 3.14159265358979323846;

double bessel_i0(double x) {
  double sum = 1.0;
  double term = 1.0;
  const double x2 = x * x * 0.25;
  for (int k = 1; k < 40; ++k) {
    term *= x2 / (static_cast<double>(k) * static_cast<double>(k));
    sum += term;
    if (term < 1e-14 * sum) break;
  }
  return sum;
}

std::vector<double> design_os_lpf(int L) {
  const int half = 48;
  const int n_raw = 2 * half + 1;
  const int n_poly = (n_raw + L - 1) / L;
  const int N = n_poly * L;
  std::vector<double> h(static_cast<std::size_t>(N), 0.0);
  const double beta = 8.5;
  const double i0b = bessel_i0(beta);
  const double fc = 0.5 / static_cast<double>(L);

  for (int n = 0; n < n_raw; ++n) {
    const double m = static_cast<double>(n - half);
    double sinc = (std::fabs(m) < 1e-12) ? (2.0 * fc)
                                         : (std::sin(2.0 * kPi * fc * m) / (kPi * m));
    const double r = m / static_cast<double>(half);
    const double w =
        (std::fabs(r) <= 1.0)
            ? (bessel_i0(beta * std::sqrt(std::max(0.0, 1.0 - r * r))) / i0b)
            : 0.0;
    const int dst = n + (N - n_raw) / 2;
    if (dst >= 0 && dst < N) {
      h[static_cast<std::size_t>(dst)] = sinc * w;
    }
  }

  double sum = 0.0;
  for (double c : h) sum += c;
  const double scale = (sum != 0.0) ? (static_cast<double>(L) / sum) : 1.0;
  for (double& c : h) c *= scale;
  return h;
}

struct OsKernel {
  int L = 4;
  int n_poly = 0;
  std::vector<double> h;
};

const OsKernel& kernel_for(int factor) {
  static std::mutex mu;
  static OsKernel k2, k4, k8;
  static bool init = false;
  std::lock_guard<std::mutex> lock(mu);
  if (!init) {
    auto fill = [](OsKernel& k, int L) {
      k.L = L;
      k.h = design_os_lpf(L);
      k.n_poly = static_cast<int>(k.h.size()) / L;
    };
    fill(k2, 2);
    fill(k4, 4);
    fill(k8, 8);
    init = true;
  }
  if (factor <= 2) return k2;
  if (factor >= 8) return k8;
  return k4;
}

double true_peak_channel(const float* interleaved,
                         std::size_t frames,
                         std::uint16_t channels,
                         std::uint16_t channel,
                         const OsKernel& kern) {
  if (frames == 0) return 0.0;

  const int L = kern.L;
  const int P = kern.n_poly;
  std::vector<double> xhist(static_cast<std::size_t>(P), 0.0);
  double peak = 0.0;

  auto emit_phases = [&]() {
    for (int phase = 0; phase < L; ++phase) {
      double acc = 0.0;
      for (int i = 0; i < P; ++i) {
        acc += xhist[static_cast<std::size_t>(i)] *
               kern.h[static_cast<std::size_t>(phase + i * L)];
      }
      peak = std::max(peak, std::fabs(acc));
    }
  };

  for (std::size_t n = 0; n < frames; ++n) {
    const double x = static_cast<double>(interleaved[n * channels + channel]);
    peak = std::max(peak, std::fabs(x));
    for (int i = P - 1; i > 0; --i) {
      xhist[static_cast<std::size_t>(i)] = xhist[static_cast<std::size_t>(i - 1)];
    }
    xhist[0] = x;
    emit_phases();
  }
  for (int f = 0; f < P; ++f) {
    for (int i = P - 1; i > 0; --i) {
      xhist[static_cast<std::size_t>(i)] = xhist[static_cast<std::size_t>(i - 1)];
    }
    xhist[0] = 0.0;
    emit_phases();
  }
  return peak;
}

}  // namespace

TruePeakIspDetector::TruePeakIspDetector(int oversample_factor) {
  set_oversample_factor(oversample_factor);
}

void TruePeakIspDetector::set_oversample_factor(int oversample_factor) {
  const int factor = oversample_factor < 2 ? 4 : oversample_factor;
  L_ = (factor <= 2) ? 2 : (factor <= 4 ? 4 : 8);
  ensure_kernel();
  reset();
}

void TruePeakIspDetector::ensure_kernel() {
  const OsKernel& k = kernel_for(L_);
  L_ = k.L;
  n_poly_ = k.n_poly;
  h_ = &k.h;
}

void TruePeakIspDetector::reset() {
  ensure_kernel();
  hist_l_.assign(static_cast<std::size_t>(n_poly_), 0.0);
  hist_r_.assign(static_cast<std::size_t>(n_poly_), 0.0);
}

double TruePeakIspDetector::emit_peak(std::vector<double>& hist) {
  double peak = 0.0;
  for (int phase = 0; phase < L_; ++phase) {
    double acc = 0.0;
    for (int i = 0; i < n_poly_; ++i) {
      acc += hist[static_cast<std::size_t>(i)] *
             (*h_)[static_cast<std::size_t>(phase + i * L_)];
    }
    peak = std::max(peak, std::fabs(acc));
  }
  return peak;
}

double TruePeakIspDetector::push_mono(float x) {
  const double xd = static_cast<double>(x);
  double peak = std::fabs(xd);
  for (int i = n_poly_ - 1; i > 0; --i) {
    hist_l_[static_cast<std::size_t>(i)] = hist_l_[static_cast<std::size_t>(i - 1)];
  }
  hist_l_[0] = xd;
  peak = std::max(peak, emit_peak(hist_l_));
  return peak;
}

double TruePeakIspDetector::push_stereo(float left, float right) {
  const double ld = static_cast<double>(left);
  const double rd = static_cast<double>(right);
  double peak = std::max(std::fabs(ld), std::fabs(rd));

  for (int i = n_poly_ - 1; i > 0; --i) {
    hist_l_[static_cast<std::size_t>(i)] = hist_l_[static_cast<std::size_t>(i - 1)];
    hist_r_[static_cast<std::size_t>(i)] = hist_r_[static_cast<std::size_t>(i - 1)];
  }
  hist_l_[0] = ld;
  hist_r_[0] = rd;
  peak = std::max(peak, emit_peak(hist_l_));
  peak = std::max(peak, emit_peak(hist_r_));
  return peak;
}

double measure_sample_peak_linear(const audio::AudioBuffer& buffer) {
  double peak = 0.0;
  for (std::size_t i = 0; i < buffer.sample_count(); ++i) {
    peak = std::max(peak, static_cast<double>(std::fabs(buffer.data()[i])));
  }
  return peak;
}

double measure_true_peak_linear(const audio::AudioBuffer& buffer, int oversample_factor) {
  if (buffer.empty() || buffer.channel_count() == 0) {
    return 0.0;
  }
  const int factor = oversample_factor < 2 ? 4 : oversample_factor;
  const int L = (factor <= 2) ? 2 : (factor <= 4 ? 4 : 8);
  const OsKernel& kern = kernel_for(L);

  double peak = measure_sample_peak_linear(buffer);
  for (std::uint16_t ch = 0; ch < buffer.channel_count(); ++ch) {
    peak = std::max(peak, true_peak_channel(buffer.data(), buffer.frame_count(),
                                            buffer.channel_count(), ch, kern));
  }
  return peak;
}

}  // namespace stemy::analysis
