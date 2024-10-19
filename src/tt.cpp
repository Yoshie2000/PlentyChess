#include "tt.h"
#include "move.h"

void TTEntry::update(uint64_t _hash, Move _bestMove, int16_t _depth, Eval _eval, Eval _value, bool wasPv, int _flags) {
    // Update bestMove if not MOVE_NONE
    // Or even clear move for a different position
    if (_bestMove != MOVE_NONE || (uint16_t)_hash != hash)
        bestMove = _bestMove;

    if (_flags == TT_EXACTBOUND || (uint16_t)_hash != hash || depth == NO_DEPTH || _depth + 2 * DEPTH_SCALING * wasPv + 4 * DEPTH_SCALING > depth) {
        hash = (uint16_t)_hash;
        depth = _depth;
        value = _value;
        eval = _eval;
        flags = (uint8_t)(_flags + (wasPv << 2)) | TT_GENERATION_COUNTER;
    }
}

TTEntry* TranspositionTable::probe(uint64_t hash, bool* found) {
    TTCluster* cluster = &table[index(hash)];
    uint64_t hash16 = (uint16_t)hash;

    TTEntry* replace = &cluster->entries[0];

    for (int i = 0; i < CLUSTER_SIZE; i++) {
        if (cluster->entries[i].hash == hash16 || cluster->entries[i].depth == NO_DEPTH) {
            // Refresh generation
            cluster->entries[i].flags = (uint8_t)(TT_GENERATION_COUNTER | (cluster->entries[i].flags & (GENERATION_DELTA - 1)));
            *found = cluster->entries[i].hash == hash16;
            return &cluster->entries[i];
        }

        if (i > 0) {
            // Check if this entry would be better suited for replacement than the current replace entry
            int replaceValue = replace->depth - DEPTH_SCALING * ((GENERATION_CYCLE + TT_GENERATION_COUNTER - replace->flags) & GENERATION_MASK);
            int entryValue = cluster->entries[i].depth - DEPTH_SCALING * ((GENERATION_CYCLE + TT_GENERATION_COUNTER - cluster->entries[i].flags) & GENERATION_MASK);
            if ((cluster->entries[i].depth == NO_DEPTH && replace->depth != NO_DEPTH) || replaceValue > entryValue)
                replace = &cluster->entries[i];
        }
    }

    *found = false;
    return replace;
}

uint8_t TT_GENERATION_COUNTER = 0;
TranspositionTable TT;