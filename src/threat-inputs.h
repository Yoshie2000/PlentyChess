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

        void remove(int idx) {
            featureIndices[idx] = featureIndices[--featureCount];
        }

        FeatureListIterator begin() { return FeatureListIterator(featureIndices); }
        FeatureListIterator end() { return FeatureListIterator(featureIndices + featureCount); }

    private:
        int featureIndices[MAX_ACTIVE_FEATURES];
        int featureCount;
    };

    namespace SquareThreatCounts {

        constexpr int PAWN = 84;
        constexpr int KNIGHT[] = { 0, 2, 5, 9, 13, 17, 21, 24, 26, 29, 33, 39, 45, 51, 57, 61, 64, 68, 74, 82, 90, 98, 106, 112, 116, 120, 126, 134, 142, 150, 158, 164, 168, 172, 178, 186, 194, 202, 210, 216, 220, 224, 230, 238, 246, 254, 262, 268, 272, 275, 279, 285, 291, 297, 303, 307, 310, 312, 315, 319, 323, 327, 331, 334, 336 };
        constexpr int BISHOP[] = { 0, 7, 14, 21, 28, 35, 42, 49, 56, 63, 72, 81, 90, 99, 108, 117, 124, 131, 140, 151, 162, 173, 184, 193, 200, 207, 216, 227, 240, 253, 264, 273, 280, 287, 296, 307, 320, 333, 344, 353, 360, 367, 376, 387, 398, 409, 420, 429, 436, 443, 452, 461, 470, 479, 488, 497, 504, 511, 518, 525, 532, 539, 546, 553, 560 };
        constexpr int ROOK[] = { 0, 14, 28, 42, 56, 70, 84, 98, 112, 126, 140, 154, 168, 182, 196, 210, 224, 238, 252, 266, 280, 294, 308, 322, 336, 350, 364, 378, 392, 406, 420, 434, 448, 462, 476, 490, 504, 518, 532, 546, 560, 574, 588, 602, 616, 630, 644, 658, 672, 686, 700, 714, 728, 742, 756, 770, 784, 798, 812, 826, 840, 854, 868, 882, 896 };
        constexpr int QUEEN[] = { 0, 21, 42, 63, 84, 105, 126, 147, 168, 189, 212, 235, 258, 281, 304, 327, 348, 369, 392, 417, 442, 467, 492, 515, 536, 557, 580, 605, 632, 659, 684, 707, 728, 749, 772, 797, 824, 851, 876, 899, 920, 941, 964, 989, 1014, 1039, 1064, 1087, 1108, 1129, 1152, 1175, 1198, 1221, 1244, 1267, 1288, 1309, 1330, 1351, 1372, 1393, 1414, 1435, 1456 };
        constexpr int KING[] = { 0, 3, 8, 13, 18, 23, 28, 33, 36, 41, 49, 57, 65, 73, 81, 89, 94, 99, 107, 115, 123, 131, 139, 147, 152, 157, 165, 173, 181, 189, 197, 205, 210, 215, 223, 231, 239, 247, 255, 263, 268, 273, 281, 289, 297, 305, 313, 321, 326, 331, 339, 347, 355, 363, 371, 379, 384, 387, 392, 397, 402, 407, 412, 417, 420 };

    }

    namespace PieceOffsets {

        constexpr int PAWN = 0;
        constexpr int KNIGHT = PAWN + 6 * SquareThreatCounts::PAWN;            // pawn can have threats to 6 different pieces (own pawn, own knight, own rook, opp pawn, opp knight, opp rook)
        constexpr int BISHOP = KNIGHT + 12 * SquareThreatCounts::KNIGHT[64];   // all 12 pieces possible
        constexpr int ROOK = BISHOP + 10 * SquareThreatCounts::BISHOP[64];     // queens excluded
        constexpr int QUEEN = ROOK + 10 * SquareThreatCounts::ROOK[64];        // queens excluded
        constexpr int KING = QUEEN + 12 * SquareThreatCounts::QUEEN[64];       // all 12 pieces possible
        constexpr int END = KING + 8 * SquareThreatCounts::KING[64];           // queen and king skipped

    }

    constexpr int MAX = std::numeric_limits<int>::max();

    int localThreatIndex(Square to, Bitboard attackedByFrom);
    Bitboard mirrorBitboard(Bitboard bitboard);
    
    void addThreatFeatures(Bitboard* byColor, Bitboard* byPiece, Piece* pieces, Threats* threats, Color side, FeatureList& features);
    void addPieceFeatures(Bitboard* byColor, Bitboard* byPiece, Color side, FeatureList& features, const uint8_t* KING_BUCKET_LAYOUT);
    
    int getPieceFeature(Piece piece, Square relativeSquare, Color relativeColor, uint8_t kingBucket);
    int getThreatFeature(Piece piece, Square from, Square to, Piece target, Color relativeSide, bool enemy, int sideOffset);

    // Piece piece, Square from, Square to, Piece target, Color relativeSide, bool enemy, int sideOffset
    extern int INDEX_LOOKUP[6][64][64][6][2][2][2];

    void initialise();

    inline int lookupThreatFeature(Piece attackingPiece, Square attackingSquare, Square attackedSquare, Piece attackedPiece, Color relativeSide, bool enemy, bool hasSideOffset) {
        return INDEX_LOOKUP[attackingPiece][attackingSquare][attackedSquare][attackedPiece][relativeSide][enemy][hasSideOffset];
    }

    int getPawnThreatFeature(Square from, Square to, Piece target, Color relativeSide, bool enemy, int sideOffset);
    int getKnightThreatFeature(Square from, Square to, Piece target, Color relativeSide, int sideOffset);
    int getBishopThreatFeature(Square from, Square to, Piece target, Color relativeSide, int sideOffset);
    int getRookThreatFeature(Square from, Square to, Piece target, Color relativeSide, int sideOffset);
    int getQueenThreatFeature(Square from, Square to, Piece target, Color relativeSide, int sideOffset);
    int getKingThreatFeature(Square from, Square to, Piece target, Color relativeSide, int sideOffset);

}