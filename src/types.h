#pragma once

#include <stdint.h>
#include <cassert>
#include <string>
#include <sstream>

// General constants and types
typedef int32_t Eval;
typedef uint8_t Square;
typedef uint64_t Bitboard;
typedef int16_t Depth;

constexpr int MAX_PLY = 246;
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

    constexpr Move() = default;
    static constexpr Move none() { return Move{}; };
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

    constexpr Square origin() {
        return Square(data & 0x3F);
    }
    constexpr Square target() {
        return Square((data >> 6) & 0x3F);
    }
    constexpr MoveType type() {
        assert(*this);
        return MoveType((data >> 12) & 0x3);
    }
    constexpr Piece promotionPiece() {
        assert(*this);
        assert(type() == MoveType::PROMOTION);
        return Piece((data >> 14) + 1);
    }

    constexpr operator bool() {
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

    std::string toString(bool chess960) {
        if (!*this) return "0000";
        std::ostringstream os;

        Square from = origin(), to = target();
        if (type() == MoveType::CASTLING && !chess960) {
            int rank = rankOf(from);
            int file = from > to ? 2 : 6;
            to = rank * 8 + file;
        }

        os << from << to;
        if (type() == MoveType::PROMOTION)
            os << promotionPiece();

        return os.str();
    }

private:

    constexpr Move(uint16_t _data) : data(_data) {
        assert(origin() < 64);
        assert(target() < 64);
    };

    uint16_t data = 0;
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

// ArrayVec
template<class T, size_t MAX>
class ArrayVec {
public:

    ArrayVec() : _size(0) {}

    void add(const T& element) {
        assert(_size + 1 < MAX);
        elements[_size++] = elements;
    }

    T& remove(size_t o) {
        T removed = elements[i];
        elements[i] = elements[--_size];
        return removed;
    }

    T& operator[](size_t i) const {
        assert(i < _size);
        return elements[i];
    }

    int size() const {
        return _size;
    }

    const T* begin() const { return elements; }
    const T* end() const { return elements + _size; }

private:
    T elements[MAX];
    size_t _size{};
};

// Search Stack
struct SearchStack {
    int pvLength;
    Move pv[MAX_PLY + 1];

    int ply;

    Eval staticEval;

    Move move;
    Piece movedPiece;
    bool inCheck, capture;
    int correctionValue;
    int16_t reduction;
    bool inLMR, ttPv;

    Move excludedMove;
    Move killer;

    int16_t* contHist;
    int16_t* contCorrHist;
};