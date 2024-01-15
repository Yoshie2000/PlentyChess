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
    uint32_t hash = 0;
    Move bestMove = MOVE_NONE;
    uint8_t depth = 0;
    uint8_t flags = TT_NOBOUND;
    Eval eval = EVAL_NONE;
    Eval value = EVAL_NONE;

    void update(uint64_t _hash, Move _bestMove, uint8_t _depth, Eval _eval, Eval _value, bool wasPv, int flags) {
        hash = (uint32_t)_hash;
        bestMove = _bestMove;
        depth = _depth;
        value = _value;
        eval = _eval;
        flags = (uint8_t)(flags + (wasPv << 2));
    }
};

inline void* alignedAlloc(size_t alignment, size_t requiredBytes) {
#if defined(__MINGW32__)
    int offset = alignment - 1;
    void* p = (void*)malloc(requiredBytes + offset);
    void* q = (void*)(((size_t)(p)+offset) & ~(alignment - 1));
    return q;
#elif defined (__GNUC__)
    return std::aligned_alloc(alignment, requiredBytes);
#else
#error "Compiler not supported"
#endif
}

class TranspositionTable {

    TTEntry* table = nullptr;
    size_t entryCount;

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

        entryCount = mb * 1024 * 1024 / sizeof(TTEntry);
        table = static_cast<TTEntry*>(alignedAlloc(sizeof(TTEntry), entryCount * sizeof(TTEntry)));

        clear();
    }

    size_t index(uint64_t hash) {
        // Find entry
        __extension__ using uint128 = unsigned __int128;
        return ((uint128)hash * (uint128)entryCount) >> 64;
    }

    void prefetch(uint64_t hash) {
        __builtin_prefetch(&table[index(hash)]);
    }

    TTEntry* probe(uint64_t hash, bool* found) {
        TTEntry* entry = &table[index(hash)];
        *found = entry->hash == (uint32_t)hash;
        return entry;
    }

    void clear() {
        for (size_t i = 0; i < entryCount; i++) {
            table[i] = TTEntry();
        }
    }

};

extern TranspositionTable TT;