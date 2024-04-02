#include <stdio.h>

#include "board.h"
#include "types.h"
#include "magic.h"
#include "move.h"

Bitboard BETWEEN[64][64];
Bitboard LINE[64][64];
Bitboard KING_ATTACKS[64];
Bitboard KNIGHT_ATTACKS[64];
Bitboard PAWN_ATTACKS[64][2];

Bitboard kingAttacks(Square origin) {
    Bitboard attacksBB = C64(0);

    int8_t direction;
    Square lastSquare, toSquare;
    Bitboard toSquareBB;

    for (direction = DIRECTIONS[PIECE_KING][0]; direction <= DIRECTIONS[PIECE_KING][1]; direction++) {
        lastSquare = LASTSQ_TABLE[origin][direction];
        toSquare = origin + DIRECTION_DELTAS[direction];
        if (toSquare >= 64) continue;

        toSquareBB = C64(1) << toSquare;
        if (origin != lastSquare && toSquareBB)
            attacksBB |= toSquareBB;
    }
    return attacksBB;
}

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

        KING_ATTACKS[a] = kingAttacks(a);
        KNIGHT_ATTACKS[a] = knightAttacks(C64(1) << a);
        PAWN_ATTACKS[a][COLOR_WHITE] = pawnAttacks((C64(1) << a), COLOR_WHITE);
        PAWN_ATTACKS[a][COLOR_BLACK] = pawnAttacks((C64(1) << a), COLOR_BLACK);
    }
}