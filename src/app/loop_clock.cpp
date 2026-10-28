#include "app/loop_clock.hpp"

#include <chrono>
#include <thread>

namespace audrop {

double now_seconds() {
    using clock = std::chrono::steady_clock;
    static const auto start = clock::now();
    return std::chrono::duration<double>(clock::now() - start).count();
}

void sleep_seconds(double seconds) {
    if (seconds <= 0) return;
    if (seconds < 0.0002) {
        const double until = now_seconds() + seconds;
        while (now_seconds() < until) std::this_thread::yield();
        return;
    }
    std::this_thread::sleep_for(std::chrono::duration<double>(seconds));
}

}  // namespace audrop
