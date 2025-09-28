#include <iostream>

#include "threat-inputs.h"

namespace ThreatInputs {

    int INDEX_LOOKUP[6][64][64][6][2][2];

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

    void addThreatFeatures(Bitboard* byColor, Bitboard* byPiece, Piece* pieces, Threats* threats, Color pov, FeatureList& features) {
        // Check HM and get occupancies
        Square king = lsb(byColor[pov] & byPiece[Piece::KING]);
        bool hm = fileOf(king) >= 4;

        Bitboard occupancy = byColor[Color::WHITE] | byColor[Color::BLACK];
        if (hm) occupancy = mirrorBitboard(occupancy);
        Bitboard nonPovOccupancy = byColor[flip(pov)];
        if (hm) nonPovOccupancy = mirrorBitboard(nonPovOccupancy);

        // Loop through sides and pieces
        for (Color side = Color::WHITE; side <= Color::BLACK; ++side) {

            Bitboard enemyOccupancy = byColor[flip(side)];
            if (hm) enemyOccupancy = mirrorBitboard(enemyOccupancy);

            for (Piece piece = Piece::PAWN; piece < Piece::TOTAL; ++piece) {
                Bitboard pieceBitboard = byColor[side] & byPiece[piece];

                // Add features for this piece
                while (pieceBitboard) {
                    Square indexSquare = popLSB(&pieceBitboard);
                    Square square = indexSquare ^ (hm * 7);
                    Square relativeSquare = square ^ (56 * pov);

                    // Add the threat features
                    Bitboard attacks = threats->toSquare[indexSquare];
                    if (hm) attacks = mirrorBitboard(attacks);
                    attacks &= occupancy;
                    while (attacks) {
                        Square attackingSquare = popLSB(&attacks);
                        Square attackingIndexSquare = attackingSquare ^ (hm * 7);
                        Square relativeAttackingSquare = attackingSquare ^ (56 * pov);
                        Piece attackingPiece = pieces[attackingIndexSquare];
                        Color attackingSide = (byColor[Color::WHITE] & bitboard(attackingIndexSquare)) ? Color::WHITE : Color::BLACK;

                        Color relativeSide = static_cast<Color>(pov != side);
                        bool enemy = attackingSide != side;
                        int sideOffset = (attackingSide != pov) * PieceOffsets::END;

                        assert(attackingPiece != Piece::NONE);

                        int threatFeature = getThreatFeature(attackingPiece, relativeAttackingSquare, relativeSquare, piece, relativeSide, enemy);
                        if (threatFeature != -1)
                            features.add(threatFeature + sideOffset);
                    }
                }
            }
        }
    }

    void addPieceFeatures(Bitboard* byColor, Bitboard* byPiece, Color pov, FeatureList& features, const uint8_t* KING_BUCKET_LAYOUT) {
        // Check HM and get occupancies
        Square king = lsb(byColor[pov] & byPiece[Piece::KING]);
        uint8_t kingBucket = KING_BUCKET_LAYOUT[king ^ (56 * pov)];
        bool hm = fileOf(king) >= 4;

        // Loop through sides and pieces
        for (Color side = Color::WHITE; side <= Color::BLACK; ++side) {
            for (Piece piece = Piece::PAWN; piece < Piece::TOTAL; ++piece) {
                Bitboard pieceBitboard = byColor[side] & byPiece[piece];

                // Add features for this piece
                while (pieceBitboard) {
                    Square indexSquare = popLSB(&pieceBitboard);
                    Square square = indexSquare ^ (hm * 7);

                    // Add the "standard" 768 piece feature
                    Square relativeSquare = square ^ (56 * pov);
                    features.add(getPieceFeature(piece, relativeSquare, static_cast<Color>(side != pov), kingBucket));
                }
            }
        }
    }

    int getPieceFeature(Piece piece, Square relativeSquare, Color relativeColor, uint8_t kingBucket) {
        return 2 * PieceOffsets::END + (384 * relativeColor + 64 * piece + relativeSquare) + 768 * kingBucket;
    }

    void initialise() {
        memset(INDEX_LOOKUP, 0xFFFF, sizeof(INDEX_LOOKUP));

        for (Piece attackingPiece = Piece::PAWN; attackingPiece < Piece::TOTAL; ++attackingPiece) {
            for (Piece attackedPiece = Piece::PAWN; attackedPiece < Piece::TOTAL; ++attackedPiece) {
                for (Color attackingColor = Color::WHITE; attackingColor <= Color::BLACK; ++attackingColor) {
                    for (Color attackedColor = Color::WHITE; attackedColor <= Color::BLACK; ++attackedColor) {
                        for (Color pov = Color::WHITE; pov <= Color::BLACK; ++pov) {
                            for (Square attackingSquare = 0; attackingSquare < 64; attackingSquare++) {

                                if (attackingPiece == Piece::PAWN && (attackingSquare < 8 || attackingSquare >= 56))
                                    continue;

                                Bitboard attacks = BB::attackedSquares(attackingPiece, attackingSquare, 0, attackingColor);
                                assert(attacks > 0);
                                while (attacks) {
                                    Square attackedSquare = popLSB(&attacks);

                                    Color relativeSide = static_cast<Color>(pov != attackedColor);
                                    bool enemy = attackingColor != attackedColor;

                                    int feature = getThreatFeature(attackingPiece, attackingSquare, attackedSquare, attackedPiece, relativeSide, enemy);
                                    INDEX_LOOKUP[attackingPiece][attackingSquare][attackedSquare][attackedPiece][relativeSide][enemy] = feature;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    int getThreatFeature(Piece piece, Square from, Square to, Piece target, Color relativeSide, bool enemy) {
        assert(piece != Piece::NONE);

        int featureIndex;
        switch (piece) {
        case Piece::PAWN:
            featureIndex = getPawnThreatFeature(from, to, target, relativeSide, enemy);
            break;
        case Piece::KNIGHT:
            featureIndex = getKnightThreatFeature(from, to, target, relativeSide);
            break;
        case Piece::BISHOP:
            featureIndex = getBishopThreatFeature(from, to, target, relativeSide);
            break;
        case Piece::ROOK:
            featureIndex = getRookThreatFeature(from, to, target, relativeSide);
            break;
        case Piece::QUEEN:
            featureIndex = getQueenThreatFeature(from, to, target, relativeSide);
            break;
        case Piece::KING:
            featureIndex = getKingThreatFeature(from, to, target, relativeSide);
            break;
        case Piece::NONE:
            featureIndex = -1;
            assert(false);
        }

        return featureIndex;
    }

    int getPawnThreatFeature(Square from, Square to, Piece target, Color relativeSide, bool enemy) {
        // Allow threats to pawns, knights and rooks
        constexpr int OFFSETS[] = { 0, 1, MAX, 2, MAX, MAX, 3, 4, MAX, 5, MAX, MAX };

        // Also skip enemy pawns with to > from to prevent duplicates
        if (OFFSETS[target] == MAX || (enemy && to > from && target == Piece::PAWN))
            return -1;

        bool up = to > from;
        int diff = std::abs(to - from);
        int id = ((up && diff == 7) || (!up && diff == 9)) ? 0 : 1;
        int attack = 2 * (from % 8) + id - 1;
        int threat = PieceOffsets::PAWN + OFFSETS[target + 6 * relativeSide] * SquareThreatCounts::PAWN + (from / 8 - 1) * 14 + attack;

        assert(threat >= PieceOffsets::PAWN);
        assert(threat < PieceOffsets::KNIGHT);

        return threat;
    }

    int getKnightThreatFeature(Square from, Square to, Piece target, Color relativeSide) {
        // Allow threats to all piece types

        // Skip knights with to > from to prevent duplicates
        if (to > from && target == Piece::KNIGHT)
            return -1;

        int localIndex = SquareThreatCounts::KNIGHT[from] + localThreatIndex(to, BB::KNIGHT_ATTACKS[from]);
        int threat = PieceOffsets::KNIGHT + (target + 6 * relativeSide) * SquareThreatCounts::KNIGHT[64] + localIndex;

        assert(threat >= PieceOffsets::KNIGHT);
        assert(threat < PieceOffsets::BISHOP);

        return threat;
    }

    int getBishopThreatFeature(Square from, Square to, Piece target, Color relativeSide) {
        // Allow threats to everything but queens
        constexpr int OFFSETS[] = { 0, 1, 2, 3, MAX, 4, 5, 6, 7, 8, MAX, 9 };

        // Also skip bishops with to > from to prevent duplicates
        if (OFFSETS[target] == MAX || (to > from && target == Piece::BISHOP))
            return -1;

        int localIndex = SquareThreatCounts::BISHOP[from] + localThreatIndex(to, getBishopMoves(from, 0));
        int threat = PieceOffsets::BISHOP + OFFSETS[target + 6 * relativeSide] * SquareThreatCounts::BISHOP[64] + localIndex;

        assert(threat >= PieceOffsets::BISHOP);
        assert(threat < PieceOffsets::ROOK);

        return threat;
    }

    int getRookThreatFeature(Square from, Square to, Piece target, Color relativeSide) {
        // Allow threats to everything but queens
        constexpr int OFFSETS[] = { 0, 1, 2, 3, MAX, 4, 5, 6, 7, 8, MAX, 9 };

        // Also skip rooks with to > from to prevent duplicates
        if (OFFSETS[target] == MAX || (to > from && target == Piece::ROOK))
            return -1;

        int localIndex = SquareThreatCounts::ROOK[from] + localThreatIndex(to, getRookMoves(from, 0));
        int threat = PieceOffsets::ROOK + OFFSETS[target + 6 * relativeSide] * SquareThreatCounts::ROOK[64] + localIndex;

        assert(threat >= PieceOffsets::ROOK);
        assert(threat < PieceOffsets::QUEEN);

        return threat;
    }

    int getQueenThreatFeature(Square from, Square to, Piece target, Color relativeSide) {
        // Allow threats to all pieces

        // Skip queens with to > from to prevent duplicates
        if (to > from && target == Piece::QUEEN)
            return -1;

        int localIndex = SquareThreatCounts::QUEEN[from] + localThreatIndex(to, getRookMoves(from, 0) | getBishopMoves(from, 0));
        int threat = PieceOffsets::QUEEN + (target + 6 * relativeSide) * SquareThreatCounts::QUEEN[64] + localIndex;

        assert(threat >= PieceOffsets::QUEEN);
        assert(threat < PieceOffsets::KING);

        return threat;
    }

    int getKingThreatFeature(Square from, Square to, Piece target, Color relativeSide) {
        // Allow threats to pawns, knights, bishops and rooks
        constexpr int OFFSETS[] = { 0, 1, 2, 3, MAX, MAX, 4, 5, 6, 7, MAX, MAX };

        if (OFFSETS[target] == MAX)
            return -1;

        int localIndex = SquareThreatCounts::KING[from] + localThreatIndex(to, BB::KING_ATTACKS[from]);
        int threat = PieceOffsets::KING + OFFSETS[target + 6 * relativeSide] * SquareThreatCounts::KING[64] + localIndex;

        assert(threat >= PieceOffsets::KING);
        assert(threat < PieceOffsets::END);

        return threat;
    }

}