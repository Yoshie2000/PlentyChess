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

    constexpr int MAX_ACTIVE_FEATURES = 128;
    constexpr int FEATURE_COUNT = 79856;

    class FeatureListIterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = int;
        using difference_type = std::ptrdiff_t;
        using pointer = int*;
        using reference = int&;

        FeatureListIterator(int* ptr) : current(ptr) {}

        reference operator*() const { return *current; }
        pointer operator->() { return current; }

        FeatureListIterator& operator++() {
            ++current;
            return *this;
        }

        FeatureListIterator operator++(int) {
            FeatureListIterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const FeatureListIterator& other) const {
            return current == other.current;
        }

        bool operator!=(const FeatureListIterator& other) const {
            return !(*this == other);
        }

    private:
        int* current;
    };

    class FeatureList {
    public:

        FeatureList() : featureCount(0) {}

        void add(int feature) {
            assert(featureCount + 1 < MAX_ACTIVE_FEATURES);
            featureIndices[featureCount++] = feature;
        }

        int operator[](int idx) const {
            assert(idx < featureCount);
            return featureIndices[idx];
        }

        int count() const {
            return featureCount;
        }

        int indexOf(int other) const {
            for (int i = 0; i < featureCount; i++) {
                if (featureIndices[i] == other)
                    return i;
            }
            return -1;
        }

        int remove(int idx) {
            int removed = featureIndices[idx];
            featureIndices[idx] = featureIndices[--featureCount];
            return removed;
        }

        FeatureListIterator begin() { return FeatureListIterator(featureIndices); }
        FeatureListIterator end() { return FeatureListIterator(featureIndices + featureCount); }

    private:
        int featureIndices[MAX_ACTIVE_FEATURES];
        int featureCount;
    };
    
    template<Color side>
    void addThreatFeatures(Board* board, FeatureList& features);
    void addPieceFeatures(Board* board, Color side, FeatureList& features, const uint8_t* KING_BUCKET_LAYOUT);
    
    void initialise();

    template<Color side>
    int getThreatFeature(Piece attackingPiece, uint8_t attackingColor, Square attackingSquare, Square attackedSquare, Piece attackedPiece, uint8_t attackedColor, bool mirrored);
    int getPieceFeature(Piece piece, Square relativeSquare, Color relativeColor, uint8_t kingBucket);

}