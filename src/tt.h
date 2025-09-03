#pragma once

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>
#include <string.h>

#if defined(__linux__)
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
    Move bestMove = 0;
    int16_t depth = 0;
    int16_t eval = 0;
    int16_t value = 0;
    uint8_t flags = 0;
    int8_t bestMoveExtension = 0;

    constexpr Move getMove() { return bestMove; };
    constexpr int16_t getDepth() { return depth; };
    constexpr uint8_t getFlag() { return flags & 0x3; };
    constexpr int8_t getBestMoveExtension() { return bestMoveExtension; };
    constexpr Eval getEval() { return eval; };
    constexpr Eval getValue() { return value; };
    constexpr bool getTtPv() { return flags & 0x4; };

    void update(uint64_t _hash, Move _bestMove, int16_t _depth, Eval _eval, Eval _value, int8_t _bestMoveExtension, bool wasPv, int _flags);
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

    size_t index(uint64_t hash) {
        // Find entry
        __extension__ using uint128 = unsigned __int128;
        return ((uint128)hash * (uint128)clusterCount) >> 64;
    }

    void prefetch(uint64_t hash) {
        __builtin_prefetch(&table[index(hash)]);
    }

    TTEntry* probe(uint64_t hash, bool* found);

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
    }

};

extern TranspositionTable TT;
