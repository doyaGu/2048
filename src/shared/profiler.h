#pragma once

#include <chrono>
#include <functional>

namespace game2048 {

class ScopedTimer {
public:
    using Callback = std::function<void(double)>;

    explicit ScopedTimer(Callback callback)
        : callback_(std::move(callback)), start_(Clock::now()) {}

    ~ScopedTimer() {
        const auto end = Clock::now();
        const auto ms = std::chrono::duration<double, std::milli>(end - start_).count();
        callback_(ms);
    }

private:
    using Clock = std::chrono::steady_clock;

    Callback callback_;
    Clock::time_point start_;
};

}  // namespace game2048