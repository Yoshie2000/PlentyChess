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
    Bitboard pawnThreats;
    Bitboard knightThreats;
    Bitboard bishopThreats;
    Bitboard rookThreats;
    Bitboard queenThreats;
    Bitboard kingThreats;
};

struct Hashes {
    Hash hash;
    Hash pawnHash;
    Hash nonPawnHash[2];
    Hash minorHash;
    Hash majorHash;
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
    void parseFen(std::string fen, bool chess960) {
        std::istringstream iss(fen);
        parseFen(iss, chess960);
    };
    void parseFen(std::istringstream& iss, bool chess960);
    std::string fen();

    template<bool add, bool computeRays = true>
    void updatePieceThreats(Piece piece, Color pieceColor, Square square, NNUE* nnue, Bitboard allowedRayUpdates = ~bitboard(0));
    void updatePieceHash(Piece piece, Color pieceColor, uint64_t hashDelta);
    void updatePieceCastling(Piece piece, Color pieceColor, Square origin);

    void addPiece(Piece piece, Color pieceColor, Square square, NNUE* nnue);
    void removePiece(Piece piece, Color pieceColor, Square square, NNUE* nnue);
    void movePiece(Piece piece, Color pieceColor, Square origin, Square target, NNUE* nnue);
    void swapPiece(Piece piece, Color pieceColor, Square square, NNUE* nnue);

    void doMove(Move move, Hash newHash, NNUE* nnue);
    void doNullMove();

    void calculateThreats();
    bool isSquareThreatened(Square square);
    bool opponentHasGoodCapture();

    constexpr bool isCapture(Move move) {
        if (move.isCastling()) return false;
        if (move.isEnpassant() || (move.isPromotion() && move.promotionPiece() == Piece::QUEEN)) return true;
        return pieces[move.target()] != Piece::NONE;
    }
    bool isPseudoLegal(Move move);
    bool isLegal(Move move);
    bool givesCheck(Move move);

    Square getCastlingRookSquare(Color side, Castling::CastlingDirection direction) {
        return castlingSquares[2 * side + direction];
    }

    Hash hashAfter(Move move);

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
