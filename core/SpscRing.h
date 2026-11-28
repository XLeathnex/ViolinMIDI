// ViolinMIDI core - a lock-free ring between exactly two threads.
//
// One thread pushes (the producer), one thread pops (the consumer): the audio
// thread and a worker, or a MIDI thread and the audio thread. Nothing here
// waits, locks or allocates after the constructor, so either side may be the
// audio thread. Every new queue to or from the audio thread is one of these.
#pragma once

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

namespace vm
{

/** Single producer, single consumer, wait-free, fixed capacity (a power of two), for trivially
    copyable T. The constructor allocates; push() and pop() never do. The producer only writes
    `head`, the consumer only writes `tail`; the 64-bit counters do not wrap. */
template <typename T>
class SpscRing
{
    static_assert (std::is_trivially_copyable_v<T>, "SpscRing copies its items as plain memory");

public:
    /** Message thread (or before either side runs). `capacityPow2` must be a power of two. */
    explicit SpscRing (size_t capacityPow2)
        : buffer (capacityPow2), mask (capacityPow2 - 1)
    {
        assert (capacityPow2 > 0 && (capacityPow2 & (capacityPow2 - 1)) == 0);
    }

    SpscRing (const SpscRing&) = delete;
    SpscRing& operator= (const SpscRing&) = delete;

    size_t capacity() const noexcept   { return buffer.size(); }

    /** Producer side: room for this many items now (more may become free). */
    size_t freeSpace() const noexcept
    {
        return capacity() - static_cast<size_t> (head.load (std::memory_order_relaxed) - tail.load (std::memory_order_acquire));
    }

    /** Consumer side: this many items can be popped now (more may arrive). */
    size_t available() const noexcept
    {
        return static_cast<size_t> (head.load (std::memory_order_acquire) - tail.load (std::memory_order_relaxed));
    }

    /** Producer: all or nothing. False, and nothing is written, if they do not fit. */
    bool push (const T* items, size_t count) noexcept
    {
        const uint64_t h = head.load (std::memory_order_relaxed);
        const uint64_t t = tail.load (std::memory_order_acquire);
        if (count > capacity() - static_cast<size_t> (h - t))
            return false;

        const size_t start = static_cast<size_t> (h) & mask;
        const size_t first = std::min (count, capacity() - start);
        std::copy_n (items, first, buffer.data() + start);
        std::copy_n (items + first, count - first, buffer.data());

        head.store (h + count, std::memory_order_release);
        return true;
    }

    /** Consumer: what is there, up to `maxCount`. Returns how many were copied to `dest`. */
    size_t pop (T* dest, size_t maxCount) noexcept
    {
        const uint64_t t = tail.load (std::memory_order_relaxed);
        const uint64_t h = head.load (std::memory_order_acquire);
        const size_t count = std::min (maxCount, static_cast<size_t> (h - t));

        const size_t start = static_cast<size_t> (t) & mask;
        const size_t first = std::min (count, capacity() - start);
        std::copy_n (buffer.data() + start, first, dest);
        std::copy_n (buffer.data(), count - first, dest + first);

        tail.store (t + count, std::memory_order_release);
        return count;
    }

    /** Consumer: skips up to `count` items without reading them. */
    void discard (size_t count) noexcept
    {
        const uint64_t t = tail.load (std::memory_order_relaxed);
        const uint64_t h = head.load (std::memory_order_acquire);
        tail.store (t + std::min (static_cast<uint64_t> (count), h - t), std::memory_order_release);
    }

private:
    std::vector<T> buffer;
    size_t mask;
    alignas (64) std::atomic<uint64_t> head { 0 };
    alignas (64) std::atomic<uint64_t> tail { 0 };
};

namespace calib
{
    /** One block of input the calibration captured (AudioHost -> capture thread, through a
        SpscRing next to the samples): its first sample since the device started, its length,
        how many analysis hops the engine had made after it, the host time (0 if unknown) and
        the capture session it belongs to. The samples are pushed first, then the stamp. */
    struct BlockStamp
    {
        long long firstSample = 0;
        int       frames = 0;
        long long hopsAfter = 0;
        uint64_t  hostTimeNs = 0;
        uint32_t  generation = 0;
    };
}

} // namespace vm
