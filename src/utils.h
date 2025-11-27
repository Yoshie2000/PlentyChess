#pragma once

// ArrayVec
template<class T, size_t MAX>
class ArrayVec {
public:

    ArrayVec() : _size(0) {}

    void add(const T& element) {
        assert(_size < MAX);
        elements[_size++] = element;
    }

    T remove(size_t i) {
        T removed = elements[i];
        elements[i] = elements[--_size];
        return removed;
    }

    T& operator[](size_t i) {
        assert(i < _size);
        return elements[i];
    }

    T const& operator[](size_t i) const {
        assert(i < _size);
        return elements[i];
    }

    int size() const {
        return _size;
    }

    int capacity() const {
        return MAX;
    }

    const T* begin() const { return elements; }
    const T* end() const { return elements + _size; }

private:
    T elements[MAX];
    size_t _size{};
};

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