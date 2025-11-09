#include <random>
#include <iostream>
#include <cassert>

#include "zobrist.h"
#include "board.h"
#include "move.h"

namespace Zobrist {

    uint64_t PIECE_SQUARES[Piece::TOTAL_PIECES][64];
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

        for (Piece i = Piece::WHITE_PAWN; i < Piece::TOTAL_PIECES; ++i) {
            for (Square j = 0; j < 64; j++) {
                PIECE_SQUARES[i][j] = dist(rng);
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

            uint64_t hash = 0;
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

        int count = 0;
        for (Color side = Color::WHITE; side <= Color::BLACK; ++side) {
            for (PieceType piece = PieceType::PAWN; piece < PieceType::TOTAL; ++piece) {
                for (Square squareA = 0; squareA < 64; squareA++) {
                    for (Square squareB = squareA + 1; squareB < 64; squareB++) {
                        // Check if there is a reversible move between squareA and squareB
                        if (piece != PieceType::PAWN && (BB::attackedSquares(piece, squareA, bitboard(0), side) & bitboard(squareB))) {
                            Move move = createMove(squareA, squareB);
                            uint64_t hash = PIECE_SQUARES[makePiece(piece, side)][squareA] ^ PIECE_SQUARES[makePiece(piece, side)][squareB] ^ STM_BLACK;
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