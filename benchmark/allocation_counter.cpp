#include "allocation_counter.hpp"

#include <cstdlib>
#include <new>

AllocationStats g_alloc_stats;

void* operator new(std::size_t size) {
    g_alloc_stats.alloc_count.fetch_add(1, std::memory_order_relaxed);
    g_alloc_stats.alloc_bytes.fetch_add(size, std::memory_order_relaxed);

    void* ptr = std::malloc(size);
    if(ptr == nullptr) {
        throw std::bad_alloc();
    }

    return ptr;
}

void operator delete(void* ptr) noexcept {
    std::free(ptr);
}

void* operator new[](std::size_t size) {
    g_alloc_stats.alloc_count.fetch_add(1, std::memory_order_relaxed);
    g_alloc_stats.alloc_bytes.fetch_add(size, std::memory_order_relaxed);

    void* ptr = std::malloc(size);
    if(ptr == nullptr) {
        throw std::bad_alloc();
    }

    return ptr;
}

void operator delete[](void* ptr) noexcept {
    std::free(ptr);
}

// Sized delete overloads.
// Some compilers call these when -fsized-deallocation is enabled.
void operator delete(void* ptr, std::size_t) noexcept {
    std::free(ptr);
}

void operator delete[](void* ptr, std::size_t) noexcept {
    std::free(ptr);
}
