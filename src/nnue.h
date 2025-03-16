#pragma once

#include <algorithm>
#include <cstdint>

#include <immintrin.h>
#include <xmmintrin.h>

#include "types.h"
#include "threat-inputs.h"

#if defined(__AVX512F__) && defined(__AVX512BW__)

using VecI8 = __m512i;
using VecIu8 = __m512i;
using VecI16 = __m512i;
using VecIu16 = __m512i;
using VecI32 = __m512i;
using VecF = __m512;

inline VecI16 addEpi16(VecI16 x, VecI16 y) {
  return _mm512_add_epi16(x, y);
}

inline VecI16 subEpi16(VecI16 x, VecI16 y) {
  return _mm512_sub_epi16(x, y);
}

inline VecI16 minEpi16(VecI16 x, VecI16 y) {
  return _mm512_min_epi16(x, y);
}

inline VecI16 maxEpi16(VecI16 x, VecI16 y) {
  return _mm512_max_epi16(x, y);
}

inline VecI16 mulloEpi16(VecI16 x, VecI16 y) {
  return _mm512_mullo_epi16(x, y);
}

inline VecI16 maddEpi16(VecI16 x, VecI16 y) {
  return _mm512_madd_epi16(x, y);
}

inline VecI16 addEpi32(VecI16 x, VecI16 y) {
  return _mm512_add_epi32(x, y);
}

inline int vecHaddEpi32(VecI16 vec) {
  return _mm512_reduce_add_epi32(vec);
}

inline VecI16 set1Epi16(int i) {
  return _mm512_set1_epi16(i);
}

inline VecI16 set1Epi32(int i) {
  return _mm512_set1_epi32(i);
}

inline VecI16 slliEpi16(VecI16 x, int shift) {
  return _mm512_slli_epi16(x, shift);
}

inline VecI16 mulhiEpi16(VecI16 x, VecI16 y) {
  return _mm512_mulhi_epi16(x, y);
}

#if defined(__AVX512VNNI__)
inline VecI32 dpbusdEpi32(VecI32 sum, VecIu8 u, VecI8 i) {
  return _mm512_dpbusd_epi32(sum, u, i);
}
inline VecI32 dpbusdEpi32x2(VecI32 sum, VecIu8 u, VecI8 i, VecIu8 u2, VecI8 i2) {
  return _mm512_dpbusd_epi32(_mm512_dpbusd_epi32(sum, u, i), u2, i2);
}
#else
inline VecI32 dpbusdEpi32(VecI32 sum, VecIu8 u, VecI8 i) {
  VecI32 sum32 = _mm512_madd_epi16(_mm512_maddubs_epi16(u, i), _mm512_set1_epi16(1));
  return _mm512_add_epi32(sum32, sum);
}
inline VecI32 dpbusdEpi32x2(VecI32 sum, VecIu8 u, VecI8 i, VecIu8 u2, VecI8 i2) {
  VecI16 mul1 = _mm512_maddubs_epi16(u, i);
  VecI16 mul2 = _mm512_maddubs_epi16(u2, i2);
  VecI32 sum32 = _mm512_madd_epi16(_mm512_add_epi16(mul1, mul2), _mm512_set1_epi16(1));
  return _mm512_add_epi32(sum32, sum);
}
#endif

inline VecIu8 packusEpi16(VecI16 x, VecI16 y) {
  return _mm512_packus_epi16(x, y);
}

inline void vecStoreI(VecI16* dest, VecI16 x) {
  _mm512_store_si512(dest, x);
}

inline VecF cvtepi32Ps(VecI32 x) {
  return _mm512_cvtepi32_ps(x);
}

inline VecF addPs(VecF x, VecF y) {
  return _mm512_add_ps(x, y);
}

inline VecF mulPs(VecF x, VecF y) {
  return _mm512_mul_ps(x, y);
}

inline VecF set1Ps(float x) {
  return _mm512_set1_ps(x);
}

inline VecF minPs(VecF x, VecF y) {
  return _mm512_min_ps(x, y);
}

inline VecF maxPs(VecF x, VecF y) {
  return _mm512_max_ps(x, y);
}

inline VecF fmaddPs(VecF a, VecF b, VecF c) {
  return _mm512_fmadd_ps(a, b, c);
}

inline float reduceAddPs(VecF* v) {
  return _mm512_reduce_add_ps(v[0]);
}

inline uint32_t vecNNZ(VecI32 chunk) {
  return _mm512_cmpgt_epi32_mask(chunk, _mm512_setzero_si512());
}

#elif defined(__AVX2__)

using VecI8 = __m256i;
using VecIu8 = __m256i;
using VecI16 = __m256i;
using VecIu16 = __m256i;
using VecI32 = __m256i;
using VecF = __m256;

inline VecI16 addEpi16(VecI16 x, VecI16 y) {
  return _mm256_add_epi16(x, y);
}

inline VecI16 subEpi16(VecI16 x, VecI16 y) {
  return _mm256_sub_epi16(x, y);
}

inline VecI16 minEpi16(VecI16 x, VecI16 y) {
  return _mm256_min_epi16(x, y);
}

inline VecI16 maxEpi16(VecI16 x, VecI16 y) {
  return _mm256_max_epi16(x, y);
}

inline VecI16 mulloEpi16(VecI16 x, VecI16 y) {
  return _mm256_mullo_epi16(x, y);
}

inline VecI16 maddEpi16(VecI16 x, VecI16 y) {
  return _mm256_madd_epi16(x, y);
}

inline VecI16 addEpi32(VecI16 x, VecI16 y) {
  return _mm256_add_epi32(x, y);
}

inline int vecHaddEpi32(VecI16 vec) {
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

inline VecI16 set1Epi16(int i) {
  return _mm256_set1_epi16(i);
}

inline VecI16 set1Epi32(int i) {
  return _mm256_set1_epi32(i);
}

inline VecI16 slliEpi16(VecI16 x, int shift) {
  return _mm256_slli_epi16(x, shift);
}

inline VecI16 mulhiEpi16(VecI16 x, VecI16 y) {
  return _mm256_mulhi_epi16(x, y);
}

inline VecIu8 packusEpi16(VecI16 x, VecI16 y) {
  return _mm256_packus_epi16(x, y);
}

inline void vecStoreI(VecI16* dest, VecI16 x) {
  _mm256_store_si256(dest, x);
}

inline VecI32 dpbusdEpi32(VecI32 sum, VecIu8 u, VecI8 i) {
  VecI32 sum32 = _mm256_madd_epi16(_mm256_maddubs_epi16(u, i), _mm256_set1_epi16(1));
  return _mm256_add_epi32(sum32, sum);
}

inline VecI32 dpbusdEpi32x2(VecI32 sum, VecIu8 u, VecI8 i, VecIu8 u2, VecI8 i2) {
  VecI16 mul1 = _mm256_maddubs_epi16(u, i);
  VecI16 mul2 = _mm256_maddubs_epi16(u2, i2);
  VecI32 sum32 = _mm256_madd_epi16(_mm256_add_epi16(mul1, mul2), _mm256_set1_epi16(1));
  return _mm256_add_epi32(sum32, sum);
}

inline VecF cvtepi32Ps(VecI32 x) {
  return _mm256_cvtepi32_ps(x);
}

inline VecF addPs(VecF x, VecF y) {
  return _mm256_add_ps(x, y);
}

inline VecF mulPs(VecF x, VecF y) {
  return _mm256_mul_ps(x, y);
}

inline VecF set1Ps(float x) {
  return _mm256_set1_ps(x);
}

inline VecF minPs(VecF x, VecF y) {
  return _mm256_min_ps(x, y);
}

inline VecF maxPs(VecF x, VecF y) {
  return _mm256_max_ps(x, y);
}

inline VecF fmaddPs(VecF a, VecF b, VecF c) {
  return _mm256_fmadd_ps(a, b, c);
}

inline float reduceAddPs(VecF* v) {
  v[0] = _mm256_add_ps(v[0], v[1]);
  __m128 high = _mm256_extractf128_ps(v[0], 1);
  __m128 low = _mm256_castps256_ps128(v[0]);
  __m128 sum = _mm_add_ps(high, low);
  __m128 high64 = _mm_movehl_ps(sum, sum);
  __m128 sum64 = _mm_add_ps(sum, high64);

  return ((float*)&sum64)[0] + ((float*)&sum64)[1];
}

inline uint32_t vecNNZ(VecI32 chunk) {
  return _mm256_movemask_ps(_mm256_castsi256_ps(_mm256_cmpgt_epi32(chunk, _mm256_setzero_si256())));
}

#else

using VecI8 = __m128i;
using VecIu8 = __m128i;
using VecI16 = __m128i;
using VecIu16 = __m128i;
using VecI32 = __m128i;
using VecF = __m128;

inline VecI16 addEpi16(VecI16 x, VecI16 y) {
  return _mm_add_epi16(x, y);
}

inline VecI16 subEpi16(VecI16 x, VecI16 y) {
  return _mm_sub_epi16(x, y);
}

inline VecI16 minEpi16(VecI16 x, VecI16 y) {
  return _mm_min_epi16(x, y);
}

inline VecI16 maxEpi16(VecI16 x, VecI16 y) {
  return _mm_max_epi16(x, y);
}

inline VecI16 mulloEpi16(VecI16 x, VecI16 y) {
  return _mm_mullo_epi16(x, y);
}

inline VecI16 maddEpi16(VecI16 x, VecI16 y) {
  return _mm_madd_epi16(x, y);
}

inline VecI16 addEpi32(VecI16 x, VecI16 y) {
  return _mm_add_epi32(x, y);
}

inline int vecHaddEpi32(VecI16 vec) {
  int* asArray = (int*)&vec;
  return asArray[0] + asArray[1] + asArray[2] + asArray[3];
}

inline VecI16 set1Epi16(int i) {
  return _mm_set1_epi16(i);
}

inline VecI16 set1Epi32(int i) {
  return _mm_set1_epi32(i);
}

inline VecI16 slliEpi16(VecI16 x, int shift) {
  return _mm_slli_epi16(x, shift);
}

inline VecI16 mulhiEpi16(VecI16 x, VecI16 y) {
  return _mm_mulhi_epi16(x, y);
}

inline VecIu8 packusEpi16(VecI16 x, VecI16 y) {
  return _mm_packus_epi16(x, y);
}

inline void vecStoreI(VecI16* dest, VecI16 x) {
  _mm_store_si128(dest, x);
}

#if defined(__SSSE3__)
inline VecI32 dpbusdEpi32(VecI32 sum, VecIu8 u, VecI8 i) {
  VecI32 sum32 = _mm_madd_epi16(_mm_maddubs_epi16(u, i), _mm_set1_epi16(1));
  return _mm_add_epi32(sum32, sum);
}
inline VecI32 dpbusdEpi32x2(VecI32 sum, VecIu8 u, VecI8 i, VecIu8 u2, VecI8 i2) {
  VecI16 mul1 = _mm_maddubs_epi16(u, i);
  VecI16 mul2 = _mm_maddubs_epi16(u2, i2);
  VecI32 sum32 = _mm_madd_epi16(_mm_add_epi16(mul1, mul2), _mm_set1_epi16(1));
  return _mm_add_epi32(sum32, sum);
}
#endif

inline VecF cvtepi32Ps(VecI32 x) {
  return _mm_cvtepi32_ps(x);
}

inline VecF addPs(VecF x, VecF y) {
  return _mm_add_ps(x, y);
}

inline VecF mulPs(VecF x, VecF y) {
  return _mm_mul_ps(x, y);
}

inline VecF set1Ps(float x) {
  return _mm_set1_ps(x);
}

inline VecF minPs(VecF x, VecF y) {
  return _mm_min_ps(x, y);
}

inline VecF maxPs(VecF x, VecF y) {
  return _mm_max_ps(x, y);
}

#if defined(__FMA__)
inline VecF fmaddPs(VecF a, VecF b, VecF c) {
  return _mm_fmadd_ps(a, b, c);
}
#endif

inline float reduceAddPsR(float* sums, int length) {
  if (length == 2) return sums[0] + sums[1];
  length /= 2;
  for (int i = 0; i < length; ++i)
    sums[i] += sums[i + length];
  return reduceAddPsR(sums, length);
}

inline float reduceAddPs(VecF* sums) {
  return reduceAddPsR((float*)sums, 64 / sizeof(float));
}

inline uint32_t vecNNZ(VecI32 chunk) {
  return _mm_movemask_ps(_mm_castsi128_ps(_mm_cmpgt_epi32(chunk, _mm_setzero_si128())));
}
#endif

constexpr int INPUT_SIZE = 2 * ThreatInputs::PieceOffsets::END + 768;
constexpr int L1_SIZE = 1024;

constexpr int OUTPUT_BUCKETS = 8;

constexpr int NETWORK_SCALE = 400;
constexpr int INPUT_QUANT = 510;
constexpr int L1_QUANT = 64;

constexpr int ALIGNMENT = 64;

constexpr int I16_VEC_SIZE = sizeof(VecI16) / sizeof(int16_t);
constexpr int I32_VEC_SIZE = sizeof(VecI32) / sizeof(int32_t);
constexpr int FLOAT_VEC_SIZE = sizeof(VecF) / sizeof(float);

constexpr int INT8_PER_INT32 = sizeof(int32_t) / sizeof(int8_t);

constexpr int L1_ITERATIONS = L1_SIZE / I16_VEC_SIZE;

struct DirtyPiece {
  Square origin;
  Square target;
  Piece piece;
  Color pieceColor;
};

struct KingBucketInfo {
  bool mirrored;
};

constexpr bool needsRefresh(KingBucketInfo* bucket1, KingBucketInfo* bucket2) {
  return bucket1->mirrored != bucket2->mirrored;
}

constexpr KingBucketInfo getKingBucket(Square kingSquare) {
  return KingBucketInfo{
    fileOf(kingSquare) >= 4
  };
}

struct Accumulator {
  alignas(ALIGNMENT) int16_t colors[2][L1_SIZE];

  DirtyPiece dirtyPieces[4];
  int32_t numDirtyPieces;

  KingBucketInfo kingBucketInfo[2];
  Bitboard byColor[2][2];
  Bitboard byPiece[2][Piece::TOTAL];
};

struct FinnyEntry {
  alignas(ALIGNMENT) int16_t colors[2][L1_SIZE];

  Bitboard byColor[2][2];
  Bitboard byPiece[2][Piece::TOTAL];
};

struct NetworkData {
  alignas(ALIGNMENT) int16_t inputWeights[INPUT_SIZE * L1_SIZE];
  alignas(ALIGNMENT) int16_t inputBiases[L1_SIZE];
  alignas(ALIGNMENT) int16_t l1Weights[OUTPUT_BUCKETS][2 * L1_SIZE];
  alignas(ALIGNMENT) int16_t l1Biases[OUTPUT_BUCKETS];
};

extern NetworkData* networkData;

void initNetworkData();

struct Board;

class NNUE {
public:

  Accumulator accumulatorStack[MAX_PLY];
  int currentAccumulator;
  int lastCalculatedAccumulator[2];

  FinnyEntry finnyTable[2];

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

  /*
  template<Color side>
  void calculateAccumulators();
  template<Color side>
  void refreshAccumulator(Accumulator* acc);
  template<Color side>
  void incrementallyUpdateAccumulator(Accumulator* inputAcc, Accumulator* outputAcc, KingBucketInfo* kingBucket);

  template<Color side>
  void addPieceToAccumulator(int16_t(*inputData)[L1_SIZE], int16_t(*outputData)[L1_SIZE], KingBucketInfo* kingBucket, Square square, Piece piece, Color pieceColor);
  template<Color side>
  void removePieceFromAccumulator(int16_t(*inputData)[L1_SIZE], int16_t(*outputData)[L1_SIZE], KingBucketInfo* kingBucket, Square square, Piece piece, Color pieceColor);
  template<Color side>
  void movePieceInAccumulator(int16_t(*inputData)[L1_SIZE], int16_t(*outputData)[L1_SIZE], KingBucketInfo* kingBucket, Square origin, Square target, Piece piece, Color pieceColor);
  */

};