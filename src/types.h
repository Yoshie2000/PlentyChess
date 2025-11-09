#pragma once

#include <stdint.h>
#include <cassert>

constexpr int MAX_PLY = 250;
constexpr int16_t MAX_DEPTH = (MAX_PLY - 1) * 100;
constexpr int MAX_MOVES = 218;
constexpr int MAX_CAPTURES = 74;

enum Color : uint8_t {
    WHITE = 0,
    BLACK = 1
};

constexpr Color& operator++(Color& color) {
    color = static_cast<Color>(static_cast<int>(color) + 1);
    return color;
}

enum PieceType: uint8_t {
    PAWN = 0,
    KNIGHT = 1,
    BISHOP = 2,
    ROOK = 3,
    QUEEN = 4,
    KING = 5,
    NONE = 6,
    TOTAL = 6
};

enum Piece : uint8_t {
    WHITE_PAWN = 0,
    WHITE_KNIGHT = 1,
    WHITE_BISHOP = 2,
    WHITE_ROOK = 3,
    WHITE_QUEEN = 4,
    WHITE_KING = 5,
    BLACK_PAWN = 6,
    BLACK_KNIGHT = 7,
    BLACK_BISHOP = 8,
    BLACK_ROOK = 9,
    BLACK_QUEEN = 10,
    BLACK_KING = 11,
    NO_PIECE = 12,
    TOTAL_PIECES = 12
};

constexpr PieceType& operator++(PieceType& piece) {
    piece = static_cast<PieceType>(static_cast<int>(piece) + 1);
    return piece;
}

constexpr Piece& operator++(Piece& piece) {
    piece = static_cast<Piece>(static_cast<int>(piece) + 1);
    return piece;
}

constexpr PieceType typeOf(Piece& piece) {
    assert(piece < Piece::NO_PIECE);
    return static_cast<PieceType>(piece % 6);
}

constexpr Color colorOf(Piece& piece) {
    assert(piece < Piece::NO_PIECE);
    return static_cast<Color>(piece / 6);
}

constexpr Piece makePiece(PieceType type, Color color) {
    return static_cast<Piece>(6 * color + type);
}

constexpr bool operator==(Piece& piece, PieceType& type) = delete;
constexpr bool operator==(Piece piece, PieceType type) = delete;
constexpr bool operator!=(Piece& piece, PieceType& type) = delete;
constexpr bool operator!=(Piece piece, PieceType type) = delete;

// 00 promotion piece 00 special move type 000000 target 000000 origin
// Special move type: 01 == promotion, 10 == en passant, 11 == castling
// Promotion piece type: 00 == knight, 01 == bishop, 10 == rook, 11 == queen
typedef uint16_t Move;
typedef uint16_t MoveType;
typedef uint16_t PromotionType;
typedef int32_t Eval;
typedef uint8_t Square;
typedef uint64_t Bitboard;

constexpr Square NO_SQUARE = 64;

constexpr MoveType moveType(Move move) {
    return move & 0x3000;
}
constexpr PromotionType promotionType(Move move) {
    return move >> 14;
}

inline Square lsb(Bitboard bb) {
    assert(bb > 0);
    return __builtin_ctzll(bb);
}
inline Square popLSB(Bitboard* bb) {
    Square l = lsb(*bb);
    *bb &= *bb - 1;
    return l;
}

inline Square msb(Bitboard bb) {
    assert(bb > 0);
    return 63 - __builtin_clzll(bb);
}
inline Square popMSB(Bitboard* bb) {
    Square l = msb(*bb);
    *bb &= *bb - 1;
    return l;
}

constexpr Bitboard bitboard(int32_t number) {
    return static_cast<Bitboard>(number);
}
constexpr Bitboard bitboard(long number) {
    return static_cast<Bitboard>(number);
}
constexpr Bitboard bitboard(uint64_t number) {
    return static_cast<Bitboard>(number);
}
constexpr Bitboard bitboard(Square square) {
    return static_cast<Bitboard>(1) << square;
}

constexpr int fileOf(Square square) {
    return square % 8;
}
constexpr int rankOf(Square square) {
    return square / 8;
}

constexpr Color flip(Color color) {
    return static_cast<Color>(1 - color);
}

struct SearchStack {
    int pvLength;
    Move pv[MAX_PLY + 1];

    int ply;

    Eval staticEval;

    Move move;
    PieceType movedPiece;
    bool inCheck, capture;
    int correctionValue;
    int16_t reduction;
    bool inLMR, ttPv;

    Move excludedMove;
    Move killer;

    int16_t* contHist;
    int16_t* contCorrHist;
};