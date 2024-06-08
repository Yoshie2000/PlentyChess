#pragma once

#include "types.h"

// Sliding piece stuff
constexpr uint8_t DIRECTION_UP = 0;
constexpr uint8_t DIRECTION_RIGHT = 1;
constexpr uint8_t DIRECTION_DOWN = 2;
constexpr uint8_t DIRECTION_LEFT = 3;
constexpr uint8_t DIRECTION_UP_RIGHT = 4;
constexpr uint8_t DIRECTION_DOWN_RIGHT = 5;
constexpr uint8_t DIRECTION_DOWN_LEFT = 6;
constexpr uint8_t DIRECTION_UP_LEFT = 7;
constexpr uint8_t DIRECTION_NONE = 8;

// Start & End indices in the direction indices for each piece type
constexpr uint8_t DIRECTIONS[Piece::TOTAL][2] = {
    { DIRECTION_NONE, DIRECTION_NONE }, // pawn
    { DIRECTION_NONE, DIRECTION_NONE }, // knight
    { DIRECTION_UP_RIGHT, DIRECTION_UP_LEFT }, // bishop
    { DIRECTION_UP, DIRECTION_LEFT }, // rook
    { DIRECTION_UP, DIRECTION_UP_LEFT }, // queen
    { DIRECTION_UP, DIRECTION_UP_LEFT }, // king
};

constexpr int8_t DIRECTION_DELTAS[8] = { 8, 1, -8, -1, 9, -7, -9, 7 };

// For a given start square and direction, contains information which square is the last possible one a sliding piece could slide to
constexpr Square LASTSQ_TABLE[64][8] = {
    { 56, 7, 0, 0, 63, 0, 0, 0 },
    { 57, 7, 1, 0, 55, 1, 1, 8 },
    { 58, 7, 2, 0, 47, 2, 2, 16 },
    { 59, 7, 3, 0, 39, 3, 3, 24 },
    { 60, 7, 4, 0, 31, 4, 4, 32 },
    { 61, 7, 5, 0, 23, 5, 5, 40 },
    { 62, 7, 6, 0, 15, 6, 6, 48 },
    { 63, 7, 7, 0, 7, 7, 7, 56 },
    { 56, 15, 0, 8, 62, 1, 8, 8 },
    { 57, 15, 1, 8, 63, 2, 0, 16 },
    { 58, 15, 2, 8, 55, 3, 1, 24 },
    { 59, 15, 3, 8, 47, 4, 2, 32 },
    { 60, 15, 4, 8, 39, 5, 3, 40 },
    { 61, 15, 5, 8, 31, 6, 4, 48 },
    { 62, 15, 6, 8, 23, 7, 5, 56 },
    { 63, 15, 7, 8, 15, 15, 6, 57 },
    { 56, 23, 0, 16, 61, 2, 16, 16 },
    { 57, 23, 1, 16, 62, 3, 8, 24 },
    { 58, 23, 2, 16, 63, 4, 0, 32 },
    { 59, 23, 3, 16, 55, 5, 1, 40 },
    { 60, 23, 4, 16, 47, 6, 2, 48 },
    { 61, 23, 5, 16, 39, 7, 3, 56 },
    { 62, 23, 6, 16, 31, 15, 4, 57 },
    { 63, 23, 7, 16, 23, 23, 5, 58 },
    { 56, 31, 0, 24, 60, 3, 24, 24 },
    { 57, 31, 1, 24, 61, 4, 16, 32 },
    { 58, 31, 2, 24, 62, 5, 8, 40 },
    { 59, 31, 3, 24, 63, 6, 0, 48 },
    { 60, 31, 4, 24, 55, 7, 1, 56 },
    { 61, 31, 5, 24, 47, 15, 2, 57 },
    { 62, 31, 6, 24, 39, 23, 3, 58 },
    { 63, 31, 7, 24, 31, 31, 4, 59 },
    { 56, 39, 0, 32, 59, 4, 32, 32 },
    { 57, 39, 1, 32, 60, 5, 24, 40 },
    { 58, 39, 2, 32, 61, 6, 16, 48 },
    { 59, 39, 3, 32, 62, 7, 8, 56 },
    { 60, 39, 4, 32, 63, 15, 0, 57 },
    { 61, 39, 5, 32, 55, 23, 1, 58 },
    { 62, 39, 6, 32, 47, 31, 2, 59 },
    { 63, 39, 7, 32, 39, 39, 3, 60 },
    { 56, 47, 0, 40, 58, 5, 40, 40 },
    { 57, 47, 1, 40, 59, 6, 32, 48 },
    { 58, 47, 2, 40, 60, 7, 24, 56 },
    { 59, 47, 3, 40, 61, 15, 16, 57 },
    { 60, 47, 4, 40, 62, 23, 8, 58 },
    { 61, 47, 5, 40, 63, 31, 0, 59 },
    { 62, 47, 6, 40, 55, 39, 1, 60 },
    { 63, 47, 7, 40, 47, 47, 2, 61 },
    { 56, 55, 0, 48, 57, 6, 48, 48 },
    { 57, 55, 1, 48, 58, 7, 40, 56 },
    { 58, 55, 2, 48, 59, 15, 32, 57 },
    { 59, 55, 3, 48, 60, 23, 24, 58 },
    { 60, 55, 4, 48, 61, 31, 16, 59 },
    { 61, 55, 5, 48, 62, 39, 8, 60 },
    { 62, 55, 6, 48, 63, 47, 0, 61 },
    { 63, 55, 7, 48, 55, 55, 1, 62 },
    { 56, 63, 0, 56, 56, 7, 56, 56 },
    { 57, 63, 1, 56, 57, 15, 48, 57 },
    { 58, 63, 2, 56, 58, 23, 40, 58 },
    { 59, 63, 3, 56, 59, 31, 32, 59 },
    { 60, 63, 4, 56, 60, 39, 24, 60 },
    { 61, 63, 5, 56, 61, 47, 16, 61 },
    { 62, 63, 6, 56, 62, 55, 8, 62 },
    { 63, 63, 7, 56, 63, 63, 0, 63 }
};

namespace BB {

    constexpr Bitboard FILE_A = 0x0101010101010101;
    constexpr Bitboard FILE_B = 0x0202020202020202;
    constexpr Bitboard FILE_C = 0x0404040404040404;
    constexpr Bitboard FILE_D = 0x0808080808080808;
    constexpr Bitboard FILE_E = 0x1010101010101010;
    constexpr Bitboard FILE_F = 0x2020202020202020;
    constexpr Bitboard FILE_G = 0x4040404040404040;
    constexpr Bitboard FILE_H = 0x8080808080808080;

    constexpr Bitboard RANK_1 = 0x00000000000000FF;
    constexpr Bitboard RANK_2 = 0x000000000000FF00;
    constexpr Bitboard RANK_3 = 0x0000000000FF0000;
    constexpr Bitboard RANK_4 = 0x00000000FF000000;
    constexpr Bitboard RANK_5 = 0x000000FF00000000;
    constexpr Bitboard RANK_6 = 0x0000FF0000000000;
    constexpr Bitboard RANK_7 = 0x00FF000000000000;
    constexpr Bitboard RANK_8 = 0xFF00000000000000;

    extern Bitboard BETWEEN[64][64];
    extern Bitboard LINE[64][64];
    extern Bitboard KING_ATTACKS[64];
    extern Bitboard KNIGHT_ATTACKS[64];

    constexpr int popcount(Bitboard bb) {
        return __builtin_popcountll(bb);
    }
    constexpr int popcount(int number) {
        return __builtin_popcount(number);
    }

    constexpr Bitboard pawnAttacksLeft(Bitboard pawns, Color side) {
        return side == Color::WHITE ?
            ((pawns & ~BB::FILE_A) << 7) :
            ((pawns & ~BB::FILE_A) >> 9);
    }

    constexpr Bitboard pawnAttacksRight(Bitboard pawns, Color side) {
        return side == Color::WHITE ?
            ((pawns & ~BB::FILE_H) << 9) :
            ((pawns & ~BB::FILE_H) >> 7);
    }

    constexpr Bitboard pawnAttacks(Bitboard pawns, Color side) {
        return pawnAttacksLeft(pawns, side) | pawnAttacksRight(pawns, side);
    }

    Bitboard kingAttacks(Square origin);
    Bitboard knightAttacks(Bitboard knightBB);
    Bitboard attackedSquares(Piece pieceType, Square square, Bitboard occupied, Color stm);

    void init();

}