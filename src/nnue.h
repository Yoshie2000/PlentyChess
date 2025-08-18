#pragma once

#include <algorithm>
#include <cstdint>

#if defined(ARCH_X86)
#include <immintrin.h>
#include <xmmintrin.h>
#else
#include <arm_neon.h>
#endif

#include "types.h"

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

#elif defined(ARCH_X86)

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

#else

using VecI8 = int8x16_t;
using VecIu8 = uint8x16_t;
using VecI16 = int16x8_t;
using VecIu16 = uint16x8_t;
using VecI32 = int32x4_t;
using VecF = float32x4_t;

// Integer operations
inline VecI16 addEpi16(VecI16 x, VecI16 y) {
  return vaddq_s16(x, y);
}

inline VecI16 subEpi16(VecI16 x, VecI16 y) {
  return vsubq_s16(x, y);
}

inline VecI16 minEpi16(VecI16 x, VecI16 y) {
  return vminq_s16(x, y);
}

inline VecI16 maxEpi16(VecI16 x, VecI16 y) {
  return vmaxq_s16(x, y);
}

inline VecI16 set1Epi16(int i) {
  return vdupq_n_s16(i);
}

inline VecI32 set1Epi32(int i) {
  return vdupq_n_s32(i);
}

inline VecI16 slliEpi16(VecI16 x, int shift) {
  return vshlq_s16(x, vdupq_n_s16(shift));
}

inline VecI16 mulhiEpi16(VecI16 x, VecI16 y) {
  VecI32 lo = vmull_s16(vget_low_s16(x), vget_low_s16(y));
  VecI32 hi = vmull_s16(vget_high_s16(x), vget_high_s16(y));
  return vcombine_s16(vshrn_n_s32(lo, 16), vshrn_n_s32(hi, 16));
}

inline int16x8_t maddubs(uint8x16_t a, int8x16_t b) {
  int16x8_t tl = vmulq_s16(vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(a))), vmovl_s8(vget_low_s8(b)));
  int16x8_t th = vmulq_s16(vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(a))), vmovl_s8(vget_high_s8(b)));
  return vqaddq_s16(vuzp1q_s16(tl, th), vuzp2q_s16(tl, th));
}

inline int32x4_t madd(int16x8_t a, int16x8_t b) {
  int32x4_t low = vmull_s16(vget_low_s16(a), vget_low_s16(b));
  int32x4_t high = vmull_high_s16(a, b);
  return vpaddq_s32(low, high);
}

// SIMD dot-product
inline VecI32 dpbusdEpi32(VecI32 sum, VecIu8 u, VecI8 i) {
  int32x4_t sum32 = madd(maddubs(u, i), vdupq_n_s16(1));
  return vaddq_s32(sum32, sum);
}

inline VecI32 dpbusdEpi32x2(VecI32 sum, VecIu8 u, VecI8 i, VecIu8 u2, VecI8 i2) {
  int16x8_t mul1 = maddubs(u, i);
  int16x8_t mul2 = maddubs(u2, i2);
  VecI32 sum32 = madd(vaddq_s16(mul1, mul2), vdupq_n_s16(1));
  return vaddq_s32(sum32, sum);
}

// Pack and store
inline VecIu8 packusEpi16(VecI16 x, VecI16 y) {
  return vcombine_u8(vqmovun_s16(x), vqmovun_s16(y));
}

inline void vecStoreI(VecI16* dest, VecI16 x) {
  vst1q_s16(reinterpret_cast<int16_t*>(dest), x);
}

// Floating-point operations
inline VecF cvtepi32Ps(VecI32 x) {
  return vcvtq_f32_s32(x);
}

inline VecF addPs(VecF x, VecF y) {
  return vaddq_f32(x, y);
}

inline VecF mulPs(VecF x, VecF y) {
  return vmulq_f32(x, y);
}

inline VecF set1Ps(float x) {
  return vdupq_n_f32(x);
}

inline VecF minPs(VecF x, VecF y) {
  return vminq_f32(x, y);
}

inline VecF maxPs(VecF x, VecF y) {
  return vmaxq_f32(x, y);
}

inline VecF fmaddPs(VecF a, VecF b, VecF c) {
  return vfmaq_f32(c, a, b);
}

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
  uint32x4_t mask = vcgtq_s32(chunk, vdupq_n_s32(0));
  uint16x4_t narrowed_mask = vmovn_u32(mask);
  uint64_t packed_mask = vget_lane_u64(vreinterpret_u64_u16(narrowed_mask), 0);
  return ((packed_mask & (1ULL << 0)) >> 0) |
    ((packed_mask & (1ULL << 16)) >> 15) |
    ((packed_mask & (1ULL << 32)) >> 30) |
    ((packed_mask & (1ULL << 48)) >> 45);
}

#endif

constexpr int INPUT_SIZE = 768;
constexpr int L1_SIZE = 1792;
constexpr int L2_SIZE = 16;
constexpr int L3_SIZE = 32;

constexpr uint8_t KING_BUCKET_LAYOUT[] = {
   0,  1,  2,  3,  3,  2,  1,  0,
   4,  5,  6,  7,  7,  6,  5,  4,
   8,  8,  9,  9,  9,  9,  8,  8,
  10, 10, 10, 10, 10, 10, 10, 10,
  11, 11, 11, 11, 11, 11, 11, 11,
  11, 11, 11, 11, 11, 11, 11, 11,
  11, 11, 11, 11, 11, 11, 11, 11,
  11, 11, 11, 11, 11, 11, 11, 11,
};
constexpr int KING_BUCKETS = 12;
constexpr bool KING_BUCKETS_FACTORIZED = true;
constexpr int OUTPUT_BUCKETS = 8;

constexpr int NETWORK_SCALE = 287;
constexpr int INPUT_QUANT = 362;
constexpr int INPUT_SHIFT = 10;
constexpr int L1_QUANT = 64;

constexpr float L1_NORMALISATION = static_cast<float>(1 << INPUT_SHIFT) / static_cast<float>(INPUT_QUANT * INPUT_QUANT * L1_QUANT);

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

struct NetworkData {
  alignas(ALIGNMENT) int16_t inputWeights[KING_BUCKETS][INPUT_SIZE * L1_SIZE];
  alignas(ALIGNMENT) int16_t inputBiases[L1_SIZE];
  alignas(ALIGNMENT) int8_t  l1Weights[OUTPUT_BUCKETS][L1_SIZE * L2_SIZE];
  alignas(ALIGNMENT) float   l1Biases[OUTPUT_BUCKETS][L2_SIZE];
  alignas(ALIGNMENT) float   l2Weights[OUTPUT_BUCKETS][2 * L2_SIZE * L3_SIZE];
  alignas(ALIGNMENT) float   l2Biases[OUTPUT_BUCKETS][L3_SIZE];
  alignas(ALIGNMENT) float   l3Weights[OUTPUT_BUCKETS][L3_SIZE + 2 * L2_SIZE];
  alignas(ALIGNMENT) float   l3Biases[OUTPUT_BUCKETS];
};

extern NetworkData* networkData;

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
  void addPieceToAccumulator(int16_t(*inputData)[L1_SIZE], int16_t(*outputData)[L1_SIZE], KingBucketInfo* kingBucket, Square square, Piece piece, Color pieceColor);
  template<Color side>
  void removePieceFromAccumulator(int16_t(*inputData)[L1_SIZE], int16_t(*outputData)[L1_SIZE], KingBucketInfo* kingBucket, Square square, Piece piece, Color pieceColor);
  template<Color side>
  void movePieceInAccumulator(int16_t(*inputData)[L1_SIZE], int16_t(*outputData)[L1_SIZE], KingBucketInfo* kingBucket, Square origin, Square target, Piece piece, Color pieceColor);

};

#if defined(PROCESS_NET)

#include <cstring>
#include <fstream>
#include <vector>
#include <unordered_set>
#include <mutex>
#include <thread>
#include <mutex>

class NNZ {
public:
  int64_t activationCounts[L1_SIZE];
  std::vector<uint64_t> activationsByNeuronBitsets[L1_SIZE];
  int64_t totalActivations;

  void addActivations(uint8_t* neurons) {
    if (totalActivations % 64 == 0) {
      for (int i = 0; i < L1_SIZE; i++) {
        activationsByNeuronBitsets[i].push_back(0);
      }
    }

    for (int i = 0; i < L1_SIZE; i++) {
      if (neurons[i]) {
        activationCounts[i]++;
        activationsByNeuronBitsets[i][totalActivations / 64] |= (1ULL << (totalActivations % 64));
      }
    }
    totalActivations++;
  }

  float minSupport = 0.7;

  int16_t oldInputWeights[KING_BUCKETS][INPUT_SIZE * L1_SIZE];
  int16_t oldInputBiases[L1_SIZE];
  int8_t  oldL1Weights[OUTPUT_BUCKETS][L1_SIZE * L2_SIZE];
  NetworkData nnzOutNet;

  int order[L1_SIZE];

  bool differByOne(const std::unordered_set<int16_t>& a, const std::unordered_set<int16_t>& b) {
    if (a.size() != b.size()) return false;

    int diff = 0;
    for (int x : a) {
      if (!b.count(x)) {
        diff++;
        if (diff > 1) return false;
      }
    }
    return diff == 1;
  }

  int getOccurrences(const std::unordered_set<int16_t>& candidateSet) {
    if (candidateSet.empty()) return 0;

    auto it = candidateSet.begin();
    std::vector<uint64_t> intersection = activationsByNeuronBitsets[*it];

    for (++it; it != candidateSet.end(); ++it) {
      const auto& acts = activationsByNeuronBitsets[*it];
      size_t minSize = std::min(intersection.size(), acts.size());

      for (size_t i = 0; i < minSize; i++) {
        intersection[i] &= acts[i];
      }
      intersection.resize(minSize);
    }

    int count = 0;
    for (uint64_t val : intersection) {
      count += __builtin_popcountll(val);
    }
    return count;
  }

  std::vector<std::pair<float, std::unordered_set<int16_t>>> apriori() {
    std::vector<std::vector<std::pair<float, std::unordered_set<int16_t>>>> frequentNeuronSets;

    // Find frequently activated neurons
    frequentNeuronSets.push_back({});
    for (int16_t i = 0; i < L1_SIZE; i++) {
      float support = float(activationCounts[i]) / float(totalActivations);
      if (support >= minSupport) {
        std::unordered_set<int16_t> set{ i };
        frequentNeuronSets[0].push_back(std::make_pair(support, set));
      }
    }
    std::cout << frequentNeuronSets[0].size() << " frequent neuron sets of size " << 1 << std::endl;

    // Find frequent neuron-sets up to size 4
    for (int k : {2, 3, 4}) {
      frequentNeuronSets.push_back({}); // For the current k

      std::mutex mtx;
      size_t nThreads = std::thread::hardware_concurrency();
      std::vector<std::thread> threads;

      auto worker = [&](size_t start, size_t end) {
        std::vector<std::pair<float, std::unordered_set<int16_t>>> localFrequentSets;

        for (size_t i = start; i < end; i++) {
          for (size_t j = 0; j < frequentNeuronSets[k - 2].size(); j++) {
            if (differByOne(frequentNeuronSets[k - 2][i].second, frequentNeuronSets[k - 2][j].second)) {
              std::unordered_set<int16_t> combined;
              combined.insert(frequentNeuronSets[k - 2][i].second.begin(), frequentNeuronSets[k - 2][i].second.end());
              combined.insert(frequentNeuronSets[k - 2][j].second.begin(), frequentNeuronSets[k - 2][j].second.end());

              // Immediately check if the candidate is frequent
              float support = float(getOccurrences(combined)) / float(totalActivations);

              if (support >= minSupport) {
                localFrequentSets.push_back(std::make_pair(support, combined));
              }
            }
          }
        }

        std::lock_guard<std::mutex> lock(mtx);
        frequentNeuronSets[k - 1].insert(frequentNeuronSets[k - 1].end(),
          localFrequentSets.begin(), localFrequentSets.end());
        };

      size_t chunkSize = (frequentNeuronSets[k - 2].size() + nThreads - 1) / nThreads;
      for (size_t t = 0; t < nThreads; ++t) {
        size_t start = t * chunkSize;
        size_t end = std::min(start + chunkSize, frequentNeuronSets[k - 2].size());
        threads.emplace_back(worker, start, end);
      }

      for (auto& th : threads) th.join();
      std::cout << frequentNeuronSets[k - 1].size() << " frequent neuron sets of size " << k << std::endl;
    }

    std::sort(frequentNeuronSets[3].begin(), frequentNeuronSets[3].end(), [](auto& a, auto& b) {
      return a.first > b.first;
      });

    return frequentNeuronSets[3];
  }

  void permuteNetwork() {
    std::ifstream infile("./quantised.bin", std::ios::binary);
    if (!infile) {
      std::cerr << "Error opening file for reading" << std::endl;
      return;
    }
    infile.read(reinterpret_cast<char*>(&nnzOutNet), sizeof(nnzOutNet));
    infile.close();

    memcpy(oldInputWeights, nnzOutNet.inputWeights, sizeof(nnzOutNet.inputWeights));
    memcpy(oldInputBiases, nnzOutNet.inputBiases, sizeof(nnzOutNet.inputBiases));
    memcpy(oldL1Weights, nnzOutNet.l1Weights, sizeof(nnzOutNet.l1Weights));

    int order_idx = 0;
    while (order_idx < L1_SIZE) {
      auto mostFrequentNeuronGroups = apriori();

      // Use most frequent groups to populate the order array
      std::unordered_set<int16_t> used_indices;
      for (const auto& pair : mostFrequentNeuronGroups) {
        // Check if any index has been used already
        bool has_overlap = false;
        for (const auto& neuron_index : pair.second) {
          if (used_indices.count(neuron_index) > 0) {
            has_overlap = true;
            break;
          }
        }

        // If not, add to the order array
        if (!has_overlap) {
          for (const auto& neuron_index : pair.second) {
            std::cout << neuron_index << " ";
            order[order_idx++] = neuron_index;
            used_indices.insert(neuron_index);
          }
          std::cout << pair.first << std::endl;
        }
      }

      // Remove data of used neurons
      for (int16_t index : used_indices) {
        for (uint64_t& bitset : activationsByNeuronBitsets[index])
          bitset = 0;
        activationCounts[index] = 0;
      }

      minSupport *= 0.9;
    }

    for (int l1 = 0; l1 < L1_SIZE / 2; l1++) {

      // Input weights
      for (int kb = 0; kb < KING_BUCKETS; kb++) {
        for (int ip = 0; ip < INPUT_SIZE; ip++) {
          nnzOutNet.inputWeights[kb][ip * L1_SIZE + l1] = oldInputWeights[kb][ip * L1_SIZE + order[l1]];
          nnzOutNet.inputWeights[kb][ip * L1_SIZE + l1 + L1_SIZE / 2] = oldInputWeights[kb][ip * L1_SIZE + order[l1] + L1_SIZE / 2];
        }
      }

      // Input biases
      nnzOutNet.inputBiases[l1] = oldInputBiases[order[l1]];
      nnzOutNet.inputBiases[l1 + L1_SIZE / 2] = oldInputBiases[order[l1] + L1_SIZE / 2];

      // L1 weights
      for (int ob = 0; ob < OUTPUT_BUCKETS; ob++) {
        for (int l2 = 0; l2 < L2_SIZE; l2++) {
          reinterpret_cast<int8_t*>(nnzOutNet.l1Weights)[l1 * OUTPUT_BUCKETS * L2_SIZE + ob * L2_SIZE + l2] = reinterpret_cast<int8_t*>(oldL1Weights)[order[l1] * OUTPUT_BUCKETS * L2_SIZE + ob * L2_SIZE + l2];
          reinterpret_cast<int8_t*>(nnzOutNet.l1Weights)[(l1 + L1_SIZE / 2) * OUTPUT_BUCKETS * L2_SIZE + ob * L2_SIZE + l2] = reinterpret_cast<int8_t*>(oldL1Weights)[(order[l1] + L1_SIZE / 2) * OUTPUT_BUCKETS * L2_SIZE + ob * L2_SIZE + l2];
        }
      }
    }

    // Write the network
    std::ofstream outfile("./quantised.bin", std::ios::binary);
    if (!outfile) {
      std::cerr << "Error opening file for writing" << std::endl;
      return;
    }
    outfile.write(reinterpret_cast<char*>(&nnzOutNet), sizeof(nnzOutNet));
    outfile.close();
    std::cout << "NNZ permuted net written to ./quantised.bin" << std::endl;
  }
};

extern NNZ nnz;
#endif