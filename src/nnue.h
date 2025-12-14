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

constexpr int INPUT_SIZE = ThreatInputs::FEATURE_COUNT + 768 * KING_BUCKETS;
constexpr int L1_SIZE = 640;
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

struct DirtyPiece {
  Square origin;
  Square target;
  Piece piece;
  Color pieceColor;
};

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

struct Accumulator {
  alignas(ALIGNMENT) int16_t threatState[2][L1_SIZE];
  alignas(ALIGNMENT) int16_t pieceState[2][L1_SIZE];

  DirtyPiece dirtyPieces[4];
  int numDirtyPieces;
  DirtyThreat dirtyThreats[256];
  int numDirtyThreats;

  KingBucketInfo kingBucketInfo[2];
  Board* board;
  bool computedThreats[2];
  bool computedPieces[2];
};

struct FinnyEntry {
  alignas(ALIGNMENT) int16_t pieceState[2][L1_SIZE];

  Bitboard byColor[2][2];
  Bitboard byPiece[2][Piece::TOTAL];
};

struct NetworkData {
  alignas(ALIGNMENT) int16_t inputPsqWeights[768 * KING_BUCKETS * L1_SIZE];
  alignas(ALIGNMENT) int8_t inputThreatWeights[ThreatInputs::FEATURE_COUNT * L1_SIZE];
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

  Accumulator accumulatorStack[MAX_PLY + 8];
  int currentAccumulator;

  FinnyEntry finnyTable[2][KING_BUCKETS];

  void addPiece(Square square, Piece piece, Color pieceColor);
  void removePiece(Square square, Piece piece, Color pieceColor);
  void movePiece(Square origin, Square target, Piece piece, Color pieceColor);
  void updateThreat(Piece piece, Piece attackedPiece, Square square, Square attackedSquare, Color pieceColor, Color attackedColor, bool add);

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
  void refreshPieceFeatures(Accumulator* acc, KingBucketInfo* kingBucket);
  template<Color side>
  void refreshThreatFeatures(Accumulator* acc);

  template<Color side>
  void forwardUpdatePieceIncremental(Accumulator* inputAcc, Accumulator* outputAcc, KingBucketInfo* kingBucket);
  template<Color side>
  void forwardUpdateThreatIncremental(Accumulator* inputAcc, Accumulator* outputAcc, KingBucketInfo* kingBucket);

  template<Color side>
  void backwardsUpdatePieceFeatures(Accumulator* inputAcc, Accumulator* outputAcc, KingBucketInfo* kingBucket);
  template<Color side>
  void backwardsUpdateThreatFeatures(Accumulator* inputAcc, Accumulator* outputAcc, KingBucketInfo* kingBucket);

  template<Color side, bool forward = true>
  void incrementallyUpdatePieceFeatures(Accumulator* inputAcc, Accumulator* outputAcc, KingBucketInfo* kingBucket);
  template<Color side, bool forward = true>
  void incrementallyUpdateThreatFeatures(Accumulator* inputAcc, Accumulator* outputAcc, KingBucketInfo* kingBucket);

  template<bool I8, Color side>
  void addToAccumulator(int16_t(*inputData)[L1_SIZE], int16_t(*outputData)[L1_SIZE], int featureIndex);
  template<bool I8, Color side>
  void subFromAccumulator(int16_t(*inputData)[L1_SIZE], int16_t(*outputData)[L1_SIZE], int featureIndex);
  template<bool I8, Color side>
  void addSubToAccumulator(int16_t(*inputData)[L1_SIZE], int16_t(*outputData)[L1_SIZE], int addIndex, int subIndex);

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

  void addActivations(uint8_t* neurons) {
    for (int i = 0; i < L1_SIZE; i++) {
      activations[i % (L1_SIZE / 2)] += bool(neurons[i]);
    }
  }

  int16_t oldInputPsqWeights[768 * KING_BUCKETS * L1_SIZE];
  int8_t oldInputThreatWeights[ThreatInputs::FEATURE_COUNT * L1_SIZE];
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

    memcpy(oldInputPsqWeights, nnzOutNet.inputPsqWeights, sizeof(nnzOutNet.inputPsqWeights));
    memcpy(oldInputThreatWeights, nnzOutNet.inputThreatWeights, sizeof(nnzOutNet.inputThreatWeights));
    memcpy(oldInputBiases, nnzOutNet.inputBiases, sizeof(nnzOutNet.inputBiases));
    memcpy(oldL1Weights, nnzOutNet.l1Weights, sizeof(nnzOutNet.l1Weights));

    for (int i = 0; i < L1_SIZE / 2; i++) {
      order[i] = i;
    }
    std::stable_sort(order, order + L1_SIZE / 2, [&](const int& a, const int& b) { return activations[a] < activations[b]; });

    for (int l1 = 0; l1 < L1_SIZE / 2; l1++) {

      // Input weights
      for (int ip = 0; ip < 768 * KING_BUCKETS; ip++) {
        nnzOutNet.inputPsqWeights[ip * L1_SIZE + l1] = oldInputPsqWeights[ip * L1_SIZE + order[l1]];
        nnzOutNet.inputPsqWeights[ip * L1_SIZE + l1 + L1_SIZE / 2] = oldInputPsqWeights[ip * L1_SIZE + order[l1] + L1_SIZE / 2];
      }
      for (int ip = 0; ip < ThreatInputs::FEATURE_COUNT; ip++) {
        nnzOutNet.inputThreatWeights[ip * L1_SIZE + l1] = oldInputThreatWeights[ip * L1_SIZE + order[l1]];
        nnzOutNet.inputThreatWeights[ip * L1_SIZE + l1 + L1_SIZE / 2] = oldInputThreatWeights[ip * L1_SIZE + order[l1] + L1_SIZE / 2];
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
    std::ofstream outfile("./compressed.bin", std::ios::binary);
    if (!outfile) {
        std::cerr << "Error opening file for writing" << std::endl;
        return;
    }
    for (auto v : nnzOutNet.inputPsqWeights) writeSLEB128(outfile, v);
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