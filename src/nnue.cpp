#include <iostream>
#include <fstream>
#include <string.h>
#include <cassert>

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

// #if defined(__AVX2__) && false
//     for (int l1 = 0; l1 < L1_SIZE / 2; l1 += 2 * sizeof(__m256i) / sizeof(int16_t)) {
//         // STM
//         __m256i clipped1 = _mm256_min_epi16(_mm256_max_epi16(*((__m256i*) & stmAcc[l1]), _mm256_set1_epi16(0)), _mm256_set1_epi16(INPUT_QUANT));
//         __m256i clipped2 = _mm256_min_epi16(_mm256_max_epi16(*((__m256i*) & stmAcc[l1 + L1_SIZE / 2]), _mm256_set1_epi16(0)), _mm256_set1_epi16(INPUT_QUANT));
//         __m256i shift = _mm256_slli_epi16(clipped1, 16 - INPUT_SHIFT);
//         __m256i mul1 = _mm256_mulhi_epi16(shift, clipped2);

//         clipped1 = _mm256_min_epi16(_mm256_max_epi16(*((__m256i*) & stmAcc[l1 + sizeof(__m256i) / sizeof(int16_t)]), _mm256_set1_epi16(0)), _mm256_set1_epi16(INPUT_QUANT));
//         clipped2 = _mm256_min_epi16(_mm256_max_epi16(*((__m256i*) & stmAcc[l1 + sizeof(__m256i) / sizeof(int16_t) + L1_SIZE / 2]), _mm256_set1_epi16(0)), _mm256_set1_epi16(INPUT_QUANT));
//         shift = _mm256_slli_epi16(clipped1, 16 - INPUT_SHIFT);
//         __m256i mul2 = _mm256_mulhi_epi16(shift, clipped2);

//         __m256i u8s = _mm256_packus_epi16(mul1, mul2);
//         u8s = _mm256_permute4x64_epi64(u8s, _MM_SHUFFLE(3, 1, 2, 0));
//         _mm256_store_si256((__m256i*) & l1Neurons[l1], u8s);

//         // NSTM
//         clipped1 = _mm256_min_epi16(_mm256_max_epi16(*((__m256i*) & oppAcc[l1]), _mm256_set1_epi16(0)), _mm256_set1_epi16(INPUT_QUANT));
//         clipped2 = _mm256_min_epi16(_mm256_max_epi16(*((__m256i*) & oppAcc[l1 + L1_SIZE / 2]), _mm256_set1_epi16(0)), _mm256_set1_epi16(INPUT_QUANT));
//         shift = _mm256_slli_epi16(clipped1, 16 - INPUT_SHIFT);
//         mul1 = _mm256_mulhi_epi16(shift, clipped2);

//         clipped1 = _mm256_min_epi16(_mm256_max_epi16(*((__m256i*) & oppAcc[l1 + sizeof(__m256i) / sizeof(int16_t)]), _mm256_set1_epi16(0)), _mm256_set1_epi16(INPUT_QUANT));
//         clipped2 = _mm256_min_epi16(_mm256_max_epi16(*((__m256i*) & oppAcc[l1 + sizeof(__m256i) / sizeof(int16_t) + L1_SIZE / 2]), _mm256_set1_epi16(0)), _mm256_set1_epi16(INPUT_QUANT));
//         shift = _mm256_slli_epi16(clipped1, 16 - INPUT_SHIFT);
//         mul2 = _mm256_mulhi_epi16(shift, clipped2);

//         u8s = _mm256_packus_epi16(mul1, mul2);
//         u8s = _mm256_permute4x64_epi64(u8s, _MM_SHUFFLE(3, 1, 2, 0));
//         _mm256_store_si256((__m256i*) & l1Neurons[l1 + L1_SIZE / 2], u8s);
//     }
// #else
    for (int l1 = 0; l1 < L1_SIZE / 2; l1++) {
        int16_t stmClipped1 = std::clamp(static_cast<int>(stmAcc[l1]), 0, INPUT_QUANT);
        int16_t stmClipped2 = std::clamp(static_cast<int>(stmAcc[l1 + L1_SIZE / 2]), 0, INPUT_QUANT);
        l1Neurons[l1] = (stmClipped1 * stmClipped2) >> INPUT_SHIFT;

        int16_t oppClipped1 = std::clamp(static_cast<int>(oppAcc[l1]), 0, INPUT_QUANT);
        int16_t oppClipped2 = std::clamp(static_cast<int>(oppAcc[l1 + L1_SIZE / 2]), 0, INPUT_QUANT);
        l1Neurons[l1 + L1_SIZE / 2] = (oppClipped1 * oppClipped2) >> INPUT_SHIFT;
    }
// #endif

    alignas(ALIGNMENT) int l2Neurons[L2_SIZE] = {};
    // for (int l1 = 0; l1 < L1_SIZE; l1 += 1) {
    //     for (int l2 = 0; l2 < L2_SIZE; l2 += 256 / 32) {
    //         int u8s = int(l1Neurons[l1]); // *((int*) &l1Neurons[l1]);
    //         u8s = (u8s << 24) | (u8s << 16) | (u8s << 8) | (u8s);
    //         __m256i tmp = _mm256_maddubs_epi16(_mm256_set1_epi32(u8s), *((__m256i*) &networkData.l1Weights[bucket][l1 * L2_SIZE + INT8_PER_INT32 * l2]));
    //         __m256i sum = _mm256_madd_epi16(tmp, _mm256_set1_epi16(1));

    //         //for (int i = 0; i < 32; i++)
    //         //    std::cout << std::to_string(((int8_t*) &(*((__m256i*) &networkData.l1Weights[bucket][l1 * L2_SIZE + INT8_PER_INT32 * l2])))[i]) << " ";

    //         *((__m256i*) &l2Neurons[l2]) = _mm256_add_epi32(*((__m256i*) &l2Neurons[l2]), sum);


    //         // *((__m256i*) &l2Neurons[l2 * 8]) = _mm256_dpbusd_avx_epi32(*((__m256i*) &l2Neurons[l2 * 8]), *((__m256i*) &l1Neurons[l1 * 32]), *((__m256i*) &networkData.l1Weights[bucket][l2 * L1_SIZE * 8 + l1 * 32]));
    //     }
    // }

    for (int l1 = 0; l1 < L1_SIZE; l1++) {
        for (int l2 = 0; l2 < L2_SIZE; l2++) {
            l2Neurons[l2] += l1Neurons[l1] * networkData.l1Weights[bucket][l1 * L2_SIZE + l2];
        }
    }

    alignas(ALIGNMENT) float l3Neurons[L3_SIZE];
    memcpy(l3Neurons, networkData.l2Biases[bucket], sizeof(l3Neurons));

// #if defined(__AVX2__) && false
//     alignas(ALIGNMENT) float l2Floats[L2_SIZE];

//     for (int l2 = 0; l2 < L2_SIZE / FLOAT_VEC_SIZE; l2++) {
//         __m256 converted = _mm256_cvtepi32_ps(*((__m256i*) & l2Neurons[l2 * FLOAT_VEC_SIZE]));
//         __m256 l2Result = _mm256_add_ps(_mm256_mul_ps(converted, _mm256_set1_ps(L1_NORMALISATION)), *((__m256*) & networkData.l1Biases[bucket][l2 * FLOAT_VEC_SIZE]));
//         __m256 l2Clipped = _mm256_max_ps(_mm256_min_ps(l2Result, _mm256_set1_ps(1.0f)), _mm256_set1_ps(0.0f));
//         *((__m256*) & l2Floats[l2 * FLOAT_VEC_SIZE]) = _mm256_mul_ps(l2Clipped, l2Clipped);
//     }

//     for (int l2 = 0; l2 < L2_SIZE; l2++) {
//         for (int l3 = 0; l3 < L3_SIZE / FLOAT_VEC_SIZE; l3++) {
//             *((__m256*) & l3Neurons[l3 * FLOAT_VEC_SIZE]) = _mm256_fmadd_ps(_mm256_set1_ps(l2Floats[l2]), *((__m256*) & networkData.l2Weights[bucket][l2 * L3_SIZE + l3 * FLOAT_VEC_SIZE]), *((__m256*) & l3Neurons[l3 * FLOAT_VEC_SIZE]));
//         }
//     }
// #else
    for (int l2 = 0; l2 < L2_SIZE; l2++) {
        float l2Result = static_cast<float>(l2Neurons[l2]) * L1_NORMALISATION + networkData.l1Biases[bucket][l2];
        float l2Activated = std::clamp(l2Result, 0.0f, 1.0f);
        l2Activated *= l2Activated;

        for (int l3 = 0; l3 < L3_SIZE; l3++) {
            l3Neurons[l3] += l2Activated * networkData.l2Weights[bucket][l2 * L3_SIZE + l3];
        }
    }
// #endif

    for (int i = 0; i < L2_SIZE; i++)
        std::cout << *((int*) &l2Neurons[i]) << " ";
    std::cout << std::endl << std::endl << std::endl << std::endl;

    // 66 1294 2944 0 3476 0 1476 2287 313 825 0 0 2077 3163 0 0
    // 66 1294 2944 0 3476 0 1476 2287 313 825 0 0 2077 3163 0 0

    // 1055119054 1060285853 1053711288 -1121996270 -1215164206 -1150172672 -1095290714 1046129120 1030338720 1059754784 1043938942 1058491193 -1122227520 -1089983462 -1098573080 1056501545 -1218708194 1055021696 1054095500 1058434654 -1100760364 1056971698 1041884441 1066270172 -1130091800 1058875571 -1152689344 -1126606617 -1220413868 1021462592 1057635198 1060871125
    // 1055119054 1060285853 1053711288 -1121996270 -1215164206 -1150172672 -1095290716 1046129121 1030338720 1059754784 1043938940 1058491194 -1122227536 -1089983462 -1098573080 1056501545 -1218708194 1055021696 1054095500 1058434654 -1100760364 1056971698 1041884442 1066270172 -1130091800 1058875572 -1152689472 -1126606624 -1220413868 1021462592 1057635198 1060871125

#if defined(__AVX512F__) && defined(__AVX512BW__)
    __m512 resultSums[1];
    resultSums[0] = _mm512_set1_ps(0.0f);

    for (int l3 = 0; l3 < L3_SIZE / (sizeof(__m512) / sizeof(float)); l3++) {
        __m512 l3Clipped = _mm512_max_ps(_mm512_min_ps(*((__m512*)&l3Neurons[l3 * 2 * FLOAT_VEC_SIZE]), _mm512_set1_ps(1.0f)), _mm512_set1_ps(0.0f));
        __m512 l3Activated = _mm512_mul_ps(l3Clipped, l3Clipped);
        // for (int i = 0; i < 16; i++)
        //     std::cout << std::to_string(((int*)resultSums)[i]) << " ";
        // std::cout << std::endl;
        resultSums[0] = _mm512_fmadd_ps(l3Activated, *((__m512*)&networkData.l3Weights[bucket][l3 * 2 * FLOAT_VEC_SIZE]), resultSums[0]);
        // for (int i = 0; i < 16; i++)
        //     std::cout << std::to_string(((int*)resultSums)[i]) << " ";
        // std::cout << std::endl;
        for (int i = 0; i < 16; i++)
            std::cout << std::to_string(((int*)&l3Neurons[l3 * 2 * FLOAT_VEC_SIZE])[i]) << " ";
        std::cout << std::endl;
    }
    /*
    0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
    -1113241193 1041988499 -1111398094 0 0 0 0 1027782587 -1151759439 1040584681 -1120895139 -1107214903 0 0 0 1040390274
    -1113241193 1041988499 -1111398094 0 0 0 0 1027782587 -1151759439 1040584681 -1120895139 -1107214903 0 0 0 1040390274
    -1113241193 1032152916 1031080608 -1105374148 0 1036308773 -1131935051 -1109907012 -1151759439 -1107455700 -1120895139 -1107214903 0 977431842 1035921954 1052688518
    */

    /*
    0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 
    -1113241193 1041988499 -1111398094 0 0 0 0 1027782589 -1151759439 1040584681 -1120895143 -1107214901 0 0 0 1040390274 
    -1113241193 1041988499 -1111398094 0 0 0 0 1027782589 -1151759439 1040584681 -1120895143 -1107214901 0 0 0 1040390274 
    -1113241193 1032152916 1031080608 -1105374148 0 1036308773 -1131935050 -1109907012 -1151759439 -1107455690 -1120895143 -1107214901 0 977431842 1035921954 1052688518
    */

    // for (int i = 0; i < 16; i++)
    //   std::cout << std::to_string(((int*)resultSums)[i]) << " ";
    std::cout << std::endl << std::endl;

    float result = networkData.l3Biases[bucket] + _mm512_reduce_add_ps(resultSums[0]); // 1792885

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
    for (int i = 0; i < 16; i++)
        std::cout << std::to_string(((int*)resultSums)[i]) << " ";
    std::cout << std::endl;
    resultSums[0] = _mm256_add_ps(resultSums[0], resultSums[1]);
    for (int i = 0; i < 8; i++)
        std::cout << std::to_string(((int*)&resultSums[0])[i]) << " ";
    std::cout << std::endl;

    // -1113241193 1032152916 1031080608 -1105374148 0 1036308773 -1131935051 -1109907012 -1151759439 -1107455700 -1120895139 -1107214903 0 977431842 1035921954 1052688518

    __m128 high = _mm256_extractf128_ps(resultSums[0], 1);
    __m128 low = _mm256_castps256_ps128(resultSums[0]);
    __m128 sum = _mm_add_ps(high, low);
    for (int i = 0; i < 4; i++)
        std::cout << std::to_string(((int*)&sum)[i]) << " ";
    std::cout << std::endl;
    __m128 high64 = _mm_movehl_ps(sum, sum);
    __m128 sum64 = _mm_add_ps(sum, high64);
    for (int i = 0; i < 2; i++)
        std::cout << std::to_string(((int*)&sum64)[i]) << " ";
    std::cout << std::endl;
    /*
    -1113241193 1032152916 1031080608 -1105374148 0 1036308773 -1131935051 -1109907012 -1151759439 -1107455700 -1120895139 -1107214903 0 977431842 1035921954 1052688518
    -1112795659 -1116712016 1015617158 -1097905918 0 1036408319 1033693045 1049146903
    -1112795659 1025267790 1035939094 -1135452832
    1008253192 1020232172
    19
    */
    float result = networkData.l3Biases[bucket] + ((float*)&sum64)[0] + ((float*)&sum64)[1]; // 1792885
#else
    constexpr int chunks = sizeof(__m512) / sizeof(float);

    float resultSums[chunks] = {};
    for (int l3 = 0; l3 < L3_SIZE; l3 += chunks) {
        // for (int i = 0; i < 16; i++)
        //     std::cout << std::to_string(((int*)resultSums)[i]) << " ";
        // std::cout << std::endl;
        for (int chunk = 0; chunk < chunks; chunk++) {
            float l3Activated = std::clamp(l3Neurons[l3 + chunk], 0.0f, 1.0f);
            float l3Squared = l3Activated * l3Activated;
            std::cout << *((int*) &l3Neurons[l3 + chunk]) << " " << *((int*) &l3Activated) << " " << *((int*) &l3Squared) << " " << *((int*) &networkData.l3Weights[bucket][l3 + chunk]) << "\t";
            resultSums[chunk] += l3Squared * networkData.l3Weights[bucket][l3 + chunk]; // 1872511
        }
        std::cout << std::endl;
        // for (int i = 0; i < 16; i++)
        //     std::cout << std::to_string(((int*)resultSums)[i]) << " ";
        // std::cout << std::endl;
    }
    std::cout << std::endl << std::endl;

    /*
    1055119054 1060285853 1053711288 -1121996270 -1215164206 -1150172672 -1095290714 1046129120 1030338720 1059754784 1043938942 1058491193 -1122227520 -1089983462 -1098573080 1056501545 
    -1218708194 1055021696 1054095500 1058434654 -1100760364 1056971698 1041884441 1066270172 -1130091800 1058875571 -1152689344 -1126606617 -1220413868 1021462592 1057635198 1060871125
    */

    /*
    1055119054 1055119054 1045087910 -1093620753
    1060285853 1060285853 1056533448 1050645881     
    1053711288 1053711288 1042700221 -1089154000    
    -1121996270 0 0 -1087303386     
    -1215164206 0 0 -1229327317     
    -1150172672 0 0 1060334188   
    -1095290716 0 0 1065654282      
    1046129121 1046129121 1027261892 1065710063     
    1030338720 1030338720 995451288 -1081966716     
    1059754784 1059754784 1055084406 1050082186     
    1043938940 1043938940 1023806302 -10794736861058491194 1058491194 1051906985 -1095171057     
    -1122227536 0 0 1056620974      
    -1089983462 0 0 -1088318171     
    -1098573080 0 0 1073571098      
    1056501545 1056501545 1047662655 1057662136
    -1218708194 0 0 908532093       
    1055021696 1055021696 1044915178 -1092411734    
    1054095500 1054095500 1043328436 1063640225     
    1058434654 1058434654 1051773707 -1092366133    
    -1100760364 0 0 -1073913917     
    1056971698 1056971698 1048590186 1053064213  
    1041884442 1041884442 1018758988 -1086572233    
    1066270172 1065353216 1065353216 -1105411379    
    -1130091800 0 0 1067733633      
    1058875572 1058875572 1052833255 -1087567855    
    -1152689472 0 0 1061531094  
    -1126606624 0 0 -1079698266      
    -1220413868 0 0 902482719       
    1021462592 1021462592 977798053 1064884499      
    1057635198 1057635198 1049970787 1050915406     
    1060871125 1060871125 1057586440 1055465608
    */

    /*
    -1113241193 1032152916 1031080608 -1105374148 0 1036308773 -1131935050 -1109907012 -1151759439 -1107455690 -1120895143 -1107214901 0 977431842 1035921954 1052688518
    -1112795659 -1116711996 1015617166 -1097905916 0 1036408319 1033693044 1049146903
    -1112795659 1025267770 1035939096 -1135452768
    1008253208 1020232100
    19
    */

    float result = networkData.l3Biases[bucket] + vecReduceAddPs(resultSums, chunks);
#endif

    // float result = networkData.l3Biases[bucket];
    // for (int l3 = 0; l3 < L3_SIZE; l3++) {
    //     float l3Activated = std::clamp(l3Neurons[l3], 0.0f, 1.0f);
    //     l3Activated *= l3Activated;
    //     result += l3Activated * networkData.l3Weights[bucket][l3];
    // }
    // TODO reduce add

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