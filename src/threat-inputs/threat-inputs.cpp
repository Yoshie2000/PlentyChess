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

    PiecePairData PIECE_PAIR_LOOKUP[14][14];
    uint16_t PIECE_OFFSET_LOOKUP[14][64];
    uint8_t ATTACK_INDEX_LOOKUP[14][64][64];

    void initialise() {

        constexpr int PIECE_INTERACTION_MAP[6][6] = {
            {-1, 0, -1,  1, -1, -1},
            {0,  1,  2,  3,  4,  -1},
            {0,  1,  2,  3, -1,  -1},
            {0,  1,  2,  3, -1,  -1},
            {0,  1,  2,  3,  4,  -1},
            {-1, -1, -1, -1, -1, -1}
        };

        constexpr int PIECE_TARGET_COUNT[6] = { 4, 10, 8, 8, 10, 0 };

        int cumulativeOffset = 0;
        int CUMULATIVE_PIECE_OFFSET[6][2];
        int CUMULATIVE_OFFSET[6][2];
        for (Color color = Color::WHITE; color <= Color::BLACK; ++color) {
            for (Piece piece = Piece::PAWN; piece < Piece::TOTAL; ++piece) {
                int cumulativePieceOffset = 0;

                for (Square origin = 0; origin < 64; origin++) {
                    PIECE_OFFSET_LOOKUP[piece | (color << 3)][origin] = cumulativePieceOffset;

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

                        assert(featureBase >= 0 || excluded);

                        if (excluded)
                            featureBase = 0;

                        PIECE_PAIR_LOOKUP[attackingPiece | (attackingColor << 3)][attackedPiece | (attackedColor << 3)] = PiecePairData(excluded, semiExcluded, featureBase);
                    }
                }
            }
        }

        for (Piece piece = Piece::PAWN; piece < Piece::TOTAL; ++piece) {
            for (Color color = Color::WHITE; color <= Color::BLACK; ++color) {
                for (Square origin = 0; origin < 64; origin++) {
                    for (Square target = 0; target < 64; target++) {
                        Bitboard attacks = BB::attackedSquares(piece, origin, 0, color);
                        ATTACK_INDEX_LOOKUP[piece | (color << 3)][origin][target] = BB::popcount((bitboard(target) - 1) & attacks);
                    }
                }
            }
        }
    }

    template<Color side>
    int getThreatFeature(uint8_t attackingPiece, uint8_t attackedPiece, Square attackingSquare, Square attackedSquare, bool mirrored) {
        uint8_t squareFlip = (7 * mirrored) ^ (56 * side);
        attackingSquare ^= squareFlip;
        attackedSquare ^= squareFlip;

        constexpr uint8_t sideFlip = side << 3;
        attackingPiece ^= sideFlip;
        attackedPiece ^= sideFlip;

        auto piecePairData = PIECE_PAIR_LOOKUP[attackingPiece][attackedPiece];
        if (piecePairData.isExcluded(attackingSquare, attackedSquare))
            return FEATURE_COUNT;

        int index = piecePairData.baseFeature()
            + PIECE_OFFSET_LOOKUP[attackingPiece][attackingSquare]
            + ATTACK_INDEX_LOOKUP[attackingPiece][attackingSquare][attackedSquare];

        assert(index < FEATURE_COUNT);
        return index;
    }
    template int getThreatFeature<Color::WHITE>(uint8_t attackingPiece, uint8_t attackedPiece, Square attackingSquare, Square attackedSquare, bool mirrored);
    template int getThreatFeature<Color::BLACK>(uint8_t attackingPiece, uint8_t attackedPiece, Square attackingSquare, Square attackedSquare, bool mirrored);

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

                        int threatFeature = getThreatFeature<pov>(attackingPiece | (attackingSide << 3), piece | (side << 3), attackingSquare, indexSquare, hm);
                        if (threatFeature < FEATURE_COUNT)
                            features.add(THREAT_OFFSET + threatFeature);
                    }
                }
            }
        }
    }
    template void addThreatFeatures<Color::WHITE>(Board* board, FeatureList& features);
    template void addThreatFeatures<Color::BLACK>(Board* board, FeatureList& features);

    int pawnPairIndex(int idA, int idB) {
        int lo = idA < idB ? idA : idB;
        int hi = idA < idB ? idB : idA;
        return hi * (hi - 1) / 2 + lo;
    }

    int pawnId(Square square, int enemyOffset, uint8_t squareFlip) {
        return enemyOffset + ((square ^ squareFlip) - 8);
    }

    template<Color side>
    void addPawnPairFeatures(Board* board, FeatureList& features) {
        Square king = lsb(board->byColor[side] & board->byPiece[Piece::KING]);
        bool mirrored = fileOf(king) >= 4;
        uint8_t squareFlip = (7 * mirrored) ^ (56 * side);

        Bitboard pawns = board->byPiece[Piece::PAWN];
        Bitboard friendly = board->byColor[side] & pawns;
        Bitboard enemy = board->byColor[flip(side)] & pawns;

        // Friendly-friendly pairs
        Bitboard outer = friendly;
        while (outer) {
            Square square = popLSB(&outer);
            int id = pawnId(square, 0, squareFlip);
            Bitboard inner = outer & PP_MASK[square];
            while (inner)
                features.add(pawnPairIndex(id, pawnId(popLSB(&inner), 0, squareFlip)));
        }

        // Friendly-enemy pairs
        outer = friendly;
        while (outer) {
            Square square = popLSB(&outer);
            int id = pawnId(square, 0, squareFlip);
            Bitboard inner = enemy & PP_MASK[square];
            while (inner)
                features.add(pawnPairIndex(id, pawnId(popLSB(&inner), PAWN_SQUARE_COUNT, squareFlip)));
        }

        // Enemy-enemy pairs
        outer = enemy;
        while (outer) {
            Square square = popLSB(&outer);
            int id = pawnId(square, PAWN_SQUARE_COUNT, squareFlip);
            Bitboard inner = outer & PP_MASK[square];
            while (inner)
                features.add(pawnPairIndex(id, pawnId(popLSB(&inner), PAWN_SQUARE_COUNT, squareFlip)));
        }
    }
    template void addPawnPairFeatures<Color::WHITE>(Board* board, FeatureList& features);
    template void addPawnPairFeatures<Color::BLACK>(Board* board, FeatureList& features);

    template<Color side>
    void addPawnPairDeltas(Board* board, const DirtyPiece& dirtyPiece, bool mirrored, FeatureList& adds, FeatureList& subs) {
        uint8_t squareFlip = (7 * mirrored) ^ (56 * side);

        bool promotion = dirtyPiece.target == NO_SQUARE;
        bool castling = dirtyPiece.addSquare != NO_SQUARE && dirtyPiece.target != NO_SQUARE;

        // Data of pawns that left their square (moved or captured)
        Square removedSquares[2];
        int removedIds[2];
        int removedCount = 0;

        // Mark moving pawn
        if (dirtyPiece.piece == Piece::PAWN) {
            int offset = dirtyPiece.pieceColor != side ? PAWN_SQUARE_COUNT : 0;
            removedSquares[removedCount] = dirtyPiece.origin;
            removedIds[removedCount] = pawnId(dirtyPiece.origin, offset, squareFlip);
            removedCount++;
        }
        // Mark captured pawn
        if (dirtyPiece.removeSquare != NO_SQUARE && dirtyPiece.removePiece == Piece::PAWN && !castling) {
            int offset = dirtyPiece.pieceColor == side ? PAWN_SQUARE_COUNT : 0;
            removedSquares[removedCount] = dirtyPiece.removeSquare;
            removedIds[removedCount] = pawnId(dirtyPiece.removeSquare, offset, squareFlip);
            removedCount++;
        }

        Bitboard exclude = 0;
        int addedId = 0;
        bool pawnAdded = dirtyPiece.piece == Piece::PAWN && !promotion;
        // Add pawn at the target square
        if (pawnAdded) {
            int offset = (dirtyPiece.pieceColor != side) ? PAWN_SQUARE_COUNT : 0;
            addedId = pawnId(dirtyPiece.target, offset, squareFlip);
            exclude = bitboard(dirtyPiece.target);
        }

        if (removedCount == 0 && !pawnAdded)
            return;

        Bitboard pawns = board->byPiece[Piece::PAWN];
        Bitboard unchangedFriendly = (board->byColor[side] & pawns) & ~exclude;
        Bitboard unchangedEnemy = (board->byColor[flip(side)] & pawns) & ~exclude;

        // Remove all pairs of removed pawns
        for (int i = 0; i < removedCount; i++) {
            Bitboard band = PP_MASK[removedSquares[i]];
            Bitboard f = unchangedFriendly & band;
            while (f)
                subs.add(pawnPairIndex(removedIds[i], pawnId(popLSB(&f), 0, squareFlip)));
            Bitboard e = unchangedEnemy & band;
            while (e)
                subs.add(pawnPairIndex(removedIds[i], pawnId(popLSB(&e), PAWN_SQUARE_COUNT, squareFlip)));
        }
        // Remove pair between two removed pawns
        if (removedCount == 2 && (PP_MASK[removedSquares[0]] & bitboard(removedSquares[1])))
            subs.add(pawnPairIndex(removedIds[0], removedIds[1]));

        // Add all pairs for the added pawn
        if (pawnAdded) {
            Bitboard band = PP_MASK[dirtyPiece.target];
            Bitboard f = unchangedFriendly & band;
            while (f)
                adds.add(pawnPairIndex(addedId, pawnId(popLSB(&f), 0, squareFlip)));
            Bitboard e = unchangedEnemy & band;
            while (e)
                adds.add(pawnPairIndex(addedId, pawnId(popLSB(&e), PAWN_SQUARE_COUNT, squareFlip)));
        }
    }
    template void addPawnPairDeltas<Color::WHITE>(Board* board, const DirtyPiece& dirtyPiece, bool mirrored, FeatureList& adds, FeatureList& subs);
    template void addPawnPairDeltas<Color::BLACK>(Board* board, const DirtyPiece& dirtyPiece, bool mirrored, FeatureList& adds, FeatureList& subs);

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