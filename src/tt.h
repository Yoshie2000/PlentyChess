#pragma once

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>
#include <string.h>

#include "types.h"
#include "movepicker.h"
#include "evaluation.h"
#include "uci.h"

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
    int16_t eval = 0;
    int16_t value = 0;
    uint8_t flags = 0;
    uint8_t rule50 = 0;

    constexpr Move getMove() { return bestMove; };
    constexpr Depth getDepth() { return depth; };
    constexpr uint8_t getFlag() { return flags & 0x3; };
    constexpr uint8_t getRule50() { return rule50; };
    constexpr Value getEval() { return eval; };
    constexpr Value getValue() { return value; };
    constexpr bool getTtPv() { return flags & 0x4; };

    void update(Hash _hash, Move _bestMove, Depth _depth, Value _eval, Value _value, uint8_t rule50, bool wasPv, int _flags);
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

    TranspositionTable();
    ~TranspositionTable();

    void newSearch();
    void resize(size_t mb);

    size_t index(Hash hash) {
        return (u128(hash) * u128(clusterCount)) >> 64;
    }

    void prefetch(Hash hash) {
        __builtin_prefetch(&table[index(hash)]);
    }

    TTEntry* probe(Hash hash, bool* found);
    int hashfull();
    void clear();

};

extern TranspositionTable TT;
