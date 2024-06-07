#pragma once

#include "types.h"

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

    constexpr Bitboard pawnAttacksLeft(Bitboard pawns, Color side) {
        return side == COLOR_WHITE ?
            ((pawns & ~BB::FILE_A) << 7) :
            ((pawns & ~BB::FILE_A) >> 9);
    }

    constexpr Bitboard pawnAttacksRight(Bitboard pawns, Color side) {
        return side == COLOR_WHITE ?
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