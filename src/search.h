#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <inttypes.h>
#include <vector>

#include "board.h"
#include "move.h"
#include "evaluation.h"

extern int REDUCTIONS[2][MAX_PLY][MAX_MOVES];
extern int SEE_MARGIN[MAX_PLY][2];
extern int LMP_MARGIN[MAX_PLY][2];

void initReductions();

uint64_t perft(Board* board, int depth);

struct SearchParameters {
    bool perft; // Perft (requires depth)

    std::vector<Move> searchmoves; // TODO: Search only these moves at root
    bool ponder; // TODO: Search in pondering mode => after "ponderhit", continue on ponder move
    uint64_t wtime; // White's remaining time (ms)
    uint64_t btime; // Black's remaining time (ms)
    uint64_t winc; // White's increment per move (ms)
    uint64_t binc; // Black's increment per move (ms)
    int movestogo; // Moves to the next time control
    int depth; // Search depth
    uint64_t nodes; // Search exactly this many nodes
    int mate; // TODO: Search for mate in X moves
    uint64_t movetime; // Search exactly this many ms
    bool infinite; // Search forever (until a stop / quit command)

    SearchParameters() {
        perft = false;

        searchmoves = std::vector<Move>();
        ponder = false;
        wtime = 0;
        btime = 0;
        winc = 0;
        binc = 0;
        movestogo = 0;
        depth = 0;
        nodes = 0;
        mate = 0;
        movetime = 0;
        infinite = true;
    }

};

struct SearchData {
    int nmpPlies;
    int rootDepth;
    int selDepth;

    uint64_t nodesSearched;

    int64_t startTime;
    int64_t optTime;
    int64_t maxTime;

    SearchData() {
        nmpPlies = 0;
        rootDepth = 0;
        nodesSearched = 0;
        startTime = 0;
        optTime = 0;
        maxTime = 0;
    }
};

enum NodeType {
    ROOT_NODE,
    PV_NODE,
    NON_PV_NODE
};