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

NetworkData* networkData;
alignas(ALIGNMENT) uint16_t nnzLookup[256][8];

#if defined(PROCESS_NET)
NNZ nnz;
#endif

void initNetworkData() {
    ThreatInputs::initialise();

    for (size_t i = 0; i < 256; i++) {
        uint64_t j = i;
        uint64_t k = 0;
        while (j) {
            nnzLookup[i][k++] = popLSB(&j);
        }
    }

    networkData = (NetworkData*)gNETWORKData;
}

void NNUE::reset(Board* board) {
    // Reset accumulator
    resetAccumulator<Color::WHITE>(board, &accumulatorStack[0]);
    resetAccumulator<Color::BLACK>(board, &accumulatorStack[0]);
    accumulatorStack[0].numDirtyPieces = 0;
    accumulatorStack[0].numDirtyThreats = 0;

    currentAccumulator = 0;
    lastCalculatedAccumulator[Color::WHITE] = 0;
    lastCalculatedAccumulator[Color::BLACK] = 0;
}

template<Color side>
__attribute_noinline__ void NNUE::resetAccumulator(Board* board, Accumulator* acc) {
    // Overwrite with biases
    memcpy(acc->threatState[side], networkData->inputBiases, sizeof(networkData->inputBiases));
    // Overwrite with zeroes
    memset(acc->pieceState[side], 0, sizeof(acc->pieceState[side]));

    ThreatInputs::FeatureList threatFeatures;
    ThreatInputs::addThreatFeatures(board->byColor, board->byPiece, board->pieces, &board->stack->threats, side, threatFeatures);
    for (int featureIndex : threatFeatures)
        addToAccumulator<side>(acc->threatState, acc->threatState, featureIndex);

    ThreatInputs::FeatureList pieceFeatures;
    ThreatInputs::addPieceFeatures(board->byColor, board->byPiece, side, pieceFeatures, KING_BUCKET_LAYOUT);
    for (int featureIndex : pieceFeatures)
        addToAccumulator<side>(acc->pieceState, acc->pieceState, featureIndex);

    acc->kingBucketInfo[side] = getKingBucket(side, lsb(board->byColor[side] & board->byPiece[Piece::KING]));
    memcpy(acc->byColor[side], board->byColor, sizeof(board->byColor));
    memcpy(acc->byPiece[side], board->byPiece, sizeof(board->byPiece));
    memcpy(acc->pieces[side], board->pieces, sizeof(board->pieces));
    memcpy(&acc->threats[side], &board->stack->threats, sizeof(board->stack->threats));
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

void NNUE::addThreat(Piece piece, Piece attackedPiece, Square square, Square attackedSquare, Color pieceColor, Color attackedColor) {
    assert(piece != Piece::NONE);
    assert(attackedPiece != Piece::NONE);

    Accumulator* acc = &accumulatorStack[currentAccumulator];
    acc->dirtyThreats[acc->numDirtyThreats++] = { piece, attackedPiece, square, attackedSquare, pieceColor, attackedColor, true };
}

void NNUE::removeThreat(Piece piece, Piece attackedPiece, Square square, Square attackedSquare, Color pieceColor, Color attackedColor) {
    assert(piece != Piece::NONE);
    assert(attackedPiece != Piece::NONE);

    Accumulator* acc = &accumulatorStack[currentAccumulator];
    acc->dirtyThreats[acc->numDirtyThreats++] = { piece, attackedPiece, square, attackedSquare, pieceColor, attackedColor, false };
}

void NNUE::incrementAccumulator() {
    currentAccumulator++;
    accumulatorStack[currentAccumulator].numDirtyPieces = 0;
    accumulatorStack[currentAccumulator].numDirtyThreats = 0;
}

void NNUE::decrementAccumulator() {
    assert(currentAccumulator > 0);
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
        memcpy(accumulator->pieces[side], board->pieces, sizeof(board->pieces));
        memcpy(&accumulator->threats[side], &board->stack->threats, sizeof(board->stack->threats));
    }
}

template<Color side>
void NNUE::calculateAccumulators(Board* board) {
    // Incrementally update all accumulators for this side
    while (lastCalculatedAccumulator[side] < currentAccumulator) {

        Accumulator* inputAcc = &accumulatorStack[lastCalculatedAccumulator[side]];
        Accumulator* outputAcc = &accumulatorStack[lastCalculatedAccumulator[side] + 1];

        KingBucketInfo* inputKingBucket = &inputAcc->kingBucketInfo[side];
        KingBucketInfo* outputKingBucket = &outputAcc->kingBucketInfo[side];

        if (inputKingBucket->mirrored != outputKingBucket->mirrored) {
            // Refresh threat features
            refreshThreatFeatures<side>(outputAcc, outputKingBucket);
        }
        else {
            // Incrementally update threat features
            ThreatInputs::FeatureList addThreatFeatureList;
            ThreatInputs::FeatureList subThreatFeatureList;
            calculateThreatFeatures<side>(outputAcc, outputKingBucket, addThreatFeatureList, subThreatFeatureList);
            applyAccumulatorUpdates<side>((VecI16*)inputAcc->threatState[side], (VecI16*)outputAcc->threatState[side], addThreatFeatureList, subThreatFeatureList);
        }

        if (inputKingBucket->bucket != outputKingBucket->bucket || inputKingBucket->mirrored != outputKingBucket->mirrored) {
            // Refresh piece features
            refreshPieceFeatures<side>(outputAcc, outputKingBucket);
        }
        else {
            // Incrementally update piece features
            ThreatInputs::FeatureList addPieceFeatureList;
            ThreatInputs::FeatureList subPieceFeatureList;
            calculatePieceFeatures<side>(outputAcc, outputKingBucket, addPieceFeatureList, subPieceFeatureList);
            applyAccumulatorUpdates<side>((VecI16*)inputAcc->pieceState[side], (VecI16*)outputAcc->pieceState[side], addPieceFeatureList, subPieceFeatureList);
        }

        lastCalculatedAccumulator[side]++;
    }
}

template<Color side>
void NNUE::refreshPieceFeatures(Accumulator* acc, KingBucketInfo* kingBucket) {
    memset(acc->pieceState[side], 0, sizeof(acc->pieceState[side]));

    ThreatInputs::FeatureList addPieceFeatureList;
    ThreatInputs::addPieceFeatures(acc->byColor[side], acc->byPiece[side], side, addPieceFeatureList, KING_BUCKET_LAYOUT);
    for (int featureIndex : addPieceFeatureList)
        addToAccumulator<side>(acc->pieceState, acc->pieceState, featureIndex);
    // for (Color c = Color::WHITE; c <= Color::BLACK; ++c) {
    //     for (Piece p = Piece::PAWN; p < Piece::TOTAL; ++p) {
    //         Bitboard bb = acc->byColor[side][c] & acc->byPiece[side][p];

    //         while (bb) {
    //             Square square = popLSB(&bb) ^ (7 * kingBucket->mirrored) ^ (56 * side);
    //             addToAccumulator<side>(acc->pieceState, acc->pieceState, ThreatInputs::getPieceFeature(p, square, static_cast<Color>(side != c), kingBucket->bucket));
    //         }
    //     }
    // }

    // FinnyEntry* finnyEntry = &finnyTable[acc->kingBucketInfo[side].mirrored][acc->kingBucketInfo[side].index];

    // // Update matching finny table with the changed pieces
    // for (Color c = Color::WHITE; c <= Color::BLACK; ++c) {
    //     for (Piece p = Piece::PAWN; p < Piece::TOTAL; ++p) {
    //         Bitboard finnyBB = finnyEntry->byColor[side][c] & finnyEntry->byPiece[side][p];
    //         Bitboard accBB = acc->byColor[side][c] & acc->byPiece[side][p];

    //         Bitboard addBB = accBB & ~finnyBB;
    //         Bitboard removeBB = ~accBB & finnyBB;

    //         while (addBB) {
    //             Square square = popLSB(&addBB);
    //             addPieceToAccumulator<side>(finnyEntry->colors, finnyEntry->colors, &acc->kingBucketInfo[side], square, p, c);
    //         }
    //         while (removeBB) {
    //             Square square = popLSB(&removeBB);
    //             removePieceFromAccumulator<side>(finnyEntry->colors, finnyEntry->colors, &acc->kingBucketInfo[side], square, p, c);
    //         }
    //     }
    // }
    // memcpy(finnyEntry->byColor[side], acc->byColor[side], sizeof(finnyEntry->byColor[side]));
    // memcpy(finnyEntry->byPiece[side], acc->byPiece[side], sizeof(finnyEntry->byPiece[side]));

    // // Copy result to the current accumulator
    // memcpy(acc->colors[side], finnyEntry->colors[side], sizeof(acc->colors[side]));
}

template<Color side>
__attribute_noinline__ void NNUE::refreshThreatFeatures(Accumulator* acc, KingBucketInfo* kingBucket) {
    // Overwrite with biases
    memcpy(acc->threatState[side], networkData->inputBiases, sizeof(networkData->inputBiases));

    ThreatInputs::FeatureList threatFeatures;
    ThreatInputs::addThreatFeatures(acc->byColor[side], acc->byPiece[side], acc->pieces[side], &acc->threats[side], side, threatFeatures);
    for (int featureIndex : threatFeatures)
        addToAccumulator<side>(acc->threatState, acc->threatState, featureIndex);
}

template<Color side>
__attribute_noinline__ void NNUE::calculatePieceFeatures(Accumulator* outputAcc, KingBucketInfo* kingBucket, ThreatInputs::FeatureList& addFeatureList, ThreatInputs::FeatureList& subFeatureList) {
    for (int dp = 0; dp < outputAcc->numDirtyPieces; dp++) {
        DirtyPiece dirtyPiece = outputAcc->dirtyPieces[dp];

        Color relativeColor = static_cast<Color>(dirtyPiece.pieceColor != side);

        if (dirtyPiece.origin == NO_SQUARE) {
            int featureIndex = ThreatInputs::getPieceFeature(dirtyPiece.piece, dirtyPiece.target ^ (56 * side) ^ (7 * kingBucket->mirrored), relativeColor, kingBucket->bucket);
            addFeatureList.add(featureIndex);
        }
        else if (dirtyPiece.target == NO_SQUARE) {
            int featureIndex = ThreatInputs::getPieceFeature(dirtyPiece.piece, dirtyPiece.origin ^ (56 * side) ^ (7 * kingBucket->mirrored), relativeColor, kingBucket->bucket);
            subFeatureList.add(featureIndex);
        }
        else {
            int subIndex = ThreatInputs::getPieceFeature(dirtyPiece.piece, dirtyPiece.origin ^ (56 * side) ^ (7 * kingBucket->mirrored), relativeColor, kingBucket->bucket);
            int addIndex = ThreatInputs::getPieceFeature(dirtyPiece.piece, dirtyPiece.target ^ (56 * side) ^ (7 * kingBucket->mirrored), relativeColor, kingBucket->bucket);
            addFeatureList.add(addIndex);
            subFeatureList.add(subIndex);
        }
    }
}

template<Color side>
__attribute_noinline__ void NNUE::calculateThreatFeatures(Accumulator* outputAcc, KingBucketInfo* kingBucket, ThreatInputs::FeatureList& addFeatureList, ThreatInputs::FeatureList& subFeatureList) {
    for (int dp = 0; dp < outputAcc->numDirtyThreats; dp++) {
        DirtyThreat dirtyThreat = outputAcc->dirtyThreats[dp];

        Piece piece = dirtyThreat.piece;
        Piece attackedPiece = dirtyThreat.attackedPiece;
        Square square = dirtyThreat.square ^ (56 * side) ^ (7 * kingBucket->mirrored);
        Square attackedSquare = dirtyThreat.attackedSquare ^ (56 * side) ^ (7 * kingBucket->mirrored);

        Color relativeSide = static_cast<Color>(side != dirtyThreat.attackedColor);
        bool enemy = dirtyThreat.pieceColor != dirtyThreat.attackedColor;
        bool hasSideOffset = dirtyThreat.pieceColor != side;

        int featureIndex = ThreatInputs::lookupThreatFeature(piece, square, attackedSquare, attackedPiece, relativeSide, enemy, hasSideOffset);

        if (featureIndex != -1) {
            if (dirtyThreat.add) {
                // int subIndex = subFeatureList.indexOf(featureIndex);
                // if (subIndex == -1)
                addFeatureList.add(featureIndex);
                // else
                //     subFeatureList.remove(subIndex);
            }
            else {
                // int addIndex = addFeatureList.indexOf(featureIndex);
                // if (addIndex == -1)
                subFeatureList.add(featureIndex);
                // else
                //     addFeatureList.remove(addIndex);
            }
        }
    }
}

template<Color side>
__attribute_noinline__ void NNUE::applyAccumulatorUpdates(VecI16* inputVec, VecI16* outputVec, ThreatInputs::FeatureList& addFeatureList, ThreatInputs::FeatureList& subFeatureList) {
    int commonEnd = std::min(addFeatureList.count(), subFeatureList.count());
    VecI16* weightsVec = (VecI16*)networkData->inputWeights;

    constexpr int UNROLL_REGISTERS = std::min(16, L1_ITERATIONS);
    VecI16 regs[UNROLL_REGISTERS];

    for (int i = 0; i < L1_ITERATIONS / UNROLL_REGISTERS; ++i) {
        int unrollOffset = i * UNROLL_REGISTERS;

        VecI16* inputs = &inputVec[unrollOffset];
        VecI16* outputs = &outputVec[unrollOffset];

        for (int j = 0; j < UNROLL_REGISTERS; j++)
            regs[j] = inputs[j];

        for (int j = 0; j < commonEnd; j++) {
            VecI16* addWeights = &weightsVec[unrollOffset + addFeatureList[j] * L1_SIZE / I16_VEC_SIZE];
            VecI16* subWeights = &weightsVec[unrollOffset + subFeatureList[j] * L1_SIZE / I16_VEC_SIZE];

            for (int k = 0; k < UNROLL_REGISTERS; k++)
                regs[k] = subEpi16(addEpi16(regs[k], addWeights[k]), subWeights[k]);
        }

        for (int j = commonEnd; j < addFeatureList.count(); j++) {
            VecI16* addWeights = &weightsVec[unrollOffset + addFeatureList[j] * L1_SIZE / I16_VEC_SIZE];

            for (int k = 0; k < UNROLL_REGISTERS; k++)
                regs[k] = addEpi16(regs[k], addWeights[k]);
        }

        for (int j = commonEnd; j < subFeatureList.count(); j++) {
            VecI16* subWeights = &weightsVec[unrollOffset + subFeatureList[j] * L1_SIZE / I16_VEC_SIZE];

            for (int k = 0; k < UNROLL_REGISTERS; k++)
                regs[k] = subEpi16(regs[k], subWeights[k]);
        }

        for (int j = 0; j < UNROLL_REGISTERS; j++)
            outputs[j] = regs[j];
    }
}

template<Color side>
void NNUE::addToAccumulator(int16_t(*inputData)[L1_SIZE], int16_t(*outputData)[L1_SIZE], int featureIndex) {
    int weightOffset = featureIndex * L1_SIZE / I16_VEC_SIZE;

    VecI16* inputVec = (VecI16*)inputData[side];
    VecI16* outputVec = (VecI16*)outputData[side];
    VecI16* weightsVec = (VecI16*)networkData->inputWeights;

    // The number of iterations to compute the hidden layer depends on the size of the vector registers
    for (int i = 0; i < L1_ITERATIONS; ++i)
        outputVec[i] = addEpi16(inputVec[i], weightsVec[weightOffset + i]);
}

template<Color side>
void NNUE::subFromAccumulator(int16_t(*inputData)[L1_SIZE], int16_t(*outputData)[L1_SIZE], int featureIndex) {
    int weightOffset = featureIndex * L1_SIZE / I16_VEC_SIZE;

    VecI16* inputVec = (VecI16*)inputData[side];
    VecI16* outputVec = (VecI16*)outputData[side];
    VecI16* weightsVec = (VecI16*)networkData->inputWeights;

    // The number of iterations to compute the hidden layer depends on the size of the vector registers
    for (int i = 0; i < L1_ITERATIONS; ++i)
        outputVec[i] = subEpi16(inputVec[i], weightsVec[weightOffset + i]);
}

template<Color side>
void NNUE::addSubToAccumulator(int16_t(*inputData)[L1_SIZE], int16_t(*outputData)[L1_SIZE], int addIndex, int subIndex) {
    int subtractWeightOffset = subIndex * L1_SIZE / I16_VEC_SIZE;
    int addWeightOffset = addIndex * L1_SIZE / I16_VEC_SIZE;

    VecI16* inputVec = (VecI16*)inputData[side];
    VecI16* outputVec = (VecI16*)outputData[side];
    VecI16* weightsVec = (VecI16*)networkData->inputWeights;

    // The number of iterations to compute the hidden layer depends on the size of the vector registers
    for (int i = 0; i < L1_ITERATIONS; ++i) {
        outputVec[i] = subEpi16(addEpi16(inputVec[i], weightsVec[addWeightOffset + i]), weightsVec[subtractWeightOffset + i]);
    }
}

alignas(ALIGNMENT) uint8_t l1Neurons[L1_SIZE];

Eval NNUE::evaluate(Board* board) {
    // Make sure the current accumulators are up to date
    calculateAccumulators<Color::WHITE>(board);
    calculateAccumulators<Color::BLACK>(board);

    assert(lastCalculatedAccumulator[Color::WHITE] == currentAccumulator && lastCalculatedAccumulator[Color::BLACK] == currentAccumulator);

    // Calculate output bucket based on piece count
    int pieceCount = BB::popcount(board->byColor[Color::WHITE] | board->byColor[Color::BLACK]);
    constexpr int divisor = ((32 + OUTPUT_BUCKETS - 1) / OUTPUT_BUCKETS);
    int bucket = (pieceCount - 2) / divisor;
    assert(0 <= bucket && bucket < OUTPUT_BUCKETS);

    Accumulator* accumulator = &accumulatorStack[currentAccumulator];

    VecI16* stmThreatAcc = reinterpret_cast<VecI16*>(accumulator->threatState[board->stm]);
    VecI16* stmPieceAcc = reinterpret_cast<VecI16*>(accumulator->pieceState[board->stm]);
    VecI16* oppThreatAcc = reinterpret_cast<VecI16*>(accumulator->threatState[1 - board->stm]);
    VecI16* oppPieceAcc = reinterpret_cast<VecI16*>(accumulator->pieceState[1 - board->stm]);

    VecIu8* l1NeuronsVec = reinterpret_cast<VecIu8*>(l1Neurons);
    VecI16 i16Zero = set1Epi16(0);
    VecI16 i16Quant = set1Epi16(INPUT_QUANT);

    constexpr int inverseShift = 16 - INPUT_SHIFT;
    constexpr int pairwiseOffset = L1_SIZE / I16_VEC_SIZE / 2;
    for (int l1 = 0; l1 < pairwiseOffset; l1 += 2) {
        // STM
        VecI16 clipped1 = minEpi16(maxEpi16(addEpi16(stmThreatAcc[l1], stmPieceAcc[l1]), i16Zero), i16Quant);
        VecI16 clipped2 = minEpi16(addEpi16(stmThreatAcc[l1 + pairwiseOffset], stmPieceAcc[l1 + pairwiseOffset]), i16Quant);
        VecI16 shift = slliEpi16(clipped1, inverseShift);
        VecI16 mul1 = mulhiEpi16(shift, clipped2);

        clipped1 = minEpi16(maxEpi16(addEpi16(stmThreatAcc[l1 + 1], stmPieceAcc[l1 + 1]), i16Zero), i16Quant);
        clipped2 = minEpi16(addEpi16(stmThreatAcc[l1 + pairwiseOffset + 1], stmPieceAcc[l1 + pairwiseOffset + 1]), i16Quant);
        shift = slliEpi16(clipped1, inverseShift);
        VecI16 mul2 = mulhiEpi16(shift, clipped2);

        l1NeuronsVec[l1 / 2] = packusEpi16(mul1, mul2);

        // NSTM
        clipped1 = minEpi16(maxEpi16(addEpi16(oppThreatAcc[l1], oppPieceAcc[l1]), i16Zero), i16Quant);
        clipped2 = minEpi16(addEpi16(oppThreatAcc[l1 + pairwiseOffset], oppPieceAcc[l1 + pairwiseOffset]), i16Quant);
        shift = slliEpi16(clipped1, inverseShift);
        mul1 = mulhiEpi16(shift, clipped2);

        clipped1 = minEpi16(maxEpi16(addEpi16(oppThreatAcc[l1 + 1], oppPieceAcc[l1 + 1]), i16Zero), i16Quant);
        clipped2 = minEpi16(addEpi16(oppThreatAcc[l1 + pairwiseOffset + 1], oppPieceAcc[l1 + pairwiseOffset + 1]), i16Quant);
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
        VecI8* weights_1 = reinterpret_cast<VecI8*>(&networkData->l1Weights[bucket][l1_1 * L2_SIZE]);
        VecI8* weights_2 = reinterpret_cast<VecI8*>(&networkData->l1Weights[bucket][l1_2 * L2_SIZE]);

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
        VecI8* weights = reinterpret_cast<VecI8*>(&networkData->l1Weights[bucket][l1 * L2_SIZE]);

        for (int l2 = 0; l2 < L2_SIZE / I32_VEC_SIZE; l2++) {
            l2NeuronsVec[l2] = dpbusdEpi32(l2NeuronsVec[l2], u8, weights[l2]);
        }
    }
#else
    for (int l1 = 0; l1 < L1_SIZE; l1++) {
        if (!l1Neurons[l1])
            continue;

        for (int l2 = 0; l2 < L2_SIZE; l2++) {
            l2Neurons[l2] += l1Neurons[l1] * networkData->l1Weights[bucket][l1 * L2_SIZE + l2];
        }
    }
#endif

    alignas(ALIGNMENT) float l3Neurons[L3_SIZE];
    memcpy(l3Neurons, networkData->l2Biases[bucket], sizeof(l3Neurons));

#if defined(__FMA__) || defined(__AVX2__) || (defined(__AVX512F__) && defined(__AVX512BW__)) || defined(ARCH_ARM)
    alignas(ALIGNMENT) float l2Floats[L2_SIZE];

    VecF psNorm = set1Ps(L1_NORMALISATION);
    VecF psZero = set1Ps(0.0f);
    VecF psOne = set1Ps(1.0f);

    VecF* l1Biases = reinterpret_cast<VecF*>(networkData->l1Biases[bucket]);
    VecF* l2FloatsVec = reinterpret_cast<VecF*>(l2Floats);

    for (int l2 = 0; l2 < L2_SIZE / FLOAT_VEC_SIZE; l2++) {
        VecF converted = cvtepi32Ps(l2NeuronsVec[l2]);
        VecF l2Result = fmaddPs(converted, psNorm, l1Biases[l2]);
        VecF l2Activated = maxPs(minPs(l2Result, psOne), psZero);
        l2FloatsVec[l2] = mulPs(l2Activated, l2Activated);
    }

    VecF* l3NeuronsVec = reinterpret_cast<VecF*>(l3Neurons);
    for (int l2 = 0; l2 < L2_SIZE; l2++) {
        VecF l2Vec = set1Ps(l2Floats[l2]);
        VecF* weights = reinterpret_cast<VecF*>(&networkData->l2Weights[bucket][l2 * L3_SIZE]);
        for (int l3 = 0; l3 < L3_SIZE / FLOAT_VEC_SIZE; l3++) {
            l3NeuronsVec[l3] = fmaddPs(l2Vec, weights[l3], l3NeuronsVec[l3]);
        }
    }
#else
    for (int l2 = 0; l2 < L2_SIZE; l2++) {
        float l2Result = static_cast<float>(l2Neurons[l2]) * L1_NORMALISATION + networkData->l1Biases[bucket][l2];
        float l2Activated = std::clamp(l2Result, 0.0f, 1.0f);
        l2Activated *= l2Activated;

        for (int l3 = 0; l3 < L3_SIZE; l3++) {
            l3Neurons[l3] = std::fma(l2Activated, networkData->l2Weights[bucket][l2 * L3_SIZE + l3], l3Neurons[l3]);
        }
    }
#endif

#if defined(__FMA__) || defined(__AVX2__) || (defined(__AVX512F__) && defined(__AVX512BW__)) || defined(ARCH_ARM)
    constexpr int chunks = 64 / sizeof(VecF);

    VecF resultSums[chunks];
    for (int i = 0; i < chunks; i++)
        resultSums[i] = psZero;

    VecF* l3WeightsVec = reinterpret_cast<VecF*>(networkData->l3Weights[bucket]);
    for (int l3 = 0; l3 < L3_SIZE / FLOAT_VEC_SIZE; l3 += chunks) {
        for (int chunk = 0; chunk < chunks; chunk++) {
            VecF l3Activated = maxPs(minPs(l3NeuronsVec[l3 + chunk], psOne), psZero);
            l3Activated = mulPs(l3Activated, l3Activated);
            resultSums[chunk] = fmaddPs(l3Activated, l3WeightsVec[l3 + chunk], resultSums[chunk]);
        }
    }

    float result = networkData->l3Biases[bucket] + reduceAddPs(resultSums);
#else
    constexpr int chunks = sizeof(VecF) / sizeof(float);
    float resultSums[chunks] = {};

    for (int l3 = 0; l3 < L3_SIZE; l3 += chunks) {
        for (int chunk = 0; chunk < chunks; chunk++) {
            float l3Activated = std::clamp(l3Neurons[l3 + chunk], 0.0f, 1.0f);
            l3Activated *= l3Activated;
            resultSums[chunk] = std::fma(l3Activated, networkData->l3Weights[bucket][l3 + chunk], resultSums[chunk]);
        }
    }

    float result = networkData->l3Biases[bucket] + reduceAddPsR(resultSums, chunks);
#endif

    return result * NETWORK_SCALE;
}