#include "threat-inputs.h"

namespace ThreatInputs {

    // Count how many attacked squares are < to, effectively creating an index for the "to" square relative to the "from" square
    int32_t localThreatIndex(Square to, Bitboard attackedByFrom) {
        return BB::popcount(attackedByFrom & (Bitboard(to) - 1));
    }

    int32_t mirrorBitboard(Bitboard bitboard) {
        constexpr Bitboard K1 = 0x5555555555555555;
        constexpr Bitboard K2 = 0x3333333333333333;
        constexpr Bitboard K4 = 0x0f0f0f0f0f0f0f0f;
        bitboard = ((bitboard >> 1) & K1) | ((bitboard & K1) << 1);
        bitboard = ((bitboard >> 2) & K2) | ((bitboard & K2) << 2);
        return ((bitboard >> 4) & K4) | ((bitboard & K4) << 4);
    }

    void addAllFeatures(Board* board, FeatureList& features) {
        // Set up bitboards
        Bitboard bitboards[8];
        for (Color side = Color::WHITE; side <= Color::BLACK; ++side)
            bitboards[side] = board->byColor[side];
        for (Piece piece = Piece::PAWN; piece < Piece::TOTAL; ++piece)
            bitboards[2 + piece] = board->byPiece[piece];
        
        // Add STM features
        addFeatures(bitboards, features);

        // Swap colors
        std::swap(bitboards[0], bitboards[1]);
        for (Piece piece = Piece::PAWN; piece < Piece::TOTAL; ++piece)
            bitboards[2 + piece] = __bswap_64(bitboards[2 + piece]);

        // Add NSTM features
        addFeatures(bitboards, features);
    }

    void addFeatures(Bitboard* bitboards, FeatureList& features) {
        // HM
        Square king = lsb(bitboards[0] & bitboards[2 + Piece::KING]);
        if (king % 8 > 3) {
            for (int i = 0; i < 8; i++) {
                bitboards[i] = mirrorBitboard(bitboards[i]);
            }
        }

        // Build mailbox
        Piece mailbox[64];
        std::memset(mailbox, Piece::NONE, sizeof(mailbox));
        for (Color side = Color::WHITE; side <= Color::BLACK; ++side) {
            for (Piece piece = Piece::PAWN; piece < Piece::TOTAL; ++piece) {
                Bitboard pieceBitboard = bitboards[side] & bitboards[2 + piece];
                while (pieceBitboard) {
                    Square square = popLSB(&pieceBitboard);
                    mailbox[square] = piece;
                }
            }
        }

        // Add features
        for (Color side = Color::WHITE; side <= Color::BLACK; ++side) {
            int32_t sideOffset = side * PieceOffsets::END;

            Bitboard occupancy = bitboards[0] | bitboards[1];
            Bitboard enemyOccupancy = bitboards[flip(side)];

            for (Piece piece = Piece::PAWN; piece < Piece::TOTAL; ++piece) {
                Bitboard pieceBitboard = bitboards[side] & bitboards[2 + piece];
                while (pieceBitboard) {
                    Square square = popLSB(&pieceBitboard);
                    Bitboard attacks = BB::attackedSquares(piece, square, occupancy, side) & occupancy;
                    
                    // Add the "standard" 768 piece feature
                    features.add(2 * PieceOffsets::END + 384 * flip(side) + 64 * piece + square);

                    while (attacks) {
                        Square attackedSquare = popLSB(&attacks);
                        bool enemy = static_cast<bool>(enemyOccupancy & Bitboard(attackedSquare));
                        
                        assert(mailbox[attackedSquare] != Piece::NONE);

                        addPieceThreat(piece, square, attackedSquare, mailbox[attackedSquare], enemy, features, sideOffset);
                    }
                }
            }
        }
    }

    void addPieceThreat(Piece piece, Square from, Square to, Piece target, bool enemy, FeatureList& features, int32_t sideOffset) {
        switch (piece) {
            case Piece::PAWN: { addPawnThreat(from, to, target, enemy, features, sideOffset); break; }
            case Piece::KNIGHT: { addKnightThreat(from, to, target, enemy, features, sideOffset); break; }
            case Piece::BISHOP: { addBishopThreat(from, to, target, enemy, features, sideOffset); break; }
            case Piece::ROOK: { addRookThreat(from, to, target, enemy, features, sideOffset); break; }
            case Piece::QUEEN: { addQueenThreat(from, to, target, enemy, features, sideOffset); break; }
            case Piece::KING: { addKingThreat(from, to, target, enemy, features, sideOffset); break; }
            default: assert(false);
        }
    }

    void addPawnThreat(Square from, Square to, Piece target, bool enemy, FeatureList& features, int32_t sideOffset) {
        // Allow threats to pawns, knights and rooks
        constexpr int32_t OFFSETS[] = { 0, 1, MAX, 2, MAX, MAX, 3, 4, MAX, 5, MAX, MAX };

        // Also skip enemy pawns with to > from to prevent duplicates
        if (OFFSETS[target] == MAX || (enemy && to > from && target == Piece::PAWN))
            return;

        bool up = to > from;
        int32_t diff = std::abs(to - from);
        int32_t id = ((up && diff == 7) || (!up && diff == 9)) ? 0 : 1;
        int32_t attack = 2 * (from % 8) + id - 1;
        int32_t threat = PieceOffsets::PAWN + OFFSETS[target + 6 * enemy] * SquareThreatCounts::PAWN + (from / 8 - 1) * 14 + attack;

        assert(threat >= PieceOffsets::PAWN);
        assert(threat < PieceOffsets::KNIGHT);

        features.add(threat + sideOffset);
    }

    void addKnightThreat(Square from, Square to, Piece target, bool enemy, FeatureList& features, int32_t sideOffset) {
        // Allow threats to all piece types

        // Skip knights with to > from to prevent duplicates
        if (to > from && target == Piece::KNIGHT)
            return;
        
        int32_t localIndex = SquareThreatCounts::KNIGHT[from] + localThreatIndex(to, BB::KNIGHT_ATTACKS[from]);
        int32_t threat = PieceOffsets::KNIGHT + (target + 6 * enemy) * SquareThreatCounts::KNIGHT[64] + localIndex;

        assert(threat >= PieceOffsets::KNIGHT);
        assert(threat < PieceOffsets::BISHOP);

        features.add(threat + sideOffset);
    }

    void addBishopThreat(Square from, Square to, Piece target, bool enemy, FeatureList& features, int32_t sideOffset) {
        // Allow threats to everything but queens
        constexpr int32_t OFFSETS[] = { 0, 1, 2, 3, MAX, 4, 5, 6, 7, 8, MAX, 9 };

        // Also skip bishops with to > from to prevent duplicates
        if (OFFSETS[target] == MAX || (to > from && target == Piece::BISHOP))
            return;

        int32_t localIndex = SquareThreatCounts::BISHOP[from] + localThreatIndex(to, getBishopMoves(from, 0));
        int32_t threat = PieceOffsets::BISHOP + OFFSETS[target + 6 * enemy] * SquareThreatCounts::BISHOP[64] + localIndex;

        assert(threat >= PieceOffsets::BISHOP);
        assert(threat < PieceOffsets::ROOK);

        features.add(threat + sideOffset);
    }

    void addRookThreat(Square from, Square to, Piece target, bool enemy, FeatureList& features, int32_t sideOffset) {
        // Allow threats to everything but queens
        constexpr int32_t OFFSETS[] = { 0, 1, 2, 3, MAX, 4, 5, 6, 7, 8, MAX, 9 };

        // Also skip rooks with to > from to prevent duplicates
        if (OFFSETS[target] == MAX || (to > from && target == Piece::ROOK))
            return;

        int32_t localIndex = SquareThreatCounts::ROOK[from] + localThreatIndex(to, getRookMoves(from, 0));
        int32_t threat = PieceOffsets::ROOK + OFFSETS[target + 6 * enemy] * SquareThreatCounts::ROOK[64] + localIndex;

        assert(threat >= PieceOffsets::ROOK);
        assert(threat < PieceOffsets::QUEEN);

        features.add(threat + sideOffset);
    }

    void addQueenThreat(Square from, Square to, Piece target, bool enemy, FeatureList& features, int32_t sideOffset) {
        // Allow threats to all pieces

        // Skip queens with to > from to prevent duplicates
        if (to > from && target == Piece::QUEEN)
            return;

        int32_t localIndex = SquareThreatCounts::QUEEN[from] + localThreatIndex(to, getRookMoves(from, 0));
        int32_t threat = PieceOffsets::QUEEN + (target + 6 * enemy) * SquareThreatCounts::QUEEN[64] + localIndex;

        assert(threat >= PieceOffsets::QUEEN);
        assert(threat < PieceOffsets::KING);

        features.add(threat + sideOffset);
    }

    void addKingThreat(Square from, Square to, Piece target, bool enemy, FeatureList& features, int32_t sideOffset) {
        // Allow threats to pawns, knights, bishops and rooks
        constexpr int32_t OFFSETS[] = { 0, 1, 2, 3, MAX, MAX, 4, 5, 6, 7, MAX, MAX };

        if (OFFSETS[target] == MAX)
            return;

        int32_t localIndex = SquareThreatCounts::KING[from] + localThreatIndex(to, BB::KING_ATTACKS[from]);
        int32_t threat = PieceOffsets::KING + OFFSETS[target + 6 * enemy] * SquareThreatCounts::KING[64] + localIndex;

        assert(threat >= PieceOffsets::KING);
        assert(threat < PieceOffsets::END);

        features.add(threat + sideOffset);
    }

}