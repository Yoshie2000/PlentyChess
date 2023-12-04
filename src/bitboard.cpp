#include <stdio.h>

#include "board.h"
#include "types.h"
#include "magic.h"

Bitboard BETWEEN[64][64];
Bitboard LINE[64][64];

void initBitboard() {
    for (Square a = 0; a < 64; a++) {
        for (Square b = 0; b < 64; b++) {
            LINE[a][b] = C64(0);
            if (getBishopMoves(a, C64(0)) & (C64(1) << b))
                LINE[a][b] |= getBishopMoves(a, C64(0)) & getBishopMoves(b, C64(0));
            if (getRookMoves(a, C64(0)) & (C64(1) << b))
                LINE[a][b] |= getRookMoves(a, C64(0)) & getRookMoves(b, C64(0));
            LINE[a][b] |= (C64(1) << a) | (C64(1) << b);

            BETWEEN[a][b] = C64(0);
            BETWEEN[a][b] |= getBishopMoves(a, C64(1) << b) & getBishopMoves(b, C64(1) << a);
            BETWEEN[a][b] |= getRookMoves(a, C64(1) << b) & getRookMoves(b, C64(1) << a);
            BETWEEN[a][b] |= C64(1) << b;
            BETWEEN[a][b] &= LINE[a][b];
        }
    }
}