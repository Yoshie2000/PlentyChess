#pragma once

#include <algorithm>
#include <cstdint>
#include <immintrin.h>

#include "types.h"

#if defined(__AVX512F__) && defined(__AVX512BW__)

using Vec = __m512i;

inline Vec addEpi16(Vec x, Vec y) {
  return _mm512_add_epi16(x, y);
}

inline Vec addEpi32(Vec x, Vec y) {
  return _mm512_add_epi32(x, y);
}

inline Vec subEpi16(Vec x, Vec y) {
  return _mm512_sub_epi16(x, y);
}

inline Vec minEpi16(Vec x, Vec y) {
  return _mm512_min_epi16(x, y);
}

inline Vec maxEpi16(Vec x, Vec y) {
  return _mm512_max_epi16(x, y);
}

inline Vec mulloEpi16(Vec x, Vec y) {
  return _mm512_mullo_epi16(x, y);
}

inline Vec maddEpi16(Vec x, Vec y) {
  return _mm512_madd_epi16(x, y);
}

inline Vec vecSetZero() {
  return _mm512_setzero_si512();
}

inline Vec vecSet1Epi16(int16_t x) {
  return _mm512_set1_epi16(x);
}

inline int vecHaddEpi32(Vec vec) {
  return _mm512_reduce_add_epi32(vec);
}

#elif defined(__AVX2__)

using Vec = __m256i;

inline Vec addEpi16(Vec x, Vec y) {
  return _mm256_add_epi16(x, y);
}

inline Vec addEpi32(Vec x, Vec y) {
  return _mm256_add_epi32(x, y);
}

inline Vec subEpi16(Vec x, Vec y) {
  return _mm256_sub_epi16(x, y);
}

inline Vec minEpi16(Vec x, Vec y) {
  return _mm256_min_epi16(x, y);
}

inline Vec maxEpi16(Vec x, Vec y) {
  return _mm256_max_epi16(x, y);
}

inline Vec mulloEpi16(Vec x, Vec y) {
  return _mm256_mullo_epi16(x, y);
}

inline Vec maddEpi16(Vec x, Vec y) {
  return _mm256_madd_epi16(x, y);
}

inline Vec vecSetZero() {
  return _mm256_setzero_si256();
}

inline Vec vecSet1Epi16(int16_t x) {
  return _mm256_set1_epi16(x);
}

inline int vecHaddEpi32(Vec vec) {
  __m128i xmm0;
  __m128i xmm1;

  // Get the lower and upper half of the register:
  xmm0 = _mm256_castsi256_si128(vec);
  xmm1 = _mm256_extracti128_si256(vec, 1);

  // Add the lower and upper half vertically:
  xmm0 = _mm_add_epi32(xmm0, xmm1);

  // Get the upper half of the result:
  xmm1 = _mm_unpackhi_epi64(xmm0, xmm0);

  // Add the lower and upper half vertically:
  xmm0 = _mm_add_epi32(xmm0, xmm1);

  // Shuffle the result so that the lower 32-bits are directly above the second-lower 32-bits:
  xmm1 = _mm_shuffle_epi32(xmm0, _MM_SHUFFLE(2, 3, 0, 1));

  // Add the lower 32-bits to the second-lower 32-bits vertically:
  xmm0 = _mm_add_epi32(xmm0, xmm1);

  // Cast the result to the 32-bit integer type and return it:
  return _mm_cvtsi128_si32(xmm0);
}

#else

using Vec = __m128i;

inline Vec addEpi16(Vec x, Vec y) {
  return _mm_add_epi16(x, y);
}

inline Vec addEpi32(Vec x, Vec y) {
  return _mm_add_epi32(x, y);
}

inline Vec subEpi16(Vec x, Vec y) {
  return _mm_sub_epi16(x, y);
}

inline Vec minEpi16(Vec x, Vec y) {
  return _mm_min_epi16(x, y);
}

inline Vec maxEpi16(Vec x, Vec y) {
  return _mm_max_epi16(x, y);
}

inline Vec mulloEpi16(Vec x, Vec y) {
  return _mm_mullo_epi16(x, y);
}

inline Vec maddEpi16(Vec x, Vec y) {
  return _mm_madd_epi16(x, y);
}

inline Vec vecSetZero() {
  return _mm_setzero_si128();
}

inline Vec vecSet1Epi16(int16_t x) {
  return _mm_set1_epi16(x);
}

inline int vecHaddEpi32(Vec vec) {
  int* asArray = (int*)&vec;
  return asArray[0] + asArray[1] + asArray[2] + asArray[3];
}

#endif

constexpr int INPUT_WIDTH = 768;
constexpr int HIDDEN_WIDTH = 1536;

constexpr uint8_t KING_BUCKET_LAYOUT[] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0
};
constexpr int KING_BUCKETS = 1;
constexpr int OUTPUT_BUCKETS = 8;

constexpr int NETWORK_SCALE = 400;
constexpr int NETWORK_QA = 255;
constexpr int NETWORK_QB = 64;
constexpr int NETWORK_QAB = NETWORK_QA * NETWORK_QB;

constexpr int ALIGNMENT = std::max<int>(8, sizeof(Vec));
constexpr int WEIGHTS_PER_VEC = sizeof(Vec) / sizeof(int16_t);

constexpr int HIDDEN_ITERATIONS = HIDDEN_WIDTH / WEIGHTS_PER_VEC;

struct DirtyPiece {
  Square origin;
  Square target;
  Piece piece;
  Color pieceColor;
};

struct KingBucketInfo {
  uint8_t index;
  bool mirrored;
};

constexpr bool needsRefresh(KingBucketInfo* bucket1, KingBucketInfo* bucket2) {
  return false;//bucket1->mirrored != bucket2->mirrored || bucket1->index != bucket2->index;
}

constexpr KingBucketInfo getKingBucket(Color color, Square kingSquare) {
  return KingBucketInfo {
    static_cast<uint8_t>(KING_BUCKET_LAYOUT[kingSquare ^ (56 * color)]),
    false//fileOf(kingSquare) >= 4
  };
}

struct Accumulator {
  alignas(ALIGNMENT) int16_t colors[2][HIDDEN_WIDTH];

  DirtyPiece dirtyPieces[4];
  int32_t numDirtyPieces;

  KingBucketInfo kingBucketInfo[2];
  Bitboard byColor[2][2];
  Bitboard byPiece[2][Piece::TOTAL];
};

struct FinnyEntry {
  alignas(ALIGNMENT) int16_t colors[2][HIDDEN_WIDTH];

  Bitboard byColor[2][2];
  Bitboard byPiece[2][Piece::TOTAL];
};

struct NetworkData {
  alignas(ALIGNMENT) int16_t featureWeights[KING_BUCKETS][INPUT_WIDTH * HIDDEN_WIDTH];
  alignas(ALIGNMENT) int16_t featureBiases[HIDDEN_WIDTH];
  alignas(ALIGNMENT) int16_t outputWeights[OUTPUT_BUCKETS][2 * HIDDEN_WIDTH];
  alignas(ALIGNMENT) int16_t outputBiases[OUTPUT_BUCKETS];
};

extern NetworkData networkData;

void initNetworkData();

struct Board;

class NNUE {
public:

  Accumulator accumulatorStack[MAX_PLY];
  int currentAccumulator;
  int lastCalculatedAccumulator[2];

  FinnyEntry finnyTable[2][KING_BUCKETS];

  void addPiece(Square square, Piece piece, Color pieceColor);
  void removePiece(Square square, Piece piece, Color pieceColor);
  void movePiece(Square origin, Square target, Piece piece, Color pieceColor);

  void incrementAccumulator();
  void decrementAccumulator();
  void finalizeMove(Board* board);

  void reset(Board* board);
  template<Color side>
  void resetAccumulator(Board* board, Accumulator* acc);

  Eval evaluate(Board* board);

  template<Color side>
  void calculateAccumulators();
  template<Color side>
  void refreshAccumulator(Accumulator* acc);
  template<Color side>
  void incrementallyUpdateAccumulator(Accumulator* inputAcc, Accumulator* outputAcc, KingBucketInfo* kingBucket);

  template<Color side>
  void addPieceToAccumulator(int16_t(* inputData)[HIDDEN_WIDTH], int16_t(* outputData)[HIDDEN_WIDTH], KingBucketInfo* kingBucket, Square square, Piece piece, Color pieceColor);
  template<Color side>
  void removePieceFromAccumulator(int16_t(* inputData)[HIDDEN_WIDTH], int16_t(* outputData)[HIDDEN_WIDTH], KingBucketInfo* kingBucket, Square square, Piece piece, Color pieceColor);
  template<Color side>
  void movePieceInAccumulator(int16_t(* inputData)[HIDDEN_WIDTH], int16_t(* outputData)[HIDDEN_WIDTH], KingBucketInfo* kingBucket, Square origin, Square target, Piece piece, Color pieceColor);

};
