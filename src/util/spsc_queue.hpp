#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <optional>

namespace k10s {
template <typename T, std::size_t Capacity> class SpscQueue {
    std::array<T, Capacity> buffer_;
    alignas(64) std::atomic<std::size_t> head_{0};
    alignas(64) std::atomic<std::size_t> tail_{0};

  public:
    bool try_push(const T& item) {
        const auto t = tail_.load(std::memory_order_relaxed);
        const auto next = (t + 1) % Capacity;
        if (next == head_.load(std::memory_order_acquire)) {
            return false;
        }

        buffer_[t] = item;
        tail_.store(next, std::memory_order_release);
        return true;
    }

    std::optional<T> try_pop() {
        const auto h = head_.load(std::memory_order_relaxed);
        if (h == tail_.load(std::memory_order_acquire)) {
            return std::nullopt;
        }

        T item = std::move(buffer_[h]);
        head_.store((h + 1) % Capacity, std::memory_order_release);
        return item;
    }
};
} // namespace k10s