#ifndef BOARD_H_INCLUDED
    #define BOARD_H_INCLUDED

    #include <stdint.h>
    #include <stdbool.h>
    
    #include "types.h"

    #define PIECE_TYPES 6

    #define PIECE_PAWN 0
    #define PIECE_KNIGHT 1
    #define PIECE_BISHOP 2
    #define PIECE_ROOK 3
    #define PIECE_QUEEN 4
    #define PIECE_KING 5
    #define NO_PIECE 6

    #define COLOR_WHITE 0
    #define COLOR_BLACK 1

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

        int stm;
        int ply;
        int rule50_ply;
        unsigned int castling; // 0000 -> black queenside, black kingside, white queenside, white kingside

        struct BoardStack* stack;
    };

    struct BoardStack {
        Piece capturedPiece;
        Bitboard enpassantTarget; // one-hot encoding -> 0 means no en passant possible

        Bitboard attackedByPiece[2][PIECE_TYPES];
        Bitboard attackedByColor[2];

        struct BoardStack* previous;
    };

    void startpos(struct Board* result);

    void doMove(struct Board* board, struct BoardStack* newStack, Move move);
    void undoMove(struct Board* board, Move move);

    inline Bitboard pawnAttacksLeft(struct Board* board, Color side);
    inline Bitboard pawnAttacksRight(struct Board* board, Color side);
    inline Bitboard pawnAttacks(struct Board* board, Color side);

    inline Bitboard knightAttacks(Bitboard knightBB);
    inline Bitboard knightAttacksAll(struct Board* board, Color side);

    inline Bitboard kingAttacks(struct Board* board, Color side);

    inline Bitboard slidingPieceAttacks(struct Board* board, Bitboard pieceBB);
    inline Bitboard slidingPieceAttacksAll(struct Board* board, Color side, Piece pieceType);

    inline bool isSquareAttacked(struct Board* board, Square square, Color side);
    inline Bitboard attackedSquares(struct Board* board, Color side);
    inline Bitboard attackedSquaresByPiece(struct Board* board, Color side, Piece pieceType);

    inline bool isInCheck(struct Board* board, Color side);

    void debugBoard(struct Board* board);
    void debugBitboard(Bitboard bb);

#endif