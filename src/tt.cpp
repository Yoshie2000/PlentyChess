#include <random>
#include <iostream>

#include "tt.h"
#include "move.h"

uint64_t ZOBRIST_PIECE_SQUARES[2][PIECE_TYPES][64];
uint64_t ZOBRIST_STM_BLACK;
uint64_t ZOBRIST_NO_PAWNS;
uint64_t ZOBRIST_CASTLING[16];
uint64_t ZOBRIST_ENPASSENT[8];

uint64_t CUCKOO_HASHES[8192];
Move CUCKOO_MOVES[8192];

uint8_t TT_GENERATION_COUNTER = 0;

TranspositionTable TT;

void initZobrist() {
    std::mt19937 rng;
    rng.seed(934572);
    std::uniform_int_distribution<uint64_t> dist;

    for (int side = 0; side <= 1; side++) {
        for (int i = 0; i < PIECE_TYPES; i++) {
            for (int j = 0; j < 64; j++) {
                ZOBRIST_PIECE_SQUARES[side][i][j] = dist(rng);
            }
        }
    }
    ZOBRIST_STM_BLACK = dist(rng);
    ZOBRIST_NO_PAWNS = dist(rng);
    for (int i = 0; i < 16; i++) {
        ZOBRIST_CASTLING[i] = dist(rng);
    }
    for (int i = 0; i < 8; i++) {
        ZOBRIST_ENPASSENT[i] = dist(rng);
    }

    int count = 0;
    for (Color side = COLOR_WHITE; side <= COLOR_BLACK; side++) {
        for (Piece piece = 0; piece < PIECE_TYPES; piece++) {
            for (Square squareA = 0; squareA < 64; squareA++) {
                for (Square squareB = squareA + 1; squareB < 64; squareB++) {
                    // Check if there is a reversible move between squareA and squareB
                    if (piece != PIECE_PAWN && (attackedSquaresByPiece(piece, squareA, C64(0), side) & (C64(1) << squareB))) {
                        Move move = createMove(squareA, squareB);
                        uint64_t hash = ZOBRIST_PIECE_SQUARES[side][piece][squareA] ^ ZOBRIST_PIECE_SQUARES[side][piece][squareB] ^ ZOBRIST_STM_BLACK;
                        int i = H1(hash);
                        // Find an empty slot in the cuckoo table for this move/hash combination
                        while (true) {
                            std::swap(CUCKOO_HASHES[i], hash);
                            std::swap(CUCKOO_MOVES[i], move);
                            if (move == 0)
                                break;
                            i = (i == H1(hash)) ? H2(hash) : H1(hash);
                        }
                        count++;
                    }
                }
            }
        }
    }
    assert(count == 3668);
}