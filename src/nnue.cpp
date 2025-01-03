#include <iostream>
#include <fstream>
#include <string.h>
#include <cassert>
#include <cmath>

#include "nnue.h"
#include "board.h"
#include "incbin/incbin.h"

#if defined(ARCH_ARM)
#include <arm_neon.h>
#endif

// This will define the following variables:
// const unsigned char        gNETWORKData[];
// const unsigned char *const gNETWORKEnd;
// const unsigned int         gNETWORKSize;
INCBIN(NETWORK, EVALFILE);

Networks* networks;
alignas(ALIGNMENT) uint16_t nnzLookup[256][8];

#if defined(PROCESS_NET)
NNZ nnz;
#endif

void initNetworkData() {
    for (size_t i = 0; i < 256; i++) {
        uint64_t j = i;
        uint64_t k = 0;
        while (j) {
            nnzLookup[i][k++] = popLSB(&j);
        }
    }

    networks = (Networks*)gNETWORKData;
}

inline int networkIndex(Board* board) {
  if (BB::popcount(board->byColor[Color::WHITE] | board->byColor[Color::BLACK]) > 2)
    return 0;
  return 1;
}

inline int networkIndexFromPtr(NetworkData* ptr) {
    return ptr == &networks->middlegame ? 0 : 1;
}

inline NetworkData* chooseNetwork(Board* board) {
  return networkIndex(board) == 0 ? &networks->middlegame : &networks->endgame;
}

inline int getFeatureOffset(Color side, Piece piece, Color pieceColor, Square square, KingBucketInfo* kingBucket) {
    square ^= kingBucket->mirrored * 7;
    int relativeSquare = square ^ (side * 56);
    if (side == pieceColor)
        return (64 * piece + relativeSquare) * L1_SIZE / I16_VEC_SIZE;
    else
        return (64 * (piece + 6) + relativeSquare) * L1_SIZE / I16_VEC_SIZE;
}

void NNUE::reset(Board* board) {
    // Reset accumulator
    currentAccumulator = 0;
    lastCalculatedAccumulator[Color::WHITE] = 0;
    lastCalculatedAccumulator[Color::BLACK] = 0;
    resetAccumulator<Color::WHITE>(board, &accumulatorStack[0]);
    resetAccumulator<Color::BLACK>(board, &accumulatorStack[0]);

    NetworkData* network = chooseNetwork(board);

    // Also reset finny tables
    for (int i = 0; i < 2; i++) {
        for (int i2 = 0; i2 < 2; i2++) {
            for (int j = 0; j < KING_BUCKETS; j++) {
                memset(finnyTable[i][i2][j].byColor, 0, sizeof(finnyTable[i][i2][j].byColor));
                memset(finnyTable[i][i2][j].byPiece, 0, sizeof(finnyTable[i][i2][j].byPiece));
                memcpy(finnyTable[i][i2][j].colors[Color::WHITE], network->inputBiases, sizeof(network->inputBiases));
                memcpy(finnyTable[i][i2][j].colors[Color::BLACK], network->inputBiases, sizeof(network->inputBiases));
            }
        }
    }
}

template<Color side>
void NNUE::resetAccumulator(Board* board, Accumulator* acc) {
    NetworkData* network = chooseNetwork(board);

    // Overwrite with biases
    memcpy(acc->colors[side], network->inputBiases, sizeof(network->inputBiases));

    // Then add every piece back one by one
    KingBucketInfo kingBucket = getKingBucket(side, lsb(board->byColor[side] & board->byPiece[Piece::KING]), networkIndex(board));
    for (Square square = 0; square < 64; square++) {
        Piece piece = board->pieces[square];
        if (piece == Piece::NONE) continue;

        Color pieceColor = (board->byColor[Color::WHITE] & bitboard(square)) ? Color::WHITE : Color::BLACK;
        addPieceToAccumulator<side>(network, acc->colors, acc->colors, &kingBucket, square, piece, pieceColor);
    }

    acc->kingBucketInfo[side] = kingBucket;
    memcpy(acc->byColor[side], board->byColor, sizeof(board->byColor));
    memcpy(acc->byPiece[side], board->byPiece, sizeof(board->byPiece));
}

void NNUE::addPiece(Square square, Piece piece, Color pieceColor) {
    assert(piece < Piece::NONE);

    Accumulator* acc = &accumulatorStack[currentAccumulator];
    acc->dirtyPieces[acc->numDirtyPieces++] = { NO_SQUARE, square, piece, pieceColor };
}

void NNUE::removePiece(Square square, Piece piece, Color pieceColor) {
    assert(piece < Piece::NONE);

    Accumulator* acc = &accumulatorStack[currentAccumulator];
    acc->dirtyPieces[acc->numDirtyPieces++] = { square, NO_SQUARE, piece, pieceColor };
}

void NNUE::movePiece(Square origin, Square target, Piece piece, Color pieceColor) {
    assert(piece < Piece::NONE);

    Accumulator* acc = &accumulatorStack[currentAccumulator];
    acc->dirtyPieces[acc->numDirtyPieces++] = { origin, target, piece, pieceColor };
}

void NNUE::incrementAccumulator() {
    currentAccumulator++;
    accumulatorStack[currentAccumulator].numDirtyPieces = 0;
}

void NNUE::decrementAccumulator() {
    accumulatorStack[currentAccumulator].numDirtyPieces = 0;
    currentAccumulator--;
    lastCalculatedAccumulator[Color::WHITE] = std::min(lastCalculatedAccumulator[Color::WHITE], currentAccumulator);
    lastCalculatedAccumulator[Color::BLACK] = std::min(lastCalculatedAccumulator[Color::BLACK], currentAccumulator);
}

void NNUE::finalizeMove(Board* board) {
    Accumulator* accumulator = &accumulatorStack[currentAccumulator];

    for (Color side = Color::WHITE; side <= Color::BLACK; ++side) {
        accumulator->kingBucketInfo[side] = getKingBucket(side, lsb(board->byPiece[Piece::KING] & board->byColor[side]), networkIndex(board));
        memcpy(accumulator->byColor[side], board->byColor, sizeof(board->byColor));
        memcpy(accumulator->byPiece[side], board->byPiece, sizeof(board->byPiece));
    }
}

template<Color side>
void NNUE::calculateAccumulators(NetworkData* network) {
    // Incrementally update all accumulators for this side
    while (lastCalculatedAccumulator[side] < currentAccumulator) {

        Accumulator* inputAcc = &accumulatorStack[lastCalculatedAccumulator[side]];
        Accumulator* outputAcc = &accumulatorStack[lastCalculatedAccumulator[side] + 1];

        KingBucketInfo* inputKingBucket = &inputAcc->kingBucketInfo[side];
        KingBucketInfo* outputKingBucket = &outputAcc->kingBucketInfo[side];

        if (needsRefresh(inputKingBucket, outputKingBucket))
            refreshAccumulator<side>(network, outputAcc);
        else
            incrementallyUpdateAccumulator<side>(network, inputAcc, outputAcc, outputKingBucket);

        lastCalculatedAccumulator[side]++;
    }
}

template<Color side>
void NNUE::refreshAccumulator(NetworkData* network, Accumulator* acc) {
    FinnyEntry* finnyEntry = &finnyTable[networkIndexFromPtr(network)][acc->kingBucketInfo[side].mirrored][acc->kingBucketInfo[side].index];

    // Update matching finny table with the changed pieces
    for (Color c = Color::WHITE; c <= Color::BLACK; ++c) {
        for (Piece p = Piece::PAWN; p < Piece::TOTAL; ++p) {
            Bitboard finnyBB = finnyEntry->byColor[side][c] & finnyEntry->byPiece[side][p];
            Bitboard accBB = acc->byColor[side][c] & acc->byPiece[side][p];

            Bitboard addBB = accBB & ~finnyBB;
            Bitboard removeBB = ~accBB & finnyBB;

            while (addBB) {
                Square square = popLSB(&addBB);
                addPieceToAccumulator<side>(network, finnyEntry->colors, finnyEntry->colors, &acc->kingBucketInfo[side], square, p, c);
            }
            while (removeBB) {
                Square square = popLSB(&removeBB);
                removePieceFromAccumulator<side>(network, finnyEntry->colors, finnyEntry->colors, &acc->kingBucketInfo[side], square, p, c);
            }
        }
    }
    memcpy(finnyEntry->byColor[side], acc->byColor[side], sizeof(finnyEntry->byColor[side]));
    memcpy(finnyEntry->byPiece[side], acc->byPiece[side], sizeof(finnyEntry->byPiece[side]));

    // Copy result to the current accumulator
    memcpy(acc->colors[side], finnyEntry->colors[side], sizeof(acc->colors[side]));
}

template<Color side>
void NNUE::incrementallyUpdateAccumulator(NetworkData* network, Accumulator* inputAcc, Accumulator* outputAcc, KingBucketInfo* kingBucket) {
    // Incrementally update all the dirty pieces
    // Input and output are the same king bucket so it's all good
    for (int dp = 0; dp < outputAcc->numDirtyPieces; dp++) {
        DirtyPiece dirtyPiece = outputAcc->dirtyPieces[dp];

        if (dirtyPiece.origin == NO_SQUARE) {
            addPieceToAccumulator<side>(network, inputAcc->colors, outputAcc->colors, kingBucket, dirtyPiece.target, dirtyPiece.piece, dirtyPiece.pieceColor);
        }
        else if (dirtyPiece.target == NO_SQUARE) {
            removePieceFromAccumulator<side>(network, inputAcc->colors, outputAcc->colors, kingBucket, dirtyPiece.origin, dirtyPiece.piece, dirtyPiece.pieceColor);
        }
        else {
            movePieceInAccumulator<side>(network, inputAcc->colors, outputAcc->colors, kingBucket, dirtyPiece.origin, dirtyPiece.target, dirtyPiece.piece, dirtyPiece.pieceColor);
        }

        // After the input was used to calculate the next accumulator, that accumulator updates itself for the rest of the dirtyPieces
        inputAcc = outputAcc;
    }
}

template<Color side>
void NNUE::addPieceToAccumulator(NetworkData* network, int16_t(*inputData)[L1_SIZE], int16_t(*outputData)[L1_SIZE], KingBucketInfo* kingBucket, Square square, Piece piece, Color pieceColor) {
    // Get the index of the piece for this color in the input layer
    int weightOffset = getFeatureOffset(side, piece, pieceColor, square, kingBucket);

    VecI16* inputVec = (VecI16*)inputData[side];
    VecI16* outputVec = (VecI16*)outputData[side];
    VecI16* weightsVec = (VecI16*)network->inputWeights[kingBucket->index];

    // The number of iterations to compute the hidden layer depends on the size of the vector registers
    for (int i = 0; i < L1_ITERATIONS; ++i)
        outputVec[i] = addEpi16(inputVec[i], weightsVec[weightOffset + i]);
}

template<Color side>
void NNUE::removePieceFromAccumulator(NetworkData* network, int16_t(*inputData)[L1_SIZE], int16_t(*outputData)[L1_SIZE], KingBucketInfo* kingBucket, Square square, Piece piece, Color pieceColor) {
    // Get the index of the piece for this color in the input layer
    int weightOffset = getFeatureOffset(side, piece, pieceColor, square, kingBucket);

    VecI16* inputVec = (VecI16*)inputData[side];
    VecI16* outputVec = (VecI16*)outputData[side];
    VecI16* weightsVec = (VecI16*)network->inputWeights[kingBucket->index];

    // The number of iterations to compute the hidden layer depends on the size of the vector registers
    for (int i = 0; i < L1_ITERATIONS; ++i)
        outputVec[i] = subEpi16(inputVec[i], weightsVec[weightOffset + i]);
}

template<Color side>
void NNUE::movePieceInAccumulator(NetworkData* network, int16_t(*inputData)[L1_SIZE], int16_t(*outputData)[L1_SIZE], KingBucketInfo* kingBucket, Square origin, Square target, Piece piece, Color pieceColor) {
    // Get the index of the piece squares for this color in the input layer
    int subtractWeightOffset = getFeatureOffset(side, piece, pieceColor, origin, kingBucket);
    int addWeightOffset = getFeatureOffset(side, piece, pieceColor, target, kingBucket);

    VecI16* inputVec = (VecI16*)inputData[side];
    VecI16* outputVec = (VecI16*)outputData[side];
    VecI16* weightsVec = (VecI16*)network->inputWeights[kingBucket->index];

    // The number of iterations to compute the hidden layer depends on the size of the vector registers
    for (int i = 0; i < L1_ITERATIONS; ++i) {
        outputVec[i] = subEpi16(addEpi16(inputVec[i], weightsVec[addWeightOffset + i]), weightsVec[subtractWeightOffset + i]);
    }
}

Eval NNUE::evaluate(Board* board) {
    assert(currentAccumulator >= lastCalculatedAccumulator[Color::WHITE] && currentAccumulator >= lastCalculatedAccumulator[Color::BLACK]);

    // Make sure the current accumulators are up to date
    NetworkData* network = chooseNetwork(board);
    calculateAccumulators<Color::WHITE>(network);
    calculateAccumulators<Color::BLACK>(network);

    assert(currentAccumulator == lastCalculatedAccumulator[Color::WHITE] && currentAccumulator == lastCalculatedAccumulator[Color::BLACK]);

    // Calculate output bucket based on piece count
    int pieceCount = BB::popcount(board->byColor[Color::WHITE] | board->byColor[Color::BLACK]);
    constexpr int divisor = ((32 + OUTPUT_BUCKETS - 1) / OUTPUT_BUCKETS);
    int bucket = (pieceCount - 2) / divisor;
    assert(0 <= bucket && bucket < OUTPUT_BUCKETS);

    Accumulator* accumulator = &accumulatorStack[currentAccumulator];

    VecI16* stmAcc = reinterpret_cast<VecI16*>(accumulator->colors[board->stm]);
    VecI16* oppAcc = reinterpret_cast<VecI16*>(accumulator->colors[1 - board->stm]);

    alignas(ALIGNMENT) uint8_t l1Neurons[L1_SIZE];
    VecI16 i16Zero = set1Epi16(0);
    VecI16 i16Quant = set1Epi16(INPUT_QUANT);

    VecIu8* l1NeuronsVec = reinterpret_cast<VecIu8*>(l1Neurons);

    constexpr int inverseShift = 16 - INPUT_SHIFT;
    constexpr int pairwiseOffset = L1_SIZE / I16_VEC_SIZE / 2;
    for (int l1 = 0; l1 < pairwiseOffset; l1 += 2) {
        // STM
        VecI16 clipped1 = minEpi16(maxEpi16(stmAcc[l1], i16Zero), i16Quant);
        VecI16 clipped2 = minEpi16(stmAcc[l1 + pairwiseOffset], i16Quant);
        VecI16 shift = slliEpi16(clipped1, inverseShift);
        VecI16 mul1 = mulhiEpi16(shift, clipped2);

        clipped1 = minEpi16(maxEpi16(stmAcc[l1 + 1], i16Zero), i16Quant);
        clipped2 = minEpi16(stmAcc[l1 + 1 + pairwiseOffset], i16Quant);
        shift = slliEpi16(clipped1, inverseShift);
        VecI16 mul2 = mulhiEpi16(shift, clipped2);

        l1NeuronsVec[l1 / 2] = packusEpi16(mul1, mul2);

        // NSTM
        clipped1 = minEpi16(maxEpi16(oppAcc[l1], i16Zero), i16Quant);
        clipped2 = minEpi16(oppAcc[l1 + pairwiseOffset], i16Quant);
        shift = slliEpi16(clipped1, inverseShift);
        mul1 = mulhiEpi16(shift, clipped2);

        clipped1 = minEpi16(maxEpi16(oppAcc[l1 + 1], i16Zero), i16Quant);
        clipped2 = minEpi16(oppAcc[l1 + 1 + pairwiseOffset], i16Quant);
        shift = slliEpi16(clipped1, inverseShift);
        mul2 = mulhiEpi16(shift, clipped2);

        l1NeuronsVec[l1 / 2 + pairwiseOffset / 2] = packusEpi16(mul1, mul2);
    }

#if defined(PROCESS_NET)
    nnz.addActivations(l1Neurons);
#endif

    alignas(ALIGNMENT) int l2Neurons[L2_SIZE] = {};
#if defined(__SSSE3__) || defined(__AVX2__) || (defined(__AVX512F__) && defined(__AVX512BW__)) || defined(ARCH_ARM)
    int nnzCount = 0;
    alignas(ALIGNMENT) uint16_t nnzIndices[L1_SIZE / INT8_PER_INT32];

#if defined(ARCH_X86)
    __m128i nnzZero = _mm_setzero_si128();
    __m128i nnzIncrement = _mm_set1_epi16(8);
    for (int i = 0; i < L1_SIZE / INT8_PER_INT32 / 16; i++) {
        uint32_t nnz = 0;

        for (int j = 0; j < 16 / I32_VEC_SIZE; j++) {
            nnz |= vecNNZ(l1NeuronsVec[i * 16 / I32_VEC_SIZE + j]) << (j * I32_VEC_SIZE);
        }

        for (int j = 0; j < 16 / 8; j++) {
            uint16_t lookup = (nnz >> (j * 8)) & 0xFF;
            __m128i offsets = _mm_loadu_si128(reinterpret_cast<__m128i*>(&nnzLookup[lookup]));
            _mm_storeu_si128(reinterpret_cast<__m128i*>(nnzIndices + nnzCount), _mm_add_epi16(nnzZero, offsets));
            nnzCount += BB::popcount(lookup);
            nnzZero = _mm_add_epi16(nnzZero, nnzIncrement);
        }
    }
#else
    VecI32* l1NeuronsVecI32 = reinterpret_cast<VecI32*>(l1Neurons);
    uint16x8_t nnzZero = vdupq_n_u16(0);
    uint16x8_t nnzIncrement = vdupq_n_u16(8);

    for (int i = 0; i < L1_SIZE / INT8_PER_INT32 / 16; i++) {
        uint32_t nnz = 0;

        for (int j = 0; j < 16 / I32_VEC_SIZE; j++) {
            nnz |= vecNNZ(l1NeuronsVecI32[i * 16 / I32_VEC_SIZE + j]) << (j * I32_VEC_SIZE);
        }

        for (int j = 0; j < 16 / 8; j++) {
            uint16_t lookup = (nnz >> (j * 8)) & 0xFF;
            uint16x8_t offsets = vld1q_u16(nnzLookup[lookup]);
            vst1q_u16(nnzIndices + nnzCount, vaddq_u16(nnzZero, offsets));
            nnzCount += BB::popcount(lookup);
            nnzZero = vaddq_u16(nnzZero, nnzIncrement);
        }
    }

#endif

    int* l1Packs = reinterpret_cast<int*>(l1Neurons);
    VecI32* l2NeuronsVec = reinterpret_cast<VecI32*>(l2Neurons);

    int i = 0;
    for (; i < nnzCount - 1; i += 2) {
        int l1_1 = nnzIndices[i] * INT8_PER_INT32;
        int l1_2 = nnzIndices[i + 1] * INT8_PER_INT32;
#if defined(ARCH_X86)
        VecIu8 u8_1 = set1Epi32(l1Packs[l1_1 / INT8_PER_INT32]);
        VecIu8 u8_2 = set1Epi32(l1Packs[l1_2 / INT8_PER_INT32]);
#else
        VecIu8 u8_1 = vreinterpretq_u8_s32(set1Epi32(l1Packs[l1_1 / INT8_PER_INT32]));
        VecIu8 u8_2 = vreinterpretq_u8_s32(set1Epi32(l1Packs[l1_2 / INT8_PER_INT32]));
#endif
        VecI8* weights_1 = reinterpret_cast<VecI8*>(&network->l1Weights[bucket][l1_1 * L2_SIZE]);
        VecI8* weights_2 = reinterpret_cast<VecI8*>(&network->l1Weights[bucket][l1_2 * L2_SIZE]);

        for (int l2 = 0; l2 < L2_SIZE / I32_VEC_SIZE; l2++) {
            l2NeuronsVec[l2] = dpbusdEpi32x2(l2NeuronsVec[l2], u8_1, weights_1[l2], u8_2, weights_2[l2]);
        }
    }

    for (; i < nnzCount; i++) {
        int l1 = nnzIndices[i] * INT8_PER_INT32;
#if defined(ARCH_X86)
        VecIu8 u8 = set1Epi32(l1Packs[l1 / INT8_PER_INT32]);
#else
        VecIu8 u8 = vreinterpretq_u8_s32(set1Epi32(l1Packs[l1 / INT8_PER_INT32]));
#endif
        VecI8* weights = reinterpret_cast<VecI8*>(&network->l1Weights[bucket][l1 * L2_SIZE]);

        for (int l2 = 0; l2 < L2_SIZE / I32_VEC_SIZE; l2++) {
            l2NeuronsVec[l2] = dpbusdEpi32(l2NeuronsVec[l2], u8, weights[l2]);
        }
    }
#else
    for (int l1 = 0; l1 < L1_SIZE; l1++) {
        if (!l1Neurons[l1])
            continue;
            
        for (int l2 = 0; l2 < L2_SIZE; l2++) {
            l2Neurons[l2] += l1Neurons[l1] * network->l1Weights[bucket][l1 * L2_SIZE + l2];
        }
    }
#endif

    alignas(ALIGNMENT) float l3Neurons[L3_SIZE];
    memcpy(l3Neurons, network->l2Biases[bucket], sizeof(l3Neurons));

#if defined(__FMA__) || defined(__AVX2__) || (defined(__AVX512F__) && defined(__AVX512BW__)) || defined(ARCH_ARM)
    alignas(ALIGNMENT) float l2Floats[L2_SIZE];

    VecF psNorm = set1Ps(L1_NORMALISATION);
    VecF psZero = set1Ps(0.0f);
    VecF psOne = set1Ps(1.0f);

    VecF* l1Biases = reinterpret_cast<VecF*>(network->l1Biases[bucket]);
    VecF* l2FloatsVec = reinterpret_cast<VecF*>(l2Floats);

    for (int l2 = 0; l2 < L2_SIZE / FLOAT_VEC_SIZE; l2++) {
        VecF converted = cvtepi32Ps(l2NeuronsVec[l2]);
        VecF l2Result = addPs(mulPs(converted, psNorm), l1Biases[l2]);
        VecF l2Clipped = maxPs(minPs(l2Result, psOne), psZero);
        l2FloatsVec[l2] = mulPs(l2Clipped, l2Clipped);
    }

    VecF* l3NeuronsVec = reinterpret_cast<VecF*>(l3Neurons);
    for (int l2 = 0; l2 < L2_SIZE; l2++) {
        VecF l2Vec = set1Ps(l2Floats[l2]);
        VecF* weights = reinterpret_cast<VecF*>(&network->l2Weights[bucket][l2 * L3_SIZE]);
        for (int l3 = 0; l3 < L3_SIZE / FLOAT_VEC_SIZE; l3++) {
            l3NeuronsVec[l3] = fmaddPs(l2Vec, weights[l3], l3NeuronsVec[l3]);
        }
    }
#else
    for (int l2 = 0; l2 < L2_SIZE; l2++) {
        float l2Result = static_cast<float>(l2Neurons[l2]) * L1_NORMALISATION + network->l1Biases[bucket][l2];
        float l2Activated = std::clamp(l2Result, 0.0f, 1.0f);
        l2Activated *= l2Activated;

        for (int l3 = 0; l3 < L3_SIZE; l3++) {
            l3Neurons[l3] = std::fma(l2Activated, network->l2Weights[bucket][l2 * L3_SIZE + l3], l3Neurons[l3]);
        }
    }
#endif

#if defined(__FMA__) || defined(__AVX2__) || (defined(__AVX512F__) && defined(__AVX512BW__)) || defined(ARCH_ARM)
    constexpr int chunks = 64 / sizeof(VecF);

    VecF resultSums[chunks];
    for (int i = 0; i < chunks; i++)
        resultSums[i] = psZero;

    VecF* l3WeightsVec = reinterpret_cast<VecF*>(network->l3Weights[bucket]);
    for (int l3 = 0; l3 < L3_SIZE / FLOAT_VEC_SIZE; l3 += chunks) {
        for (int chunk = 0; chunk < chunks; chunk++) {
            VecF l3Clipped = maxPs(minPs(l3NeuronsVec[l3 + chunk], psOne), psZero);
            VecF l3Activated = mulPs(l3Clipped, l3Clipped);
            resultSums[chunk] = fmaddPs(l3Activated, l3WeightsVec[l3 + chunk], resultSums[chunk]);
        }
    }

    float result = network->l3Biases[bucket] + reduceAddPs(resultSums);
#else
    constexpr int chunks = sizeof(VecF) / sizeof(float);
    float resultSums[chunks] = {};

    for (int l3 = 0; l3 < L3_SIZE; l3 += chunks) {
        for (int chunk = 0; chunk < chunks; chunk++) {
            float l3Activated = std::clamp(l3Neurons[l3 + chunk], 0.0f, 1.0f);
            float l3Squared = l3Activated * l3Activated;
            resultSums[chunk] = std::fma(l3Squared, network->l3Weights[bucket][l3 + chunk], resultSums[chunk]);
        }
    }

    float result = network->l3Biases[bucket] + reduceAddPsR(resultSums, chunks);
#endif

    return result * NETWORK_SCALE;
}