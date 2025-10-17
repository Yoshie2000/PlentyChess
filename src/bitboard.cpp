#include <stdio.h>
#include <iostream>

#include "board.h"
#include "types.h"
#include "magic.h"
#include "move.h"

namespace BB {

    Bitboard BETWEEN[64][64];
    Bitboard LINE[64][64];
    Bitboard KING_ATTACKS[64];
    Bitboard KNIGHT_ATTACKS[64];
    Bitboard RAY_PASS[64][64];

    Bitboard kingAttacks(Square origin) {
        Bitboard attacksBB = bitboard(0);

        int8_t direction;
        Square lastSquare, toSquare;
        Bitboard toSquareBB;

        for (direction = DIRECTIONS[Piece::KING][0]; direction <= DIRECTIONS[Piece::KING][1]; direction++) {
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
        Bitboard l1 = (knightBB >> 1) & Bitboard(0x7f7f7f7f7f7f7f7f);
        Bitboard l2 = (knightBB >> 2) & Bitboard(0x3f3f3f3f3f3f3f3f);
        Bitboard r1 = (knightBB << 1) & Bitboard(0xfefefefefefefefe);
        Bitboard r2 = (knightBB << 2) & Bitboard(0xfcfcfcfcfcfcfcfc);
        Bitboard h1 = l1 | r1;
        Bitboard h2 = l2 | r2;
        return (h1 << 16) | (h1 >> 16) | (h2 << 8) | (h2 >> 8);
    }

    Bitboard attackedSquares(Piece pieceType, Square square, Bitboard occupied, Color stm) {
        switch (pieceType) {
        case Piece::PAWN:
            return pawnAttacks(bitboard(square), stm);
        case Piece::KNIGHT:
            return KNIGHT_ATTACKS[square];
        case Piece::KING:
            return KING_ATTACKS[square];
        case Piece::BISHOP:
            return getBishopMoves(square, occupied);
        case Piece::ROOK:
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

                for (Piece piece : {Piece::BISHOP, Piece::ROOK}) {
                    Bitboard pseudoAttacks = piece == Piece::ROOK ? getRookMoves(a, 0) : getBishopMoves(a, 0);
                    if (pseudoAttacks & bitboard(b)) {
                        RAY_PASS[a][b] = pseudoAttacks;
                        RAY_PASS[a][b] &= (piece == Piece::ROOK ? getRookMoves(b, bitboard(a)) : getBishopMoves(b, bitboard(a))) | bitboard(b);
                        RAY_PASS[a][b] &= ~BETWEEN[a][b];
                    }
                }
            }

            KING_ATTACKS[a] = kingAttacks(a);
            KNIGHT_ATTACKS[a] = knightAttacks(bitboard(a));
        }
    }

}