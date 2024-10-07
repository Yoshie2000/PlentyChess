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

    // Transpose L1 / L2 / L3 weights
    for (int b = 0; b < OUTPUT_BUCKETS; b++) {
        for (int l1 = 0; l1 < L1_SIZE; l1++) {
            for (int l2 = 0; l2 < L2_SIZE; l2++) {
                networkData.l1Weights[b][l2 * L1_SIZE + l1] = reinterpret_cast<int8_t*>(incNetwork->l1Weights)[l1 * OUTPUT_BUCKETS * L2_SIZE + b * L2_SIZE + l2];
            }
        }
        for (int l1 = 0; l1 < L1_SIZE; l1++) {
            for (int l2 = 0; l2 < L2_SIZE; l2++) {
                networkData.l1Weights[b][l1 * L2_SIZE + l2] = reinterpret_cast<int8_t*>(incNetwork->l1Weights)[l1 * OUTPUT_BUCKETS * L2_SIZE + b * L2_SIZE + l2];
            }
        }
        // for (int l1 = 0; l1 < L1_SIZE / INT8_PER_INT32; l1++) {
        //     for (int l2 = 0; l2 < L2_SIZE; l2++) {
        //         for (int c = 0; c < INT8_PER_INT32; c++) {
        //             networkData.l1Weights[b][l1 * INT8_PER_INT32 * L2_SIZE + l2 * INT8_PER_INT32 + c] = reinterpret_cast<int8_t*>(incNetwork->l1Weights)[(l1 * INT8_PER_INT32 + c) * OUTPUT_BUCKETS * L2_SIZE + b * L2_SIZE + l2];
        //         }
        //     }
        // }

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

    Vec* inputVec = (Vec*)inputData[side];
    Vec* outputVec = (Vec*)outputData[side];
    Vec* weightsVec = (Vec*)networkData.inputWeights[kingBucket->index];

    // The number of iterations to compute the hidden layer depends on the size of the vector registers
    for (int i = 0; i < L1_ITERATIONS; ++i)
        outputVec[i] = addEpi16(inputVec[i], weightsVec[weightOffset + i]);
}

template<Color side>
void NNUE::removePieceFromAccumulator(int16_t(*inputData)[L1_SIZE], int16_t(*outputData)[L1_SIZE], KingBucketInfo* kingBucket, Square square, Piece piece, Color pieceColor) {
    // Get the index of the piece for this color in the input layer
    int weightOffset = getFeatureOffset(side, piece, pieceColor, square, kingBucket);

    Vec* inputVec = (Vec*)inputData[side];
    Vec* outputVec = (Vec*)outputData[side];
    Vec* weightsVec = (Vec*)networkData.inputWeights[kingBucket->index];

    // The number of iterations to compute the hidden layer depends on the size of the vector registers
    for (int i = 0; i < L1_ITERATIONS; ++i)
        outputVec[i] = subEpi16(inputVec[i], weightsVec[weightOffset + i]);
}

template<Color side>
void NNUE::movePieceInAccumulator(int16_t(*inputData)[L1_SIZE], int16_t(*outputData)[L1_SIZE], KingBucketInfo* kingBucket, Square origin, Square target, Piece piece, Color pieceColor) {
    // Get the index of the piece squares for this color in the input layer
    int subtractWeightOffset = getFeatureOffset(side, piece, pieceColor, origin, kingBucket);
    int addWeightOffset = getFeatureOffset(side, piece, pieceColor, target, kingBucket);

    Vec* inputVec = (Vec*)inputData[side];
    Vec* outputVec = (Vec*)outputData[side];
    Vec* weightsVec = (Vec*)networkData.inputWeights[kingBucket->index];

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
    // constexpr int divisor = ((32 + OUTPUT_BUCKETS - 1) / OUTPUT_BUCKETS);
    int bucket = std::min((63 - pieceCount) * (32 - pieceCount) / 225, 7);
    assert(0 <= bucket && bucket < OUTPUT_BUCKETS);

    Accumulator* accumulator = &accumulatorStack[currentAccumulator];

    int16_t* stmAcc = accumulator->colors[board->stm];
    int16_t* oppAcc = accumulator->colors[1 - board->stm];

    alignas(ALIGNMENT) uint8_t l1Neurons[L1_SIZE];

#if defined(__AVX2__)
    for (int l1 = 0; l1 < L1_SIZE / 2; l1 += 2 * sizeof(__m256i) / sizeof(int16_t)) {
        // STM
        __m256i clipped1 = _mm256_min_epi16(_mm256_max_epi16(*((__m256i*) & stmAcc[l1]), _mm256_set1_epi16(0)), _mm256_set1_epi16(INPUT_QUANT));
        __m256i clipped2 = _mm256_min_epi16(_mm256_max_epi16(*((__m256i*) & stmAcc[l1 + L1_SIZE / 2]), _mm256_set1_epi16(0)), _mm256_set1_epi16(INPUT_QUANT));
        __m256i shift = _mm256_slli_epi16(clipped1, 16 - INPUT_SHIFT);
        __m256i mul1 = _mm256_mulhi_epi16(shift, clipped2);

        clipped1 = _mm256_min_epi16(_mm256_max_epi16(*((__m256i*) & stmAcc[l1 + sizeof(__m256i) / sizeof(int16_t)]), _mm256_set1_epi16(0)), _mm256_set1_epi16(INPUT_QUANT));
        clipped2 = _mm256_min_epi16(_mm256_max_epi16(*((__m256i*) & stmAcc[l1 + sizeof(__m256i) / sizeof(int16_t) + L1_SIZE / 2]), _mm256_set1_epi16(0)), _mm256_set1_epi16(INPUT_QUANT));
        shift = _mm256_slli_epi16(clipped1, 16 - INPUT_SHIFT);
        __m256i mul2 = _mm256_mulhi_epi16(shift, clipped2);

        __m256i u8s = _mm256_packus_epi16(mul1, mul2);
        u8s = _mm256_permute4x64_epi64(u8s, _MM_SHUFFLE(3, 1, 2, 0));
        _mm256_store_si256((__m256i*) & l1Neurons[l1], u8s);

        // NSTM
        clipped1 = _mm256_min_epi16(_mm256_max_epi16(*((__m256i*) & oppAcc[l1]), _mm256_set1_epi16(0)), _mm256_set1_epi16(INPUT_QUANT));
        clipped2 = _mm256_min_epi16(_mm256_max_epi16(*((__m256i*) & oppAcc[l1 + L1_SIZE / 2]), _mm256_set1_epi16(0)), _mm256_set1_epi16(INPUT_QUANT));
        shift = _mm256_slli_epi16(clipped1, 16 - INPUT_SHIFT);
        mul1 = _mm256_mulhi_epi16(shift, clipped2);

        clipped1 = _mm256_min_epi16(_mm256_max_epi16(*((__m256i*) & oppAcc[l1 + sizeof(__m256i) / sizeof(int16_t)]), _mm256_set1_epi16(0)), _mm256_set1_epi16(INPUT_QUANT));
        clipped2 = _mm256_min_epi16(_mm256_max_epi16(*((__m256i*) & oppAcc[l1 + sizeof(__m256i) / sizeof(int16_t) + L1_SIZE / 2]), _mm256_set1_epi16(0)), _mm256_set1_epi16(INPUT_QUANT));
        shift = _mm256_slli_epi16(clipped1, 16 - INPUT_SHIFT);
        mul2 = _mm256_mulhi_epi16(shift, clipped2);

        u8s = _mm256_packus_epi16(mul1, mul2);
        u8s = _mm256_permute4x64_epi64(u8s, _MM_SHUFFLE(3, 1, 2, 0));
        _mm256_store_si256((__m256i*) & l1Neurons[l1 + L1_SIZE / 2], u8s);
    }
#else
    for (int l1 = 0; l1 < L1_SIZE / 2; l1++) {
        int16_t stmClipped1 = std::clamp(static_cast<int>(stmAcc[l1]), 0, INPUT_QUANT);
        int16_t stmClipped2 = std::clamp(static_cast<int>(stmAcc[l1 + L1_SIZE / 2]), 0, INPUT_QUANT);
        l1Neurons[l1] = (stmClipped1 * stmClipped2) >> INPUT_SHIFT;

        int16_t oppClipped1 = std::clamp(static_cast<int>(oppAcc[l1]), 0, INPUT_QUANT);
        int16_t oppClipped2 = std::clamp(static_cast<int>(oppAcc[l1 + L1_SIZE / 2]), 0, INPUT_QUANT);
        l1Neurons[l1 + L1_SIZE / 2] = (oppClipped1 * oppClipped2) >> INPUT_SHIFT;
    }
#endif

    alignas(ALIGNMENT) int l2Neurons[L2_SIZE] = {};
#if defined(__AVX2__)
    for (int l1 = 0; l1 < L1_SIZE; l1 += INT8_PER_INT32) {
        for (int l2 = 0; l2 < L2_SIZE; l2 += 256 / 32) {
            __m256i u8 = _mm256_set1_epi32(l1Neurons[l1]);
            __m256i i8 = *((__m256i*) &networkData.l1Weights[bucket][l1 * L2_SIZE + INT8_PER_INT32 * l2]);
            __m256i tmp = _mm256_maddubs_epi16(u8, i8);
            __m256i sum = _mm256_madd_epi16(tmp, _mm256_set1_epi16(1));

            // std::cout << l1 << " " << l2 << " " << (((char*) &networkData.l1Weights[bucket][l1 * L2_SIZE + INT8_PER_INT32 * l2]) - ((char*) &networkData.l1Weights[bucket])) << std::endl;
            
            *((__m256i*) &l2Neurons[l2]) = _mm256_add_epi32(*((__m256i*) &l2Neurons[l2]), sum);
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

#if defined(__AVX2__)
    alignas(ALIGNMENT) float l2Floats[L2_SIZE];

    for (int l2 = 0; l2 < L2_SIZE / FLOAT_VEC_SIZE; l2++) {
        __m256 converted = _mm256_cvtepi32_ps(*((__m256i*) & l2Neurons[l2 * FLOAT_VEC_SIZE]));
        __m256 l2Result = _mm256_add_ps(_mm256_mul_ps(converted, _mm256_set1_ps(L1_NORMALISATION)), *((__m256*) & networkData.l1Biases[bucket][l2 * FLOAT_VEC_SIZE]));
        __m256 l2Clipped = _mm256_max_ps(_mm256_min_ps(l2Result, _mm256_set1_ps(1.0f)), _mm256_set1_ps(0.0f));
        *((__m256*) & l2Floats[l2 * FLOAT_VEC_SIZE]) = _mm256_mul_ps(l2Clipped, l2Clipped);
    }

    for (int l2 = 0; l2 < L2_SIZE; l2++) {
        for (int l3 = 0; l3 < L3_SIZE / FLOAT_VEC_SIZE; l3++) {
            *((__m256*) & l3Neurons[l3 * FLOAT_VEC_SIZE]) = _mm256_fmadd_ps(_mm256_set1_ps(l2Floats[l2]), *((__m256*) & networkData.l2Weights[bucket][l2 * L3_SIZE + l3 * FLOAT_VEC_SIZE]), *((__m256*) & l3Neurons[l3 * FLOAT_VEC_SIZE]));
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

#if defined(__AVX512F__) && defined(__AVX512BW__)
    __m512 resultSums[1];
    resultSums[0] = _mm512_set1_ps(0.0f);

    for (int l3 = 0; l3 < L3_SIZE / (sizeof(__m512) / sizeof(float)); l3++) {
        __m512 l3Clipped = _mm512_max_ps(_mm512_min_ps(*((__m512*)&l3Neurons[l3 * 2 * FLOAT_VEC_SIZE]), _mm512_set1_ps(1.0f)), _mm512_set1_ps(0.0f));
        __m512 l3Activated = _mm512_mul_ps(l3Clipped, l3Clipped);
        resultSums[0] = _mm512_fmadd_ps(l3Activated, *((__m512*)&networkData.l3Weights[bucket][l3 * 2 * FLOAT_VEC_SIZE]), resultSums[0]);
    }

    float result = networkData.l3Biases[bucket] + _mm512_reduce_add_ps(resultSums[0]);

#elif defined(__AVX2__)
    __m256 resultSums[2];
    resultSums[0] = _mm256_set1_ps(0.0f);
    resultSums[1] = _mm256_set1_ps(0.0f);

    for (int l3 = 0; l3 < L3_SIZE / FLOAT_VEC_SIZE; l3 += 2) {
        __m256 l3Clipped1 = _mm256_max_ps(_mm256_min_ps(*((__m256*) & l3Neurons[l3 * FLOAT_VEC_SIZE]), _mm256_set1_ps(1.0f)), _mm256_set1_ps(0.0f));
        __m256 l3Clipped2 = _mm256_max_ps(_mm256_min_ps(*((__m256*) & l3Neurons[(l3 + 1) * FLOAT_VEC_SIZE]), _mm256_set1_ps(1.0f)), _mm256_set1_ps(0.0f));
        __m256 l3Activated1 = _mm256_mul_ps(l3Clipped1, l3Clipped1);
        __m256 l3Activated2 = _mm256_mul_ps(l3Clipped2, l3Clipped2);
        resultSums[0] = _mm256_fmadd_ps(l3Activated1, *((__m256*) & networkData.l3Weights[bucket][l3 * FLOAT_VEC_SIZE]), resultSums[0]);
        resultSums[1] = _mm256_fmadd_ps(l3Activated2, *((__m256*) & networkData.l3Weights[bucket][(l3 + 1) * FLOAT_VEC_SIZE]), resultSums[1]);
    }

    resultSums[0] = _mm256_add_ps(resultSums[0], resultSums[1]);
    __m128 high = _mm256_extractf128_ps(resultSums[0], 1);
    __m128 low = _mm256_castps256_ps128(resultSums[0]);
    __m128 sum = _mm_add_ps(high, low);
    __m128 high64 = _mm_movehl_ps(sum, sum);
    __m128 sum64 = _mm_add_ps(sum, high64);

    float result = networkData.l3Biases[bucket] + ((float*)&sum64)[0] + ((float*)&sum64)[1];
#else
    constexpr int chunks = sizeof(__m512) / sizeof(float);
    float resultSums[chunks] = {};

    for (int l3 = 0; l3 < L3_SIZE; l3 += chunks) {
        for (int chunk = 0; chunk < chunks; chunk++) {
            float l3Activated = std::clamp(l3Neurons[l3 + chunk], 0.0f, 1.0f);
            float l3Squared = l3Activated * l3Activated;
            resultSums[chunk] += l3Squared * networkData.l3Weights[bucket][l3 + chunk];
        }
    }

    float result = networkData.l3Biases[bucket] + vecReduceAddPs(resultSums, chunks);
#endif

    return result * NETWORK_SCALE;
    /*Vec* stmAcc = (Vec*)accumulator->colors[board->stm];
    Vec* oppAcc = (Vec*)accumulator->colors[1 - board->stm];

    Vec* stmWeights = (Vec*)&networkData.l1Weights[bucket][0];
    Vec* oppWeights = (Vec*)&networkData.l1Weights[bucket][L1_SIZE];

    Vec reluClipMin = vecSetZero();
    Vec reluClipMax = vecSet1Epi16(INPUT_QUANT);

    Vec sum = vecSetZero();
    Vec vec0, vec1;

    for (int i = 0; i < L1_ITERATIONS; ++i) {
        // Side to move
        vec0 = maxEpi16(stmAcc[i], reluClipMin); // clip (screlu min)
        vec0 = minEpi16(vec0, reluClipMax); // clip (screlu max)
        vec1 = mulloEpi16(vec0, stmWeights[i]); // square (screlu square)
        vec1 = maddEpi16(vec0, vec1); // multiply with output layer
        sum = addEpi32(sum, vec1); // collect the result

        // Non side to move
        vec0 = maxEpi16(oppAcc[i], reluClipMin);
        vec0 = minEpi16(vec0, reluClipMax);
        vec1 = mulloEpi16(vec0, oppWeights[i]);
        vec1 = maddEpi16(vec0, vec1);
        sum = addEpi32(sum, vec1);
    }

    int unsquared = vecHaddEpi32(sum) / INPUT_QUANT + networkData.l1Biases[bucket];

    return (Eval)((unsquared * NETWORK_SCALE) / NETWORK_QAB);*/
}