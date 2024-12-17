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
inline VecI dpbusdEpi32x2(VecI sum, VecI u, VecI i, VecI u2, VecI i2) {
  return _mm512_dpbusd_epi32(_mm512_dpbusd_epi32(sum, u, i), u2, i2);
}
#else
inline VecI dpbusdEpi32(VecI sum, VecI u, VecI i) {
  VecI sum32 = _mm512_madd_epi16(_mm512_maddubs_epi16(u, i), _mm512_set1_epi16(1));
  return _mm512_add_epi32(sum32, sum);
}
inline VecI dpbusdEpi32x2(VecI sum, VecI u, VecI i, VecI u2, VecI i2) {
  VecI mul1 = _mm512_maddubs_epi16(u, i);
  VecI mul2 = _mm512_maddubs_epi16(u2, i2);
  VecI sum32 = _mm512_madd_epi16(_mm512_add_epi16(mul1, mul2), _mm512_set1_epi16(1));
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

inline uint32_t vecNNZ(VecI chunk) {
  return _mm512_cmpgt_epi32_mask(chunk, _mm512_setzero_si512());
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

inline VecI dpbusdEpi32x2(VecI sum, VecI u, VecI i, VecI u2, VecI i2) {
  VecI mul1 = _mm256_maddubs_epi16(u, i);
  VecI mul2 = _mm256_maddubs_epi16(u2, i2);
  VecI sum32 = _mm256_madd_epi16(_mm256_add_epi16(mul1, mul2), _mm256_set1_epi16(1));
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

inline uint32_t vecNNZ(VecI chunk) {
  return _mm256_movemask_ps(_mm256_castsi256_ps(_mm256_cmpgt_epi32(chunk, _mm256_setzero_si256())));
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
inline VecI dpbusdEpi32x2(VecI sum, VecI u, VecI i, VecI u2, VecI i2) {
  VecI mul1 = _mm_maddubs_epi16(u, i);
  VecI mul2 = _mm_maddubs_epi16(u2, i2);
  VecI sum32 = _mm_madd_epi16(_mm_add_epi16(mul1, mul2), _mm_set1_epi16(1));
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

inline uint32_t vecNNZ(VecI chunk) {
  return _mm_movemask_ps(_mm_castsi128_ps(_mm_cmpgt_epi32(chunk, _mm_setzero_si128())));
}

#endif

constexpr int INPUT_SIZE = 768;
constexpr int L1_SIZE = 1536;
constexpr int L2_SIZE = 16;
constexpr int L3_SIZE = 32;

constexpr uint8_t KING_BUCKET_LAYOUT[] = {
  0, 1, 2, 3, 3, 2, 1, 0,
  4, 4, 5, 5, 5, 5, 4, 4,
  6, 6, 6, 6, 6, 6, 6, 6,
  7, 7, 7, 7, 7, 7, 7, 7,
  8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8
};
constexpr int KING_BUCKETS = 9;
constexpr bool KING_BUCKETS_FACTORIZED = true;
constexpr int OUTPUT_BUCKETS = 8;

constexpr int NETWORK_SCALE = 400;
constexpr int INPUT_QUANT = 362;
constexpr int INPUT_SHIFT = 10;
constexpr int L1_QUANT = 64;

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

class NNZ {
public:
  int64_t activations[L1_SIZE / 2] = {};

  void addActivations(uint8_t* neurons) {
    for (int i = 0; i < L1_SIZE; i++) {
      activations[i % (L1_SIZE / 2)] += bool(neurons[i]);
    }
  }

  int16_t oldInputWeights[KING_BUCKETS][INPUT_SIZE * L1_SIZE];
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
    std::stable_sort(order, order + L1_SIZE / 2, [&](const int &a, const int &b) { return activations[a] < activations[b]; });
  
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
