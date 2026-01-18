#pragma once

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cstring>
#include <iostream>
#ifndef ARCH_WASM
#include <thread>
#endif
#include <vector>
#include <string.h>

#if defined(__linux__) && !defined(ARCH_WASM)
#include <sys/mman.h>
#endif

#include "types.h"
#include "move.h"
#include "evaluation.h"
#include "uci.h"

inline void* alignedAlloc(size_t alignment, size_t requiredBytes) {
    void* ptr;
#if defined(_WIN32)
    ptr = _aligned_malloc(requiredBytes, alignment);
#else
    ptr = std::aligned_alloc(alignment, requiredBytes);
#endif

#if defined(__linux__) && !defined(ARCH_WASM)
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

constexpr uint8_t TT_NOBOUND = 0;
constexpr uint8_t TT_UPPERBOUND = 1;
constexpr uint8_t TT_LOWERBOUND = 2;
constexpr uint8_t TT_EXACTBOUND = 3;

constexpr int CLUSTER_SIZE = 5;

constexpr int GENERATION_PADDING = 3; // Reserved bits for flag / ttPv
constexpr int GENERATION_DELTA = (1 << GENERATION_PADDING);
constexpr int GENERATION_CYCLE = 255 + GENERATION_DELTA;
constexpr int GENERATION_MASK = (0xFF << GENERATION_PADDING) & 0xFF;

extern uint8_t TT_GENERATION_COUNTER;

struct TTEntry {
    uint16_t hash = 0;
    Move bestMove = Move::none();
    Depth depth = 0;
    Eval eval = 0;
    Eval value = 0;
    uint8_t flags = 0;
    uint8_t rule50 = 0;

    constexpr Move getMove() { return bestMove; };
    constexpr Depth getDepth() { return depth; };
    constexpr uint8_t getFlag() { return flags & 0x3; };
    constexpr uint8_t getRule50() { return rule50; };
    constexpr Eval getEval() { return eval; };
    constexpr Eval getValue() { return value; };
    constexpr bool getTtPv() { return flags & 0x4; };

    void update(Hash _hash, Move _bestMove, Depth _depth, Eval _eval, Eval _value, uint8_t rule50, bool wasPv, int _flags);
    bool isInitialised() { return hash != 0; };
};

struct TTCluster {
    TTEntry entries[CLUSTER_SIZE];
    char padding[4];
};

static_assert(sizeof(TTCluster) == 64, "TTCluster size not correct!");

class TranspositionTable {

    TTCluster* table = nullptr;
    size_t clusterCount = 0;

public:

    TranspositionTable() {
#ifdef PROFILE_GENERATE
        resize(64);
#else
        resize(16);
#endif
    }

    ~TranspositionTable() {
        alignedFree(table);
    }

    void newSearch() {
        TT_GENERATION_COUNTER += GENERATION_DELTA;
    }

    void resize(size_t mb) {
        size_t newClusterCount = mb * 1024 * 1024 / sizeof(TTCluster);
        if (newClusterCount == clusterCount)
            return;
        
        if (table)
            alignedFree(table);
        
        clusterCount = newClusterCount;
        table = static_cast<TTCluster*>(alignedAlloc(sizeof(TTCluster), clusterCount * sizeof(TTCluster)));

        clear();
    }

    size_t index(Hash hash) {
        // Find entry using mulhi64 (high 64 bits of 64x64 multiplication)
#if defined(ARCH_WASM)
        // Portable implementation for Wasm
        uint64_t a = hash;
        uint64_t b = clusterCount;
        uint64_t a_lo = a & 0xFFFFFFFF;
        uint64_t a_hi = a >> 32;
        uint64_t b_lo = b & 0xFFFFFFFF;
        uint64_t b_hi = b >> 32;

        uint64_t p0 = a_lo * b_lo;
        uint64_t p1 = a_lo * b_hi;
        uint64_t p2 = a_hi * b_lo;
        uint64_t p3 = a_hi * b_hi;

        uint64_t carry = ((p0 >> 32) + (p1 & 0xFFFFFFFF) + (p2 & 0xFFFFFFFF)) >> 32;
        return p3 + (p1 >> 32) + (p2 >> 32) + carry;
#else
        __extension__ using uint128 = unsigned __int128;
        return ((uint128)hash * (uint128)clusterCount) >> 64;
#endif
    }

    void prefetch(Hash hash) {
        __builtin_prefetch(&table[index(hash)]);
    }

    TTEntry* probe(Hash hash, bool* found);

    int hashfull() {
        int count = 0;
        for (int i = 0; i < 1000; i++) {
            for (int j = 0; j < CLUSTER_SIZE; j++) {
                if ((table[i].entries[j].flags & GENERATION_MASK) == TT_GENERATION_COUNTER && table[i].entries[j].isInitialised())
                    count++;
            }
        }
        return count / CLUSTER_SIZE;
    }

    void clear() {
#ifdef ARCH_WASM
        // Single-threaded clear for Wasm
        std::memset(static_cast<void*>(table), 0, sizeof(TTCluster) * clusterCount);
#else
        size_t threadCount = UCI::Options.threads.value;
        std::vector<std::thread> ts;

        for (size_t thread = 0; thread < threadCount; thread++) {
            size_t startCluster = thread * (clusterCount / threadCount);
            size_t endCluster = thread == threadCount - 1 ? clusterCount : (thread + 1) * (clusterCount / threadCount);
            ts.push_back(std::thread([this, startCluster, endCluster]() {
                std::memset(static_cast<void*>(&table[startCluster]), 0, sizeof(TTCluster) * (endCluster - startCluster));
            }));
        }

        for (auto& t : ts) {
            t.join();
        }
#endif
    }

};

extern TranspositionTable TT;
