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

#define TT_NOBOUND 0
#define TT_UPPERBOUND 1
#define TT_LOWERBOUND 2
#define TT_EXACTBOUND 3

extern uint64_t ZOBRIST_PIECE_SQUARES[2][PIECE_TYPES][64];
extern uint64_t ZOBRIST_STM_BLACK;
extern uint64_t ZOBRIST_NO_PAWNS;
extern uint64_t ZOBRIST_CASTLING[16]; // 2^4
extern uint64_t ZOBRIST_ENPASSENT[8]; // 8 files

extern uint64_t CUCKOO_HASHES[8192];
extern Move CUCKOO_MOVES[8192];

extern uint8_t TT_GENERATION_COUNTER;

constexpr int H1(uint64_t h) { return h & 0x1fff; }
constexpr int H2(uint64_t h) { return (h >> 16) & 0x1fff; }

void initZobrist();

struct TTEntry {
    uint16_t hash = 0;
    Move bestMove = MOVE_NONE;
    uint8_t depth = 0;
    uint8_t flags = TT_NOBOUND;
    int16_t eval = EVAL_NONE;
    int16_t value = EVAL_NONE;

    void update(uint64_t _hash, Move _bestMove, uint8_t _depth, Eval _eval, Eval _value, bool wasPv, int _flags) {
        // Update bestMove if not MOVE_NONE
        // Or even clear move for a different position
        if (_bestMove != MOVE_NONE || (uint16_t)_hash != hash)
            bestMove = _bestMove;

        if (_flags == TT_EXACTBOUND || (uint16_t)_hash != hash || _depth + 2 * wasPv + 4 > depth) {
            hash = (uint16_t)_hash;
            depth = _depth;
            value = _value;
            eval = _eval;
            flags = (uint8_t)(_flags + (wasPv << 2)) | TT_GENERATION_COUNTER;
        }
    }
};

inline void* alignedAlloc(size_t alignment, size_t requiredBytes) {
    void* ptr;
#if defined(__MINGW32__)
    int offset = alignment - 1;
    void* p = (void*)malloc(requiredBytes + offset);
    ptr = (void*)(((size_t)(p)+offset) & ~(alignment - 1));
#elif defined (__GNUC__)
    ptr = std::aligned_alloc(alignment, requiredBytes);
#else
#error "Compiler not supported"
#endif

#if defined(__linux__)
    madvise(ptr, requiredBytes, MADV_HUGEPAGE);
#endif 

    return ptr;
}

#define CLUSTER_SIZE 3 // Bits reserved for bound & ttPv 
#define GENERATION_PADDING 3
#define GENERATION_DELTA (1 << GENERATION_PADDING)
#define GENERATION_CYCLE 255 + GENERATION_DELTA
#define GENERATION_MASK (0xFF << GENERATION_PADDING) & 0xFF

struct TTCluster {
    TTEntry entries[CLUSTER_SIZE];
    char padding[2];
};

static_assert(sizeof(TTCluster) == 32, "TTCluster size not correct!");

class TranspositionTable {

    TTCluster* table = nullptr;
    size_t clusterCount;

public:

    TranspositionTable() {
        resize(16);
    }

    ~TranspositionTable() {
        std::free(table);
    }

    void newSearch() {
        TT_GENERATION_COUNTER += GENERATION_DELTA;
    }

    void resize(size_t mb) {
        if (table)
            std::free(table);

        clusterCount = mb * 1024 * 1024 / sizeof(TTCluster);
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

    TTEntry* probe(uint64_t hash, bool* found) {
        TTCluster* cluster = &table[index(hash)];
        uint64_t hash16 = (uint16_t)hash;

        TTEntry* replace = &cluster->entries[0];

        for (int i = 0; i < CLUSTER_SIZE; i++) {
            if (cluster->entries[i].hash == hash16 || !cluster->entries[i].depth) {
                // Refresh generation
                cluster->entries[i].flags = (uint8_t)(TT_GENERATION_COUNTER | (cluster->entries[i].flags & (GENERATION_DELTA - 1)));
                *found = cluster->entries[i].hash == hash16;
                return &cluster->entries[i];
            }

            if (i > 0) {
                // Check if this entry would be better suited for replacement than the current replace entry
                int replaceValue = replace->depth - ((GENERATION_CYCLE + TT_GENERATION_COUNTER - replace->flags) & GENERATION_MASK);
                int entryValue = cluster->entries[i].depth - ((GENERATION_CYCLE + TT_GENERATION_COUNTER - cluster->entries[i].flags) & GENERATION_MASK);
                if (replaceValue > entryValue)
                    replace = &cluster->entries[i];
            }
        }

        *found = false;
        return replace;
    }

    int hashfull() {
        int count = 0;
        for (int i = 0; i < 1000; i++) {
            for (int j = 0; j < CLUSTER_SIZE; j++) {
                if ((table[i].entries[j].flags & GENERATION_MASK) == TT_GENERATION_COUNTER && table[i].entries[j].eval != EVAL_NONE)
                    count++;
            }
        }
        return count / CLUSTER_SIZE;
    }

    void clear() {
        for (size_t i = 0; i < clusterCount; i++) {
            table[i] = TTCluster();
        }
    }

};

extern TranspositionTable TT;