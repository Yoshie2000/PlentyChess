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
    Bitboard toSquare[64];
};

struct Hashes {
    uint64_t hash;
    uint64_t pawnHash;
    uint64_t nonPawnHash[2];
    uint64_t minorHash;
    uint64_t majorHash;
};

struct Board {
    Threats threats;
    Hashes hashes;

    Bitboard byPiece[Piece::TOTAL];
    Bitboard byColor[2];
    Bitboard enpassantTarget;
    Bitboard blockers[2];
    Bitboard checkers;
    Move lastMove;
    uint8_t checkerCount;
    Piece pieces[64];

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
    void updatePieceThreats(Square square, Bitboard attacked, NNUE* nnue);
    template<bool add>
    void updateThreatFeaturesFromPiece(Piece piece, Color pieceColor, Square square, Bitboard attacked, NNUE* nnue);
    template<bool add>
    void updateThreatFeaturesToPiece(Piece piece, Color pieceColor, Square square, NNUE* nnue);

    void updatePieceHash(Piece piece, Color pieceColor, uint64_t hashDelta);
    void addPiece(Piece piece, Color pieceColor, Square square, Bitboard squareBB, NNUE* nnue);
    void removePiece(Piece piece, Color pieceColor, Square square, Bitboard squareBB, NNUE* nnue);
    void movePiece(Piece piece, Color pieceColor, Square origin, Square target, Bitboard fromTo, NNUE* nnue);

    void doMove(Move move, uint64_t newHash, NNUE* nnue);
    void doNullMove();

    Threats calculateAllThreats();
    bool isSquareThreatened(Square square);
    bool opponentHasGoodCapture();

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

    constexpr bool hasNonPawns() {
        Bitboard nonPawns = byPiece[Piece::KNIGHT] | byPiece[Piece::BISHOP] | byPiece[Piece::ROOK] | byPiece[Piece::QUEEN];
        return BB::popcount(byColor[stm] & nonPawns) > 0;
    }

    Bitboard attackersTo(Square square, Bitboard occupied);
    Bitboard attackersTo(Square square);

    void debugBoard();
    int validateBoard();
};

void debugBitboard(Bitboard bb);
