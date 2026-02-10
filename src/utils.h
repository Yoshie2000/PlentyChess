#pragma once

#include "types.h"

#if defined(__linux__)
#include <sys/mman.h>
#endif

inline void* alignedAlloc(size_t alignment, size_t requiredBytes) {
    void* ptr;
#if defined(_WIN32)
    ptr = _aligned_malloc(requiredBytes, alignment);
#else
    ptr = std::aligned_alloc(alignment, requiredBytes);
#endif

#if defined(__linux__)
    madvise(ptr, requiredBytes, MADV_HUGEPAGE);
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