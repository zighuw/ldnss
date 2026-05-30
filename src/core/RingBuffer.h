#pragma once

#include <array>
#include <atomic>
#include <cstddef>

// Lock-free SPSC ring buffer. Capacity must be a power of 2.
// Producer: audio thread (write). Consumer: main thread (readLatest).
//
// readLatest() is skip-to-latest: if the consumer is slower than the producer,
// it only sees the most recent item, dropping intermediate ones.
template <typename T, size_t Capacity>
class RingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a power of 2");

public:
    RingBuffer() = default;

    // Producer: write one item. Returns false if the buffer is full.
    bool write(const T& item)
    {
        size_t w = writeIdx_.load(std::memory_order_relaxed);
        size_t r = readIdx_.load(std::memory_order_acquire);
        if (w - r >= Capacity)
            return false;
        buffer_[w % Capacity] = item;
        writeIdx_.store(w + 1, std::memory_order_release);
        return true;
    }

    // Consumer: get the latest item. Returns false if nothing new.
    bool readLatest(T& out)
    {
        size_t w = writeIdx_.load(std::memory_order_acquire);
        size_t r = readIdx_.load(std::memory_order_relaxed);
        if (w == r)
            return false;
        // Skip to latest: read the item just before the current write position
        out = buffer_[(w - 1) % Capacity];
        readIdx_.store(w, std::memory_order_release);
        return true;
    }

private:
    std::array<T, Capacity> buffer_;
    alignas(64) std::atomic<size_t> writeIdx_{0};
    alignas(64) std::atomic<size_t> readIdx_{0};
};
