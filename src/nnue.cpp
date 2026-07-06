#include <iostream>
#include <fstream>
#include <string.h>
#include <cassert>
#include <cmath>

#include "nnue.h"
#include "board.h"
#include "threat-geometry.h"

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
    accumulatorStack[0].numThreatsAdded = 0;
    accumulatorStack[0].numThreatsRemoved = 0;

    currentAccumulator = 0;
}

template<Color side>
void NNUE::resetAccumulator(Board* board, Accumulator* acc) {
    acc->kingBucketInfo[side] = getKingBucket(side, lsb(board->byColor[side] & board->byPiece[Piece::KING]));
    acc->board = board;
    acc->computed[side] = true;

    refreshAccumulator<side>(acc);
}

void NNUE::updateThreat(Piece piece, Piece attackedPiece, Square square, Square attackedSquare, Color pieceColor, Color attackedColor, bool add) {
    assert(piece != Piece::NONE);
    assert(attackedPiece != Piece::NONE);

    Accumulator* acc = &accumulatorStack[currentAccumulator];
    DirtyThreat threat(piece, pieceColor, attackedPiece, attackedColor, square, attackedSquare);
    if (add)
        acc->dirtyThreatsAdded[acc->numThreatsAdded++] = threat;
    else
        acc->dirtyThreatsRemoved[acc->numThreatsRemoved++] = threat;
}

#if defined(__AVX512VBMI2__)
template<bool add, bool computeRays>
void NNUE::updatePieceThreatsGeometry(Board* board, Piece piece, Color pieceColor, Square square, Square ignore) {
    using namespace ThreatGeometry;
    Accumulator* acc = &accumulatorStack[currentAccumulator];
    uint8_t colouredPiece = static_cast<uint8_t>(piece | (pieceColor << 3));

    __m512i mailbox = colouredMailbox((const uint8_t*)board->pieces, board->byColor[Color::BLACK]);
    if (ignore != NO_SQUARE)
        mailbox = _mm512_mask_blend_epi8(1ULL << ignore, mailbox, _mm512_set1_epi8(Piece::NONE));
    Permutation perm = permutationFor(square);
    auto [permuted, bits] = permuteMailbox(perm, mailbox);
    Bitrays closest = closestOccupied(bits);

    int& focusCount = add ? acc->numThreatsAdded : acc->numThreatsRemoved;
    DirtyThreat* focusList = add ? acc->dirtyThreatsAdded : acc->dirtyThreatsRemoved;
    focusCount += pushFocusThreats<true>(focusList + focusCount, perm.indexes, permuted, outgoingThreats(colouredPiece, closest), colouredPiece, square);
    focusCount += pushFocusThreats<false>(focusList + focusCount, perm.indexes, permuted, incomingAttackers(bits, closest), colouredPiece, square);

    if constexpr (computeRays) {
        Bitrays sliders = incomingSliders(bits, closest);
        Bitrays masked = closest & 0xFEFEFEFEFEFEFEFEULL;
        Bitrays victims = (masked >> 32) | (masked << 32);
        Bitrays valid = rayFill(victims) & rayFill(sliders);
        int& discCount = add ? acc->numThreatsRemoved : acc->numThreatsAdded;
        DirtyThreat* discList = add ? acc->dirtyThreatsRemoved : acc->dirtyThreatsAdded;
        discCount += pushDiscoveredThreats(discList + discCount, perm.indexes, permuted, sliders & valid, victims & valid);
    }
}
template void NNUE::updatePieceThreatsGeometry<true, true>(Board*, Piece, Color, Square, Square);
template void NNUE::updatePieceThreatsGeometry<true, false>(Board*, Piece, Color, Square, Square);
template void NNUE::updatePieceThreatsGeometry<false, true>(Board*, Piece, Color, Square, Square);
template void NNUE::updatePieceThreatsGeometry<false, false>(Board*, Piece, Color, Square, Square);
#endif

void NNUE::incrementAccumulator() {
    currentAccumulator++;
    accumulatorStack[currentAccumulator].numThreatsAdded = 0;
    accumulatorStack[currentAccumulator].numThreatsRemoved = 0;
    accumulatorStack[currentAccumulator].computed[Color::WHITE] = false;
    accumulatorStack[currentAccumulator].computed[Color::BLACK] = false;
}

void NNUE::decrementAccumulator() {
    assert(currentAccumulator > 0);
    currentAccumulator--;
}

void NNUE::finalizeMove(Board* board, DirtyPiece dirtyPiece) {
    Accumulator* accumulator = &accumulatorStack[currentAccumulator];
    accumulator->board = board;
    accumulator->dirtyPiece = dirtyPiece;

    for (Color side = Color::WHITE; side <= Color::BLACK; ++side) {
        accumulator->kingBucketInfo[side] = getKingBucket(side, lsb(board->byPiece[Piece::KING] & board->byColor[side]));
    }
}

template<Color side>
void NNUE::calculateAccumulators() {
    // Scan backwards through accumulators to find a usable one
    int usableIndex = currentAccumulator;
    while (!accumulatorStack[usableIndex].computed[side]) {
        Accumulator* acc = &accumulatorStack[usableIndex];
        KingBucketInfo* kingBucket = &acc->kingBucketInfo[side];
        KingBucketInfo* prevKingBucket = &accumulatorStack[usableIndex - 1].kingBucketInfo[side];

        if (kingBucket->bucket != prevKingBucket->bucket || kingBucket->mirrored != prevKingBucket->mirrored) {
            // No UE chain possible, full refresh
            refreshAccumulator<side>(acc);
            acc->computed[side] = true;
            break;
        }
        usableIndex--;
    }

    // Incrementally update
    while (usableIndex < currentAccumulator) {
        Accumulator* inputAcc = &accumulatorStack[usableIndex];
        Accumulator* outputAcc = &accumulatorStack[usableIndex + 1];

        KingBucketInfo* inputKingBucket = &inputAcc->kingBucketInfo[side];
        KingBucketInfo* outputKingBucket = &outputAcc->kingBucketInfo[side];

        if (inputKingBucket->bucket != outputKingBucket->bucket || inputKingBucket->mirrored != outputKingBucket->mirrored)
            refreshAccumulator<side>(outputAcc);
        else
            incrementallyUpdateAccumulator<side>(inputAcc, outputAcc, outputKingBucket);

        outputAcc->computed[side] = true;
        usableIndex++;
    }
}

template<Color side>
void NNUE::refreshAccumulator(Accumulator* acc) {
    memcpy(acc->state[side], networkData->inputBiases, sizeof(networkData->inputBiases));

    ThreatInputs::FeatureList pieceFeatures;
    ThreatInputs::addPieceFeatures(acc->board, side, pieceFeatures, KING_BUCKET_LAYOUT);

    ThreatInputs::FeatureList threatFeatures;
    ThreatInputs::addThreatFeatures<side>(acc->board, threatFeatures);
    ThreatInputs::addPawnPairFeatures<side>(acc->board, threatFeatures);

    applyIncrementalUpdates<side>(acc->state, acc->state, pieceFeatures, ThreatInputs::FeatureList{}, threatFeatures, ThreatInputs::FeatureList{});
}

template<Color side>
void NNUE::incrementallyUpdateAccumulator(Accumulator* inputAcc, Accumulator* outputAcc, KingBucketInfo* kingBucket) {

    Square squareFlip = (56 * side) ^ (7 * kingBucket->mirrored);
    DirtyPiece& dirtyPiece = outputAcc->dirtyPiece;
    Color color = static_cast<Color>(dirtyPiece.pieceColor != side);

    ThreatInputs::FeatureList psqAdds, psqSubs;

    // Promotion
    if (dirtyPiece.target == NO_SQUARE) {
        psqSubs.add(ThreatInputs::getPieceFeature(dirtyPiece.piece, dirtyPiece.origin ^ squareFlip, color, kingBucket->bucket));
        psqAdds.add(ThreatInputs::getPieceFeature(dirtyPiece.addPiece, dirtyPiece.addSquare ^ squareFlip, color, kingBucket->bucket));

        if (dirtyPiece.removeSquare != NO_SQUARE) {
            // Promotion capture
            psqSubs.add(ThreatInputs::getPieceFeature(dirtyPiece.removePiece, dirtyPiece.removeSquare ^ squareFlip, flip(color), kingBucket->bucket));
        }
    }
    // Castling
    else if (dirtyPiece.addSquare != NO_SQUARE) {
        psqSubs.add(ThreatInputs::getPieceFeature(Piece::KING, dirtyPiece.origin ^ squareFlip, color, kingBucket->bucket));
        psqAdds.add(ThreatInputs::getPieceFeature(Piece::KING, dirtyPiece.target ^ squareFlip, color, kingBucket->bucket));
        psqSubs.add(ThreatInputs::getPieceFeature(Piece::ROOK, dirtyPiece.removeSquare ^ squareFlip, color, kingBucket->bucket));
        psqAdds.add(ThreatInputs::getPieceFeature(Piece::ROOK, dirtyPiece.addSquare ^ squareFlip, color, kingBucket->bucket));
    }
    // Other
    else {
        psqSubs.add(ThreatInputs::getPieceFeature(dirtyPiece.piece, dirtyPiece.origin ^ squareFlip, color, kingBucket->bucket));
        psqAdds.add(ThreatInputs::getPieceFeature(dirtyPiece.piece, dirtyPiece.target ^ squareFlip, color, kingBucket->bucket));

        if (dirtyPiece.removeSquare != NO_SQUARE) {
            // Capture / EP
            psqSubs.add(ThreatInputs::getPieceFeature(dirtyPiece.removePiece, dirtyPiece.removeSquare ^ squareFlip, flip(color), kingBucket->bucket));
        }
    }

    ThreatInputs::FeatureList threatAdds, threatSubs;
    ThreatInputs::addPawnPairDeltas<side>(outputAcc->board, outputAcc->dirtyPiece, kingBucket->mirrored, threatAdds, threatSubs);

    for (int dp = 0; dp < outputAcc->numThreatsAdded; dp++) {
        DirtyThreat& dt = outputAcc->dirtyThreatsAdded[dp];
        int featureIndex = ThreatInputs::getThreatFeature<side>(dt.piece, dt.attackedPiece, dt.square, dt.attackedSquare, kingBucket->mirrored);
        __builtin_prefetch(&networkData->inputThreatWeights[(ThreatInputs::THREAT_OFFSET + featureIndex) * L1_SIZE]);
        threatAdds.addIf(ThreatInputs::THREAT_OFFSET + featureIndex, featureIndex < ThreatInputs::FEATURE_COUNT);
    }
    for (int dp = 0; dp < outputAcc->numThreatsRemoved; dp++) {
        DirtyThreat& dt = outputAcc->dirtyThreatsRemoved[dp];
        int featureIndex = ThreatInputs::getThreatFeature<side>(dt.piece, dt.attackedPiece, dt.square, dt.attackedSquare, kingBucket->mirrored);
        __builtin_prefetch(&networkData->inputThreatWeights[(ThreatInputs::THREAT_OFFSET + featureIndex) * L1_SIZE]);
        threatSubs.addIf(ThreatInputs::THREAT_OFFSET + featureIndex, featureIndex < ThreatInputs::FEATURE_COUNT);
    }

    applyIncrementalUpdates<side>(inputAcc->state, outputAcc->state, psqAdds, psqSubs, threatAdds, threatSubs);
}

template<Color side>
void NNUE::applyIncrementalUpdates(int16_t(*inputData)[L1_SIZE], int16_t(*outputData)[L1_SIZE],
                        const ThreatInputs::FeatureList& psqAdds, const ThreatInputs::FeatureList& psqSubs,
                        const ThreatInputs::FeatureList& threatAdds, const ThreatInputs::FeatureList& threatSubs) {
    VecI16* input = (VecI16*)inputData[side];
    VecI16* output = (VecI16*)outputData[side];

    for (int base = 0; base < L1_ITERATIONS; base += UPDATE_TILE) {
        VecI16 registers[UPDATE_TILE];
        for (int t = 0; t < UPDATE_TILE; t++)
            registers[t] = input[base + t];

        for (int feature : psqSubs) {
            VecI16* weights = (VecI16*)&networkData->inputPsqWeights[feature * L1_SIZE];
            for (int t = 0; t < UPDATE_TILE; t++)
                registers[t] = subEpi16(registers[t], weights[base + t]);
        }
        for (int feature : psqAdds) {
            VecI16* weights = (VecI16*)&networkData->inputPsqWeights[feature * L1_SIZE];
            for (int t = 0; t < UPDATE_TILE; t++)
                registers[t] = addEpi16(registers[t], weights[base + t]);
        }
        for (int feature : threatSubs) {
            VecI16s* weights = (VecI16s*)&networkData->inputThreatWeights[feature * L1_SIZE];
            for (int t = 0; t < UPDATE_TILE; t++)
                registers[t] = subEpi16(registers[t], convertEpi8Epi16(weights[base + t]));
        }
        for (int feature : threatAdds) {
            VecI16s* weights = (VecI16s*)&networkData->inputThreatWeights[feature * L1_SIZE];
            for (int t = 0; t < UPDATE_TILE; t++)
                registers[t] = addEpi16(registers[t], convertEpi8Epi16(weights[base + t]));
        }

        for (int t = 0; t < UPDATE_TILE; t++)
            output[base + t] = registers[t];
    }
}

Eval NNUE::evaluate(Board* board) {
    // Make sure the current accumulators are up to date
    calculateAccumulators<Color::WHITE>();
    calculateAccumulators<Color::BLACK>();

    assert(accumulatorStack[currentAccumulator].computed[Color::WHITE] && accumulatorStack[currentAccumulator].computed[Color::BLACK]);

    // Calculate output bucket based on piece count
    int pieceCount = BB::popcount(board->byColor[Color::WHITE] | board->byColor[Color::BLACK]);
    constexpr int divisor = ((32 + OUTPUT_BUCKETS - 1) / OUTPUT_BUCKETS);
    int bucket = (pieceCount - 2) / divisor;
    assert(0 <= bucket && bucket < OUTPUT_BUCKETS);

    Accumulator* accumulator = &accumulatorStack[currentAccumulator];

    VecI16* stmAcc = reinterpret_cast<VecI16*>(accumulator->state[board->stm]);
    VecI16* oppAcc = reinterpret_cast<VecI16*>(accumulator->state[1 - board->stm]);

    VecI16 i16Zero = set1Epi16(0);
    VecI16 i16Quant = set1Epi16(INPUT_QUANT);

    // ---------------------- FT ACTIVATION & PAIRWISE ----------------------

    alignas(ALIGNMENT) uint8_t pairwiseOutputs[L1_SIZE];
    VecIu8* pairwiseOutputsVec = reinterpret_cast<VecIu8*>(pairwiseOutputs);

    constexpr int inverseShift = 16 - INPUT_SHIFT;
    constexpr int pairwiseOffset = L1_SIZE / I16_VEC_SIZE / 2;
    for (int pw = 0; pw < pairwiseOffset; pw += 2) {
        // STM
        VecI16 clipped1 = minEpi16(maxEpi16(stmAcc[pw], i16Zero), i16Quant);
        VecI16 clipped2 = minEpi16(stmAcc[pw + pairwiseOffset], i16Quant);
        VecI16 shift = slliEpi16(clipped1, inverseShift);
        VecI16 mul1 = mulhiEpi16(shift, clipped2);

        clipped1 = minEpi16(maxEpi16(stmAcc[pw + 1], i16Zero), i16Quant);
        clipped2 = minEpi16(stmAcc[pw + 1 + pairwiseOffset], i16Quant);
        shift = slliEpi16(clipped1, inverseShift);
        VecI16 mul2 = mulhiEpi16(shift, clipped2);

        pairwiseOutputsVec[pw / 2] = packusEpi16(mul1, mul2);

        // NSTM
        clipped1 = minEpi16(maxEpi16(oppAcc[pw], i16Zero), i16Quant);
        clipped2 = minEpi16(oppAcc[pw + pairwiseOffset], i16Quant);
        shift = slliEpi16(clipped1, inverseShift);
        mul1 = mulhiEpi16(shift, clipped2);

        clipped1 = minEpi16(maxEpi16(oppAcc[pw + 1], i16Zero), i16Quant);
        clipped2 = minEpi16(oppAcc[pw + 1 + pairwiseOffset], i16Quant);
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

    VecI32* pairwiseOutputsVecI32 = reinterpret_cast<VecI32*>(pairwiseOutputs);

#if defined(__AVX512VBMI2__)
    VecIu16 nnzBase = _mm512_set_epi16(
        31, 30, 29, 28, 27, 26, 25, 24,
        23, 22, 21, 20, 19, 18, 17, 16,
        15, 14, 13, 12, 11, 10,  9,  8,
         7,  6,  5,  4,  3,  2,  1,  0
    );
    for (int i = 0; i < L1_SIZE / INT8_PER_INT32 / 32; i++) {
        uint32_t nnzMask = vecNNZ(pairwiseOutputsVecI32[i * 2]) | (vecNNZ(pairwiseOutputsVecI32[i * 2 + 1]) << 16);
        _mm512_storeu_si512(nnzIndices + nnzCount, _mm512_maskz_compress_epi16(nnzMask, nnzBase));
        nnzBase = _mm512_add_epi16(nnzBase, _mm512_set1_epi16(32));
        nnzCount += __builtin_popcount(nnzMask);
    }
#else
    VecI16_v128 nnzZero = setZero_v128();
    VecI16_v128 nnzIncrement = set1Epi16_v128(8);
    for (int i = 0; i < L1_SIZE / INT8_PER_INT32 / 16; i++) {
        uint32_t nnzMask = 0;

        for (int j = 0; j < 16 / I32_VEC_SIZE; j++) {
            nnzMask |= vecNNZ(pairwiseOutputsVecI32[i * 16 / I32_VEC_SIZE + j]) << (j * I32_VEC_SIZE);
        }

        for (int j = 0; j < 16 / 8; j++) {
            uint16_t lookup = (nnzMask >> (j * 8)) & 0xFF;
            VecI16_v128 offsets = loadu_v128(nnzLookup[lookup]);
            storeu_v128(nnzIndices + nnzCount, addEpi16_v128(nnzZero, offsets));
            nnzCount += BB::popcount(lookup);
            nnzZero = addEpi16_v128(nnzZero, nnzIncrement);
        }
    }
#endif

    // ---------------------- SPARSE L1 PROPAGATION ----------------------

    int* pairwiseOutputsPacks = reinterpret_cast<int*>(pairwiseOutputs);
    VecI32* l1MatmulOutputsVec = reinterpret_cast<VecI32*>(l1MatmulOutputs);
    int8_t* l1Weights = networkData->l1Weights[bucket];

#if defined(__AVX512VBMI2__)
    VecI32 acc0{}, acc1{};

    int i = 0;
    for (; i < nnzCount - 3; i += 4) {
        int pw_1 = nnzIndices[i];
        int pw_2 = nnzIndices[i + 1];
        int pw_3 = nnzIndices[i + 2];
        int pw_4 = nnzIndices[i + 3];
        VecIu8 u8_1 = set1Epi32(pairwiseOutputsPacks[pw_1]);
        VecIu8 u8_2 = set1Epi32(pairwiseOutputsPacks[pw_2]);
        VecIu8 u8_3 = set1Epi32(pairwiseOutputsPacks[pw_3]);
        VecIu8 u8_4 = set1Epi32(pairwiseOutputsPacks[pw_4]);
        VecI8* weights_1 = reinterpret_cast<VecI8*>(&l1Weights[pw_1 * INT8_PER_INT32 * L2_SIZE]);
        VecI8* weights_2 = reinterpret_cast<VecI8*>(&l1Weights[pw_2 * INT8_PER_INT32 * L2_SIZE]);
        VecI8* weights_3 = reinterpret_cast<VecI8*>(&l1Weights[pw_3 * INT8_PER_INT32 * L2_SIZE]);
        VecI8* weights_4 = reinterpret_cast<VecI8*>(&l1Weights[pw_4 * INT8_PER_INT32 * L2_SIZE]);

        acc0 = dpbusdEpi32x2(acc0, u8_1, weights_1[0], u8_2, weights_2[0]);
        acc1 = dpbusdEpi32x2(acc1, u8_3, weights_3[0], u8_4, weights_4[0]);
    }

    l1MatmulOutputsVec[0] = _mm512_add_epi32(acc0, acc1);
#else
    int i = 0;
    for (; i < nnzCount - 1; i += 2) {
        int pw_1 = nnzIndices[i];
        int pw_2 = nnzIndices[i + 1];
        VecIu8 u8_1 = set1Epi32(pairwiseOutputsPacks[pw_1]);
        VecIu8 u8_2 = set1Epi32(pairwiseOutputsPacks[pw_2]);
        VecI8* weights_1 = reinterpret_cast<VecI8*>(&l1Weights[pw_1 * INT8_PER_INT32 * L2_SIZE]);
        VecI8* weights_2 = reinterpret_cast<VecI8*>(&l1Weights[pw_2 * INT8_PER_INT32 * L2_SIZE]);

        for (int l1 = 0; l1 < L2_SIZE / I32_VEC_SIZE; l1++) {
            l1MatmulOutputsVec[l1] = dpbusdEpi32x2(l1MatmulOutputsVec[l1], u8_1, weights_1[l1], u8_2, weights_2[l1]);
        }
    }
#endif

    for (; i < nnzCount; i++) {
        int pw = nnzIndices[i];
        VecIu8 u8 = set1Epi32(pairwiseOutputsPacks[pw]);
        VecI8* weights = reinterpret_cast<VecI8*>(&l1Weights[pw * INT8_PER_INT32 * L2_SIZE]);

        for (int l1 = 0; l1 < L2_SIZE / I32_VEC_SIZE; l1++) {
            l1MatmulOutputsVec[l1] = dpbusdEpi32(l1MatmulOutputsVec[l1], u8, weights[l1]);
        }
    }

#else
    for (int ft = 0; ft < L1_SIZE; ft++) {
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
        float l1Result = std::fma(static_cast<float>(l1MatmulOutputs[l1]), L1_NORMALISATION, networkData->l1Biases[bucket][l1]);
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
    for (int j = 0; j < chunks; j++)
        resultSums[j] = psZero;

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
    constexpr int chunks = 64 / sizeof(float);
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
