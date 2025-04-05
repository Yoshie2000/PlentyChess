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

    Bitboard DIRECTION_BETWEEN[64][64];

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


                if (fileOf(a) == fileOf(b)) {
                    DIRECTION_BETWEEN[a][b] = b > a ? DIRECTION_UP : DIRECTION_DOWN;
                }
                else if (rankOf(a) == rankOf(b)) {
                    DIRECTION_BETWEEN[a][b] = b > a ? DIRECTION_RIGHT : DIRECTION_LEFT;
                }
                else if (std::abs(fileOf(a) - fileOf(b)) == std::abs(rankOf(a) - rankOf(b))) {
                    if (b > a) {
                        if (fileOf(b) > fileOf(a)) {
                            DIRECTION_BETWEEN[a][b] = DIRECTION_UP_RIGHT;
                        }
                        else {
                            DIRECTION_BETWEEN[a][b] = DIRECTION_UP_LEFT;
                        }
                    }
                    else {
                        if (fileOf(b) > fileOf(a)) {
                            DIRECTION_BETWEEN[a][b] = DIRECTION_DOWN_RIGHT;
                        }
                        else {
                            DIRECTION_BETWEEN[a][b] = DIRECTION_DOWN_LEFT;
                        }
                    }
                }
                else {
                    DIRECTION_BETWEEN[a][b] = DIRECTION_NONE;
                }
            }

            KING_ATTACKS[a] = kingAttacks(a);
            KNIGHT_ATTACKS[a] = knightAttacks(bitboard(a));
        }
    }

}