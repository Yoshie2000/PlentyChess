#include <stdio.h>

#include "board.h"
#include "types.h"
#include "magic.h"
#include "move.h"

namespace BB {

    Bitboard BETWEEN[64][64];
    Bitboard LINE[64][64];
    Bitboard KING_ATTACKS[64];
    Bitboard KNIGHT_ATTACKS[64];

    Bitboard kingAttacks(Square origin) {
        Bitboard attacksBB = bitboard(0);

        int8_t direction;
        Square lastSquare, toSquare;
        Bitboard toSquareBB;

        for (direction = DIRECTIONS[PIECE_KING][0]; direction <= DIRECTIONS[PIECE_KING][1]; direction++) {
            lastSquare = LASTSQ_TABLE[origin][direction];
            toSquare = origin + DIRECTION_DELTAS[direction];
            if (toSquare >= 64) continue;

            toSquareBB = bitboard(toSquare);
            if (origin != lastSquare && toSquareBB)
                attacksBB |= toSquareBB;
        }
        return attacksBB;
    }

    Bitboard knightAttacks(Bitboard knightBB) {
        Bitboard l1 = (knightBB >> 1) & bitboard(0x7f7f7f7f7f7f7f7f);
        Bitboard l2 = (knightBB >> 2) & bitboard(0x3f3f3f3f3f3f3f3f);
        Bitboard r1 = (knightBB << 1) & bitboard(0xfefefefefefefefe);
        Bitboard r2 = (knightBB << 2) & bitboard(0xfcfcfcfcfcfcfcfc);
        Bitboard h1 = l1 | r1;
        Bitboard h2 = l2 | r2;
        return (h1 << 16) | (h1 >> 16) | (h2 << 8) | (h2 >> 8);
    }

    Bitboard attackedSquares(Piece pieceType, Square square, Bitboard occupied, Color stm) {
        switch (pieceType) {
        case PIECE_PAWN:
            return pawnAttacks(bitboard(square), stm);
        case PIECE_KNIGHT:
            return KNIGHT_ATTACKS[square];
        case PIECE_KING:
            return KING_ATTACKS[square];
        case PIECE_BISHOP:
            return getBishopMoves(square, occupied);
        case PIECE_ROOK:
            return getRookMoves(square, occupied);
        default:
            return getBishopMoves(square, occupied) | getRookMoves(square, occupied);
        }
    }

    void init() {
        for (Square a = 0; a < 64; a++) {
            for (Square b = 0; b < 64; b++) {
                LINE[a][b] = bitboard(0);
                if (getBishopMoves(a, bitboard(0)) & bitboard(b))
                    LINE[a][b] |= getBishopMoves(a, bitboard(0)) & getBishopMoves(b, bitboard(0));
                if (getRookMoves(a, bitboard(0)) & bitboard(b))
                    LINE[a][b] |= getRookMoves(a, bitboard(0)) & getRookMoves(b, bitboard(0));
                LINE[a][b] |= bitboard(a) | bitboard(b);

                BETWEEN[a][b] = bitboard(0);
                BETWEEN[a][b] |= getBishopMoves(a, bitboard(b)) & getBishopMoves(b, bitboard(a));
                BETWEEN[a][b] |= getRookMoves(a, bitboard(b)) & getRookMoves(b, bitboard(a));
                BETWEEN[a][b] |= bitboard(b);
                BETWEEN[a][b] &= LINE[a][b];
            }

            KING_ATTACKS[a] = kingAttacks(a);
            KNIGHT_ATTACKS[a] = knightAttacks(bitboard(a));
        }
    }

}