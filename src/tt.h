#ifndef TT_H_INCLUDED
#define TT_H_INCLUDED

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>
#include <string.h>

#include "types.h"
#include "move.h"
#include "evaluation.h"

#define TT_NOBOUND 0
#define TT_UPPERBOUND 1
#define TT_LOWERBOUND 2
#define TT_EXACTBOUND 3

extern uint64_t ZOBRIST_PIECE_SQUARES[PIECE_TYPES][64];
extern uint64_t ZOBRIST_STM_BLACK;
extern uint64_t ZOBRIST_CASTLING[16]; // 2^4
extern uint64_t ZOBRIST_ENPASSENT[8]; // 8 files

void initZobrist();

struct TTEntry {
    uint16_t hash = 0;
    Move bestMove = MOVE_NONE;
    uint8_t depth = 0;
    uint8_t flags = TT_NOBOUND;
    Eval eval = EVAL_NONE;
    Eval value = EVAL_NONE;

    void update(uint64_t _hash, Move _bestMove, uint8_t _depth, Eval _eval, Eval _value, bool wasPv, int flags) {
        if (_depth >= depth || bestMove == MOVE_NONE) {
            hash = (uint16_t)_hash;
            bestMove = _bestMove;
            depth = _depth;
            value = _value;
            eval = _eval;
            flags = (uint8_t)(flags + (wasPv << 2));
        }
    }
};

#define CLUSTER_SIZE 3

struct TTCluster {
    TTEntry entry[CLUSTER_SIZE];
    char padding[2];
};

inline void* alignedAlloc(size_t alignment, size_t requiredBytes) {
#if defined(__MINGW32__)
    int offset = alignment - 1;
    void* p = (void * ) malloc(requiredBytes + offset);
    void* q = (void * ) (((size_t)(p) + offset) & ~(alignment - 1));
    return q;
#elif defined (__GNUC__)
    return std::aligned_alloc(alignment, requiredBytes);
#else
    #error "Compiler not supported"
#endif
}

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

    void resize(size_t mb) {
        if (table)
            std::free(table);

        clusterCount = mb * 1024 * 1024 / sizeof(TTCluster);
        table = static_cast<TTCluster*>(alignedAlloc(sizeof(TTCluster), clusterCount * sizeof(TTCluster)));

        clear();
    }

    TTEntry* probe(uint64_t hash, bool* found) {
        // Find cluster
        __extension__ using uint128 = unsigned __int128;
        TTCluster* cluster = &table[((uint128)hash * (uint128)clusterCount) >> 64];

        int smallestDepth = 0;
        for (int i = 0; i < CLUSTER_SIZE; i++) {
            if (cluster->entry[i].hash == (uint16_t)hash) {
                *found = true;
                return &cluster->entry[i];
            }
            if (cluster->entry[i].depth < cluster->entry[smallestDepth].depth)
                smallestDepth = i;
        }
        *found = false;
        return &cluster->entry[smallestDepth];
    }

    void clear() {
        // size_t maxMemsetInput = 65536;
        // size_t clustersAtOnce = maxMemsetInput / sizeof(TTCluster);

        for (size_t i = 0; i < clusterCount; i++) {
            table[i] = TTCluster();
        }
    }

};

extern TranspositionTable TT;

#endif