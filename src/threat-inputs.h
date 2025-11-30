#pragma once

#include <cstdint>
#include <cassert>
#include <limits>
#include <cstring>

#include "types.h"
#include "bitboard.h"
#include "magic.h"
#include "board.h"

namespace ThreatInputs {

    constexpr int FEATURE_COUNT = 79856;
    constexpr int MAX_ACTIVE_FEATURES = 128;
    using FeatureList = ArrayVec<int, MAX_ACTIVE_FEATURES>;

    template<Color side>
    void addThreatFeatures(Board* board, FeatureList& features);
    void addPieceFeatures(Board* board, Color side, FeatureList& features, const uint8_t* KING_BUCKET_LAYOUT);
    
    void initialise();

    template<Color side>
    int getThreatFeature(Piece attackingPiece, uint8_t attackingColor, Square attackingSquare, Square attackedSquare, Piece attackedPiece, uint8_t attackedColor, bool mirrored);
    int getPieceFeature(Piece piece, Square relativeSquare, Color relativeColor, bool pinned, uint8_t kingBucket);

}