#pragma once

#include <algorithm>
#include <cstdint>

#include "types.h"
#include "threat-inputs.h"
#include "simd.h"

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

constexpr int INPUT_SIZE_BIG = ThreatInputs::FEATURE_COUNT + 768 * KING_BUCKETS;
constexpr int INPUT_SIZE_SMALL = 768 * KING_BUCKETS;
constexpr int L1_SIZE_BIG = 640;
constexpr int L1_SIZE_SMALL = 128;
constexpr int L2_SIZE = 16;
constexpr int L3_SIZE = 32;
constexpr int OUTPUT_BUCKETS = 8;

constexpr int INPUT_QUANT = 255;
constexpr int L1_QUANT = 64;
constexpr int INPUT_SHIFT = 9;
constexpr float L1_NORMALISATION = static_cast<float>(1 << INPUT_SHIFT) / static_cast<float>(INPUT_QUANT * INPUT_QUANT * L1_QUANT);

constexpr int NETWORK_SCALE = 287;

constexpr int ALIGNMENT = 64;

constexpr int I16_VEC_SIZE = sizeof(VecI16) / sizeof(int16_t);
constexpr int I32_VEC_SIZE = sizeof(VecI32) / sizeof(int32_t);
constexpr int FLOAT_VEC_SIZE = sizeof(VecF) / sizeof(float);
constexpr int INT8_PER_INT32 = sizeof(int32_t) / sizeof(int8_t);

constexpr int L1_ITERATIONS_BIG = L1_SIZE_BIG / I16_VEC_SIZE;
constexpr int L1_ITERATIONS_SMALL = L1_SIZE_SMALL / I16_VEC_SIZE;

struct DirtyThreat {
  uint8_t piece;
  uint8_t attackedPiece;
  Square square;
  uint8_t attackedSquare;

  DirtyThreat() = default;

  DirtyThreat(Piece _piece, Color _pieceColor, Piece _attackedPiece, Color _attackedPieceColor, Square _square, Square _attackedSquare, bool _add) {
    piece = static_cast<uint8_t>(_piece | (_pieceColor << 3));
    attackedPiece = static_cast<uint8_t>(_attackedPiece | (_attackedPieceColor << 3));
    square = _square;
    attackedSquare = static_cast<uint8_t>(_attackedSquare | (_add << 7));
  }

};

static_assert(sizeof(DirtyThreat) == 4);

struct KingBucketInfo {
  uint8_t bucket;
  bool mirrored;
};

constexpr KingBucketInfo getKingBucket(Color color, Square kingSquare) {
  return KingBucketInfo{
    static_cast<uint8_t>(KING_BUCKET_LAYOUT[kingSquare ^ (56 * color)]),
    fileOf(kingSquare) >= 4,
  };
}

constexpr int ACCUMULATOR_STACK_SIZE = MAX_PLY + 8;

struct UEData {
  DirtyPiece dirtyPiece;
  DirtyThreat dirtyThreats[256];
  int numDirtyThreats;

  KingBucketInfo kingBucketInfo[2];
  Board* board;

  void updateThreat(Piece piece, Piece attackedPiece, Square square, Square attackedSquare, Color pieceColor, Color attackedColor, bool add);
  void finalizeMove(Board* board, DirtyPiece dirtyPiece);
  void reset(Board* board);
};

template<int L1_SIZE>
struct Accumulator {
  alignas(ALIGNMENT) int16_t threatState[2][L1_SIZE];
  alignas(ALIGNMENT) int16_t pieceState[2][L1_SIZE];

  UEData* ueData;
};

template<int L1_SIZE>
struct FinnyEntry {
  alignas(ALIGNMENT) int16_t pieceState[2][L1_SIZE];

  Bitboard byColor[2][2];
  Bitboard byPiece[2][Piece::TOTAL];
};

struct NetworkDataBig {
  alignas(ALIGNMENT) int16_t inputPsqWeights[768 * KING_BUCKETS * L1_SIZE_BIG];
  alignas(ALIGNMENT) int8_t  inputThreatWeights[ThreatInputs::FEATURE_COUNT * L1_SIZE_BIG];
  alignas(ALIGNMENT) int16_t inputBiases[L1_SIZE_BIG];
  alignas(ALIGNMENT) int8_t  l1Weights[OUTPUT_BUCKETS][L1_SIZE_BIG * L2_SIZE];
  alignas(ALIGNMENT) float   l1Biases[OUTPUT_BUCKETS][L2_SIZE];
  alignas(ALIGNMENT) float   l2Weights[OUTPUT_BUCKETS][2 * L2_SIZE * L3_SIZE];
  alignas(ALIGNMENT) float   l2Biases[OUTPUT_BUCKETS][L3_SIZE];
  alignas(ALIGNMENT) float   l3Weights[OUTPUT_BUCKETS][L3_SIZE + 2 * L2_SIZE];
  alignas(ALIGNMENT) float   l3Biases[OUTPUT_BUCKETS];
};

struct NetworkDataSmall {
  alignas(ALIGNMENT) int16_t inputWeights[768 * KING_BUCKETS * L1_SIZE_SMALL];
  alignas(ALIGNMENT) int16_t inputBiases[L1_SIZE_SMALL];
  alignas(ALIGNMENT) int8_t  l1Weights[OUTPUT_BUCKETS][L1_SIZE_SMALL * L2_SIZE];
  alignas(ALIGNMENT) float   l1Biases[OUTPUT_BUCKETS][L2_SIZE];
  alignas(ALIGNMENT) float   l2Weights[OUTPUT_BUCKETS][2 * L2_SIZE * L3_SIZE];
  alignas(ALIGNMENT) float   l2Biases[OUTPUT_BUCKETS][L3_SIZE];
  alignas(ALIGNMENT) float   l3Weights[OUTPUT_BUCKETS][L3_SIZE + 2 * L2_SIZE];
  alignas(ALIGNMENT) float   l3Biases[OUTPUT_BUCKETS];
};

struct NetworkData {
  NetworkDataBig bigNet;
  NetworkDataSmall smallNet;
};

extern NetworkData* networkData;

void initNetworkData();

struct Board;

template<int L1_SIZE>
class Network {
public:

  using AccumType = Accumulator<L1_SIZE>;
  using FinnyType = FinnyEntry<L1_SIZE>;

  AccumType accumulatorStack[ACCUMULATOR_STACK_SIZE];
  int currentAccumulator;
  int lastCalculatedAccumulator[2];

  FinnyType finnyTable[2][KING_BUCKETS];

  void incrementAccumulator(UEData* _ueData);
  void decrementAccumulator();

  void reset(Board* board, UEData* ueData);
  template<Color side>
  void resetAccumulator(Board* board, AccumType* acc);

  Eval evaluate(Board* board);
  template<Color side>
  void calculateAccumulators();

  template<Color side>
  void refreshPieceFeatures(AccumType* acc, KingBucketInfo* kingBucket);
  template<Color side>
  void refreshThreatFeatures(AccumType* acc);

  template<Color side>
  void incrementallyUpdatePieceFeatures(AccumType* inputAcc, AccumType* outputAcc, KingBucketInfo* kingBucket);
  template<Color side>
  void incrementallyUpdateThreatFeatures(AccumType* inputAcc, AccumType* outputAcc, KingBucketInfo* kingBucket);

  template<bool I8, Color side>
  void addToAccumulator(int16_t(*inputData)[L1_SIZE], int16_t(*outputData)[L1_SIZE], int featureIndex);
  template<bool I8, Color side>
  void subFromAccumulator(int16_t(*inputData)[L1_SIZE], int16_t(*outputData)[L1_SIZE], int featureIndex);
  template<bool I8, Color side>
  void addSubToAccumulator(int16_t(*inputData)[L1_SIZE], int16_t(*outputData)[L1_SIZE], int addIndex, int subIndex);

};

using BigNetwork = Network<L1_SIZE_BIG>;
using SmallNetwork = Network<L1_SIZE_SMALL>;

#if defined(PROCESS_NET)

#include <cstring>
#include <fstream>

class NNZ {
public:
  int64_t activations[L1_SIZE_BIG / 2] = {};

  void addActivations(uint8_t* neurons) {
    for (int i = 0; i < L1_SIZE_BIG; i++) {
      activations[i % (L1_SIZE_BIG / 2)] += bool(neurons[i]);
    }
  }

  int16_t oldInputPsqWeights[768 * KING_BUCKETS * L1_SIZE_BIG];
  int8_t oldInputThreatWeights[ThreatInputs::FEATURE_COUNT * L1_SIZE_BIG];
  int16_t oldInputBiases[L1_SIZE_BIG];
  int8_t  oldL1Weights[OUTPUT_BUCKETS][L1_SIZE_BIG * L2_SIZE];
  NetworkData nnzOutNet;

  int order[L1_SIZE_BIG / 2];

  void permuteNetwork() {
    std::ifstream infile("quantised.bin", std::ios::binary);
    if (!infile) {
        std::cerr << "Error opening file for reading" << std::endl;
        return;
    }
    infile.read(reinterpret_cast<char*>(&nnzOutNet), sizeof(nnzOutNet));
    infile.close();

    memcpy(oldInputPsqWeights, nnzOutNet.bigNet.inputPsqWeights, sizeof(nnzOutNet.bigNet.inputPsqWeights));
    memcpy(oldInputThreatWeights, nnzOutNet.bigNet.inputThreatWeights, sizeof(nnzOutNet.bigNet.inputThreatWeights));
    memcpy(oldInputBiases, nnzOutNet.bigNet.inputBiases, sizeof(nnzOutNet.bigNet.inputBiases));
    memcpy(oldL1Weights, nnzOutNet.bigNet.l1Weights, sizeof(nnzOutNet.bigNet.l1Weights));

    for (int i = 0; i < L1_SIZE_BIG / 2; i++) {
      order[i] = i;
    }
    std::stable_sort(order, order + L1_SIZE_BIG / 2, [&](const int& a, const int& b) { return activations[a] < activations[b]; });

    for (int l1 = 0; l1 < L1_SIZE_BIG / 2; l1++) {

      // Input weights
      for (int ip = 0; ip < 768 * KING_BUCKETS; ip++) {
        nnzOutNet.bigNet.inputPsqWeights[ip * L1_SIZE_BIG + l1] = oldInputPsqWeights[ip * L1_SIZE_BIG + order[l1]];
        nnzOutNet.bigNet.inputPsqWeights[ip * L1_SIZE_BIG + l1 + L1_SIZE_BIG / 2] = oldInputPsqWeights[ip * L1_SIZE_BIG + order[l1] + L1_SIZE_BIG / 2];
      }
      for (int ip = 0; ip < ThreatInputs::FEATURE_COUNT; ip++) {
        nnzOutNet.bigNet.inputThreatWeights[ip * L1_SIZE_BIG + l1] = oldInputThreatWeights[ip * L1_SIZE_BIG + order[l1]];
        nnzOutNet.bigNet.inputThreatWeights[ip * L1_SIZE_BIG + l1 + L1_SIZE_BIG / 2] = oldInputThreatWeights[ip * L1_SIZE_BIG + order[l1] + L1_SIZE_BIG / 2];
      }

      // Input biases
      nnzOutNet.bigNet.inputBiases[l1] = oldInputBiases[order[l1]];
      nnzOutNet.bigNet.inputBiases[l1 + L1_SIZE_BIG / 2] = oldInputBiases[order[l1] + L1_SIZE_BIG / 2];

      // L1 weights
      for (int ob = 0; ob < OUTPUT_BUCKETS; ob++) {
        for (int l2 = 0; l2 < L2_SIZE; l2++) {
          reinterpret_cast<int8_t*>(nnzOutNet.bigNet.l1Weights)[l1 * OUTPUT_BUCKETS * L2_SIZE + ob * L2_SIZE + l2] = reinterpret_cast<int8_t*>(oldL1Weights)[order[l1] * OUTPUT_BUCKETS * L2_SIZE + ob * L2_SIZE + l2];
          reinterpret_cast<int8_t*>(nnzOutNet.bigNet.l1Weights)[(l1 + L1_SIZE_BIG / 2) * OUTPUT_BUCKETS * L2_SIZE + ob * L2_SIZE + l2] = reinterpret_cast<int8_t*>(oldL1Weights)[(order[l1] + L1_SIZE_BIG / 2) * OUTPUT_BUCKETS * L2_SIZE + ob * L2_SIZE + l2];
        }
      }
    }

    // Write the network
    std::ofstream outfile("quantised.bin", std::ios::binary);
    if (!outfile) {
        std::cerr << "Error opening file for writing" << std::endl;
        return;
    }
    outfile.write(reinterpret_cast<char*>(&nnzOutNet), sizeof(nnzOutNet));
    outfile.close();
    std::cout << "NNZ permuted net written to quantised.bin" << std::endl;
  }
};

extern NNZ nnz;
#endif