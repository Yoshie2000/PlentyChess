#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <string>
#include <sstream>

#include "move.h"
#include "bitboard.h"
#include "types.h"

class NNUE;

namespace Castling {

    using CastlingDirection = size_t;
    constexpr CastlingDirection KINGSIDE = 0;
    constexpr CastlingDirection QUEENSIDE = 1;

    constexpr CastlingDirection getDirection(Square kingOrigin, Square kingTarget) {
        return (kingTarget <= kingOrigin) ? QUEENSIDE : KINGSIDE;
    }
    
    constexpr uint8_t getMask(Color side, CastlingDirection direction) {
        return 1 << (2 * side + direction);
    }

    constexpr uint8_t getMask(Color side) {
        return getMask(side, KINGSIDE) | getMask(side, QUEENSIDE);
    }

    constexpr uint8_t getKingSquare(Color side, CastlingDirection direction) {
        constexpr Square KING_SQUARES[] = { 6, 2, 62, 58 };
        return KING_SQUARES[2 * side + direction];
    }

    constexpr uint8_t getRookSquare(Color side, CastlingDirection direction) {
        constexpr Square ROOK_SQUARES[] = { 5, 3, 61, 59 };
        return ROOK_SQUARES[2 * side + direction];
    }

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
    void parseFen(std::string fen, bool chess960) {
        std::istringstream iss(fen);
        parseFen(iss, chess960);
    };
    void parseFen(std::istringstream& iss, bool chess960);
    std::string fen();

    template<bool add>
    void updatePieceThreats(Piece piece, Color pieceColor, Square square, NNUE* nnue);
    void updatePieceHash(Piece piece, Color pieceColor, uint64_t hashDelta);
    void updatePieceCastling(Piece piece, Color pieceColor, Square origin);

    void addPiece(Piece piece, Color pieceColor, Square square, NNUE* nnue);
    void removePiece(Piece piece, Color pieceColor, Square square, NNUE* nnue);
    void movePiece(Piece piece, Color pieceColor, Square origin, Square target, NNUE* nnue);

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

    Square getCastlingRookSquare(Color side, Castling::CastlingDirection direction) {
        return castlingSquares[2 * side + direction];
    }

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
