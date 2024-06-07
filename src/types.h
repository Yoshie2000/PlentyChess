#pragma once

#include <stdint.h>

constexpr int MAX_PLY = 246;
constexpr int NO_DEPTH = 255;
constexpr int MAX_MOVES = 218;
constexpr int MAX_CAPTURES = 74;

constexpr int PIECE_TYPES = 6;

#define PIECE_PAWN 0
#define PIECE_KNIGHT 1
#define PIECE_BISHOP 2
#define PIECE_ROOK 3
#define PIECE_QUEEN 4
#define PIECE_KING 5
#define NO_PIECE 6

#define COLOR_WHITE 0
#define COLOR_BLACK 1

#define NO_SQUARE 64

// 00 promotion piece 00 special move type 000000 target 000000 origin
// Special move type: 01 == promotion, 10 == en passant, 11 == castling
// Promotion piece type: 00 == knight, 01 == bishop, 10 == rook, 11 == queen
typedef uint16_t Move;
typedef uint16_t MoveType;
typedef uint16_t PromotionType;
typedef uint8_t Piece;
typedef int32_t Eval;
typedef uint8_t Square;
typedef uint64_t Bitboard;
typedef uint8_t Color;

constexpr MoveType moveType(Move move) {
    return move & 0x3000;
}
constexpr PromotionType promotionType(Move move) {
    return move >> 14;
}

inline Square lsb(Bitboard bb) {
    return __builtin_ctzll(bb);
}
inline Square popLSB(Bitboard* bb) {
    Square l = lsb(*bb);
    *bb &= *bb - 1;
    return l;
}

constexpr Bitboard bitboard(int32_t number) {
    return Bitboard(number);
}
constexpr Bitboard bitboard(long number) {
    return Bitboard(number);
}
constexpr Bitboard bitboard(uint64_t number) {
    return Bitboard(number);
}
constexpr Bitboard bitboard(Square square) {
    return Bitboard(1) << square;
}

constexpr int fileOf(Square square) {
    return square % 8;
}
constexpr Color flip(Color color) {
    return 1 - color;
}

struct SearchStack {
    int pvLength;
    Move pv[MAX_PLY + 1];

    int ply;

    Eval staticEval;

    Move move;
    Piece movedPiece;

    Move excludedMove;
    Move killers[2];

    int16_t* contHist;
};