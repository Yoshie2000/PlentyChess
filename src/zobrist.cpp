#include <random>
#include <iostream>
#include <cassert>

#include "zobrist.h"
#include "board.h"
#include "move.h"

namespace Zobrist {

    uint64_t PIECE_SQUARES[2][Piece::TOTAL][64];
    uint64_t STM_BLACK;
    uint64_t NO_PAWNS;
    uint64_t CASTLING[16];
    uint64_t ENPASSENT[8];

    uint64_t CUCKOO_HASHES[8192];
    Move CUCKOO_MOVES[8192];

    void init() {
        std::mt19937 rng;
        rng.seed(934572);
        std::uniform_int_distribution<uint64_t> dist;

        for (Color side = Color::WHITE; side <= Color::BLACK; ++side) {
            for (Piece i = Piece::PAWN; i < Piece::TOTAL; ++i) {
                for (Square j = 0; j < 64; j++) {
                    PIECE_SQUARES[side][i][j] = dist(rng);
                }
            }
        }
        STM_BLACK = dist(rng);
        NO_PAWNS = dist(rng);

        CASTLING[0] = 0;
        CASTLING[CASTLING_WHITE_KINGSIDE] = dist(rng);
        CASTLING[CASTLING_WHITE_QUEENSIDE] = dist(rng);
        CASTLING[CASTLING_BLACK_KINGSIDE] = dist(rng);
        CASTLING[CASTLING_BLACK_QUEENSIDE] = dist(rng);
        CASTLING[CASTLING_WHITE_KINGSIDE | CASTLING_WHITE_QUEENSIDE] = CASTLING[CASTLING_WHITE_KINGSIDE] ^ CASTLING[CASTLING_WHITE_QUEENSIDE];
        CASTLING[CASTLING_WHITE_KINGSIDE | CASTLING_BLACK_KINGSIDE] = CASTLING[CASTLING_WHITE_KINGSIDE] ^ CASTLING[CASTLING_BLACK_KINGSIDE];
        CASTLING[CASTLING_WHITE_KINGSIDE | CASTLING_BLACK_QUEENSIDE] = CASTLING[CASTLING_WHITE_KINGSIDE] ^ CASTLING[CASTLING_BLACK_QUEENSIDE];
        CASTLING[CASTLING_WHITE_QUEENSIDE | CASTLING_BLACK_KINGSIDE] = CASTLING[CASTLING_WHITE_QUEENSIDE] ^ CASTLING[CASTLING_BLACK_KINGSIDE];
        CASTLING[CASTLING_WHITE_QUEENSIDE | CASTLING_BLACK_QUEENSIDE] = CASTLING[CASTLING_WHITE_QUEENSIDE] ^ CASTLING[CASTLING_BLACK_QUEENSIDE];
        CASTLING[CASTLING_BLACK_KINGSIDE | CASTLING_BLACK_QUEENSIDE] = CASTLING[CASTLING_BLACK_KINGSIDE] ^ CASTLING[CASTLING_BLACK_QUEENSIDE];
        CASTLING[CASTLING_WHITE_KINGSIDE | CASTLING_WHITE_QUEENSIDE | CASTLING_BLACK_KINGSIDE] = CASTLING[CASTLING_WHITE_KINGSIDE] ^ CASTLING[CASTLING_WHITE_QUEENSIDE] ^ CASTLING[CASTLING_BLACK_KINGSIDE];
        CASTLING[CASTLING_WHITE_KINGSIDE | CASTLING_WHITE_QUEENSIDE | CASTLING_BLACK_QUEENSIDE] = CASTLING[CASTLING_WHITE_KINGSIDE] ^ CASTLING[CASTLING_WHITE_QUEENSIDE] ^ CASTLING[CASTLING_BLACK_QUEENSIDE];
        CASTLING[CASTLING_WHITE_KINGSIDE | CASTLING_BLACK_KINGSIDE | CASTLING_BLACK_QUEENSIDE] = CASTLING[CASTLING_WHITE_KINGSIDE] ^ CASTLING[CASTLING_BLACK_KINGSIDE] ^ CASTLING[CASTLING_BLACK_QUEENSIDE];
        CASTLING[CASTLING_WHITE_QUEENSIDE | CASTLING_BLACK_KINGSIDE | CASTLING_BLACK_QUEENSIDE] = CASTLING[CASTLING_WHITE_QUEENSIDE] ^ CASTLING[CASTLING_BLACK_KINGSIDE] ^ CASTLING[CASTLING_BLACK_QUEENSIDE];
        CASTLING[CASTLING_WHITE_KINGSIDE | CASTLING_WHITE_QUEENSIDE | CASTLING_BLACK_KINGSIDE | CASTLING_BLACK_QUEENSIDE] = CASTLING[CASTLING_WHITE_KINGSIDE] ^ CASTLING[CASTLING_WHITE_QUEENSIDE] ^ CASTLING[CASTLING_BLACK_KINGSIDE] ^ CASTLING[CASTLING_BLACK_QUEENSIDE];

        for (int i = 0; i < 8; i++) {
            ENPASSENT[i] = dist(rng);
        }

        int count = 0;
        for (Color side = Color::WHITE; side <= Color::BLACK; ++side) {
            for (Piece piece = Piece::PAWN; piece < Piece::TOTAL; ++piece) {
                for (Square squareA = 0; squareA < 64; squareA++) {
                    for (Square squareB = squareA + 1; squareB < 64; squareB++) {
                        // Check if there is a reversible move between squareA and squareB
                        if (piece != Piece::PAWN && (BB::attackedSquares(piece, squareA, bitboard(0), side) & bitboard(squareB))) {
                            Move move = createMove(squareA, squareB);
                            uint64_t hash = PIECE_SQUARES[side][piece][squareA] ^ PIECE_SQUARES[side][piece][squareB] ^ STM_BLACK;
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
        if (count || count == 0) { // ensure usage to avoid warning
            assert(count == 3668);
        }
    }

}