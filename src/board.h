#ifndef BOARD_H_INCLUDED
    #define BOARD_H_INCLUDED

    #include <stdint.h>
    #include <stdbool.h>
    #include <string>
    
    #include "types.h"
    #include "move.h"

    extern const Bitboard FILE_A;
    extern const Bitboard FILE_B;
    extern const Bitboard FILE_C;
    extern const Bitboard FILE_D;
    extern const Bitboard FILE_E;
    extern const Bitboard FILE_F;
    extern const Bitboard FILE_G;
    extern const Bitboard FILE_H;

    extern const Bitboard RANK_1;
    extern const Bitboard RANK_2;
    extern const Bitboard RANK_3;
    extern const Bitboard RANK_4;
    extern const Bitboard RANK_5;
    extern const Bitboard RANK_6;
    extern const Bitboard RANK_7;
    extern const Bitboard RANK_8;

    struct Board {
        Bitboard byPiece[2][PIECE_TYPES];
        Bitboard byColor[2];
        Bitboard board;
        Piece pieces[64];

        Color stm;
        uint8_t ply;
        uint8_t rule50_ply;

        struct BoardStack* stack;
    };

    struct BoardStack {        
        Piece capturedPiece;
        Bitboard enpassantTarget; // one-hot encoding -> 0 means no en passant possible

        // MEMCPY GOES FROM HERE
        Bitboard attackedByPiece[2][PIECE_TYPES];
        Bitboard attackedByColor[2];

        uint8_t castling; // 0000 -> black queenside, black kingside, white queenside, white kingside
        // TO HERE

        struct BoardStack* previous;
    };

    void startpos(struct Board* result);
    size_t parseFen(struct Board* board, std::string fen);

    void doMove(struct Board* board, struct BoardStack* newStack, Move move);
    void undoMove(struct Board* board, Move move);

    constexpr Bitboard pawnAttacksLeft(struct Board* board, Color side) {
        Bitboard pawns = board->byPiece[side][PIECE_PAWN];
        return side == COLOR_WHITE ?
            ((pawns & (~FILE_A)) << 7) :
            ((pawns & (~FILE_A)) >> 9);
    }

    constexpr Bitboard pawnAttacksRight(struct Board* board, Color side) {
        Bitboard pawns = board->byPiece[side][PIECE_PAWN];
        return side == COLOR_WHITE ?
            ((pawns & (~FILE_H)) << 9) :
            ((pawns & (~FILE_H)) >> 7);
    }

    constexpr Bitboard pawnAttacks(struct Board* board, Color side) {
        return pawnAttacksLeft(board, side) | pawnAttacksRight(board, side);
    }

    constexpr Bitboard knightAttacks(Bitboard knightBB) {
        Bitboard l1 = (knightBB >> 1) & C64(0x7f7f7f7f7f7f7f7f);
        Bitboard l2 = (knightBB >> 2) & C64(0x3f3f3f3f3f3f3f3f);
        Bitboard r1 = (knightBB << 1) & C64(0xfefefefefefefefe);
        Bitboard r2 = (knightBB << 2) & C64(0xfcfcfcfcfcfcfcfc);
        Bitboard h1 = l1 | r1;
        Bitboard h2 = l2 | r2;
        return (h1 << 16) | (h1 >> 16) | (h2 << 8) | (h2 >> 8);
    }

    constexpr Bitboard knightAttacksAll(struct Board* board, Color side) {
        Bitboard knights = board->byPiece[side][PIECE_KNIGHT];
        return knightAttacks(knights);
    }

    inline Bitboard slidingPieceAttacks(struct Board* board, Bitboard pieceBB) {
        Bitboard attacksBB = C64(0);
        Square origin = lsb(pieceBB);
        Piece pieceType = board->pieces[origin];

        int8_t delta, direction;
        Square lastSquare, toSquare;
        Bitboard toSquareBB;

        for (direction = DIRECTIONS[pieceType][0]; direction <= DIRECTIONS[pieceType][1]; direction++) {
            delta = DIRECTION_DELTAS[direction];
            lastSquare = LASTSQ_TABLE[origin][direction];

            if (origin != lastSquare)
                for (toSquare = origin + delta; (toSquareBB = C64(1) << toSquare); toSquare += delta) {
                    attacksBB |= toSquareBB;
                    if ((board->board & toSquareBB) || (toSquare == lastSquare))
                        break;
                }
        }
        return attacksBB;
    }

    inline Bitboard kingAttacks(struct Board* board, Color color) {
        Bitboard attacksBB = C64(0);
        Square origin = lsb(board->byPiece[color][PIECE_KING]);

        int8_t direction;
        Square lastSquare;
        Bitboard toSquareBB;

        for (direction = DIRECTIONS[PIECE_KING][0]; direction <= DIRECTIONS[PIECE_KING][1]; direction++) {
            lastSquare = LASTSQ_TABLE[origin][direction];

            toSquareBB = C64(1) << (origin + DIRECTION_DELTAS[direction]);
            if (origin != lastSquare && toSquareBB)
                attacksBB |= toSquareBB;
        }
        return attacksBB;
    }

    inline Bitboard slidingPieceAttacksAll(struct Board* board, Color side, Piece pieceType) {
        Bitboard attacksBB = C64(0);

        Bitboard pieces = board->byPiece[side][pieceType];
        while (pieces) {
            Bitboard pieceBB = C64(1) << popLSB(&pieces);
            attacksBB |= slidingPieceAttacks(board, pieceBB);
        }
        return attacksBB;
    }

    inline bool isSquareAttacked(struct Board* board, Square square, Color side) {
        Bitboard squareBB = C64(1) << square;

        if (pawnAttacks(board, side) & squareBB)
            return true;
        if (knightAttacksAll(board, side) & squareBB)
            return true;
        if (kingAttacks(board, side) & squareBB)
            return true;
        if (slidingPieceAttacksAll(board, side, PIECE_BISHOP) & squareBB)
            return true;
        if (slidingPieceAttacksAll(board, side, PIECE_ROOK) & squareBB)
            return true;
        if (slidingPieceAttacksAll(board, side, PIECE_QUEEN) & squareBB)
            return true;
        return false;
    }

    inline Bitboard attackedSquares(struct Board* board, Color side) {
        return
            pawnAttacks(board, side) |
            knightAttacksAll(board, side) |
            kingAttacks(board, side) |
            slidingPieceAttacksAll(board, side, PIECE_BISHOP) |
            slidingPieceAttacksAll(board, side, PIECE_ROOK) |
            slidingPieceAttacksAll(board, side, PIECE_QUEEN);
    }

    constexpr Bitboard attackedSquaresByPiece(struct Board* board, Color side, Piece pieceType) {
        switch (pieceType) {
        case PIECE_PAWN:
            return pawnAttacks(board, side);
        case PIECE_KNIGHT:
            return knightAttacksAll(board, side);
        case PIECE_KING:
            return kingAttacks(board, side);
        case PIECE_BISHOP:
            return slidingPieceAttacksAll(board, side, PIECE_BISHOP);
        case PIECE_ROOK:
            return slidingPieceAttacksAll(board, side, PIECE_ROOK);
        default:
            return slidingPieceAttacksAll(board, side, PIECE_QUEEN);
        }
    }

    constexpr bool isInCheck(struct Board* board, Color side) {
        Square kingSquare = lsb(board->byPiece[side][PIECE_KING]);
        Bitboard attackedEnemy = board->stack->attackedByColor[1 - side];
        return (C64(1) << kingSquare) & attackedEnemy;
    }

    void debugBoard(struct Board* board);
    void debugBitboard(Bitboard bb);

#endif