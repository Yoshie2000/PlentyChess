#pragma once

#include "types.h"

#if defined(__linux__)
#include <sys/mman.h>
#endif

constexpr size_t HUGE_PAGE_SIZE = 2 * 1024 * 1024;

inline void* alignedAlloc(size_t alignment, size_t requiredBytes) {
    bool hugePages = requiredBytes >= HUGE_PAGE_SIZE;
    if (hugePages) {
        alignment = HUGE_PAGE_SIZE;
        requiredBytes = (requiredBytes + HUGE_PAGE_SIZE - 1) & ~(HUGE_PAGE_SIZE - 1);
    }

    void* ptr;
#if defined(_WIN32)
    ptr = _aligned_malloc(requiredBytes, alignment);
#else
    ptr = std::aligned_alloc(alignment, requiredBytes);
#endif

#if defined(__linux__)
    if (hugePages) {
        madvise(ptr, requiredBytes, MADV_HUGEPAGE);
    }
#endif

    return ptr;
}

inline void alignedFree(void* ptr) {
#if defined(_WIN32)
    _aligned_free(ptr);
#else
    std::free(ptr);
#endif
}