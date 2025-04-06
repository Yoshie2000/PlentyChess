#pragma once

#include <algorithm>
#include <cstdint>

#include <immintrin.h>
#include <xmmintrin.h>

#include "types.h"
#include "threat-inputs.h"

inline float reduceAddPsR(float* sums, int length) {
  if (length == 2) return sums[0] + sums[1];
  length /= 2;
  for (int i = 0; i < length; ++i)
    sums[i] += sums[i + length];
  return reduceAddPsR(sums, length);
}

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

inline VecI16 srliEpi16(VecI16 x, int shift) {
  return _mm512_srli_epi16(x, shift);
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
  VecI16 packed = _mm512_packus_epi16(x, y);
  packed = _mm512_permutexvar_epi64(_mm512_setr_epi64(0, 2, 4, 6, 1, 3, 5, 7), packed);
  return packed;
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

inline VecI16 srliEpi16(VecI16 x, int shift) {
  return _mm256_srli_epi16(x, shift);
}

inline VecI16 mulhiEpi16(VecI16 x, VecI16 y) {
  return _mm256_mulhi_epi16(x, y);
}

inline VecIu8 packusEpi16(VecI16 x, VecI16 y) {
  VecI16 packed = _mm256_packus_epi16(x, y);
  packed = _mm256_permute4x64_epi64(packed, _MM_SHUFFLE(3, 1, 2, 0));
  return packed;
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

inline VecI16 set1Epi16(int i) {
  return _mm_set1_epi16(i);
}

inline VecI16 set1Epi32(int i) {
  return _mm_set1_epi32(i);
}

inline VecI16 slliEpi16(VecI16 x, int shift) {
  return _mm_slli_epi16(x, shift);
}

inline VecI16 srliEpi16(VecI16 x, int shift) {
  return _mm_srli_epi16(x, shift);
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

inline float reduceAddPs(VecF* sums) {
  return reduceAddPsR((float*)sums, 64 / sizeof(float));
}

inline uint32_t vecNNZ(VecI32 chunk) {
  return _mm_movemask_ps(_mm_castsi128_ps(_mm_cmpgt_epi32(chunk, _mm_setzero_si128())));
}

#endif

constexpr int INPUT_SIZE = 2 * ThreatInputs::PieceOffsets::END + 768;
constexpr int L1_SIZE = 256;
constexpr int L2_SIZE = 16;
constexpr int L3_SIZE = 32;

constexpr int OUTPUT_BUCKETS = 8;

constexpr int NETWORK_SCALE = 400;
constexpr int INPUT_QUANT = 255;
constexpr int L1_QUANT = 64;

constexpr int INPUT_SHIFT = 9;
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

struct DirtyThreat {
  Piece piece;
  Piece attackedPiece;
  Square square;
  Square attackedSquare;
  Color pieceColor;
  Color attackedColor;
  bool add;
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
  int numDirtyPieces;
  DirtyThreat dirtyThreats[256];
  int numDirtyThreats;

  KingBucketInfo kingBucketInfo[2];
  Bitboard byColor[2][2];
  Bitboard byPiece[2][Piece::TOTAL];

  bool updated[2];
};

struct NetworkData {
  alignas(ALIGNMENT) int16_t inputWeights[INPUT_SIZE * L1_SIZE];
  alignas(ALIGNMENT) int16_t inputBiases[L1_SIZE];
  alignas(ALIGNMENT) int8_t l1Weights[OUTPUT_BUCKETS][L1_SIZE * L2_SIZE];
  alignas(ALIGNMENT) float l1Biases[OUTPUT_BUCKETS][L2_SIZE];
  alignas(ALIGNMENT) float l2Weights[OUTPUT_BUCKETS][L2_SIZE * L3_SIZE];
  alignas(ALIGNMENT) float l2Biases[OUTPUT_BUCKETS][L3_SIZE];
  alignas(ALIGNMENT) float l3Weights[OUTPUT_BUCKETS][L3_SIZE];
  alignas(ALIGNMENT) float l3Biases[OUTPUT_BUCKETS];
};

extern NetworkData* networkData;

void initNetworkData();

struct Board;

class NNUE {
public:

  Accumulator accumulatorStack[MAX_PLY + 8];
  int currentAccumulator;

  void addPiece(Square square, Piece piece, Color pieceColor);
  void removePiece(Square square, Piece piece, Color pieceColor);
  void movePiece(Square origin, Square target, Piece piece, Color pieceColor);
  void addThreat(Piece piece, Piece attackedPiece, Square square, Square attackedSquare, Color pieceColor, Color attackedColor);
  void removeThreat(Piece piece, Piece attackedPiece, Square square, Square attackedSquare, Color pieceColor, Color attackedColor);

  void incrementAccumulator();
  void decrementAccumulator();
  void finalizeMove(Board* board);

  void reset(Board* board);
  template<Color side>
  __attribute_noinline__ void resetAccumulator(Board* board, Accumulator* acc);

  Eval evaluate(Board* board);
  template<Color side>
  void calculateAccumulators(Board* board);

  template<Color side>
  __attribute_noinline__ void incrementallyUpdateAccumulator(Accumulator* inputAcc, Accumulator* outputAcc, KingBucketInfo* kingBucket);
  template<Color side>
  __attribute_noinline__ void applyAccumulatorUpdates(Accumulator* inputAcc, Accumulator* outputAcc, ThreatInputs::FeatureList& addFeatureList, ThreatInputs::FeatureList& subFeatureList);

  template<Color side>
  __attribute_noinline__ void calculatePieceFeatures(Accumulator* outputAcc, KingBucketInfo* kingBucket, ThreatInputs::FeatureList& addFeatureList, ThreatInputs::FeatureList& subFeatureList);
  template<Color side>
  __attribute_noinline__ void calculateThreatFeatures(Accumulator* outputAcc, KingBucketInfo* kingBucket, ThreatInputs::FeatureList& addFeatureList, ThreatInputs::FeatureList& subFeatureList);

  template<Color side>
  void addToAccumulator(int16_t(*inputData)[L1_SIZE], int16_t(*outputData)[L1_SIZE], int featureIndex);
  template<Color side>
  void subFromAccumulator(int16_t(*inputData)[L1_SIZE], int16_t(*outputData)[L1_SIZE], int featureIndex);
  template<Color side>
  void addSubToAccumulator(int16_t(*inputData)[L1_SIZE], int16_t(*outputData)[L1_SIZE], int addIndex, int subIndex);

};

#if defined(PROCESS_NET)

#include <cstring>
#include <fstream>

class NNZ {
public:
  int64_t activations[L1_SIZE / 2] = {};

  void addActivations(uint8_t* neurons) {
    for (int i = 0; i < L1_SIZE; i++) {
      activations[i % (L1_SIZE / 2)] += bool(neurons[i]);
    }
  }

  int16_t oldInputWeights[INPUT_SIZE * L1_SIZE];
  int16_t oldInputBiases[L1_SIZE];
  int8_t  oldL1Weights[OUTPUT_BUCKETS][L1_SIZE * L2_SIZE];
  NetworkData nnzOutNet;

  int order[L1_SIZE / 2];

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

    for (int i = 0; i < L1_SIZE / 2; i++) {
      order[i] = i;
    }
    std::stable_sort(order, order + L1_SIZE / 2, [&](const int& a, const int& b) { return activations[a] < activations[b]; });

    for (int l1 = 0; l1 < L1_SIZE / 2; l1++) {

      // Input weights
      for (int ip = 0; ip < INPUT_SIZE; ip++) {
        nnzOutNet.inputWeights[ip * L1_SIZE + l1] = oldInputWeights[ip * L1_SIZE + order[l1]];
        nnzOutNet.inputWeights[ip * L1_SIZE + l1 + L1_SIZE / 2] = oldInputWeights[ip * L1_SIZE + order[l1] + L1_SIZE / 2];
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