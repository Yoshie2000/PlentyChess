#include "tt.h"
#include "move.h"
#include "spsa.h"

TUNE_INT(ttReplaceTtpvBonus, 231, 0, 400);
TUNE_INT(ttReplaceOffset, 432, 0, 800);

void TTEntry::update(Hash _hash, Move _bestMove, Depth _depth, Eval _eval, Eval _value, bool wasPv, int _flags) {
    // Update bestMove if it exists
    // Or clear it for a different position
    if (_bestMove || !matches(_hash))
        bestMove = _bestMove;

    if (_flags == TT_EXACTBOUND || !matches(_hash) || _depth + ttReplaceTtpvBonus * wasPv + ttReplaceOffset > depth) {
        hash = (uint16_t)_hash;
        hashExtra = (uint8_t)(_hash >> 16);
        depth = _depth;
        value = _value;
        eval = _eval;
        flags = (uint8_t)(_flags + (wasPv << 2)) | TT_GENERATION_COUNTER;
    }
}

TTEntry* TranspositionTable::probe(Hash hash, bool* found) {
    TTCluster* cluster = &table[index(hash)];

    TTEntry* replace = &cluster->entries[0];

    for (int i = 0; i < CLUSTER_SIZE; i++) {
        if (cluster->entries[i].matches(hash) || !cluster->entries[i].isInitialised()) {
            // Refresh generation
            cluster->entries[i].flags = (uint8_t)(TT_GENERATION_COUNTER | (cluster->entries[i].flags & (GENERATION_DELTA - 1)));
            *found = cluster->entries[i].matches(hash);
            return &cluster->entries[i];
        }

        if (i > 0) {
            // Check if this entry would be better suited for replacement than the current replace entry
            int replaceValue = replace->depth - 100 * ((GENERATION_CYCLE + TT_GENERATION_COUNTER - replace->flags) & GENERATION_MASK);
            int entryValue = cluster->entries[i].depth - 100 * ((GENERATION_CYCLE + TT_GENERATION_COUNTER - cluster->entries[i].flags) & GENERATION_MASK);
            if ((!cluster->entries[i].isInitialised() && replace->isInitialised()) || replaceValue > entryValue)
                replace = &cluster->entries[i];
        }
    }

    *found = false;
    return replace;
}

uint8_t TT_GENERATION_COUNTER = 0;
TranspositionTable TT;
