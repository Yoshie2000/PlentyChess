#include <iostream>
#include <fstream>
#include <string.h>
#include <cassert>
#include <cmath>

#include "nnue.h"
#include "board.h"
#include "incbin/incbin.h"

// This will define the following variables:
// const unsigned char        gNETWORKData[];
// const unsigned char *const gNETWORKEnd;
// const unsigned int         gNETWORKSize;
INCBIN(NETWORK, EVALFILE);

NetworkData networkData;

void initNetworkData() {
    UnalignedNetworkData* incNetwork = (UnalignedNetworkData*)gNETWORKData;
    memcpy(networkData.inputWeights, incNetwork->inputWeights, sizeof(networkData.inputWeights));
    memcpy(networkData.inputBiases, incNetwork->inputBiases, sizeof(networkData.inputBiases));
    memcpy(networkData.l1Biases, incNetwork->l1Biases, sizeof(networkData.l1Biases));
    memcpy(networkData.l2Biases, incNetwork->l2Biases, sizeof(networkData.l2Biases));
    memcpy(networkData.l3Biases, incNetwork->l3Biases, sizeof(networkData.l3Biases));

//     __m128i* inputWeights = reinterpret_cast<__m128i*>(networkData.inputWeights);
//     __m128i* inputBiases = reinterpret_cast<__m128i*>(networkData.inputBiases);
//     constexpr int chunkSize = 16;
//     // Permute input weights and biases to make packus work
// #if false && (defined(__AVX512F__) && defined(__AVX512BW__))
//     constexpr int chunks = 8;
//     constexpr int permutation[] = {0, 2, 4, 6, 1, 3, 5, 7};
// #elif false && defined(__AVX2__)
//     constexpr int chunks = 4;
//     constexpr int permutation[] = {0, 2, 1, 3};
// #else
//     constexpr int chunks = 1;
//     constexpr int permutation[] = {0};
// #endif
//     __m128i registers[chunks];

//     for (size_t l1 = 0; l1 < sizeof(networkData.inputWeights) / chunkSize; l1 += chunks) {
//         for (int i = 0; i < chunks; i++)
//             registers[i] = inputWeights[l1 + i];
//         for (int i = 0; i < chunks; i++)
//             inputWeights[l1 + i] = registers[permutation[i]];
//     }
//     for (size_t l1 = 0; l1 < sizeof(networkData.inputBiases) / chunkSize; l1 += chunks) {
//         for (int i = 0; i < chunks; i++)
//             registers[i] = inputBiases[l1 + i];
//         for (int i = 0; i < chunks; i++)
//             inputBiases[l1 + i] = registers[permutation[i]];
//     }

    // Transpose L1 / L2 / L3 weights
    for (int b = 0; b < OUTPUT_BUCKETS; b++) {
#if defined(__SSSE3__) || defined(__AVX2__) || (defined(__AVX512F__) && defined(__AVX512BW__))
        for (int l1 = 0; l1 < L1_SIZE / INT8_PER_INT32; l1++) {
            for (int l2 = 0; l2 < L2_SIZE; l2++) {
                for (int c = 0; c < INT8_PER_INT32; c++) {
                    networkData.l1Weights[b][l1 * INT8_PER_INT32 * L2_SIZE + l2 * INT8_PER_INT32 + c] = reinterpret_cast<int8_t*>(incNetwork->l1Weights)[(l1 * INT8_PER_INT32 + c) * OUTPUT_BUCKETS * L2_SIZE + b * L2_SIZE + l2];
                }
            }
        }
#else
        for (int l1 = 0; l1 < L1_SIZE; l1++) {
            for (int l2 = 0; l2 < L2_SIZE; l2++) {
                networkData.l1Weights[b][l1 * L2_SIZE + l2] = reinterpret_cast<int8_t*>(incNetwork->l1Weights)[l1 * OUTPUT_BUCKETS * L2_SIZE + b * L2_SIZE + l2];
            }
        }
#endif

        for (int l2 = 0; l2 < L2_SIZE; l2++) {
            for (int l3 = 0; l3 < L3_SIZE; l3++) {
                networkData.l2Weights[b][l2 * L3_SIZE + l3] = reinterpret_cast<float*>(incNetwork->l2Weights)[l2 * OUTPUT_BUCKETS * L3_SIZE + b * L3_SIZE + l3];
            }
        }

        for (int l3 = 0; l3 < L3_SIZE; l3++) {
            networkData.l3Weights[b][l3] = reinterpret_cast<float*>(incNetwork->l3Weights)[l3 * OUTPUT_BUCKETS + b];
        }
    }
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

    // Also reset finny tables
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < KING_BUCKETS; j++) {
            memset(finnyTable[i][j].byColor, 0, sizeof(finnyTable[i][j].byColor));
            memset(finnyTable[i][j].byPiece, 0, sizeof(finnyTable[i][j].byPiece));
            memcpy(finnyTable[i][j].colors[Color::WHITE], networkData.inputBiases, sizeof(networkData.inputBiases));
            memcpy(finnyTable[i][j].colors[Color::BLACK], networkData.inputBiases, sizeof(networkData.inputBiases));
        }
    }
}

template<Color side>
void NNUE::resetAccumulator(Board* board, Accumulator* acc) {
    // Overwrite with biases
    memcpy(acc->colors[side], networkData.inputBiases, sizeof(networkData.inputBiases));

    // Then add every piece back one by one
    KingBucketInfo kingBucket = getKingBucket(side, lsb(board->byColor[side] & board->byPiece[Piece::KING]));
    for (Square square = 0; square < 64; square++) {
        Piece piece = board->pieces[square];
        if (piece == Piece::NONE) continue;

        Color pieceColor = (board->byColor[Color::WHITE] & bitboard(square)) ? Color::WHITE : Color::BLACK;
        addPieceToAccumulator<side>(acc->colors, acc->colors, &kingBucket, square, piece, pieceColor);
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
        accumulator->kingBucketInfo[side] = getKingBucket(side, lsb(board->byPiece[Piece::KING] & board->byColor[side]));
        memcpy(accumulator->byColor[side], board->byColor, sizeof(board->byColor));
        memcpy(accumulator->byPiece[side], board->byPiece, sizeof(board->byPiece));
    }
}

template<Color side>
void NNUE::calculateAccumulators() {
    // Incrementally update all accumulators for this side
    while (lastCalculatedAccumulator[side] < currentAccumulator) {

        Accumulator* inputAcc = &accumulatorStack[lastCalculatedAccumulator[side]];
        Accumulator* outputAcc = &accumulatorStack[lastCalculatedAccumulator[side] + 1];

        KingBucketInfo* inputKingBucket = &inputAcc->kingBucketInfo[side];
        KingBucketInfo* outputKingBucket = &outputAcc->kingBucketInfo[side];

        if (needsRefresh(inputKingBucket, outputKingBucket))
            refreshAccumulator<side>(outputAcc);
        else
            incrementallyUpdateAccumulator<side>(inputAcc, outputAcc, outputKingBucket);

        lastCalculatedAccumulator[side]++;
    }
}

template<Color side>
void NNUE::refreshAccumulator(Accumulator* acc) {
    FinnyEntry* finnyEntry = &finnyTable[acc->kingBucketInfo[side].mirrored][acc->kingBucketInfo[side].index];

    // Update matching finny table with the changed pieces
    for (Color c = Color::WHITE; c <= Color::BLACK; ++c) {
        for (Piece p = Piece::PAWN; p < Piece::TOTAL; ++p) {
            Bitboard finnyBB = finnyEntry->byColor[side][c] & finnyEntry->byPiece[side][p];
            Bitboard accBB = acc->byColor[side][c] & acc->byPiece[side][p];

            Bitboard addBB = accBB & ~finnyBB;
            Bitboard removeBB = ~accBB & finnyBB;

            while (addBB) {
                Square square = popLSB(&addBB);
                addPieceToAccumulator<side>(finnyEntry->colors, finnyEntry->colors, &acc->kingBucketInfo[side], square, p, c);
            }
            while (removeBB) {
                Square square = popLSB(&removeBB);
                removePieceFromAccumulator<side>(finnyEntry->colors, finnyEntry->colors, &acc->kingBucketInfo[side], square, p, c);
            }
        }
    }
    memcpy(finnyEntry->byColor[side], acc->byColor[side], sizeof(finnyEntry->byColor[side]));
    memcpy(finnyEntry->byPiece[side], acc->byPiece[side], sizeof(finnyEntry->byPiece[side]));

    // Copy result to the current accumulator
    memcpy(acc->colors[side], finnyEntry->colors[side], sizeof(acc->colors[side]));
}

template<Color side>
void NNUE::incrementallyUpdateAccumulator(Accumulator* inputAcc, Accumulator* outputAcc, KingBucketInfo* kingBucket) {
    // Incrementally update all the dirty pieces
    // Input and output are the same king bucket so it's all good
    for (int dp = 0; dp < outputAcc->numDirtyPieces; dp++) {
        DirtyPiece dirtyPiece = outputAcc->dirtyPieces[dp];

        if (dirtyPiece.origin == NO_SQUARE) {
            addPieceToAccumulator<side>(inputAcc->colors, outputAcc->colors, kingBucket, dirtyPiece.target, dirtyPiece.piece, dirtyPiece.pieceColor);
        }
        else if (dirtyPiece.target == NO_SQUARE) {
            removePieceFromAccumulator<side>(inputAcc->colors, outputAcc->colors, kingBucket, dirtyPiece.origin, dirtyPiece.piece, dirtyPiece.pieceColor);
        }
        else {
            movePieceInAccumulator<side>(inputAcc->colors, outputAcc->colors, kingBucket, dirtyPiece.origin, dirtyPiece.target, dirtyPiece.piece, dirtyPiece.pieceColor);
        }

        // After the input was used to calculate the next accumulator, that accumulator updates itself for the rest of the dirtyPieces
        inputAcc = outputAcc;
    }
}

template<Color side>
void NNUE::addPieceToAccumulator(int16_t(*inputData)[L1_SIZE], int16_t(*outputData)[L1_SIZE], KingBucketInfo* kingBucket, Square square, Piece piece, Color pieceColor) {
    // Get the index of the piece for this color in the input layer
    int weightOffset = getFeatureOffset(side, piece, pieceColor, square, kingBucket);

    VecI* inputVec = (VecI*)inputData[side];
    VecI* outputVec = (VecI*)outputData[side];
    VecI* weightsVec = (VecI*)networkData.inputWeights[kingBucket->index];

    // The number of iterations to compute the hidden layer depends on the size of the vector registers
    for (int i = 0; i < L1_ITERATIONS; ++i)
        outputVec[i] = addEpi16(inputVec[i], weightsVec[weightOffset + i]);
}

template<Color side>
void NNUE::removePieceFromAccumulator(int16_t(*inputData)[L1_SIZE], int16_t(*outputData)[L1_SIZE], KingBucketInfo* kingBucket, Square square, Piece piece, Color pieceColor) {
    // Get the index of the piece for this color in the input layer
    int weightOffset = getFeatureOffset(side, piece, pieceColor, square, kingBucket);

    VecI* inputVec = (VecI*)inputData[side];
    VecI* outputVec = (VecI*)outputData[side];
    VecI* weightsVec = (VecI*)networkData.inputWeights[kingBucket->index];

    // The number of iterations to compute the hidden layer depends on the size of the vector registers
    for (int i = 0; i < L1_ITERATIONS; ++i)
        outputVec[i] = subEpi16(inputVec[i], weightsVec[weightOffset + i]);
}

template<Color side>
void NNUE::movePieceInAccumulator(int16_t(*inputData)[L1_SIZE], int16_t(*outputData)[L1_SIZE], KingBucketInfo* kingBucket, Square origin, Square target, Piece piece, Color pieceColor) {
    // Get the index of the piece squares for this color in the input layer
    int subtractWeightOffset = getFeatureOffset(side, piece, pieceColor, origin, kingBucket);
    int addWeightOffset = getFeatureOffset(side, piece, pieceColor, target, kingBucket);

    VecI* inputVec = (VecI*)inputData[side];
    VecI* outputVec = (VecI*)outputData[side];
    VecI* weightsVec = (VecI*)networkData.inputWeights[kingBucket->index];

    // The number of iterations to compute the hidden layer depends on the size of the vector registers
    for (int i = 0; i < L1_ITERATIONS; ++i) {
        outputVec[i] = subEpi16(addEpi16(inputVec[i], weightsVec[addWeightOffset + i]), weightsVec[subtractWeightOffset + i]);
    }
}

Eval NNUE::evaluate(Board* board) {
    assert(currentAccumulator >= lastCalculatedAccumulator[Color::WHITE] && currentAccumulator >= lastCalculatedAccumulator[Color::BLACK]);

    // Make sure the current accumulators are up to date
    calculateAccumulators<Color::WHITE>();
    calculateAccumulators<Color::BLACK>();

    assert(currentAccumulator == lastCalculatedAccumulator[Color::WHITE] && currentAccumulator == lastCalculatedAccumulator[Color::BLACK]);

    // Calculate output bucket based on piece count
    int pieceCount = BB::popcount(board->byColor[Color::WHITE] | board->byColor[Color::BLACK]);
    constexpr int divisor = ((32 + OUTPUT_BUCKETS - 1) / OUTPUT_BUCKETS);
    int bucket = (pieceCount - 2) / divisor;
    assert(0 <= bucket && bucket < OUTPUT_BUCKETS);

    Accumulator* accumulator = &accumulatorStack[currentAccumulator];

    VecI* stmAcc = reinterpret_cast<VecI*>(accumulator->colors[board->stm]);
    VecI* oppAcc = reinterpret_cast<VecI*>(accumulator->colors[1 - board->stm]);

    alignas(ALIGNMENT) uint8_t l1Neurons[L1_SIZE];
    VecI i16Zero = set1Epi16(0);
    VecI i16Quant = set1Epi16(INPUT_QUANT);

    VecI* l1NeuronsVec = reinterpret_cast<VecI*>(l1Neurons);

    constexpr int inverseShift = 16 - INPUT_SHIFT;
    constexpr int pairwiseOffset = L1_SIZE / I16_VEC_SIZE / 2;
    for (int l1 = 0; l1 < pairwiseOffset; l1 += 2) {
        // STM
        VecI clipped1 = minEpi16(maxEpi16(stmAcc[l1], i16Zero), i16Quant);
        VecI clipped2 = minEpi16(maxEpi16(stmAcc[l1 + pairwiseOffset], i16Zero), i16Quant);
        VecI shift = slliEpi16(clipped1, inverseShift);
        VecI mul1 = mulhiEpi16(shift, clipped2);

        clipped1 = minEpi16(maxEpi16(stmAcc[l1 + 1], i16Zero), i16Quant);
        clipped2 = minEpi16(maxEpi16(stmAcc[l1 + 1 + pairwiseOffset], i16Zero), i16Quant);
        shift = slliEpi16(clipped1, inverseShift);
        VecI mul2 = mulhiEpi16(shift, clipped2);

        l1NeuronsVec[l1 / 2] = packusEpi16(mul1, mul2);

        // NSTM
        clipped1 = minEpi16(maxEpi16(oppAcc[l1], i16Zero), i16Quant);
        clipped2 = minEpi16(maxEpi16(oppAcc[l1 + pairwiseOffset], i16Zero), i16Quant);
        shift = slliEpi16(clipped1, inverseShift);
        mul1 = mulhiEpi16(shift, clipped2);

        clipped1 = minEpi16(maxEpi16(oppAcc[l1 + 1], i16Zero), i16Quant);
        clipped2 = minEpi16(maxEpi16(oppAcc[l1 + 1 + pairwiseOffset], i16Zero), i16Quant);
        shift = slliEpi16(clipped1, inverseShift);
        mul2 = mulhiEpi16(shift, clipped2);

        l1NeuronsVec[l1 / 2 + pairwiseOffset / 2] = packusEpi16(mul1, mul2);
    }
    // for (int l1 = 0; l1 < L1_SIZE / 2; l1++) {
    //     int16_t stmClipped1 = std::clamp(static_cast<int>(stmAcc[l1]), 0, INPUT_QUANT);
    //     int16_t stmClipped2 = std::clamp(static_cast<int>(stmAcc[l1 + L1_SIZE / 2]), 0, INPUT_QUANT);
    //     l1Neurons[l1] = (stmClipped1 * stmClipped2) >> INPUT_SHIFT;

    //     int16_t oppClipped1 = std::clamp(static_cast<int>(oppAcc[l1]), 0, INPUT_QUANT);
    //     int16_t oppClipped2 = std::clamp(static_cast<int>(oppAcc[l1 + L1_SIZE / 2]), 0, INPUT_QUANT);
    //     l1Neurons[l1 + L1_SIZE / 2] = (oppClipped1 * oppClipped2) >> INPUT_SHIFT;
    // }

    alignas(ALIGNMENT) int l2Neurons[L2_SIZE] = {};
#if defined(__SSSE3__) || defined(__AVX2__) || (defined(__AVX512F__) && defined(__AVX512BW__))
    int* l1Packs = reinterpret_cast<int*>(l1Neurons);

    for (int l1 = 0; l1 < L1_SIZE; l1 += INT8_PER_INT32) {
        VecI u8 = set1Epi32(l1Packs[l1 / INT8_PER_INT32]);
        VecI* weights = reinterpret_cast<VecI*>(&networkData.l1Weights[bucket][l1 * L2_SIZE]);
        VecI* l2NeuronsVec = reinterpret_cast<VecI*>(l2Neurons);
        for (int l2 = 0; l2 < L2_SIZE / I32_VEC_SIZE; l2++) {
            l2NeuronsVec[l2] = dpbusdEpi32(l2NeuronsVec[l2], u8, weights[l2]);
        }
    }
#else
    for (int l1 = 0; l1 < L1_SIZE; l1++) {
        for (int l2 = 0; l2 < L2_SIZE; l2++) {
            l2Neurons[l2] += l1Neurons[l1] * networkData.l1Weights[bucket][l1 * L2_SIZE + l2];
        }
    }
#endif

    alignas(ALIGNMENT) float l3Neurons[L3_SIZE];
    memcpy(l3Neurons, networkData.l2Biases[bucket], sizeof(l3Neurons));

#if defined(__FMA__) || defined(__AVX2__) || (defined(__AVX512F__) && defined(__AVX512BW__))
    alignas(ALIGNMENT) float l2Floats[L2_SIZE];

    VecF psNorm = set1Ps(L1_NORMALISATION);
    VecF psZero = set1Ps(0.0f);
    VecF psOne = set1Ps(1.0f);

    VecI* l2NeuronsVec = reinterpret_cast<VecI*>(l2Neurons);
    VecF* l1Biases = reinterpret_cast<VecF*>(networkData.l1Biases[bucket]);
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
        VecF* weights = reinterpret_cast<VecF*>(&networkData.l2Weights[bucket][l2 * L3_SIZE]);
        for (int l3 = 0; l3 < L3_SIZE / FLOAT_VEC_SIZE; l3++) {
            l3NeuronsVec[l3] = fmaddPs(l2Vec, weights[l3], l3NeuronsVec[l3]);
        }
    }
#else
    for (int l2 = 0; l2 < L2_SIZE; l2++) {
        float l2Result = static_cast<float>(l2Neurons[l2]) * L1_NORMALISATION + networkData.l1Biases[bucket][l2];
        float l2Activated = std::clamp(l2Result, 0.0f, 1.0f);
        l2Activated *= l2Activated;

        for (int l3 = 0; l3 < L3_SIZE; l3++) {
            l3Neurons[l3] = std::fma(l2Activated, networkData.l2Weights[bucket][l2 * L3_SIZE + l3], l3Neurons[l3]);
        }
    }
#endif

#if defined(__FMA__) || defined(__AVX2__) || (defined(__AVX512F__) && defined(__AVX512BW__))
    constexpr int chunks = 64 / sizeof(VecF);

    VecF resultSums[chunks];
    for (int i = 0; i < chunks; i++)
        resultSums[i] = psZero;

    VecF* l3WeightsVec = reinterpret_cast<VecF*>(networkData.l3Weights[bucket]);
    for (int l3 = 0; l3 < L3_SIZE / FLOAT_VEC_SIZE; l3 += chunks) {
        for (int chunk = 0; chunk < chunks; chunk++) {
            VecF l3Clipped = maxPs(minPs(l3NeuronsVec[l3 + chunk], psOne), psZero);
            VecF l3Activated = mulPs(l3Clipped, l3Clipped);
            resultSums[chunk] = fmaddPs(l3Activated, l3WeightsVec[l3 + chunk], resultSums[chunk]);
        }
    }

    float result = networkData.l3Biases[bucket] + reduceAddPs(resultSums);
#else
    constexpr int chunks = sizeof(VecF) / sizeof(float);
    float resultSums[chunks] = {};

    for (int l3 = 0; l3 < L3_SIZE; l3 += chunks) {
        for (int chunk = 0; chunk < chunks; chunk++) {
            float l3Activated = std::clamp(l3Neurons[l3 + chunk], 0.0f, 1.0f);
            float l3Squared = l3Activated * l3Activated;
            resultSums[chunk] = std::fma(l3Squared, networkData.l3Weights[bucket][l3 + chunk], resultSums[chunk]);
        }
    }

    float result = networkData.l3Biases[bucket] + reduceAddPsR(resultSums, chunks);
#endif

    return result * NETWORK_SCALE;
}