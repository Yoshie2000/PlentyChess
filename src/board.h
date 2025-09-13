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
    Bitboard byPiece[6];
    uint16_t toSquare[2][64];
};

struct Hashes {
    uint64_t hash;
    uint64_t pawnHash;
    uint64_t nonPawnHash[2];
    uint64_t minorHash;
    uint64_t majorHash;
};

struct MailboxEntry {
    uint8_t data;

    MailboxEntry(): data(Piece::NONE) {}

    MailboxEntry(Piece piece, Color color, PieceID pieceID): data(piece | (pieceID << 3) | (color << 7)) {}

    Piece piece() {
        return Piece(data & 0x7);
    }

    PieceID pieceID() {
        return PieceID((data >> 3) & 0xF);
    }

    Color color() {
        return (data & 0x80) ? Color::BLACK : Color::WHITE;
    }

    bool isEmpty() {
        return piece() == Piece::NONE;
    }
};

static_assert(sizeof(MailboxEntry) == sizeof(Piece));

struct Board {
    Threats threats;
    Hashes hashes;

    Square piecelistSquares[2][16];
    Piece piecelistPieces[2][16];

    Bitboard byPiece[Piece::TOTAL];
    Bitboard byColor[2];
    Bitboard enpassantTarget;
    Bitboard blockers[2];
    Bitboard checkers;
    Move lastMove;
    uint8_t checkerCount;
    MailboxEntry mailbox[64];

    Color stm;

    uint8_t castling;
    Square castlingSquares[4]; // For each castling right, stores the square of the corresponding rook

    uint8_t ply;
    uint8_t rule50_ply;
    uint8_t nullmove_ply;

    bool chess960;

    void startpos();
    size_t parseFen(std::string fen, bool chess960);
    std::string fen();

    template<bool add>
    void updateThreatsFromPiece(Piece piece, Color pieceColor, PieceID pieceID, Square square, Bitboard squareBB, NNUE* nnue);
    template<bool add>
    void updateThreatsToPiece(Piece piece, Color pieceColor, PieceID pieceID, Square square, NNUE* nnue);

    void addPiece(Piece piece, Color pieceColor, PieceID pieceID, Square square, Bitboard squareBB, NNUE* nnue);
    void removePiece(Piece piece, Color pieceColor, PieceID pieceID, Square square, Bitboard squareBB, NNUE* nnue);
    void movePiece(Piece piece, Color pieceColor, PieceID pieceID, Square origin, Square target, Bitboard fromTo, NNUE* nnue);

    void doMove(Move move, uint64_t newHash, NNUE* nnue);
    void doNullMove();

    void finishThreatsUpdate();
    Threats calculateAllThreats();
    bool isSquareThreatened(Square square);

    constexpr bool isCapture(Move move) {
        MoveType type = moveType(move);
        if (type == MOVE_CASTLING) return false;
        if (type == MOVE_ENPASSANT || (type == MOVE_PROMOTION && promotionType(move) == PROMOTION_QUEEN)) return true;
        return mailbox[moveTarget(move)].piece() != Piece::NONE;
    }
    bool isPseudoLegal(Move move);
    bool isLegal(Move move);
    bool givesCheck(Move move);

    void calculateCastlingSquares(Square kingOrigin, Square* kingTarget, Square* rookOrigin, Square* rookTarget, uint8_t* castling);

    uint64_t hashAfter(Move move);

    void updateSliderPins(Color side);

    constexpr bool hasNonPawns() {
        Bitboard nonPawns = byPiece[Piece::KNIGHT] | byPiece[Piece::BISHOP] | byPiece[Piece::ROOK] | byPiece[Piece::QUEEN];
        return BB::popcount(byColor[stm] & nonPawns) > 0;
    }

    constexpr bool opponentHasGoodCapture() {
        Bitboard queens = byColor[stm] & byPiece[Piece::QUEEN];
        Bitboard rooks = byColor[stm] & byPiece[Piece::ROOK];
        rooks |= queens;
        Bitboard minors = byColor[stm] & (byPiece[Piece::KNIGHT] | byPiece[Piece::BISHOP]);
        minors |= rooks;

        Bitboard minorThreats = threats.byPiece[Piece::KNIGHT] | threats.byPiece[Piece::BISHOP] | threats.byPiece[Piece::PAWN];
        Bitboard rookThreats = minorThreats | threats.byPiece[Piece::ROOK];

        return (queens & rookThreats) | (rooks & minorThreats) | (minors & threats.byPiece[Piece::PAWN]);
    }

    Bitboard attackersTo(Square square, Bitboard occupied);

    void debugBoard();
    int validateBoard();
};

void debugBitboard(Bitboard bb);
