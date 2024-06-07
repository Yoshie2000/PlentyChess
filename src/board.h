#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <string>

#include "bitboard.h"
#include "types.h"

class NNUE;

constexpr uint8_t CASTLING_WHITE_KINGSIDE = 0x1;
constexpr uint8_t CASTLING_WHITE_QUEENSIDE = 0x2;
constexpr uint8_t CASTLING_BLACK_KINGSIDE = 0x4;
constexpr uint8_t CASTLING_BLACK_QUEENSIDE = 0x8;
constexpr uint8_t CASTLING_MASK = 0xF;

constexpr Square CASTLING_KING_SQUARES[] = { 6, 2, 62, 58 };
constexpr Square CASTLING_ROOK_SQUARES[] = { 5, 3, 61, 59 };

struct BoardStack {
    Piece capturedPiece;
    Bitboard enpassantTarget; // one-hot encoding -> 0 means no en passant possible

    uint8_t rule50_ply;
    uint8_t nullmove_ply;
    uint64_t hash;
    uint64_t pawnHash;

    Bitboard blockers[2];
    Bitboard checkers;
    uint8_t checkerCount;

    Move move;

    // MEMCPY GOES FROM HERE
    int pieceCount[2][PIECE_TYPES];

    uint8_t castling; // 0000 -> black queenside, black kingside, white queenside, white kingside
    // TO HERE

    BoardStack* previous;
};

struct Board {
    Bitboard byPiece[PIECE_TYPES];
    Bitboard byColor[2];
    Piece pieces[64];

    Color stm;
    uint8_t ply;
    bool chess960;
    Square castlingSquares[4]; // For each castling right, stores the square of the corresponding rook

    struct BoardStack* stack;

    void startpos();
    size_t parseFen(std::string fen, bool chess960);

    constexpr void movePiece(Piece piece, Square origin, Square target, Bitboard fromTo) {
        byColor[stm] ^= fromTo;
        byPiece[piece] ^= fromTo;

        pieces[origin] = NO_PIECE;
        pieces[target] = piece;
    };
    void doMove(BoardStack* newStack, Move move, uint64_t newHash, NNUE* nnue);
    void undoMove(Move move, NNUE* nnue);
    void doNullMove(BoardStack* newStack);
    void undoNullMove();

    void calculateCastlingSquares(Square kingOrigin, Square* kingTarget, Square* rookOrigin, Square* rookTarget, uint8_t* castling);

    uint64_t hashAfter(Move move);

    void updateSliderPins(Color side);

    bool hasUpcomingRepetition(int ply);
    bool isDraw();

    constexpr bool hasNonPawns() {
        return stack->pieceCount[stm][PIECE_KNIGHT] > 0 || stack->pieceCount[stm][PIECE_BISHOP] > 0 || stack->pieceCount[stm][PIECE_ROOK] > 0 || stack->pieceCount[stm][PIECE_QUEEN] > 0;
    }

    Bitboard attackersTo(Square square, Bitboard occupied);

    void debugBoard();
    int validateBoard();
};

void debugBitboard(Bitboard bb);
