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

constexpr int INPUT_SIZE = ThreatInputs::FEATURE_COUNT + ThreatInputs::PAWN_PAIR_FEATURE_COUNT + 768 * KING_BUCKETS;
constexpr int L1_SIZE = 512;
constexpr int L1_SIZE_TOTAL = 2 * L1_SIZE;
constexpr int L2_SIZE = 16;
constexpr int L3_SIZE = 32;

constexpr int OUTPUT_BUCKETS = 8;

constexpr int NETWORK_SCALE = 287;
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
constexpr int L1_TOTAL_ITERATIONS = L1_SIZE_TOTAL / I16_VEC_SIZE;

enum class FtType {
  Psq,
  Threat,
  PawnPair
};

struct DirtyThreat {
  uint8_t piece;
  Square square;
  uint8_t attackedPiece;
  uint8_t attackedSquare;

  DirtyThreat() = default;

  DirtyThreat(Piece _piece, Color _pieceColor, Piece _attackedPiece, Color _attackedPieceColor, Square _square, Square _attackedSquare) {
    piece = static_cast<uint8_t>(_piece | (_pieceColor << 3));
    square = _square;
    attackedPiece = static_cast<uint8_t>(_attackedPiece | (_attackedPieceColor << 3));
    attackedSquare = _attackedSquare;
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

struct Accumulator {
  alignas(ALIGNMENT) int16_t threatState[2][L1_SIZE_TOTAL];
  alignas(ALIGNMENT) int16_t pieceState[2][L1_SIZE_TOTAL];

  DirtyPiece dirtyPiece;
  DirtyThreat dirtyThreatsAdded[128];
  DirtyThreat dirtyThreatsRemoved[128];
  int numThreatsAdded;
  int numThreatsRemoved;

  KingBucketInfo kingBucketInfo[2];
  Board* board;
};

struct FinnyEntry {
  alignas(ALIGNMENT) int16_t pieceState[2][L1_SIZE_TOTAL];

  Bitboard byColor[2][2];
  Bitboard byPiece[2][Piece::TOTAL];
};

struct NetworkData {
  alignas(ALIGNMENT) int16_t inputPsqWeights[768 * KING_BUCKETS * L1_SIZE_TOTAL];
  alignas(ALIGNMENT) int8_t inputPawnPairWeights[ThreatInputs::PAWN_PAIR_FEATURE_COUNT * L1_SIZE];
  alignas(ALIGNMENT) int8_t inputThreatWeights[ThreatInputs::FEATURE_COUNT * L1_SIZE_TOTAL];
  alignas(ALIGNMENT) int16_t inputBiases[L1_SIZE_TOTAL];
  alignas(ALIGNMENT) int8_t  l1Weights[OUTPUT_BUCKETS][L1_SIZE_TOTAL * L2_SIZE];
  alignas(ALIGNMENT) float   l1Biases[OUTPUT_BUCKETS][L2_SIZE];
  alignas(ALIGNMENT) float   l2Weights[OUTPUT_BUCKETS][2 * L2_SIZE * L3_SIZE];
  alignas(ALIGNMENT) float   l2Biases[OUTPUT_BUCKETS][L3_SIZE];
  alignas(ALIGNMENT) float   l3Weights[OUTPUT_BUCKETS][L3_SIZE + 2 * L2_SIZE];
  alignas(ALIGNMENT) float   l3Biases[OUTPUT_BUCKETS];
};

extern NetworkData* globalNetworkData;

void initNetworkData();

struct Board;

class NNUE {
public:

  NetworkData* networkData;

  NNUE() = default;
  NNUE(NetworkData* _networkData) {
    networkData = _networkData;
  }

  Accumulator accumulatorStack[MAX_PLY + 8];
  int currentAccumulator;
  int lastCalculatedAccumulator[2];

  FinnyEntry finnyTable[2][KING_BUCKETS];

  void updateThreat(Piece piece, Piece attackedPiece, Square square, Square attackedSquare, Color pieceColor, Color attackedColor, bool add);

#if defined(__AVX512VBMI2__)
  template<bool add, bool computeRays>
  void updatePieceThreatsGeometry(Board* board, Piece piece, Color pieceColor, Square square, Square ignore);
#endif

  void incrementAccumulator();
  void decrementAccumulator();
  void finalizeMove(Board* board, DirtyPiece dirtyPiece);

  void reset(Board* board);
  template<Color side>
  void resetAccumulator(Board* board, Accumulator* acc);

  Eval evaluate(Board* board);
  template<Color side>
  void calculateAccumulators();

  template<Color side>
  void refreshPieceFeatures(Accumulator* acc, KingBucketInfo* kingBucket);
  template<Color side>
  void refreshThreatFeatures(Accumulator* acc);

  template<Color side>
  void incrementallyUpdatePieceFeatures(Accumulator* inputAcc, Accumulator* outputAcc, KingBucketInfo* kingBucket);
  template<Color side>
  void incrementallyUpdateThreatFeatures(Accumulator* inputAcc, Accumulator* outputAcc, KingBucketInfo* kingBucket);

  template<FtType type, Color side>
  void addToAccumulator(int16_t(*inputData)[L1_SIZE_TOTAL], int16_t(*outputData)[L1_SIZE_TOTAL], int featureIndex);
  template<FtType type, Color side>
  void subFromAccumulator(int16_t(*inputData)[L1_SIZE_TOTAL], int16_t(*outputData)[L1_SIZE_TOTAL], int featureIndex);
  template<FtType type, Color side>
  void addSubToAccumulator(int16_t(*inputData)[L1_SIZE_TOTAL], int16_t(*outputData)[L1_SIZE_TOTAL], int addIndex, int subIndex);

  template<Color side>
  void applyThreatRows(int16_t(*inputData)[L1_SIZE_TOTAL], int16_t(*outputData)[L1_SIZE_TOTAL],
                       const ThreatInputs::FeatureList& adds, const ThreatInputs::FeatureList& subs);
  template<Color side>
  void applyPawnPairRows(int16_t(*inputData)[L1_SIZE_TOTAL], int16_t(*outputData)[L1_SIZE_TOTAL],
                         const ThreatInputs::FeatureList& adds, const ThreatInputs::FeatureList& subs);

};

#if defined(PROCESS_NET)

#include <cstring>
#include <fstream>

inline void writeSLEB128(std::ostream& os, int64_t value) {
    bool more = true;
    while (more) {
        uint8_t byte = value & 0x7F;
        bool sign = byte & 0x40;
        value >>= 7;
        if ((value == 0 && !sign) || (value == -1 && sign)) {
            more = false;
        } else {
            byte |= 0x80;
        }
        os.put(static_cast<char>(byte));
    }
}

inline void writeULEB128(std::ostream& os, uint64_t value) {
    do {
        uint8_t byte = value & 0x7F;
        value >>= 7;
        if (value != 0) byte |= 0x80;
        os.put(static_cast<char>(byte));
    } while (value != 0);
}

inline void writeFloat(std::ostream& os, float f) {
    uint32_t v;
    std::memcpy(&v, &f, sizeof(v));
    writeULEB128(os, v);
}

class NNZ {
public:
  int64_t activations[L1_SIZE / 2] = {};
  int64_t activationsPt[L1_SIZE / 2] = {};

  void addActivations(uint8_t* neurons) {
    for (int i = 0; i < L1_SIZE; i++) {
      activations[i % (L1_SIZE / 2)] += bool(neurons[i]);
    }
    for (int i = 0; i < L1_SIZE; i++) {
      activationsPt[i % (L1_SIZE / 2)] += bool(neurons[L1_SIZE + i]);
    }
  }

  int16_t oldInputPsqWeights[768 * KING_BUCKETS * L1_SIZE_TOTAL];
  int8_t oldInputPawnPairWeights[ThreatInputs::PAWN_PAIR_FEATURE_COUNT * L1_SIZE];
  int8_t oldInputThreatWeights[ThreatInputs::FEATURE_COUNT * L1_SIZE_TOTAL];
  int16_t oldInputBiases[L1_SIZE_TOTAL];
  int8_t  oldL1Weights[OUTPUT_BUCKETS][L1_SIZE_TOTAL * L2_SIZE];
  NetworkData nnzOutNet;

  int order[L1_SIZE / 2];
  int orderPt[L1_SIZE / 2];

  void permuteNetwork() {
    std::ifstream infile("./quantised.bin", std::ios::binary);
    if (!infile) {
        std::cerr << "Error opening file for reading" << std::endl;
        return;
    }
    infile.read(reinterpret_cast<char*>(&nnzOutNet), sizeof(nnzOutNet));
    infile.close();

    memcpy(oldInputPsqWeights, nnzOutNet.inputPsqWeights, sizeof(nnzOutNet.inputPsqWeights));
    memcpy(oldInputPawnPairWeights, nnzOutNet.inputPawnPairWeights, sizeof(nnzOutNet.inputPawnPairWeights));
    memcpy(oldInputThreatWeights, nnzOutNet.inputThreatWeights, sizeof(nnzOutNet.inputThreatWeights));
    memcpy(oldInputBiases, nnzOutNet.inputBiases, sizeof(nnzOutNet.inputBiases));
    memcpy(oldL1Weights, nnzOutNet.l1Weights, sizeof(nnzOutNet.l1Weights));

    for (int i = 0; i < L1_SIZE / 2; i++) {
      order[i] = i;
    }
    std::stable_sort(order, order + L1_SIZE / 2, [&](const int& a, const int& b) { return activations[a] < activations[b]; });

    for (int i = 0; i < L1_SIZE / 2; i++) {
      orderPt[i] = i;
    }
    std::stable_sort(orderPt, orderPt + L1_SIZE / 2, [&](const int& a, const int& b) { return activationsPt[a] < activationsPt[b]; });

    // All features
    for (int l1 = 0; l1 < L1_SIZE / 2; l1++) {
      int src = order[l1];

      // PSQ
      for (int ip = 0; ip < 768 * KING_BUCKETS; ip++) {
        nnzOutNet.inputPsqWeights[ip * L1_SIZE_TOTAL + l1] = oldInputPsqWeights[ip * L1_SIZE_TOTAL + src];
        nnzOutNet.inputPsqWeights[ip * L1_SIZE_TOTAL + l1 + L1_SIZE / 2] = oldInputPsqWeights[ip * L1_SIZE_TOTAL + src + L1_SIZE / 2];
      }
      // Pawn pairs
      for (int ip = 0; ip < ThreatInputs::PAWN_PAIR_FEATURE_COUNT; ip++) {
        nnzOutNet.inputPawnPairWeights[ip * L1_SIZE + l1] = oldInputPawnPairWeights[ip * L1_SIZE + src];
        nnzOutNet.inputPawnPairWeights[ip * L1_SIZE + l1 + L1_SIZE / 2] = oldInputPawnPairWeights[ip * L1_SIZE + src + L1_SIZE / 2];
      }
      // Threats
      for (int ip = 0; ip < ThreatInputs::FEATURE_COUNT; ip++) {
        nnzOutNet.inputThreatWeights[ip * L1_SIZE_TOTAL + l1] = oldInputThreatWeights[ip * L1_SIZE_TOTAL + src];
        nnzOutNet.inputThreatWeights[ip * L1_SIZE_TOTAL + l1 + L1_SIZE / 2] = oldInputThreatWeights[ip * L1_SIZE_TOTAL + src + L1_SIZE / 2];
      }

      // Input biases
      nnzOutNet.inputBiases[l1] = oldInputBiases[src];
      nnzOutNet.inputBiases[l1 + L1_SIZE / 2] = oldInputBiases[src + L1_SIZE / 2];

      // L1 weights
      for (int ob = 0; ob < OUTPUT_BUCKETS; ob++) {
        for (int l2 = 0; l2 < L2_SIZE; l2++) {
          reinterpret_cast<int8_t*>(nnzOutNet.l1Weights)[l1 * OUTPUT_BUCKETS * L2_SIZE + ob * L2_SIZE + l2] = reinterpret_cast<int8_t*>(oldL1Weights)[src * OUTPUT_BUCKETS * L2_SIZE + ob * L2_SIZE + l2];
          reinterpret_cast<int8_t*>(nnzOutNet.l1Weights)[(l1 + L1_SIZE / 2) * OUTPUT_BUCKETS * L2_SIZE + ob * L2_SIZE + l2] = reinterpret_cast<int8_t*>(oldL1Weights)[(src + L1_SIZE / 2) * OUTPUT_BUCKETS * L2_SIZE + ob * L2_SIZE + l2];
        }
      }
    }

    // PSQ + Threats only
    for (int k = 0; k < L1_SIZE / 2; k++) {
      int dstA = L1_SIZE + k;
      int dstB = L1_SIZE + L1_SIZE / 2 + k;
      int srcA = L1_SIZE + orderPt[k];
      int srcB = L1_SIZE + L1_SIZE / 2 + orderPt[k];

      // PSQ
      for (int ip = 0; ip < 768 * KING_BUCKETS; ip++) {
        nnzOutNet.inputPsqWeights[ip * L1_SIZE_TOTAL + dstA] = oldInputPsqWeights[ip * L1_SIZE_TOTAL + srcA];
        nnzOutNet.inputPsqWeights[ip * L1_SIZE_TOTAL + dstB] = oldInputPsqWeights[ip * L1_SIZE_TOTAL + srcB];
      }
      // Threats
      for (int ip = 0; ip < ThreatInputs::FEATURE_COUNT; ip++) {
        nnzOutNet.inputThreatWeights[ip * L1_SIZE_TOTAL + dstA] = oldInputThreatWeights[ip * L1_SIZE_TOTAL + srcA];
        nnzOutNet.inputThreatWeights[ip * L1_SIZE_TOTAL + dstB] = oldInputThreatWeights[ip * L1_SIZE_TOTAL + srcB];
      }

      // Input biases
      nnzOutNet.inputBiases[dstA] = oldInputBiases[srcA];
      nnzOutNet.inputBiases[dstB] = oldInputBiases[srcB];

      // L1 weights
      for (int ob = 0; ob < OUTPUT_BUCKETS; ob++) {
        for (int l2 = 0; l2 < L2_SIZE; l2++) {
          reinterpret_cast<int8_t*>(nnzOutNet.l1Weights)[dstA * OUTPUT_BUCKETS * L2_SIZE + ob * L2_SIZE + l2] = reinterpret_cast<int8_t*>(oldL1Weights)[srcA * OUTPUT_BUCKETS * L2_SIZE + ob * L2_SIZE + l2];
          reinterpret_cast<int8_t*>(nnzOutNet.l1Weights)[dstB * OUTPUT_BUCKETS * L2_SIZE + ob * L2_SIZE + l2] = reinterpret_cast<int8_t*>(oldL1Weights)[srcB * OUTPUT_BUCKETS * L2_SIZE + ob * L2_SIZE + l2];
        }
      }
    }

    // Write the network
    std::ofstream outfile("./compressed.bin", std::ios::binary);
    if (!outfile) {
        std::cerr << "Error opening file for writing" << std::endl;
        return;
    }
    for (auto v : nnzOutNet.inputPsqWeights) writeSLEB128(outfile, v);
    for (auto v : nnzOutNet.inputPawnPairWeights) writeSLEB128(outfile, v);
    for (auto v : nnzOutNet.inputThreatWeights) writeSLEB128(outfile, v);
    for (auto v : nnzOutNet.inputBiases)  writeSLEB128(outfile, v);
    for (auto& b : nnzOutNet.l1Weights)   for (auto v : b) writeSLEB128(outfile, v);
    for (auto& b : nnzOutNet.l1Biases)    for (auto v : b) writeFloat(outfile, v);
    for (auto& b : nnzOutNet.l2Weights)   for (auto v : b) writeFloat(outfile, v);
    for (auto& b : nnzOutNet.l2Biases)    for (auto v : b) writeFloat(outfile, v);
    for (auto& b : nnzOutNet.l3Weights)   for (auto v : b) writeFloat(outfile, v);
    for (auto v : nnzOutNet.l3Biases)     writeFloat(outfile, v);
    outfile.close();
    std::cout << "NNZ permuted net written to ./compressed.bin" << std::endl;
  }
};

extern NNZ nnz;
#endif