#include <random>
#include <iostream>
#include <cassert>

#include "zobrist.h"
#include "board.h"
#include "move.h"

uint64_t ZOBRIST_PIECE_SQUARES[2][Piece::TOTAL][64];
uint64_t ZOBRIST_STM_BLACK;
uint64_t ZOBRIST_NO_PAWNS;
uint64_t ZOBRIST_CASTLING[16];
uint64_t ZOBRIST_ENPASSENT[8];

uint64_t CUCKOO_HASHES[8192];
Move CUCKOO_MOVES[8192];

void initZobrist() {
    std::mt19937 rng;
    rng.seed(934572);
    std::uniform_int_distribution<uint64_t> dist;

    for (Color side = Color::WHITE; side <= Color::BLACK; ++side) {
        for (Piece i = Piece::PAWN; i < Piece::TOTAL; ++i) {
            for (Square j = 0; j < 64; j++) {
                ZOBRIST_PIECE_SQUARES[side][i][j] = dist(rng);
            }
        }
    }
    ZOBRIST_STM_BLACK = dist(rng);
    ZOBRIST_NO_PAWNS = dist(rng);

    ZOBRIST_CASTLING[0] = 0;
    ZOBRIST_CASTLING[CASTLING_WHITE_KINGSIDE] = dist(rng);
    ZOBRIST_CASTLING[CASTLING_WHITE_QUEENSIDE] = dist(rng);
    ZOBRIST_CASTLING[CASTLING_BLACK_KINGSIDE] = dist(rng);
    ZOBRIST_CASTLING[CASTLING_BLACK_QUEENSIDE] = dist(rng);
    ZOBRIST_CASTLING[CASTLING_WHITE_KINGSIDE | CASTLING_WHITE_QUEENSIDE] = ZOBRIST_CASTLING[CASTLING_WHITE_KINGSIDE] ^ ZOBRIST_CASTLING[CASTLING_WHITE_QUEENSIDE];
    ZOBRIST_CASTLING[CASTLING_WHITE_KINGSIDE | CASTLING_BLACK_KINGSIDE] = ZOBRIST_CASTLING[CASTLING_WHITE_KINGSIDE] ^ ZOBRIST_CASTLING[CASTLING_BLACK_KINGSIDE];
    ZOBRIST_CASTLING[CASTLING_WHITE_KINGSIDE | CASTLING_BLACK_QUEENSIDE] = ZOBRIST_CASTLING[CASTLING_WHITE_KINGSIDE] ^ ZOBRIST_CASTLING[CASTLING_BLACK_QUEENSIDE];
    ZOBRIST_CASTLING[CASTLING_WHITE_QUEENSIDE | CASTLING_BLACK_KINGSIDE] = ZOBRIST_CASTLING[CASTLING_WHITE_QUEENSIDE] ^ ZOBRIST_CASTLING[CASTLING_BLACK_KINGSIDE];
    ZOBRIST_CASTLING[CASTLING_WHITE_QUEENSIDE | CASTLING_BLACK_QUEENSIDE] = ZOBRIST_CASTLING[CASTLING_WHITE_QUEENSIDE] ^ ZOBRIST_CASTLING[CASTLING_BLACK_QUEENSIDE];
    ZOBRIST_CASTLING[CASTLING_BLACK_KINGSIDE | CASTLING_BLACK_QUEENSIDE] = ZOBRIST_CASTLING[CASTLING_BLACK_KINGSIDE] ^ ZOBRIST_CASTLING[CASTLING_BLACK_QUEENSIDE];
    ZOBRIST_CASTLING[CASTLING_WHITE_KINGSIDE | CASTLING_WHITE_QUEENSIDE | CASTLING_BLACK_KINGSIDE] = ZOBRIST_CASTLING[CASTLING_WHITE_KINGSIDE] ^ ZOBRIST_CASTLING[CASTLING_WHITE_QUEENSIDE] ^ ZOBRIST_CASTLING[CASTLING_BLACK_KINGSIDE];
    ZOBRIST_CASTLING[CASTLING_WHITE_KINGSIDE | CASTLING_WHITE_QUEENSIDE | CASTLING_BLACK_QUEENSIDE] = ZOBRIST_CASTLING[CASTLING_WHITE_KINGSIDE] ^ ZOBRIST_CASTLING[CASTLING_WHITE_QUEENSIDE] ^ ZOBRIST_CASTLING[CASTLING_BLACK_QUEENSIDE];
    ZOBRIST_CASTLING[CASTLING_WHITE_KINGSIDE | CASTLING_BLACK_KINGSIDE | CASTLING_BLACK_QUEENSIDE] = ZOBRIST_CASTLING[CASTLING_WHITE_KINGSIDE] ^ ZOBRIST_CASTLING[CASTLING_BLACK_KINGSIDE] ^ ZOBRIST_CASTLING[CASTLING_BLACK_QUEENSIDE];
    ZOBRIST_CASTLING[CASTLING_WHITE_QUEENSIDE | CASTLING_BLACK_KINGSIDE | CASTLING_BLACK_QUEENSIDE] = ZOBRIST_CASTLING[CASTLING_WHITE_QUEENSIDE] ^ ZOBRIST_CASTLING[CASTLING_BLACK_KINGSIDE] ^ ZOBRIST_CASTLING[CASTLING_BLACK_QUEENSIDE];
    ZOBRIST_CASTLING[CASTLING_WHITE_KINGSIDE | CASTLING_WHITE_QUEENSIDE | CASTLING_BLACK_KINGSIDE | CASTLING_BLACK_QUEENSIDE] = ZOBRIST_CASTLING[CASTLING_WHITE_KINGSIDE] ^ ZOBRIST_CASTLING[CASTLING_WHITE_QUEENSIDE] ^ ZOBRIST_CASTLING[CASTLING_BLACK_KINGSIDE] ^ ZOBRIST_CASTLING[CASTLING_BLACK_QUEENSIDE];

    for (int i = 0; i < 8; i++) {
        ZOBRIST_ENPASSENT[i] = dist(rng);
    }

    int count = 0;
    for (Color side = Color::WHITE; side <= Color::BLACK; ++side) {
        for (Piece piece = Piece::PAWN; piece < Piece::TOTAL; ++piece) {
            for (Square squareA = 0; squareA < 64; squareA++) {
                for (Square squareB = squareA + 1; squareB < 64; squareB++) {
                    // Check if there is a reversible move between squareA and squareB
                    if (piece != Piece::PAWN && (BB::attackedSquares(piece, squareA, bitboard(0), side) & bitboard(squareB))) {
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