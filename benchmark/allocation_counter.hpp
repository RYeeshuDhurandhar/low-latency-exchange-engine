#pragma once

#include <atomic>
#include <cstdint>

struct AllocationStats {
    std::atomic<std::uint64_t> alloc_count{0};
    std::atomic<std::uint64_t> alloc_bytes{0};

    void reset() {
        alloc_count.store(0, std::memory_order_relaxed);
        alloc_bytes.store(0, std::memory_order_relaxed);
    }
};

extern AllocationStats g_alloc_stats;
