#pragma once

#include "types.h"

extern Bitboard BETWEEN[64][64];
extern Bitboard LINE[64][64];
extern Bitboard KING_ATTACKS[64];

void initBitboard();