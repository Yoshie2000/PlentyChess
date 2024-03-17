#pragma once

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
    Bitboard byPiece[PIECE_TYPES];
    Bitboard byColor[2];
    Piece pieces[64];

    Color stm;
    uint8_t ply;
    bool chess960;
    Square castlingSquares[4]; // For each castling right, stores the square of the corresponding rook

    struct BoardStack* stack;
};

struct BoardStack {
    Piece capturedPiece;
    Bitboard enpassantTarget; // one-hot encoding -> 0 means no en passant possible

    uint8_t rule50_ply;
    uint8_t nullmove_ply;
    uint64_t hash;
    uint64_t pawnHash;

    Bitboard blockers[2];
    Bitboard pinners[2];
    Bitboard checkers;
    uint8_t checkerCount;

    Move move;

    // MEMCPY GOES FROM HERE
    int pieceCount[2][PIECE_TYPES];

    uint8_t castling; // 0000 -> black queenside, black kingside, white queenside, white kingside
    // TO HERE

    BoardStack* previous;
};

void startpos(Board* result);
size_t parseFen(Board* board, std::string fen, bool chess960);

#include "nnue.h"

void doMove(Board* board, BoardStack* newStack, Move move, uint64_t newHash, NNUE* nnue);
void undoMove(Board* board, Move move, NNUE* nnue);
void doNullMove(Board* board, BoardStack* newStack);
void undoNullMove(Board* board);

uint64_t hashAfter(Board* board, Move move);

void updateSliderPins(Board* board, Color side);

bool hasUpcomingRepetition(Board* board, int ply);
bool isDraw(Board* board);

constexpr bool hasNonPawns(Board* board) {
    return board->stack->pieceCount[board->stm][PIECE_KNIGHT] > 0 || board->stack->pieceCount[board->stm][PIECE_BISHOP] > 0 || board->stack->pieceCount[board->stm][PIECE_ROOK] > 0 || board->stack->pieceCount[board->stm][PIECE_QUEEN] > 0;
}

Bitboard attackersTo(Board* board, Square square, Bitboard occupied);

constexpr Bitboard pawnAttacksLeft(Bitboard pawns, Color side) {
    return side == COLOR_WHITE ?
        ((pawns & (~FILE_A)) << 7) :
        ((pawns & (~FILE_A)) >> 9);
}

constexpr Bitboard pawnAttacksRight(Bitboard pawns, Color side) {
    return side == COLOR_WHITE ?
        ((pawns & (~FILE_H)) << 9) :
        ((pawns & (~FILE_H)) >> 7);
}

constexpr Bitboard pawnAttacks(Bitboard pawns, Color side) {
    return pawnAttacksLeft(pawns, side) | pawnAttacksRight(pawns, side);
}

constexpr Bitboard pawnAttacks(Board* board, Color side) {
    Bitboard pawns = board->byPiece[PIECE_PAWN] & board->byColor[side];
    return pawnAttacks(pawns, side);
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
Bitboard knightAttacksAll(Board* board, Color side);

Bitboard kingAttacks(Board* board, Color side);

Bitboard slidingPieceAttacksAll(Board* board, Color side, Piece pieceType);

Bitboard attackedSquares(Board* board, Color side);
Bitboard attackedSquaresByPiece(Board* board, Color side, Piece pieceType);
Bitboard attackedSquaresByPiece(Piece pieceType, Square square, Bitboard occupied, Color stm);

void debugBoard(Board* board);
void debugBitboard(Bitboard bb);
int validateBoard(Board* board);