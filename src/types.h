#pragma once

#include <stdint.h>
#include <cassert>
#include <string>
#include <sstream>
#include <iostream>

#include "utils.h"

// General constants and types
using Value = int32_t;
using Square = uint8_t;
using Bitboard = uint64_t;
using Depth = int16_t;
using Hash = uint64_t;

constexpr int MAX_PLY = 250;
constexpr Depth MAX_DEPTH = (MAX_PLY - 1) * 100;
constexpr int MAX_MOVES = 218;
constexpr int MAX_CAPTURES = 74;
constexpr Square NO_SQUARE = 64;

constexpr int fileOf(const Square square) {
    return square % 8;
}
constexpr int rankOf(const Square square) {
    return square / 8;
}

inline std::ostream& operator<<(std::ostream& os, const Square square) {
    static std::string rankChars = "12345678";
    static std::string fileChars = "abcdefgh";
    os << fileChars[fileOf(square)] << rankChars[rankOf(square)];
    return os;
};

// Color
enum Color : uint8_t {
    WHITE = 0,
    BLACK = 1
};

constexpr Color& operator++(Color& color) {
    color = static_cast<Color>(static_cast<int>(color) + 1);
    return color;
}

// Piece
enum Piece : uint8_t {
    PAWN = 0,
    KNIGHT = 1,
    BISHOP = 2,
    ROOK = 3,
    QUEEN = 4,
    KING = 5,
    NONE = 6,
    TOTAL = 6
};

constexpr Piece& operator++(Piece& piece) {
    piece = static_cast<Piece>(static_cast<int>(piece) + 1);
    return piece;
}

inline std::ostream& operator<<(std::ostream& os, const Piece piece) {
    static std::string pieceChars = "pnbrqk ";
    os << pieceChars[piece];
    return os;
}

// Move
enum MoveType : uint8_t {
    NORMAL = 0,
    PROMOTION = 1,
    ENPASSANT = 2,
    CASTLING = 3
};

struct Move {

    Move() = default;
    
    static constexpr Move none() { return Move{0}; };
    static constexpr Move makeNormal(Square origin, Square target) {
        return Move((origin & 0x3F) | ((target & 0x3F) << 6));
    }
    static constexpr Move makePromotion(Square origin, Square target, Piece promotionPiece) {
        assert(promotionPiece >= Piece::KNIGHT);
        assert(promotionPiece <= Piece::QUEEN);
        return Move(origin | (target << 6) | (MoveType::PROMOTION << 12) | ((promotionPiece - 1) << 14));
    }
    static constexpr Move makeEnpassant(Square origin, Square target) {
        return Move(origin | (target << 6) | (MoveType::ENPASSANT << 12));
    }
    static constexpr Move makeCastling(Square origin, Square target) {
        return Move(origin | (target << 6) | (MoveType::CASTLING << 12));
    }

    constexpr Square origin() const {
        return Square(data & 0x3F);
    }
    constexpr Square target() const {
        return Square((data >> 6) & 0x3F);
    }
    constexpr MoveType type() const {
        assert(*this);
        return MoveType((data >> 12) & 0x3);
    }
    constexpr Piece promotionPiece() const {
        assert(*this);
        assert(isPromotion());
        return Piece((data >> 14) + 1);
    }

    constexpr bool isSpecial() const {
        assert(*this);
        return type() != MoveType::NORMAL;
    }
    constexpr bool isPromotion() const {
        assert(*this);
        return type() == MoveType::PROMOTION;
    }
    constexpr bool isCastling() const {
        assert(*this);
        return type() == MoveType::CASTLING;
    }
    constexpr bool isEnpassant() const {
        assert(*this);
        return type() == MoveType::ENPASSANT;
    }

    constexpr operator bool() const {
        return static_cast<bool>(data);
    }

    constexpr bool operator<(const Move& other) const {
        return data < other.data;
    }
    constexpr bool operator<(Move& other) {
        return data < other.data;
    }

    constexpr bool operator==(const Move& other) const {
        return data == other.data;
    }
    constexpr bool operator==(Move& other) {
        return data == other.data;
    }
    constexpr bool operator!=(const Move& other) const {
        return data != other.data;
    }
    constexpr bool operator!=(Move& other) {
        return data != other.data;
    }

    std::string toString(bool chess960) const {
        if (!*this) return "0000";
        std::ostringstream os;

        Square from = origin(), to = target();
        if (isCastling() && !chess960) {
            int rank = rankOf(from);
            int file = from > to ? 2 : 6;
            to = rank * 8 + file;
        }

        os << from << to;
        if (isPromotion())
            os << promotionPiece();

        return os.str();
    }

private:

    constexpr Move(uint16_t _data) : data(_data) {
        assert(origin() < 64);
        assert(target() < 64);
    };

    uint16_t data;
};

// Helper functions
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

constexpr Color flip(Color color) {
    return static_cast<Color>(1 - color);
}

inline Square stringToSquare(const char* string) {
    int file = string[0] - 'a';
    int rank = string[1] - '1';
    return Square(8 * rank + file);
}

// Search Stack
struct SearchStack {
    int pvLength;
    Move pv[MAX_PLY + 1];

    int ply;

    Value staticEval;

    Move move;
    Piece movedPiece;
    bool inCheck, capture;
    int correctionValue;
    Depth reduction;
    bool inLMR, ttPv;

    Move excludedMove;
    Move killer;

    int16_t* contHist;
    int16_t* contCorrHist;
};