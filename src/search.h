#ifndef SEARCH_H_INCLUDED
#define SEARCH_H_INCLUDED

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

void initReductions();

struct SearchParameters {
    bool perft; // Perft (requires depth)

    std::vector<Move> searchmoves; // TODO: Search only these moves at root
    bool ponder; // TODO: Search in pondering mode => after "ponderhit", continue on ponder move
    uint64_t wtime; // TODO: White's remaining time (ms)
    uint64_t btime; // TODO: Black's remaining time (ms)
    uint64_t winc; // TODO: White's increment per move (ms)
    uint64_t binc; // TODO: Black's increment per move (ms)
    int movestogo; // TODO: Moves to the next time control
    int depth; // Search depth
    uint64_t nodes; // TODO: Search exactly this many nodes
    int mate; // TODO: Search for mate in X moves
    uint64_t movetime; // TODO: Search exactly this many ms
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

struct SearchStack {
    Move* pv;
    int ply;
    uint64_t nodes;

    Move killers[2];
};

enum NodeType {
    ROOT_NODE,
    PV_NODE,
    NON_PV_NODE
};

uint64_t perft(Board* board, int depth);

#endif