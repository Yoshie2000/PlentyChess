#pragma once

#include <algorithm>
#include <cstdint>
#include <immintrin.h>
#include <xmmintrin.h>

#include "types.h"

#if defined(__AVX512F__) && defined(__AVX512BW__)

using VecI = __m512i;
using VecF = __m512;

inline VecI addEpi16(VecI x, VecI y) {
  return _mm512_add_epi16(x, y);
}

inline VecI subEpi16(VecI x, VecI y) {
  return _mm512_sub_epi16(x, y);
}

inline VecI minEpi16(VecI x, VecI y) {
  return _mm512_min_epi16(x, y);
}

inline VecI maxEpi16(VecI x, VecI y) {
  return _mm512_max_epi16(x, y);
}

inline VecI set1Epi16(int i) {
  return _mm512_set1_epi16(i);
}

inline VecI set1Epi32(int i) {
  return _mm512_set1_epi32(i);
}

inline VecI slliEpi16(VecI x, int shift) {
  return _mm512_slli_epi16(x, shift);
}

inline VecI mulhiEpi16(VecI x, VecI y) {
  return _mm512_mulhi_epi16(x, y);
}

#if defined(__AVX512VNNI__)
inline VecI dpbusdEpi32(VecI sum, VecI u, VecI i) {
  return _mm512_dpbusd_epi32(sum, u, i);
}
#else
inline VecI dpbusdEpi32(VecI sum, VecI u, VecI i) {
  VecI sum32 = _mm512_madd_epi16(_mm512_maddubs_epi16(u, i), _mm512_set1_epi16(1));
  return _mm512_add_epi32(sum32, sum);
}
#endif

inline VecI packusEpi16(VecI x, VecI y) {
  VecI packed = _mm512_packus_epi16(x, y);
  packed = _mm512_permutexvar_epi64(_mm512_setr_epi64(0, 2, 4, 6, 1, 3, 5, 7), packed);
  return packed;
}

inline void vecStoreI(VecI* dest, VecI x) {
  _mm512_store_si512(dest, x);
}

inline VecF cvtepi32Ps(VecI x) {
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

#elif defined(__AVX2__)

using VecI = __m256i;
using VecF = __m256;

inline VecI addEpi16(VecI x, VecI y) {
  return _mm256_add_epi16(x, y);
}

inline VecI subEpi16(VecI x, VecI y) {
  return _mm256_sub_epi16(x, y);
}

inline VecI minEpi16(VecI x, VecI y) {
  return _mm256_min_epi16(x, y);
}

inline VecI maxEpi16(VecI x, VecI y) {
  return _mm256_max_epi16(x, y);
}

inline VecI set1Epi16(int i) {
  return _mm256_set1_epi16(i);
}

inline VecI set1Epi32(int i) {
  return _mm256_set1_epi32(i);
}

inline VecI slliEpi16(VecI x, int shift) {
  return _mm256_slli_epi16(x, shift);
}

inline VecI mulhiEpi16(VecI x, VecI y) {
  return _mm256_mulhi_epi16(x, y);
}

inline VecI packusEpi16(VecI x, VecI y) {
  VecI packed = _mm256_packus_epi16(x, y);
  packed = _mm256_permute4x64_epi64(packed, _MM_SHUFFLE(3, 1, 2, 0));
  return packed;
}

inline void vecStoreI(VecI* dest, VecI x) {
  _mm256_store_si256(dest, x);
}

inline VecI dpbusdEpi32(VecI sum, VecI u, VecI i) {
  VecI sum32 = _mm256_madd_epi16(_mm256_maddubs_epi16(u, i), _mm256_set1_epi16(1));
  return _mm256_add_epi32(sum32, sum);
}

inline VecF cvtepi32Ps(VecI x) {
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

#else

using VecI = __m128i;
using VecF = __m128;

inline VecI addEpi16(VecI x, VecI y) {
  return _mm_add_epi16(x, y);
}

inline VecI subEpi16(VecI x, VecI y) {
  return _mm_sub_epi16(x, y);
}

inline VecI minEpi16(VecI x, VecI y) {
  return _mm_min_epi16(x, y);
}

inline VecI maxEpi16(VecI x, VecI y) {
  return _mm_max_epi16(x, y);
}

inline VecI set1Epi16(int i) {
  return _mm_set1_epi16(i);
}

inline VecI set1Epi32(int i) {
  return _mm_set1_epi32(i);
}

inline VecI slliEpi16(VecI x, int shift) {
  return _mm_slli_epi16(x, shift);
}

inline VecI mulhiEpi16(VecI x, VecI y) {
  return _mm_mulhi_epi16(x, y);
}

inline VecI packusEpi16(VecI x, VecI y) {
  return _mm_packus_epi16(x, y);
}

inline void vecStoreI(VecI* dest, VecI x) {
  _mm_store_si128(dest, x);
}

#if defined(__SSSE3__)
inline VecI dpbusdEpi32(VecI sum, VecI u, VecI i) {
  VecI sum32 = _mm_madd_epi16(_mm_maddubs_epi16(u, i), _mm_set1_epi16(1));
  return _mm_add_epi32(sum32, sum);
}
#endif

inline VecF cvtepi32Ps(VecI x) {
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
  return reduceAddPsR((float*) sums, 64 / sizeof(float));
}

#endif

constexpr int INPUT_SIZE = 768;
constexpr int L1_SIZE = 1536;
constexpr int L2_SIZE = 16;
constexpr int L3_SIZE = 32;

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
constexpr int INPUT_QUANT = 362;
constexpr int INPUT_SHIFT = 10;
constexpr int L1_QUANT = 32;

constexpr float L1_NORMALISATION = static_cast<float>(1 << INPUT_SHIFT) / static_cast<float>(INPUT_QUANT * INPUT_QUANT * L1_QUANT);

constexpr int ALIGNMENT = 64;

constexpr int I16_VEC_SIZE = sizeof(VecI) / sizeof(int16_t);
constexpr int I32_VEC_SIZE = sizeof(VecI) / sizeof(int32_t);
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
  uint8_t index;
  bool mirrored;
};

constexpr bool needsRefresh(KingBucketInfo* bucket1, KingBucketInfo* bucket2) {
  return bucket1->mirrored != bucket2->mirrored || bucket1->index != bucket2->index;
}

constexpr KingBucketInfo getKingBucket(Color color, Square kingSquare) {
  return KingBucketInfo{
    static_cast<uint8_t>(KING_BUCKET_LAYOUT[kingSquare ^ (56 * color)]),
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

struct UnalignedNetworkData {
  int16_t inputWeights[KING_BUCKETS][INPUT_SIZE * L1_SIZE];
  int16_t inputBiases[L1_SIZE];
  int8_t  l1Weights[OUTPUT_BUCKETS][L1_SIZE * L2_SIZE];
  float   l1Biases[OUTPUT_BUCKETS][L2_SIZE];
  float   l2Weights[OUTPUT_BUCKETS][L2_SIZE * L3_SIZE];
  float   l2Biases[OUTPUT_BUCKETS][L3_SIZE];
  float   l3Weights[OUTPUT_BUCKETS][L3_SIZE];
  float   l3Biases[OUTPUT_BUCKETS];
};

struct NetworkData {
  alignas(ALIGNMENT) int16_t inputWeights[KING_BUCKETS][INPUT_SIZE * L1_SIZE];
  alignas(ALIGNMENT) int16_t inputBiases[L1_SIZE];
  alignas(ALIGNMENT) int8_t  l1Weights[OUTPUT_BUCKETS][L1_SIZE * L2_SIZE];
  alignas(ALIGNMENT) float   l1Biases[OUTPUT_BUCKETS][L2_SIZE];
  alignas(ALIGNMENT) float   l2Weights[OUTPUT_BUCKETS][L2_SIZE * L3_SIZE];
  alignas(ALIGNMENT) float   l2Biases[OUTPUT_BUCKETS][L3_SIZE];
  alignas(ALIGNMENT) float   l3Weights[OUTPUT_BUCKETS][L3_SIZE];
  alignas(ALIGNMENT) float   l3Biases[OUTPUT_BUCKETS];
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
  void pairwise(VecI* stmAcc, VecI* oppAcc, VecI* l1NeuronsVec);
  void l2Matmul(uint8_t* l1Neurons, int* l2Neurons, int bucket);
  void propagateL3(int* l2Neurons, float* l3Neurons, int bucket);
  float propagateOutput(float* l3Neurons, int bucket);

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

};
