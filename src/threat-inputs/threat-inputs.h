#pragma once

#include <cstdint>
#include <cassert>
#include <limits>
#include <cstring>
#include <array>

#include "../types.h"
#include "../bitboard.h"
#include "../magic.h"
#include "../board.h"

namespace ThreatInputs {

    constexpr int FEATURE_COUNT = 59808;
    constexpr int MAX_ACTIVE_FEATURES = 128;

    constexpr int PAWN_SQUARE_COUNT = 48;
    constexpr int PAWN_PAIR_FEATURE_COUNT = 96 * 95 / 2;
    constexpr int MAX_PAWN_PAIR_FEATURES = 64;
    constexpr int THREAT_OFFSET = PAWN_PAIR_FEATURE_COUNT;

    using FeatureList = ArrayVec<int, MAX_ACTIVE_FEATURES + MAX_PAWN_PAIR_FEATURES>;

    inline constexpr std::array<Bitboard, 64> PP_MASK = [] {
        std::array<Bitboard, 64> table{};
        constexpr Bitboard FILE_A = 0x0101010101010101ULL;
        for (int sq = 8; sq < 56; sq++) {
            int file = sq & 7;
            Bitboard mask = FILE_A << file;
            if (file > 0) mask |= FILE_A << (file - 1);
            if (file < 7) mask |= FILE_A << (file + 1);
            table[sq] = mask;
        }
        return table;
    }();

    template<Color side>
    void addThreatFeatures(Board* board, FeatureList& features);
    void addPieceFeatures(Board* board, Color side, FeatureList& features, const uint8_t* KING_BUCKET_LAYOUT);

    template<Color side>
    void addPawnPairFeatures(Board* board, FeatureList& features);
    template<Color side>
    void addPawnPairDeltas(Board* board, const DirtyPiece& dirtyPiece, bool mirrored, FeatureList& adds, FeatureList& subs);

    void initialise();

    template<Color side>
    int getThreatFeature(uint8_t attackingPiece, uint8_t attackedPiece, Square attackingSquare, Square attackedSquare, bool mirrored);
    int getPieceFeature(Piece piece, Square relativeSquare, Color relativeColor, uint8_t kingBucket);

}