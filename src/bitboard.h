#pragma once

#include "types.h"

namespace BB {

    extern Bitboard BETWEEN[64][64];
    extern Bitboard LINE[64][64];
    extern Bitboard KING_ATTACKS[64];
    extern Bitboard KNIGHT_ATTACKS[64];

    Bitboard kingAttacks(Square origin);
    Bitboard knightAttacks(Bitboard knightBB);

    void init();

}