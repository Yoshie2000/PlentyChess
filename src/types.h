#pragma once

#include <stdint.h>

#define C64(x) ((uint64_t)(x))

#define MAX_PLY 246
#define MAX_MOVES 218

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

#define NO_SQUARE 64

typedef uint8_t Piece;
typedef uint8_t Square;
typedef uint8_t Color;
typedef uint64_t Bitboard;
typedef int32_t Eval;

// 00 promotion piece 00 special move type 000000 target 000000 origin
// Special move type: 01 == promotion, 10 == en passant, 11 == castling
// Promotion piece type: 00 == knight, 01 == bishop, 10 == rook, 11 == queen
typedef uint16_t Move;

inline Square lsb(Bitboard bb) {
    return __builtin_ctzll(bb);
}

inline Square popLSB(Bitboard* bb) {
    Square l = lsb(*bb);
    *bb &= *bb - 1;
    return l;
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

    int doubleExtensions;

    int16_t* contHist;
};