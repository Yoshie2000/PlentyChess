#include <random>
#include <iostream>

#include "tt.h"

uint64_t ZOBRIST_PIECE_SQUARES[PIECE_TYPES][64];
uint64_t ZOBRIST_STM_BLACK;
uint64_t ZOBRIST_CASTLING[16];
uint64_t ZOBRIST_ENPASSENT[8];

TranspositionTable TT;
int quietHistory[2][64][64];
Move counterMoves[64][64];

void initHistory() {
    memset(quietHistory, 0, sizeof(quietHistory));
    memset(counterMoves, MOVE_NONE, sizeof(counterMoves));
}

void initZobrist() {
    std::mt19937 rng;
    rng.seed(934572);
    std::uniform_int_distribution<uint64_t> dist;

    for (int i = 0; i < PIECE_TYPES; i++) {
        for (int j = 0; j < 64; j++) {
            ZOBRIST_PIECE_SQUARES[i][j] = dist(rng);
        }
    }
    ZOBRIST_STM_BLACK = dist(rng);
    for (int i = 0; i < 16; i++) {
        ZOBRIST_CASTLING[i] = dist(rng);
    }
    for (int i = 0; i < 8; i++) {
        ZOBRIST_ENPASSENT[i] = dist(rng);
    }
}