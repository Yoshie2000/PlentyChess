#include "tt.h"
#include "movepicker.h"
#include "spsa.h"

TUNE_INT(ttReplaceTtpvBonus, 214, 0, 400);
TUNE_INT(ttReplaceOffset, 451, 0, 800);

uint8_t TT_GENERATION_COUNTER = 0;
TranspositionTable TT;

void TTEntry::update(Hash _hash, Move _bestMove, Depth _depth, Value _eval, Value _value, uint8_t _rule50, bool wasPv, int _flags) {
    // Update bestMove if it exists
    // Or clear it for a different position
    if (_bestMove || (uint16_t)_hash != hash)
        bestMove = _bestMove;

    if (_flags == TT_EXACTBOUND || (uint16_t)_hash != hash || _depth + ttReplaceTtpvBonus * wasPv + ttReplaceOffset > depth) {
        hash = (uint16_t)_hash;
        depth = _depth;
        value = _value;
        eval = _eval;
        rule50 = _rule50;
        flags = (uint8_t)(_flags + (wasPv << 2)) | TT_GENERATION_COUNTER;
    }
}

TranspositionTable::TranspositionTable() {
#ifdef PROFILE_GENERATE
    resize(64);
#else
    resize(16);
#endif
}

TranspositionTable::~TranspositionTable() {
    alignedFree(table);
}

void TranspositionTable::newSearch() {
    TT_GENERATION_COUNTER += GENERATION_DELTA;
}

void TranspositionTable::resize(size_t mb) {
    size_t newClusterCount = mb * 1024 * 1024 / sizeof(TTCluster);
    if (newClusterCount == clusterCount)
        return;

    if (table)
        alignedFree(table);

    clusterCount = newClusterCount;
    table = static_cast<TTCluster*>(alignedAlloc(sizeof(TTCluster), clusterCount * sizeof(TTCluster)));

    clear();
}

TTEntry* TranspositionTable::probe(Hash hash, bool* found) {
    TTCluster* cluster = &table[index(hash)];
    uint16_t hash16 = (uint16_t)hash;

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
            int replaceValue = replace->depth - 100 * ((GENERATION_CYCLE + TT_GENERATION_COUNTER - replace->flags) & GENERATION_MASK);
            int entryValue = cluster->entries[i].depth - 100 * ((GENERATION_CYCLE + TT_GENERATION_COUNTER - cluster->entries[i].flags) & GENERATION_MASK);
            if ((!cluster->entries[i].isInitialised() && replace->isInitialised()) || replaceValue > entryValue)
                replace = &cluster->entries[i];
        }
    }

    *found = false;
    return replace;
}

int TranspositionTable::hashfull() {
    int count = 0;
    for (int i = 0; i < 1000; i++) {
        for (int j = 0; j < CLUSTER_SIZE; j++) {
            if ((table[i].entries[j].flags & GENERATION_MASK) == TT_GENERATION_COUNTER && table[i].entries[j].isInitialised())
                count++;
        }
    }
    return count / CLUSTER_SIZE;
}

void TranspositionTable::clear() {
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