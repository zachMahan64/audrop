#pragma once

#include <atomic>
#include <cstddef>
#include <span>
#include <vector>

namespace audrop::audio {

// Lock-free single-producer / single-consumer ring of floats.
// The producer is the audio mixing thread; the consumer is the render thread.
class SpscRing {
public:
    explicit SpscRing(std::size_t capacity_pow2) : buf_(capacity_pow2), mask_(capacity_pow2 - 1) {}

    // Drops samples that don't fit (consumer fell behind).
    void push(std::span<const float> in) {
        const std::size_t w = write_.load(std::memory_order_relaxed);
        const std::size_t r = read_.load(std::memory_order_acquire);
        const std::size_t free = buf_.size() - (w - r);
        const std::size_t n = std::min(free, in.size());
        for (std::size_t i = 0; i < n; ++i) buf_[(w + i) & mask_] = in[i];
        write_.store(w + n, std::memory_order_release);
    }

    // Appends everything available to `out`.
    std::size_t drain(std::vector<float>& out) {
        const std::size_t r = read_.load(std::memory_order_relaxed);
        const std::size_t w = write_.load(std::memory_order_acquire);
        const std::size_t n = w - r;
        out.reserve(out.size() + n);
        for (std::size_t i = 0; i < n; ++i) out.push_back(buf_[(r + i) & mask_]);
        read_.store(w, std::memory_order_release);
        return n;
    }

private:
    std::vector<float> buf_;
    std::size_t mask_;
    std::atomic<std::size_t> write_{0};
    std::atomic<std::size_t> read_{0};
};

}  // namespace audrop::audio
