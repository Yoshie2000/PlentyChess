#include <iostream>

#include "threat-inputs.h"

namespace ThreatInputs {

    // Count how many attacked squares are < to, effectively creating an index for the "to" square relative to the "from" square
    int localThreatIndex(Square to, Bitboard attackedByFrom) {
        return BB::popcount(attackedByFrom & (bitboard(to) - 1));
    }

    Bitboard mirrorBitboard(Bitboard bitboard) {
        constexpr Bitboard K1 = 0x5555555555555555;
        constexpr Bitboard K2 = 0x3333333333333333;
        constexpr Bitboard K4 = 0x0f0f0f0f0f0f0f0f;
        bitboard = ((bitboard >> 1) & K1) | ((bitboard & K1) << 1);
        bitboard = ((bitboard >> 2) & K2) | ((bitboard & K2) << 2);
        return ((bitboard >> 4) & K4) | ((bitboard & K4) << 4);
    }

    void addSideFeatures(Board* board, Color pov, FeatureList& features) {
        // Check HM and get occupancies
        Square king = lsb(board->byColor[pov] & board->byPiece[Piece::KING]);
        bool hm = fileOf(king) >= 4;

        Bitboard occupancy = board->byColor[Color::WHITE] | board->byColor[Color::BLACK];
        if (hm) occupancy = mirrorBitboard(occupancy);
        Bitboard nonPovOccupancy = board->byColor[flip(pov)];
        if (hm) nonPovOccupancy = mirrorBitboard(nonPovOccupancy);

        // Loop through sides and pieces
        for (Color side = Color::WHITE; side <= Color::BLACK; ++side) {
            int sideOffset = (side != pov) * PieceOffsets::END;

            Bitboard enemyOccupancy = board->byColor[flip(side)];
            if (hm) enemyOccupancy = mirrorBitboard(enemyOccupancy);

            for (Piece piece = Piece::PAWN; piece < Piece::TOTAL; ++piece) {
                Bitboard pieceBitboard = board->byColor[side] & board->byPiece[piece];

                // Add features for this piece
                while (pieceBitboard) {
                    Square indexSquare = popLSB(&pieceBitboard);
                    Square square = indexSquare ^ (hm * 7);

                    // Add the "standard" 768 piece feature
                    Square relativeSquare = square ^ (56 * pov);
                    addPieceFeature(piece, relativeSquare, static_cast<Color>(side != pov), features);

                    // Add the threat features
                    Bitboard attacks = board->stack->threats.bySquare[indexSquare];
                    if (hm) attacks = mirrorBitboard(attacks);
                    attacks &= occupancy;
                    while (attacks) {
                        Square attackedSquare = popLSB(&attacks);
                        bool enemy = static_cast<bool>(enemyOccupancy & bitboard(attackedSquare));
                        Color relativeSide = static_cast<Color>(static_cast<bool>(nonPovOccupancy & bitboard(attackedSquare)));
                        Piece attackedPiece = board->pieces[attackedSquare ^ (hm * 7)];

                        assert(attackedPiece != Piece::NONE);
                        
                        Square relativeAttackedSquare = attackedSquare ^ (56 * pov);
                        addThreatFeature(piece, relativeSquare, relativeAttackedSquare, attackedPiece, relativeSide, enemy, features, sideOffset);
                    }
                }
            }
        }
    }

    void addPieceFeature(Piece piece, Square relativeSquare, Color relativeColor, FeatureList& features) {
        features.add(2 * PieceOffsets::END + 384 * relativeColor + 64 * piece + relativeSquare);
    }

    void addThreatFeature(Piece piece, Square from, Square to, Piece target, Color relativeSide, bool enemy, FeatureList& features, int sideOffset) {
        switch (piece) {
        case Piece::PAWN: { addPawnThreatFeature(from, to, target, relativeSide, enemy, features, sideOffset); break; }
        case Piece::KNIGHT: { addKnightThreatFeature(from, to, target, relativeSide, features, sideOffset); break; }
        case Piece::BISHOP: { addBishopThreatFeature(from, to, target, relativeSide, features, sideOffset); break; }
        case Piece::ROOK: { addRookThreatFeature(from, to, target, relativeSide, features, sideOffset); break; }
        case Piece::QUEEN: { addQueenThreatFeature(from, to, target, relativeSide, features, sideOffset); break; }
        case Piece::KING: { addKingThreatFeature(from, to, target, relativeSide, features, sideOffset); break; }
        default: assert(false);
        }
    }

    void addPawnThreatFeature(Square from, Square to, Piece target, Color relativeSide, bool enemy, FeatureList& features, int sideOffset) {
        // Allow threats to pawns, knights and rooks
        constexpr int OFFSETS[] = { 0, 1, MAX, 2, MAX, MAX, 3, 4, MAX, 5, MAX, MAX };

        // Also skip enemy pawns with to > from to prevent duplicates
        if (OFFSETS[target] == MAX || (enemy && to > from && target == Piece::PAWN))
            return;

        bool up = to > from;
        int diff = std::abs(to - from);
        int id = ((up && diff == 7) || (!up && diff == 9)) ? 0 : 1;
        int attack = 2 * (from % 8) + id - 1;
        int threat = PieceOffsets::PAWN + OFFSETS[target + 6 * relativeSide] * SquareThreatCounts::PAWN + (from / 8 - 1) * 14 + attack;

        assert(threat >= PieceOffsets::PAWN);
        assert(threat < PieceOffsets::KNIGHT);

        features.add(threat + sideOffset);
    }

    void addKnightThreatFeature(Square from, Square to, Piece target, Color relativeSide, FeatureList& features, int sideOffset) {
        // Allow threats to all piece types

        // Skip knights with to > from to prevent duplicates
        if (to > from && target == Piece::KNIGHT)
            return;
        
        int localIndex = SquareThreatCounts::KNIGHT[from] + localThreatIndex(to, BB::KNIGHT_ATTACKS[from]);
        int threat = PieceOffsets::KNIGHT + (target + 6 * relativeSide) * SquareThreatCounts::KNIGHT[64] + localIndex;

        assert(threat >= PieceOffsets::KNIGHT);
        assert(threat < PieceOffsets::BISHOP);

        features.add(threat + sideOffset);
    }

    void addBishopThreatFeature(Square from, Square to, Piece target, Color relativeSide, FeatureList& features, int sideOffset) {
        // Allow threats to everything but queens
        constexpr int OFFSETS[] = { 0, 1, 2, 3, MAX, 4, 5, 6, 7, 8, MAX, 9 };

        // Also skip bishops with to > from to prevent duplicates
        if (OFFSETS[target] == MAX || (to > from && target == Piece::BISHOP))
            return;

        int localIndex = SquareThreatCounts::BISHOP[from] + localThreatIndex(to, getBishopMoves(from, 0));
        int threat = PieceOffsets::BISHOP + OFFSETS[target + 6 * relativeSide] * SquareThreatCounts::BISHOP[64] + localIndex;

        assert(threat >= PieceOffsets::BISHOP);
        assert(threat < PieceOffsets::ROOK);

        features.add(threat + sideOffset);
    }

    void addRookThreatFeature(Square from, Square to, Piece target, Color relativeSide, FeatureList& features, int sideOffset) {
        // Allow threats to everything but queens
        constexpr int OFFSETS[] = { 0, 1, 2, 3, MAX, 4, 5, 6, 7, 8, MAX, 9 };

        // Also skip rooks with to > from to prevent duplicates
        if (OFFSETS[target] == MAX || (to > from && target == Piece::ROOK))
            return;

        int localIndex = SquareThreatCounts::ROOK[from] + localThreatIndex(to, getRookMoves(from, 0));
        int threat = PieceOffsets::ROOK + OFFSETS[target + 6 * relativeSide] * SquareThreatCounts::ROOK[64] + localIndex;

        assert(threat >= PieceOffsets::ROOK);
        assert(threat < PieceOffsets::QUEEN);

        features.add(threat + sideOffset);
    }

    void addQueenThreatFeature(Square from, Square to, Piece target, Color relativeSide, FeatureList& features, int sideOffset) {
        // Allow threats to all pieces

        // Skip queens with to > from to prevent duplicates
        if (to > from && target == Piece::QUEEN)
            return;

        int localIndex = SquareThreatCounts::QUEEN[from] + localThreatIndex(to, getRookMoves(from, 0) | getBishopMoves(from, 0));
        int threat = PieceOffsets::QUEEN + (target + 6 * relativeSide) * SquareThreatCounts::QUEEN[64] + localIndex;

        assert(threat >= PieceOffsets::QUEEN);
        assert(threat < PieceOffsets::KING);

        features.add(threat + sideOffset);
    }

    void addKingThreatFeature(Square from, Square to, Piece target, Color relativeSide, FeatureList& features, int sideOffset) {
        // Allow threats to pawns, knights, bishops and rooks
        constexpr int OFFSETS[] = { 0, 1, 2, 3, MAX, MAX, 4, 5, 6, 7, MAX, MAX };

        if (OFFSETS[target] == MAX)
            return;

        int localIndex = SquareThreatCounts::KING[from] + localThreatIndex(to, BB::KING_ATTACKS[from]);
        int threat = PieceOffsets::KING + OFFSETS[target + 6 * relativeSide] * SquareThreatCounts::KING[64] + localIndex;

        assert(threat >= PieceOffsets::KING);
        assert(threat < PieceOffsets::END);

        features.add(threat + sideOffset);
    }

}