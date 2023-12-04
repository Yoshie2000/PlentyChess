#ifndef BITBOARD_H_INCLUDED
#define BITBOARD_H_INCLUDED

#include "types.h"

extern Bitboard BETWEEN[64][64];
extern Bitboard LINE[64][64];

void initBitboard();

#endif