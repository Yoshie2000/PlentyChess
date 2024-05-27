#pragma once

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>

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

#define TT_DEPTH_OFFSET -7

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
        // Preserve existing moves for the same position
        if (_bestMove != MOVE_NONE || (uint16_t)_hash != hash)
            bestMove = _bestMove;

        if (_flags == TT_EXACTBOUND || (uint16_t)_hash != hash || _depth - TT_DEPTH_OFFSET + 2 * wasPv > depth - 4) {
            hash = (uint16_t)_hash;
            if (_bestMove != MOVE_NONE)
                bestMove = _bestMove;
            depth = _depth - TT_DEPTH_OFFSET;
            value = _value;
            eval = _eval;
            flags = (uint8_t)(_flags + (wasPv << 2)) | TT_GENERATION_COUNTER;
        }
    }
};

template <typename T, std::size_t N = 16>
class AlignmentAllocator {
public:

    typedef T value_type;
    typedef std::size_t size_type;
    typedef std::ptrdiff_t difference_type;

    inline AlignmentAllocator() throw () {}

    template <typename T2>
    inline AlignmentAllocator(const AlignmentAllocator<T2, N>&) throw () {}

    inline ~AlignmentAllocator() throw () {}

    inline T* adress(T& r) {
        return &r;
    }

    inline const T* adress(const T& r) const {
        return &r;
    }

    inline T* allocate(size_type n) {
        T* p = (T*)aligned_alloc(N, n * sizeof(value_type));
#if defined(__linux__)
        madvise(p, n * sizeof(value_type), MADV_HUGEPAGE);
#endif 
        return p;
    }

    inline void deallocate(T* p, size_type) {
        free(p);
    }

    inline void construct(T* p, const value_type& wert) {
        new (p) value_type(wert);
    }

    inline void destroy(T* p) {
        p->~value_type();
    }

    inline size_type max_size() const throw () {
        return size_type(-1) / sizeof(value_type);
    }

    template <typename T2>
    struct rebind {
        typedef AlignmentAllocator<T2, N> other;
    };

    bool operator!=(const AlignmentAllocator<T, N>& other) const {
        return !(*this == other);
    }

    bool operator==(const AlignmentAllocator<T, N>& other) const {
        return true;
    }
};

#define CLUSTER_SIZE 3 // Bits reserved for bound & ttPv 
#define GENERATION_PADDING 3
#define GENERATION_DELTA (1 << GENERATION_PADDING)
#define GENERATION_CYCLE 255 + GENERATION_DELTA
#define GENERATION_MASK (0xFF << GENERATION_PADDING) & 0xFF

struct __attribute__((aligned(32))) TTCluster {
    alignas(16) TTEntry entries[CLUSTER_SIZE];
    char padding[2]; // ca. 1.4Mnps
};

static_assert(sizeof(TTCluster) == 32, "TTCluster size not correct!");

class TranspositionTable {

    std::vector<TTCluster, AlignmentAllocator<TTCluster, 64>> table;
    size_t clusterCount;

public:

    TranspositionTable() {
        resize(16);
    }

    ~TranspositionTable() {
        table.clear();
    }

    void newSearch() {
        TT_GENERATION_COUNTER += GENERATION_DELTA;
    }

    void resize(size_t mb) {
        table.clear();
        clusterCount = mb * 1024 * 1024 / sizeof(TTCluster);
        table.resize(clusterCount);
    }

    size_t index(uint64_t hash) {
        // Find entry
        __extension__ using uint128 = unsigned __int128;
        return ((uint128)hash * (uint128)clusterCount) >> 64;
    }

    void prefetch(uint64_t hash) {
        __builtin_prefetch(&table[index(hash)]);
    }

    __attribute_noinline__ TTEntry* probe(uint64_t hash, bool* found) {
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