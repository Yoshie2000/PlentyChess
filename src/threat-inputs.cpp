#include <iostream>

#include "threat-inputs.h"

namespace ThreatInputs {

    struct PiecePairData {
        // Bit 0: Excluded if origin < target
        // Bit 1: Always excluded
        // Bits 8-31: Base feature
        uint32_t data;

        PiecePairData() {}
        PiecePairData(bool excluded, bool semiExcluded, int baseFeature) {
            data = (semiExcluded && !excluded) | (excluded << 1) | (baseFeature << 8);
        }

        bool isExcluded(Square attackingSquare, Square attackedSquare) const {
            bool lessThan = attackingSquare < attackedSquare;
            return (((uint8_t)data + lessThan) & 2);
        }
        int baseFeature() const { return data >> 8; }
    };

    PiecePairData PIECE_PAIR_LOOKUP[6][2][6][2];
    int PIECE_OFFSET_LOOKUP[6][2][64];
    uint8_t ATTACK_INDEX_LOOKUP[6][2][64][64];

    void initialise() {

        constexpr int PIECE_INTERACTION_MAP[6][6] = {
            {0,  1, -1,  2, -1, -1},
            {0,  1,  2,  3,  4,  5},
            {0,  1,  2,  3, -1,  4},
            {0,  1,  2,  3, -1,  4},
            {0,  1,  2,  3,  4,  5},
            {0,  1,  2,  3, -1, -1}
        };

        constexpr int PIECE_TARGET_COUNT[6] = { 6, 12, 10, 10, 12, 8 };

        int cumulativeOffset = 0;
        int CUMULATIVE_PIECE_OFFSET[6][2];
        int CUMULATIVE_OFFSET[6][2];
        for (Color color = Color::WHITE; color <= Color::BLACK; ++color) {
            for (Piece piece = Piece::PAWN; piece < Piece::TOTAL; ++piece) {
                int cumulativePieceOffset = 0;

                for (Square origin = 0; origin < 64; origin++) {
                    PIECE_OFFSET_LOOKUP[piece][color][origin] = cumulativePieceOffset;

                    if (piece != Piece::PAWN || (origin >= 8 && origin < 56)) {
                        Bitboard attacks = BB::attackedSquares(piece, origin, 0, color);
                        cumulativePieceOffset += BB::popcount(attacks);
                    }
                }

                CUMULATIVE_PIECE_OFFSET[piece][color] = cumulativePieceOffset;
                CUMULATIVE_OFFSET[piece][color] = cumulativeOffset;

                cumulativeOffset += PIECE_TARGET_COUNT[piece] * cumulativePieceOffset;
            }
        }

        for (Piece attackingPiece = Piece::PAWN; attackingPiece < Piece::TOTAL; ++attackingPiece) {
            for (Piece attackedPiece = Piece::PAWN; attackedPiece < Piece::TOTAL; ++attackedPiece) {
                for (Color attackingColor = Color::WHITE; attackingColor <= Color::BLACK; ++attackingColor) {
                    for (Color attackedColor = Color::WHITE; attackedColor <= Color::BLACK; ++attackedColor) {

                        int map = PIECE_INTERACTION_MAP[attackingPiece][attackedPiece];
                        int featureBase = CUMULATIVE_OFFSET[attackingPiece][attackingColor] + (attackedColor * (PIECE_TARGET_COUNT[attackingPiece] / 2) + map) * CUMULATIVE_PIECE_OFFSET[attackingPiece][attackingColor];

                        bool enemy = attackingColor != attackedColor;
                        bool semiExcluded = attackingPiece == attackedPiece && (enemy || attackingPiece != Piece::PAWN);
                        bool excluded = map < 0;

                        PIECE_PAIR_LOOKUP[attackingPiece][attackingColor][attackedPiece][attackedColor] = PiecePairData(excluded, semiExcluded, featureBase);
                    }
                }
            }
        }

        for (Piece piece = Piece::PAWN; piece < Piece::TOTAL; ++piece) {
            for (Color color = Color::WHITE; color <= Color::BLACK; ++color) {
                for (Square origin = 0; origin < 64; origin++) {
                    for (Square target = 0; target < 64; target++) {
                        Bitboard attacks = BB::attackedSquares(piece, origin, 0, color);
                        ATTACK_INDEX_LOOKUP[piece][color][origin][target] = BB::popcount((bitboard(target) - 1) & attacks);
                    }
                }
            }
        }
    }

    template<Color side>
    int getThreatFeature(Piece attackingPiece, uint8_t attackingColor, Square attackingSquare, Square attackedSquare, Piece attackedPiece, uint8_t attackedColor, bool mirrored) {
        uint8_t squareFlip = (7 * mirrored) ^ (56 * side);
        attackingSquare ^= squareFlip;
        attackedSquare ^= squareFlip;

        attackingColor ^= side;
        attackedColor ^= side;

        auto piecePairData = PIECE_PAIR_LOOKUP[attackingPiece][attackingColor][attackedPiece][attackedColor];
        if (piecePairData.isExcluded(attackingSquare, attackedSquare))
            return FEATURE_COUNT;

        int index = piecePairData.baseFeature()
            + PIECE_OFFSET_LOOKUP[attackingPiece][attackingColor][attackingSquare]
            + ATTACK_INDEX_LOOKUP[attackingPiece][attackingColor][attackingSquare][attackedSquare];

        assert(index < FEATURE_COUNT);
        return index;
    }
    template int getThreatFeature<Color::WHITE>(Piece attackingPiece, uint8_t attackingColor, Square attackingSquare, Square attackedSquare, Piece attackedPiece, uint8_t attackedColor, bool mirrored);
    template int getThreatFeature<Color::BLACK>(Piece attackingPiece, uint8_t attackingColor, Square attackingSquare, Square attackedSquare, Piece attackedPiece, uint8_t attackedColor, bool mirrored);

    int getPieceFeature(Piece piece, Square relativeSquare, Color relativeColor, uint8_t kingBucket) {
        return 768 * kingBucket + 384 * relativeColor + 64 * piece + relativeSquare;
    }

    template<Color pov>
    void addThreatFeatures(Board* board, FeatureList& features) {
        // Check HM and get occupancies
        Square king = lsb(board->byColor[pov] & board->byPiece[Piece::KING]);
        bool hm = fileOf(king) >= 4;

        // Loop through sides and pieces
        for (Color side = Color::WHITE; side <= Color::BLACK; ++side) {

            for (Piece piece = Piece::PAWN; piece < Piece::TOTAL; ++piece) {
                Bitboard pieceBitboard = board->byColor[side] & board->byPiece[piece];

                // Add features for this piece
                while (pieceBitboard) {
                    Square indexSquare = popLSB(&pieceBitboard);

                    // Add the threat features
                    Bitboard attacks = board->attackersTo(indexSquare);
                    while (attacks) {
                        Square attackingSquare = popLSB(&attacks);
                        Piece attackingPiece = board->pieces[attackingSquare];
                        Color attackingSide = (board->byColor[Color::WHITE] & bitboard(attackingSquare)) ? Color::WHITE : Color::BLACK;

                        assert(attackingPiece != Piece::NONE);

                        int threatFeature = getThreatFeature<pov>(attackingPiece, attackingSide, attackingSquare, indexSquare, piece, side, hm);
                        if (threatFeature < FEATURE_COUNT)
                            features.add(threatFeature);
                    }
                }
            }
        }
    }
    template void addThreatFeatures<Color::WHITE>(Board* board, FeatureList& features);
    template void addThreatFeatures<Color::BLACK>(Board* board, FeatureList& features);

    void addPieceFeatures(Board* board, Color pov, FeatureList& features, const uint8_t* KING_BUCKET_LAYOUT) {
        // Check HM and get occupancies
        Square king = lsb(board->byColor[pov] & board->byPiece[Piece::KING]);
        uint8_t kingBucket = KING_BUCKET_LAYOUT[king ^ (56 * pov)];
        bool hm = fileOf(king) >= 4;

        // Loop through sides and pieces
        for (Color side = Color::WHITE; side <= Color::BLACK; ++side) {
            for (Piece piece = Piece::PAWN; piece < Piece::TOTAL; ++piece) {
                Bitboard pieceBitboard = board->byColor[side] & board->byPiece[piece];

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

}