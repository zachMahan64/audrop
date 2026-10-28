#pragma once

#include <cmath>
#include <complex>
#include <numbers>
#include <span>
#include <vector>

namespace audrop::audio {

// Small radix-2 FFT for real input. Sizes must be powers of two.
class RealFft {
public:
    explicit RealFft(std::size_t n) : n_(n), window_(n), buf_(n), twiddle_(n / 2), bitrev_(n) {
        for (std::size_t i = 0; i < n; ++i)
            window_[i] = 0.5f - 0.5f * std::cos(2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(n));
        for (std::size_t i = 0; i < n / 2; ++i)
            twiddle_[i] = std::polar(1.0f, -2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(n));
        std::size_t bits = 0;
        while ((std::size_t{1} << bits) < n) ++bits;
        for (std::size_t i = 0; i < n; ++i) {
            std::size_t r = 0;
            for (std::size_t b = 0; b < bits; ++b)
                if (i & (std::size_t{1} << b)) r |= std::size_t{1} << (bits - 1 - b);
            bitrev_[i] = r;
        }
    }

    std::size_t size() const { return n_; }

    // Hann-windows `in` (size n) and writes n/2+1 magnitudes to `mag`.
    void magnitudes(std::span<const float> in, std::span<float> mag) {
        for (std::size_t i = 0; i < n_; ++i) buf_[bitrev_[i]] = {in[i] * window_[i], 0.0f};
        for (std::size_t len = 2; len <= n_; len <<= 1) {
            const std::size_t half = len / 2, step = n_ / len;
            for (std::size_t i = 0; i < n_; i += len) {
                for (std::size_t j = 0; j < half; ++j) {
                    const auto t = twiddle_[j * step] * buf_[i + j + half];
                    buf_[i + j + half] = buf_[i + j] - t;
                    buf_[i + j] += t;
                }
            }
        }
        for (std::size_t i = 0; i <= n_ / 2; ++i) mag[i] = std::abs(buf_[i]);
    }

private:
    std::size_t n_;
    std::vector<float> window_;
    std::vector<std::complex<float>> buf_;
    std::vector<std::complex<float>> twiddle_;
    std::vector<std::size_t> bitrev_;
};

}  // namespace audrop::audio
