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
    int getThreatFeature(uint8_t attackingPiece, uint8_t attackedPiece, Square attackingSquare, Square attackedSquare, bool mirrored);
    int getPieceFeature(Piece piece, Square relativeSquare, Color relativeColor, uint8_t kingBucket);

}