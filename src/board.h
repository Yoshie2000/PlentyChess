#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <string>

#include "move.h"
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
constexpr uint8_t CASTLING_FLAGS[] = { CASTLING_WHITE_KINGSIDE, CASTLING_WHITE_QUEENSIDE, CASTLING_BLACK_KINGSIDE, CASTLING_BLACK_QUEENSIDE };

constexpr int castlingIndex(Color side, Square kingOrigin, Square kingTarget) {
    return 2 * (side != Color::WHITE) + (kingTarget <= kingOrigin);
}

struct Threats {
    Bitboard pawnThreats;
    Bitboard knightThreats;
    Bitboard bishopThreats;
    Bitboard rookThreats;
    Bitboard queenThreats;
    Bitboard kingThreats;
};

struct BoardStack {
    Piece capturedPiece;
    Bitboard enpassantTarget; // one-hot encoding -> 0 means no en passant possible

    uint8_t rule50_ply;
    uint8_t nullmove_ply;
    uint64_t hash;
    uint64_t pawnHash;
    uint64_t nonPawnHash[2];
    uint64_t minorHash;
    uint64_t majorHash;

    Bitboard blockers[2];
    Bitboard checkers;
    uint8_t checkerCount;

    Move move;

    Threats threats;

    // MEMCPY GOES FROM HERE
    int pieceCount[2][Piece::TOTAL];

    uint8_t castling; // 0000 -> black queenside, black kingside, white queenside, white kingside
    // TO HERE

    BoardStack* previous;
};

struct Board {
    Bitboard byPiece[Piece::TOTAL];
    Bitboard byColor[2];
    Piece pieces[64];

    Color stm;
    uint8_t ply;
    bool chess960;
    Square castlingSquares[4]; // For each castling right, stores the square of the corresponding rook

    struct BoardStack* stack;

    void startpos();
    size_t parseFen(std::string fen, bool chess960);
    std::string fen();

    void movePiece(Piece piece, Square origin, Square target, Bitboard fromTo);
    void doMove(BoardStack* newStack, Move move, uint64_t newHash, NNUE* nnue);
    void undoMove(Move move, NNUE* nnue);
    void doNullMove(BoardStack* newStack);
    void undoNullMove();

    void calculateThreats();
    uint64_t getThreatsHash() {
        Bitboard key = stack->threats.pawnThreats | stack->threats.knightThreats | stack->threats.bishopThreats | stack->threats.rookThreats | stack->threats.queenThreats | stack->threats.kingThreats;
        key &= byColor[stm];
        key ^= key >> 33;
        key *= 0xff51afd7ed558ccd;
        key ^= key >> 33;
        key *= 0xc4ceb9fe1a85ec53;
        key ^= key >> 33;
        return key;
    }
    bool isSquareThreatened(Square square, BoardStack* bs);

    constexpr bool isCapture(Move move) {
        MoveType type = moveType(move);
        if (type == MOVE_CASTLING) return false;
        if (type == MOVE_ENPASSANT || (type == MOVE_PROMOTION && promotionType(move) == PROMOTION_QUEEN)) return true;
        return pieces[moveTarget(move)] != Piece::NONE;
    }
    bool isPseudoLegal(Move move);
    bool isLegal(Move move);
    bool givesCheck(Move move);

    void calculateCastlingSquares(Square kingOrigin, Square* kingTarget, Square* rookOrigin, Square* rookTarget, uint8_t* castling);

    uint64_t hashAfter(Move move);

    void updateSliderPins(Color side);

    bool hasUpcomingRepetition(int ply);
    bool isDraw();

    constexpr bool hasNonPawns() {
        return stack->pieceCount[stm][Piece::KNIGHT] > 0 || stack->pieceCount[stm][Piece::BISHOP] > 0 || stack->pieceCount[stm][Piece::ROOK] > 0 || stack->pieceCount[stm][Piece::QUEEN] > 0;
    }

    Bitboard attackersTo(Square square, Bitboard occupied);

    void debugBoard();
    int validateBoard();
};

void debugBitboard(Bitboard bb);
