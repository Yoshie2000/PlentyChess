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

    struct BoardStack* stack;

    bool stopSearching;
};

struct BoardStack {
    Piece capturedPiece;
    Bitboard enpassantTarget; // one-hot encoding -> 0 means no en passant possible

    uint8_t rule50_ply;
    uint8_t nullmove_ply;
    int8_t repetition;
    uint64_t hash;

    // MEMCPY GOES FROM HERE
    Bitboard attackedByPiece[2][PIECE_TYPES];
    Bitboard attackedByColor[2];
    int pieceCount[2][PIECE_TYPES];
    Eval psq[2];

    uint8_t castling; // 0000 -> black queenside, black kingside, white queenside, white kingside
    // TO HERE

    struct BoardStack* previous;
};

void startpos(Board* result);
size_t parseFen(Board* board, std::string fen);

void doMove(Board* board, BoardStack* newStack, Move move);
void undoMove(Board* board, Move move);

bool hasRepeated(Board* board);
bool isDraw(Board* board, int ply);

Bitboard pawnAttacksLeft(Board* board, Color side);
Bitboard pawnAttacksRight(Board* board, Color side);
Bitboard pawnAttacks(Board* board, Color side);

constexpr Bitboard knightAttacks(Bitboard knightBB) {
    Bitboard l1 = (knightBB >> 1) & C64(0x7f7f7f7f7f7f7f7f);
    Bitboard l2 = (knightBB >> 2) & C64(0x3f3f3f3f3f3f3f3f);
    Bitboard r1 = (knightBB << 1) & C64(0xfefefefefefefefe);
    Bitboard r2 = (knightBB << 2) & C64(0xfcfcfcfcfcfcfcfc);
    Bitboard h1 = l1 | r1;
    Bitboard h2 = l2 | r2;
    return (h1 << 16) | (h1 >> 16) | (h2 << 8) | (h2 >> 8);
}
Bitboard knightAttacksAll(Board* board, Color side);

Bitboard kingAttacks(Board* board, Color side);

Bitboard slidingPieceAttacksAll(Board* board, Color side, Piece pieceType);

Bitboard attackedSquares(Board* board, Color side);
Bitboard attackedSquaresByPiece(Board* board, Color side, Piece pieceType);

bool isInCheck(Board* board, Color side);

void debugBoard(Board* board);
void debugBitboard(Bitboard bb);

#endif