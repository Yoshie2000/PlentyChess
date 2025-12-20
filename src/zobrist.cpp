#include <random>
#include <iostream>
#include <cassert>

#include "zobrist.h"
#include "board.h"
#include "move.h"

namespace Zobrist {

    Hash PIECE_SQUARES[2][Piece::TOTAL][64];
    Hash STM_BLACK;
    Hash NO_PAWNS;
    Hash CASTLING[16];
    Hash ENPASSENT[8];
    Hash FMR[100 / FMR_GRANULARITY];

    Hash CUCKOO_HASHES[8192];
    Move CUCKOO_MOVES[8192];

    void init() {
        std::mt19937 rng;
        rng.seed(934572);
        std::uniform_int_distribution<Hash> dist;

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
        CASTLING[Castling::getMask(Color::WHITE, Castling::KINGSIDE)] = dist(rng);
        CASTLING[Castling::getMask(Color::WHITE, Castling::QUEENSIDE)] = dist(rng);
        CASTLING[Castling::getMask(Color::BLACK, Castling::KINGSIDE)] = dist(rng);
        CASTLING[Castling::getMask(Color::BLACK, Castling::QUEENSIDE)] = dist(rng);
        for (uint8_t i = 0; i < 16; i++) {
            if (BB::popcount(i) < 2)
                continue;
            
            Hash hash = 0;
            if (i & Castling::getMask(Color::WHITE, Castling::KINGSIDE))
                hash ^= CASTLING[Castling::getMask(Color::WHITE, Castling::KINGSIDE)];
            if (i & Castling::getMask(Color::WHITE, Castling::QUEENSIDE))
                hash ^= CASTLING[Castling::getMask(Color::WHITE, Castling::QUEENSIDE)];
            if (i & Castling::getMask(Color::BLACK, Castling::KINGSIDE))
                hash ^= CASTLING[Castling::getMask(Color::BLACK, Castling::KINGSIDE)];
            if (i & Castling::getMask(Color::BLACK, Castling::QUEENSIDE))
                hash ^= CASTLING[Castling::getMask(Color::BLACK, Castling::QUEENSIDE)];
            CASTLING[i] = hash;
        }

        for (int i = 0; i < 8; i++) {
            ENPASSENT[i] = dist(rng);
        }

        for (int i = 0; i < 100 / FMR_GRANULARITY; i++) {
            FMR[i] = dist(rng);
        }

        int count = 0;
        for (Color side = Color::WHITE; side <= Color::BLACK; ++side) {
            for (Piece piece = Piece::PAWN; piece < Piece::TOTAL; ++piece) {
                for (Square squareA = 0; squareA < 64; squareA++) {
                    for (Square squareB = squareA + 1; squareB < 64; squareB++) {
                        // Check if there is a reversible move between squareA and squareB
                        if (piece != Piece::PAWN && (BB::attackedSquares(piece, squareA, bitboard(0), side) & bitboard(squareB))) {
                            Move move = Move::makeNormal(squareA, squareB);
                            Hash hash = PIECE_SQUARES[side][piece][squareA] ^ PIECE_SQUARES[side][piece][squareB] ^ STM_BLACK;
                            int i = H1(hash);
                            // Find an empty slot in the cuckoo table for this move/hash combination
                            while (true) {
                                std::swap(CUCKOO_HASHES[i], hash);
                                std::swap(CUCKOO_MOVES[i], move);
                                if (!move)
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