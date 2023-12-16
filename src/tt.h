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

extern uint64_t ZOBRIST_PIECE_SQUARES[PIECE_TYPES][64];
extern uint64_t ZOBRIST_STM_BLACK;
extern uint64_t ZOBRIST_CASTLING[16]; // 2^4
extern uint64_t ZOBRIST_ENPASSENT[8]; // 8 files

void initZobrist();

struct TTEntry {
    uint16_t hash;
    Move bestMove;
    uint8_t depth;
    Eval value;

    void update(uint64_t _hash, Move _bestMove, uint8_t _depth, Eval _value) {
        if (_depth >= depth || bestMove == MOVE_NONE) {
            hash = (uint16_t) _hash;
            bestMove = _bestMove;
            depth = _depth;
            value = _value;
        }
    }
};

#define CLUSTER_SIZE 4

struct TTCluster {
    TTEntry entry[CLUSTER_SIZE];
};

class TranspositionTable {

    TTCluster* table;
    size_t clusterCount;

public:

    TranspositionTable() {
        size_t mb = 64;

        clusterCount = mb * 1024 * 1024 / sizeof(TTCluster);
        table = static_cast<TTCluster*>(std::aligned_alloc(sizeof(TTCluster), clusterCount * sizeof(TTCluster)));

        clear();
    }

    ~TranspositionTable() {
        std::free(table);
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
        size_t maxMemsetInput = 65536;
        size_t clustersAtOnce = maxMemsetInput / sizeof(TTCluster);

        for (size_t i = 0; i < clusterCount; i += clustersAtOnce) {
            std::memset(&table[i], 0, clustersAtOnce * sizeof(TTCluster));
        }
    }

};

extern TranspositionTable TT;

void initHistory();
extern int quietHistory[2][64][64];

#endif