#include "tt.h"
#include "move.h"

void __attribute__((no_sanitize("thread"))) TTEntry::update(uint64_t _hash, Move _bestMove, uint8_t _depth, Eval _eval, Eval _value, bool wasPv, int _flags) {
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

TTEntry* __attribute__((no_sanitize("thread"))) TranspositionTable::probe(uint64_t hash, bool* found) {
    TTCluster* cluster = &table[index(hash)];
    uint64_t hash16 = (uint16_t)hash;

    TTEntry* replace = &cluster->entries[0];

    for (int i = 0; i < CLUSTER_SIZE; i++) {
        if (cluster->entries[i].hash == hash16 || !cluster->entries[i].isInitialised()) {
            // Refresh generation
            cluster->entries[i].flags = (uint8_t)(TT_GENERATION_COUNTER | (cluster->entries[i].flags & (GENERATION_DELTA - 1)));
            *found = cluster->entries[i].hash == hash16;
            return &cluster->entries[i];
        }

        if (i > 0) {
            // Check if this entry would be better suited for replacement than the current replace entry
            int replaceValue = replace->depth - ((GENERATION_CYCLE + TT_GENERATION_COUNTER - replace->flags) & GENERATION_MASK);
            int entryValue = cluster->entries[i].depth - ((GENERATION_CYCLE + TT_GENERATION_COUNTER - cluster->entries[i].flags) & GENERATION_MASK);
            if ((!cluster->entries[i].isInitialised() && replace->isInitialised()) || replaceValue > entryValue)
                replace = &cluster->entries[i];
        }
    }

    *found = false;
    return replace;
}

uint8_t TT_GENERATION_COUNTER = 0;
TranspositionTable TT;