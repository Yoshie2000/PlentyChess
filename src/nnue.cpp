#include <iostream>
#include <fstream>
#include <string.h>
#include <cassert>
#include <cmath>

#include "nnue.h"
#include "board.h"

#if defined(ARCH_ARM)
#include <arm_neon.h>
#endif

#ifdef _MSC_VER
#define SP_MSVC
#pragma push_macro("_MSC_VER")
#undef _MSC_VER
#endif

#include "incbin/incbin.h"
// This will define the following variables:
// const unsigned char        gNETWORKData[];
// const unsigned char *const gNETWORKEnd;
// const unsigned int         gNETWORKSize;
INCBIN(NETWORK, EVALFILE);

#ifdef SP_MSVC
#pragma pop_macro("_MSC_VER")
#undef SP_MSVC
#endif

NetworkData* globalNetworkData;
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

    globalNetworkData = (NetworkData*)gNETWORKData;
}

void NNUE::reset(Board* board) {
    if (!networkData) {
        assert(globalNetworkData);
        networkData = globalNetworkData;
    }

    // Reset accumulator
    resetAccumulator<Color::WHITE>(board, &accumulatorStack[0]);
    resetAccumulator<Color::BLACK>(board, &accumulatorStack[0]);
    accumulatorStack[0].numDirtyPieces = 0;
    accumulatorStack[0].numDirtyThreats = 0;

    currentAccumulator = 0;
    lastCalculatedAccumulator[Color::WHITE] = 0;
    lastCalculatedAccumulator[Color::BLACK] = 0;

    // Also reset finny tables
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < KING_BUCKETS; j++) {
            memset(finnyTable[i][j].byColor, 0, sizeof(finnyTable[i][j].byColor));
            memset(finnyTable[i][j].byPiece, 0, sizeof(finnyTable[i][j].byPiece));
            memset(finnyTable[i][j].pieceState[Color::WHITE], 0, sizeof(networkData->inputBiases));
            memset(finnyTable[i][j].pieceState[Color::BLACK], 0, sizeof(networkData->inputBiases));
        }
    }
}

template<Color side>
__attribute_noinline__ void NNUE::resetAccumulator(Board* board, Accumulator* acc) {
    // Overwrite with biases
    memcpy(acc->threatState[side], networkData->inputBiases, sizeof(networkData->inputBiases));
    // Overwrite with zeroes
    memset(acc->pieceState[side], 0, sizeof(acc->pieceState[side]));

    ThreatInputs::FeatureList threatFeatures;
    ThreatInputs::addThreatFeatures(board, side, threatFeatures);
    for (int featureIndex : threatFeatures)
        addToAccumulator<side>(acc->threatState, acc->threatState, featureIndex);

    ThreatInputs::FeatureList pieceFeatures;
    ThreatInputs::addPieceFeatures(board, side, pieceFeatures, KING_BUCKET_LAYOUT);
    for (int featureIndex : pieceFeatures)
        addToAccumulator<side>(acc->pieceState, acc->pieceState, featureIndex);

    acc->kingBucketInfo[side] = getKingBucket(side, lsb(board->byColor[side] & board->byPiece[Piece::KING]));
    acc->board = board;
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

void NNUE::updateThreat(Piece piece, Piece attackedPiece, Square square, Square attackedSquare, Color pieceColor, Color attackedColor, bool add) {
    assert(piece != Piece::NONE);
    assert(attackedPiece != Piece::NONE);

    Accumulator* acc = &accumulatorStack[currentAccumulator];
    acc->dirtyThreats[acc->numDirtyThreats++] = { piece, attackedPiece, square, attackedSquare, pieceColor, attackedColor, add };
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
    accumulator->board = board;

    for (Color side = Color::WHITE; side <= Color::BLACK; ++side) {
        accumulator->kingBucketInfo[side] = getKingBucket(side, lsb(board->byPiece[Piece::KING] & board->byColor[side]));
    }
}

template<Color side>
__attribute_noinline__ void NNUE::calculateAccumulators() {
    // Incrementally update all accumulators for this side
    while (lastCalculatedAccumulator[side] < currentAccumulator) {

        Accumulator* inputAcc = &accumulatorStack[lastCalculatedAccumulator[side]];
        Accumulator* outputAcc = &accumulatorStack[lastCalculatedAccumulator[side] + 1];

        KingBucketInfo* inputKingBucket = &inputAcc->kingBucketInfo[side];
        KingBucketInfo* outputKingBucket = &outputAcc->kingBucketInfo[side];

        if (inputKingBucket->mirrored != outputKingBucket->mirrored)
            refreshThreatFeatures<side>(outputAcc);
        else
            incrementallyUpdateThreatFeatures<side>(inputAcc, outputAcc, outputKingBucket);

        if (inputKingBucket->bucket != outputKingBucket->bucket || inputKingBucket->mirrored != outputKingBucket->mirrored)
            refreshPieceFeatures<side>(outputAcc, outputKingBucket);
        else
            incrementallyUpdatePieceFeatures<side>(inputAcc, outputAcc, outputKingBucket);

        lastCalculatedAccumulator[side]++;
    }
}

template<Color side>
void NNUE::refreshPieceFeatures(Accumulator* acc, KingBucketInfo* kingBucket) {
    FinnyEntry* finnyEntry = &finnyTable[kingBucket->mirrored][kingBucket->bucket];

    // Update matching finny table with the changed pieces
    for (Color c = Color::WHITE; c <= Color::BLACK; ++c) {
        for (Piece p = Piece::PAWN; p < Piece::TOTAL; ++p) {
            Bitboard finnyBB = finnyEntry->byColor[side][c] & finnyEntry->byPiece[side][p];
            Bitboard accBB = acc->board->byColor[c] & acc->board->byPiece[p];

            Bitboard addBB = accBB & ~finnyBB;
            Bitboard removeBB = ~accBB & finnyBB;

            while (addBB) {
                Square square = popLSB(&addBB);
                int featureIndex = ThreatInputs::getPieceFeature(p, square ^ (side * 56) ^ (kingBucket->mirrored * 7), static_cast<Color>(c != side), kingBucket->bucket);
                addToAccumulator<side>(finnyEntry->pieceState, finnyEntry->pieceState, featureIndex);
            }
            while (removeBB) {
                Square square = popLSB(&removeBB);
                int featureIndex = ThreatInputs::getPieceFeature(p, square ^ (side * 56) ^ (kingBucket->mirrored * 7), static_cast<Color>(c != side), kingBucket->bucket);
                subFromAccumulator<side>(finnyEntry->pieceState, finnyEntry->pieceState, featureIndex);
            }
        }
    }
    memcpy(finnyEntry->byColor[side], acc->board->byColor, sizeof(finnyEntry->byColor[side]));
    memcpy(finnyEntry->byPiece[side], acc->board->byPiece, sizeof(finnyEntry->byPiece[side]));

    // Copy result to the current accumulator
    memcpy(acc->pieceState[side], finnyEntry->pieceState[side], sizeof(acc->pieceState[side]));
}

template<Color side>
void NNUE::refreshThreatFeatures(Accumulator* acc) {
    // Overwrite with biases
    memcpy(acc->threatState[side], networkData->inputBiases, sizeof(networkData->inputBiases));

    ThreatInputs::FeatureList threatFeatures;
    ThreatInputs::addThreatFeatures(acc->board, side, threatFeatures);
    for (int featureIndex : threatFeatures)
        addToAccumulator<side>(acc->threatState, acc->threatState, featureIndex);
}

template<Color side>
void NNUE::incrementallyUpdatePieceFeatures(Accumulator* inputAcc, Accumulator* outputAcc, KingBucketInfo* kingBucket) {
    for (int dp = 0; dp < outputAcc->numDirtyPieces; dp++) {
        DirtyPiece dirtyPiece = outputAcc->dirtyPieces[dp];

        Color relativeColor = static_cast<Color>(dirtyPiece.pieceColor != side);

        if (dirtyPiece.origin == NO_SQUARE) {
            int featureIndex = ThreatInputs::getPieceFeature(dirtyPiece.piece, dirtyPiece.target ^ (56 * side) ^ (7 * kingBucket->mirrored), relativeColor, kingBucket->bucket);
            addToAccumulator<side>(inputAcc->pieceState, outputAcc->pieceState, featureIndex);
        }
        else if (dirtyPiece.target == NO_SQUARE) {
            int featureIndex = ThreatInputs::getPieceFeature(dirtyPiece.piece, dirtyPiece.origin ^ (56 * side) ^ (7 * kingBucket->mirrored), relativeColor, kingBucket->bucket);
            subFromAccumulator<side>(inputAcc->pieceState, outputAcc->pieceState, featureIndex);
        }
        else {
            int subIndex = ThreatInputs::getPieceFeature(dirtyPiece.piece, dirtyPiece.origin ^ (56 * side) ^ (7 * kingBucket->mirrored), relativeColor, kingBucket->bucket);
            int addIndex = ThreatInputs::getPieceFeature(dirtyPiece.piece, dirtyPiece.target ^ (56 * side) ^ (7 * kingBucket->mirrored), relativeColor, kingBucket->bucket);
            addSubToAccumulator<side>(inputAcc->pieceState, outputAcc->pieceState, addIndex, subIndex);
        }
        inputAcc = outputAcc;
    }
}

template<Color side>
void NNUE::incrementallyUpdateThreatFeatures(Accumulator* inputAcc, Accumulator* outputAcc, KingBucketInfo* kingBucket) {
    ThreatInputs::FeatureList addFeatures, subFeatures;

    for (int dp = 0; dp < outputAcc->numDirtyThreats; dp++) {
        DirtyThreat dirtyThreat = outputAcc->dirtyThreats[dp];

        Piece piece = dirtyThreat.piece;
        Piece attackedPiece = dirtyThreat.attackedPiece;
        Square square = dirtyThreat.square ^ (56 * side) ^ (7 * kingBucket->mirrored);
        Square attackedSquare = dirtyThreat.attackedSquare ^ (56 * side) ^ (7 * kingBucket->mirrored);

        Color relativeSide = static_cast<Color>(side != dirtyThreat.attackedColor);
        bool enemy = dirtyThreat.pieceColor != dirtyThreat.attackedColor;
        bool hasSideOffset = dirtyThreat.pieceColor != side;

        int featureIndex = ThreatInputs::lookupThreatFeature(piece, square, attackedSquare, attackedPiece, relativeSide, enemy);

        if (featureIndex != -1) {
            featureIndex += hasSideOffset * ThreatInputs::PieceOffsets::END;
            if (dirtyThreat.add)
                addFeatures.add(featureIndex);
            else
                subFeatures.add(featureIndex);
        }
    }

    while (addFeatures.count() && subFeatures.count()) {
        addSubToAccumulator<side>(inputAcc->threatState, outputAcc->threatState, addFeatures.remove(0), subFeatures.remove(0));
        inputAcc = outputAcc;
    }

    while (addFeatures.count()) {
        addToAccumulator<side>(inputAcc->threatState, outputAcc->threatState, addFeatures.remove(0));
        inputAcc = outputAcc;
    }

    while (subFeatures.count()) {
        subFromAccumulator<side>(inputAcc->threatState, outputAcc->threatState, subFeatures.remove(0));
        inputAcc = outputAcc;
    }

    if (!outputAcc->numDirtyThreats)
        memcpy(outputAcc->threatState[side], inputAcc->threatState[side], sizeof(networkData->inputBiases));
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

Eval NNUE::evaluate(Board* board) {
    // Make sure the current accumulators are up to date
    calculateAccumulators<Color::WHITE>();
    calculateAccumulators<Color::BLACK>();

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

    VecI16 i16Zero = set1Epi16(0);
    VecI16 i16Quant = set1Epi16(INPUT_QUANT);

  // ---------------------- FT ACTIVATION & PAIRWISE ----------------------

    alignas(ALIGNMENT) uint8_t pairwiseOutputs[L1_SIZE];
    VecIu8* pairwiseOutputsVec = reinterpret_cast<VecIu8*>(pairwiseOutputs);

    constexpr int inverseShift = 16 - INPUT_SHIFT;
    constexpr int pairwiseOffset = L1_SIZE / I16_VEC_SIZE / 2;
    for (int pw = 0; pw < pairwiseOffset; pw += 2) {
        // STM
        VecI16 clipped1 = minEpi16(maxEpi16(addEpi16(stmPieceAcc[pw], stmThreatAcc[pw]), i16Zero), i16Quant);
        VecI16 clipped2 = minEpi16(addEpi16(stmPieceAcc[pw + pairwiseOffset], stmThreatAcc[pw + pairwiseOffset]), i16Quant);
        VecI16 shift = slliEpi16(clipped1, inverseShift);
        VecI16 mul1 = mulhiEpi16(shift, clipped2);

        clipped1 = minEpi16(maxEpi16(addEpi16(stmPieceAcc[pw + 1], stmThreatAcc[pw + 1]), i16Zero), i16Quant);
        clipped2 = minEpi16(addEpi16(stmPieceAcc[pw + 1 + pairwiseOffset], stmThreatAcc[pw + 1 + pairwiseOffset]), i16Quant);
        shift = slliEpi16(clipped1, inverseShift);
        VecI16 mul2 = mulhiEpi16(shift, clipped2);

        pairwiseOutputsVec[pw / 2] = packusEpi16(mul1, mul2);

        // NSTM
        clipped1 = minEpi16(maxEpi16(addEpi16(oppPieceAcc[pw], oppThreatAcc[pw]), i16Zero), i16Quant);
        clipped2 = minEpi16(addEpi16(oppPieceAcc[pw + pairwiseOffset], oppThreatAcc[pw + pairwiseOffset]), i16Quant);
        shift = slliEpi16(clipped1, inverseShift);
        mul1 = mulhiEpi16(shift, clipped2);

        clipped1 = minEpi16(maxEpi16(addEpi16(oppPieceAcc[pw + 1], oppThreatAcc[pw + 1]), i16Zero), i16Quant);
        clipped2 = minEpi16(addEpi16(oppPieceAcc[pw + 1 + pairwiseOffset], oppThreatAcc[pw + 1 + pairwiseOffset]), i16Quant);
        shift = slliEpi16(clipped1, inverseShift);
        mul2 = mulhiEpi16(shift, clipped2);

        pairwiseOutputsVec[pw / 2 + pairwiseOffset / 2] = packusEpi16(mul1, mul2);
    }

#if defined(PROCESS_NET)
    nnz.addActivations(pairwiseOutputs);
#endif

    alignas(ALIGNMENT) int l1MatmulOutputs[L2_SIZE] = {};

    // ---------------------- NNZ COMPUTATION ----------------------

#if defined(__SSSE3__) || defined(__AVX2__) || (defined(__AVX512F__) && defined(__AVX512BW__)) || defined(ARCH_ARM)
    int nnzCount = 0;
    alignas(ALIGNMENT) uint16_t nnzIndices[L1_SIZE / INT8_PER_INT32];

#if defined(ARCH_X86)
    __m128i nnzZero = _mm_setzero_si128();
    __m128i nnzIncrement = _mm_set1_epi16(8);
    for (int i = 0; i < L1_SIZE / INT8_PER_INT32 / 16; i++) {
        uint32_t nnz = 0;

        for (int j = 0; j < 16 / I32_VEC_SIZE; j++) {
            nnz |= vecNNZ(pairwiseOutputsVec[i * 16 / I32_VEC_SIZE + j]) << (j * I32_VEC_SIZE);
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
    VecI32* pairwiseOutputsVecI32 = reinterpret_cast<VecI32*>(pairwiseOutputs);
    uint16x8_t nnzZero = vdupq_n_u16(0);
    uint16x8_t nnzIncrement = vdupq_n_u16(8);

    for (int i = 0; i < L1_SIZE / INT8_PER_INT32 / 16; i++) {
        uint32_t nnz = 0;

        for (int j = 0; j < 16 / I32_VEC_SIZE; j++) {
            nnz |= vecNNZ(pairwiseOutputsVecI32[i * 16 / I32_VEC_SIZE + j]) << (j * I32_VEC_SIZE);
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

    // ---------------------- SPARSE L1 PROPAGATION ----------------------

    int* pairwiseOutputsPacks = reinterpret_cast<int*>(pairwiseOutputs);
    VecI32* l1MatmulOutputsVec = reinterpret_cast<VecI32*>(l1MatmulOutputs);

    int i = 0;
    for (; i < nnzCount - 1; i += 2) {
        int pw_1 = nnzIndices[i] * INT8_PER_INT32;
        int pw_2 = nnzIndices[i + 1] * INT8_PER_INT32;
#if defined(ARCH_X86)
        VecIu8 u8_1 = set1Epi32(pairwiseOutputsPacks[pw_1 / INT8_PER_INT32]);
        VecIu8 u8_2 = set1Epi32(pairwiseOutputsPacks[pw_2 / INT8_PER_INT32]);
#else
        VecIu8 u8_1 = vreinterpretq_u8_s32(set1Epi32(pairwiseOutputsPacks[pw_1 / INT8_PER_INT32]));
        VecIu8 u8_2 = vreinterpretq_u8_s32(set1Epi32(pairwiseOutputsPacks[pw_2 / INT8_PER_INT32]));
#endif
        VecI8* weights_1 = reinterpret_cast<VecI8*>(&networkData->l1Weights[bucket][pw_1 * L2_SIZE]);
        VecI8* weights_2 = reinterpret_cast<VecI8*>(&networkData->l1Weights[bucket][pw_2 * L2_SIZE]);

        for (int l1 = 0; l1 < L2_SIZE / I32_VEC_SIZE; l1++) {
            l1MatmulOutputsVec[l1] = dpbusdEpi32x2(l1MatmulOutputsVec[l1], u8_1, weights_1[l1], u8_2, weights_2[l1]);
        }
    }

    for (; i < nnzCount; i++) {
        int pw = nnzIndices[i] * INT8_PER_INT32;
#if defined(ARCH_X86)
        VecIu8 u8 = set1Epi32(pairwiseOutputsPacks[pw / INT8_PER_INT32]);
#else
        VecIu8 u8 = vreinterpretq_u8_s32(set1Epi32(pairwiseOutputsPacks[pw / INT8_PER_INT32]));
#endif
        VecI8* weights = reinterpret_cast<VecI8*>(&networkData->l1Weights[bucket][pw * L2_SIZE]);

        for (int l1 = 0; l1 < L2_SIZE / I32_VEC_SIZE; l1++) {
            l1MatmulOutputsVec[l1] = dpbusdEpi32(l1MatmulOutputsVec[l1], u8, weights[l1]);
        }
    }
#else
    for (int ft = 0; ft < 2 * L1_SIZE; ft++) {
        if (!pairwiseOutputs[ft])
            continue;

        for (int l1 = 0; l1 < L2_SIZE; l1++) {
            l1MatmulOutputs[l1] += pairwiseOutputs[ft] * networkData->l1Weights[bucket][ft * L2_SIZE + l1];
        }
}
#endif

    // ---------------------- CONVERT TO FLOATS & ACTIVATE L1 ----------------------

    alignas(ALIGNMENT) float l1Outputs[2 * L2_SIZE];
#if defined(__FMA__) || defined(__AVX2__) || (defined(__AVX512F__) && defined(__AVX512BW__)) || defined(ARCH_ARM)

    VecF psNorm = set1Ps(L1_NORMALISATION);
    VecF psZero = set1Ps(0.0f);
    VecF psOne = set1Ps(1.0f);

    VecF* l1Biases = reinterpret_cast<VecF*>(networkData->l1Biases[bucket]);
    VecF* l1OutputsVec = reinterpret_cast<VecF*>(l1Outputs);

    for (int l2 = 0; l2 < L2_SIZE / FLOAT_VEC_SIZE; l2++) {
        VecF converted = cvtepi32Ps(l1MatmulOutputsVec[l2]);
        VecF l1Result = fmaddPs(converted, psNorm, l1Biases[l2]);
        l1OutputsVec[l2] = maxPs(minPs(l1Result, psOne), psZero);
        l1OutputsVec[l2 + L2_SIZE / FLOAT_VEC_SIZE] = minPs(mulPs(l1Result, l1Result), psOne);
    }
#else
    for (int l1 = 0; l1 < L2_SIZE; l1++) {
        float l1Result = static_cast<float>(l1MatmulOutputs[l1]) * L1_NORMALISATION + networkData->l1Biases[bucket][l1];
        l1Outputs[l1] = std::clamp(l1Result, 0.0f, 1.0f);
        l1Outputs[l1 + L2_SIZE] = std::clamp(l1Result * l1Result, 0.0f, 1.0f);
    }
#endif

    // ---------------------- L2 PROPAGATION & ACTIVATION ----------------------

    alignas(ALIGNMENT) float l2Outputs[L3_SIZE];
    memcpy(l2Outputs, networkData->l2Biases[bucket], sizeof(l2Outputs));

#if defined(__FMA__) || defined(__AVX2__) || (defined(__AVX512F__) && defined(__AVX512BW__)) || defined(ARCH_ARM)
    VecF* l2OutputsVec = reinterpret_cast<VecF*>(l2Outputs);
    for (int l1 = 0; l1 < 2 * L2_SIZE; l1++) {
        VecF l1Vec = set1Ps(l1Outputs[l1]);
        VecF* weights = reinterpret_cast<VecF*>(&networkData->l2Weights[bucket][l1 * L3_SIZE]);
        for (int l2 = 0; l2 < L3_SIZE / FLOAT_VEC_SIZE; l2++) {
            l2OutputsVec[l2] = fmaddPs(l1Vec, weights[l2], l2OutputsVec[l2]);
        }
    }
    for (int l2 = 0; l2 < L3_SIZE / FLOAT_VEC_SIZE; l2++) {
        VecF l2Activated = maxPs(minPs(l2OutputsVec[l2], psOne), psZero);
        l2OutputsVec[l2] = mulPs(l2Activated, l2Activated);
    }
#else
    for (int l1 = 0; l1 < 2 * L2_SIZE; l1++) {
        for (int l2 = 0; l2 < L3_SIZE; l2++) {
            l2Outputs[l2] = std::fma(l1Outputs[l1], networkData->l2Weights[bucket][l1 * L3_SIZE + l2], l2Outputs[l2]);
        }
    }
    for (int l2 = 0; l2 < L3_SIZE; l2++) {
        float l2Activated = std::clamp(l2Outputs[l2], 0.0f, 1.0f);
        l2Outputs[l2] = l2Activated * l2Activated;
    }
#endif

    // ---------------------- L3 PROPAGATION ----------------------

#if defined(__FMA__) || defined(__AVX2__) || (defined(__AVX512F__) && defined(__AVX512BW__)) || defined(ARCH_ARM)
    constexpr int chunks = 64 / sizeof(VecF);

    VecF resultSums[chunks];
    for (int i = 0; i < chunks; i++)
        resultSums[i] = psZero;

    VecF* l3WeightsVec = reinterpret_cast<VecF*>(networkData->l3Weights[bucket]);
    for (int l2 = 0; l2 < L3_SIZE / FLOAT_VEC_SIZE; l2 += chunks) {
        for (int chunk = 0; chunk < chunks; chunk++) {
            resultSums[chunk] = fmaddPs(l2OutputsVec[l2 + chunk], l3WeightsVec[l2 + chunk], resultSums[chunk]);
        }
    }
    for (int l1 = 0; l1 < 2 * L2_SIZE / FLOAT_VEC_SIZE; l1 += chunks) {
        for (int chunk = 0; chunk < chunks; chunk++) {
            resultSums[chunk] = fmaddPs(l1OutputsVec[l1 + chunk], l3WeightsVec[L3_SIZE / FLOAT_VEC_SIZE + l1 + chunk], resultSums[chunk]);
        }
    }

    float result = networkData->l3Biases[bucket] + reduceAddPs(resultSums);
#else
    constexpr int chunks = sizeof(VecF) / sizeof(float);
    float resultSums[chunks] = {};

    for (int l2 = 0; l2 < L3_SIZE; l2 += chunks) {
        for (int chunk = 0; chunk < chunks; chunk++) {
            resultSums[chunk] = std::fma(l2Outputs[l2 + chunk], networkData->l3Weights[bucket][l2 + chunk], resultSums[chunk]);
        }
    }
    for (int l1 = 0; l1 < 2 * L2_SIZE; l1 += chunks) {
        for (int chunk = 0; chunk < chunks; chunk++) {
            resultSums[chunk] = std::fma(l1Outputs[l1 + chunk], networkData->l3Weights[bucket][L3_SIZE + l1 + chunk], resultSums[chunk]);
        }
    }

    float result = networkData->l3Biases[bucket] + reduceAddPsR(resultSums, chunks);
#endif

    return result * NETWORK_SCALE;
}
