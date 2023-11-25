#ifndef BOARD_H_INCLUDED
    #define BOARD_H_INCLUDED

    #include <stdint.h>
    #include <stdbool.h>
    #include <string>
    
    #include "types.h"

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

    Bitboard pawnAttacksLeft(struct Board* board, Color side);
    Bitboard pawnAttacksRight(struct Board* board, Color side);
    Bitboard pawnAttacks(struct Board* board, Color side);

    Bitboard knightAttacks(Bitboard knightBB);
    Bitboard knightAttacksAll(struct Board* board, Color side);

    Bitboard kingAttacks(struct Board* board, Color side);

    Bitboard slidingPieceAttacks(struct Board* board, Bitboard pieceBB);
    Bitboard slidingPieceAttacksAll(struct Board* board, Color side, Piece pieceType);

    bool isSquareAttacked(struct Board* board, Square square, Color side);
    Bitboard attackedSquares(struct Board* board, Color side);
    Bitboard attackedSquaresByPiece(struct Board* board, Color side, Piece pieceType);

    bool isInCheck(struct Board* board, Color side);

    void debugBoard(struct Board* board);
    void debugBitboard(Bitboard bb);

#endif