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
    accumulatorStack[0].numDirtyThreats = 0;

    currentAccumulator = 0;

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
void NNUE::resetAccumulator(Board* board, Accumulator* acc) {
    // Overwrite with biases
    memcpy(acc->threatState[side], networkData->inputBiases, sizeof(networkData->inputBiases));
    // Overwrite with zeroes
    memset(acc->pieceState[side], 0, sizeof(acc->pieceState[side]));

    ThreatInputs::FeatureList threatFeatures;
    ThreatInputs::addThreatFeatures<side>(board, threatFeatures);
    for (int featureIndex : threatFeatures)
        addToAccumulator<true, side>(acc->threatState, acc->threatState, featureIndex);

    ThreatInputs::FeatureList pieceFeatures;
    ThreatInputs::addPieceFeatures(board, side, pieceFeatures, KING_BUCKET_LAYOUT);
    for (int featureIndex : pieceFeatures)
        addToAccumulator<false, side>(acc->pieceState, acc->pieceState, featureIndex);

    acc->kingBucketInfo[side] = getKingBucket(side, lsb(board->byColor[side] & board->byPiece[Piece::KING]));
    acc->board = board;
    acc->computedPieces[side] = true;
    acc->computedThreats[side] = true;
}

void NNUE::updatePiece(DirtyPiece dp) {
    Accumulator* acc = &accumulatorStack[currentAccumulator];
    acc->dirtyPiece = dp;
}

void NNUE::updateThreat(Piece piece, Piece attackedPiece, Square square, Square attackedSquare, Color pieceColor, Color attackedColor, bool add) {
    assert(piece != Piece::NONE);
    assert(attackedPiece != Piece::NONE);

    Accumulator* acc = &accumulatorStack[currentAccumulator];
    acc->dirtyThreats[acc->numDirtyThreats++] = DirtyThreat(piece, pieceColor, attackedPiece, attackedColor, square, attackedSquare, add);
}

void NNUE::incrementAccumulator() {
    currentAccumulator++;
    accumulatorStack[currentAccumulator].computedPieces[Color::WHITE] = false;
    accumulatorStack[currentAccumulator].computedPieces[Color::BLACK] = false;
    accumulatorStack[currentAccumulator].computedThreats[Color::WHITE] = false;
    accumulatorStack[currentAccumulator].computedThreats[Color::BLACK] = false;
    accumulatorStack[currentAccumulator].numDirtyThreats = 0;
}

void NNUE::decrementAccumulator() {
    assert(currentAccumulator > 0);
    currentAccumulator--;
}

void NNUE::finalizeMove(Board* board) {
    Accumulator* accumulator = &accumulatorStack[currentAccumulator];
    accumulator->board = board;

    for (Color side = Color::WHITE; side <= Color::BLACK; ++side) {
        accumulator->kingBucketInfo[side] = getKingBucket(side, lsb(board->byPiece[Piece::KING] & board->byColor[side]));
    }
}

template<Color side>
void NNUE::calculateAccumulators() {

    Accumulator* current = &accumulatorStack[currentAccumulator];
    KingBucketInfo* kingBucket = &current->kingBucketInfo[side];

    // Find last usable accumulator for threat updates
    Accumulator* acc = current;
    while (true) {
        if (acc->kingBucketInfo[side].mirrored != kingBucket->mirrored) {
            // Full refresh + backwards update
            refreshThreatFeatures<side>(current);
            backwardsUpdateThreatFeatures<side>(acc, current, kingBucket);
            break;
        }
        if (acc->computedThreats[side]) {
            // Incrementally update from here
            forwardUpdateThreatIncremental<side>(acc, current, kingBucket);
            break;
        }
        acc--;
    }

    // Find last usable accumulator for threat updates
    acc = current;
    while (true) {
        if (acc->kingBucketInfo[side].mirrored != kingBucket->mirrored || acc->kingBucketInfo[side].bucket != kingBucket->bucket) {
            // Full refresh + backwards update
            refreshPieceFeatures<side>(current, kingBucket);
            backwardsUpdatePieceFeatures<side>(acc, current, kingBucket);
            break;
        }
        if (acc->computedPieces[side]) {
            // Incrementally update from here
            forwardUpdatePieceIncremental<side>(acc, current, kingBucket);
            break;
        }
        acc--;
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
                addToAccumulator<false, side>(finnyEntry->pieceState, finnyEntry->pieceState, featureIndex);
            }
            while (removeBB) {
                Square square = popLSB(&removeBB);
                int featureIndex = ThreatInputs::getPieceFeature(p, square ^ (side * 56) ^ (kingBucket->mirrored * 7), static_cast<Color>(c != side), kingBucket->bucket);
                subFromAccumulator<false, side>(finnyEntry->pieceState, finnyEntry->pieceState, featureIndex);
            }
        }
    }
    memcpy(finnyEntry->byColor[side], acc->board->byColor, sizeof(finnyEntry->byColor[side]));
    memcpy(finnyEntry->byPiece[side], acc->board->byPiece, sizeof(finnyEntry->byPiece[side]));

    // Copy result to the current accumulator
    memcpy(acc->pieceState[side], finnyEntry->pieceState[side], sizeof(acc->pieceState[side]));

    acc->computedPieces[side] = true;
}

template<Color side>
void NNUE::refreshThreatFeatures(Accumulator* acc) {
    // Overwrite with biases
    memcpy(acc->threatState[side], networkData->inputBiases, sizeof(networkData->inputBiases));

    ThreatInputs::FeatureList threatFeatures;
    ThreatInputs::addThreatFeatures<side>(acc->board, threatFeatures);
    for (int featureIndex : threatFeatures)
        addToAccumulator<true, side>(acc->threatState, acc->threatState, featureIndex);
    
    acc->computedThreats[side] = true;
}

template <Color side>
void NNUE::forwardUpdatePieceIncremental(Accumulator* begin, Accumulator* end, KingBucketInfo* kingBucket) {

    for (Accumulator* acc = begin + 1; acc <= end; acc++) {

        if (acc + 1 <= end) {
            DirtyPiece& dp1 = acc->dirtyPiece;
            DirtyPiece& dp2 = (acc + 1)->dirtyPiece;

            if (dp1.target != NO_SQUARE && dp2.target != NO_SQUARE && dp1.target == dp2.removeSquare) {
                Square captureSquare = dp1.target;
                dp1.target = dp2.removeSquare = NO_SQUARE;
                doubleIncrementallyUpdatePieceFeatures<side>(acc - 1, acc, acc + 1, kingBucket);
                dp1.target = dp2.removeSquare = captureSquare;
                acc++;
                acc->computedPieces[side] = true;
                continue;
            }
        }

        incrementallyUpdatePieceFeatures<side>(acc - 1, acc, kingBucket);
        acc->computedPieces[side] = true;
    }

}

template <Color side>
void NNUE::forwardUpdateThreatIncremental(Accumulator* begin, Accumulator* end, KingBucketInfo* kingBucket) {

    for (Accumulator* acc = begin + 1; acc <= end; acc++) {

        // TODO doubleIncUpdate

        incrementallyUpdateThreatFeatures<side>(acc - 1, acc, kingBucket);
        acc->computedThreats[side] = true;
    }

}

template <Color side>
void NNUE::backwardsUpdatePieceFeatures(Accumulator* begin, Accumulator* end, KingBucketInfo* kingBucket) {
    for (Accumulator* acc = end - 1; acc > begin; acc--) {
        incrementallyUpdatePieceFeatures<side, false>(acc, acc + 1, kingBucket);
        acc->computedPieces[side] = true;
    }
}

template <Color side>
void NNUE::backwardsUpdateThreatFeatures(Accumulator* begin, Accumulator* end, KingBucketInfo* kingBucket) {
    for (Accumulator* acc = end - 1; acc > begin; acc--) {
        incrementallyUpdateThreatFeatures<side, false>(acc, acc + 1, kingBucket);
        acc->computedThreats[side] = true;
    }
}

template<Color side, bool forward>
void NNUE::incrementallyUpdatePieceFeatures(Accumulator* inputAcc, Accumulator* outputAcc, KingBucketInfo* kingBucket) {
    assert(inputAcc->kingBucketInfo[side].mirrored == outputAcc->kingBucketInfo[side].mirrored);
    assert(inputAcc->kingBucketInfo[side].bucket == outputAcc->kingBucketInfo[side].bucket);
    assert((forward && inputAcc->computedPieces[side]) || (!forward && outputAcc->computedPieces[side]));

    Square squareFlip = (56 * side) ^ (7 * kingBucket->mirrored);
    DirtyPiece& dp = outputAcc->dirtyPiece;
    Color color = static_cast<Color>(dp.pieceColor != side);

    if (!forward)
        std::swap(inputAcc, outputAcc);
    
    // Promotion
    if (dp.target == NO_SQUARE) {
        int sub1 = ThreatInputs::getPieceFeature(dp.piece, dp.origin ^ squareFlip, color, kingBucket->bucket);
        int add1 = ThreatInputs::getPieceFeature(dp.addPiece, dp.addSquare ^ squareFlip, color, kingBucket->bucket);
        if (!forward)
            std::swap(add1, sub1);
        addSubToAccumulator<false, side>(inputAcc->pieceState, outputAcc->pieceState, add1, sub1);

        if (dp.removeSquare != NO_SQUARE) {
            // Promotion capture
            int sub2 = ThreatInputs::getPieceFeature(dp.removePiece, dp.removeSquare ^ squareFlip, flip(color), kingBucket->bucket);
            if (forward)
                subFromAccumulator<false, side>(outputAcc->pieceState, outputAcc->pieceState, sub2);
            else
                addToAccumulator<false, side>(outputAcc->pieceState, outputAcc->pieceState, sub2);
        }
    }
    // Castling
    else if (dp.addSquare != NO_SQUARE) {
        int sub1 = ThreatInputs::getPieceFeature(dp.piece, dp.origin ^ squareFlip, color, kingBucket->bucket);
        int add1 = ThreatInputs::getPieceFeature(dp.piece, dp.target ^ squareFlip, color, kingBucket->bucket);
        int sub2 = ThreatInputs::getPieceFeature(dp.removePiece, dp.removeSquare ^ squareFlip, color, kingBucket->bucket);
        int add2 = ThreatInputs::getPieceFeature(dp.removePiece, dp.addSquare ^ squareFlip, color, kingBucket->bucket);
        if (!forward) {
            std::swap(add1, sub1);
            std::swap(add2, sub2);
        }
        addSubToAccumulator<false, side>(inputAcc->pieceState, outputAcc->pieceState, add1, sub1);
        addSubToAccumulator<false, side>(outputAcc->pieceState, outputAcc->pieceState, add2, sub2);
    }
    // Other
    else {
        int sub1 = ThreatInputs::getPieceFeature(dp.piece, dp.origin ^ squareFlip, color, kingBucket->bucket);
        int add1 = ThreatInputs::getPieceFeature(dp.piece, dp.target ^ squareFlip, color, kingBucket->bucket);
        if (!forward)
            std::swap(add1, sub1);
        addSubToAccumulator<false, side>(inputAcc->pieceState, outputAcc->pieceState, add1, sub1);

        if (dp.removeSquare != NO_SQUARE) {
            // Capture / EP
            int sub2 = ThreatInputs::getPieceFeature(dp.removePiece, dp.removeSquare ^ squareFlip, flip(color), kingBucket->bucket);
            if (forward)
                subFromAccumulator<false, side>(outputAcc->pieceState, outputAcc->pieceState, sub2);
            else
                addToAccumulator<false, side>(outputAcc->pieceState, outputAcc->pieceState, sub2);
        }
    }
}

template<Color side>
void NNUE::doubleIncrementallyUpdatePieceFeatures(Accumulator* inputAcc, Accumulator* middle, Accumulator* outputAcc, KingBucketInfo* kingBucket) {

    Square squareFlip = (56 * side) ^ (7 * kingBucket->mirrored);
    DirtyPiece& dp1 = middle->dirtyPiece;
    DirtyPiece& dp2 = outputAcc->dirtyPiece;
    Color color1 = static_cast<Color>(dp1.pieceColor != side);
    Color color2 = static_cast<Color>(dp2.pieceColor != side);

    assert(dp1.target == NO_SQUARE);
    assert(dp2.removeSquare == NO_SQUARE);

    assert(dp1.addSquare == NO_SQUARE);
    assert(dp2.addSquare == NO_SQUARE);
    assert(dp2.target != NO_SQUARE);

    // dp1 moved a piece to the capture square, which got then removed in dp2
    // hence, we can skip an addsub

    int sub1 = ThreatInputs::getPieceFeature(dp1.piece, dp1.origin ^ squareFlip, color1, kingBucket->bucket);
    int sub2 = ThreatInputs::getPieceFeature(dp2.piece, dp2.origin ^ squareFlip, color2, kingBucket->bucket);
    int add = ThreatInputs::getPieceFeature(dp2.piece, dp2.target ^ squareFlip, color2, kingBucket->bucket);
    addSubToAccumulator<false, side>(inputAcc->pieceState, outputAcc->pieceState, add, sub1);
    subFromAccumulator<false, side>(outputAcc->pieceState, outputAcc->pieceState, sub2);
    if (dp1.removeSquare != NO_SQUARE) {
        // dp1 already captured a piece
        int sub3 = ThreatInputs::getPieceFeature(dp1.removePiece, dp1.removeSquare ^ squareFlip, flip(color1), kingBucket->bucket);
        subFromAccumulator<false, side>(outputAcc->pieceState, outputAcc->pieceState, sub3);
    }
}

template<Color side, bool forward>
void NNUE::incrementallyUpdateThreatFeatures(Accumulator* inputAcc, Accumulator* outputAcc, KingBucketInfo* kingBucket) {
    assert(inputAcc->kingBucketInfo[side].mirrored == outputAcc->kingBucketInfo[side].mirrored);
    assert((forward && inputAcc->computedThreats[side]) || (!forward && outputAcc->computedThreats[side]));

    ThreatInputs::FeatureList addFeatures, subFeatures;

    const Accumulator* dataAcc = outputAcc;
    if (!forward)
        std::swap(inputAcc, outputAcc);

    for (int dp = 0; dp < dataAcc->numDirtyThreats; dp++) {
        const DirtyThreat& dt = dataAcc->dirtyThreats[dp];
        bool add = dt.attackedSquare >> 7;

        if (!forward)
            add = !add;

        int featureIndex = ThreatInputs::getThreatFeature<side>(dt.piece, dt.attackedPiece, dt.square, dt.attackedSquare & 0b111111, kingBucket->mirrored);
        if (featureIndex < ThreatInputs::FEATURE_COUNT) {
            (add ? addFeatures : subFeatures).add(featureIndex);
        }
    }

    if (!addFeatures.size() && !subFeatures.size()) {
        memcpy(outputAcc->threatState[side], inputAcc->threatState[side], sizeof(networkData->inputBiases));
        return;
    }

#if defined(__AVX512F__) && defined(__AVX512BW__)

    VecI16* inputVec = (VecI16*)inputAcc->threatState[side];
    VecI16* outputVec = (VecI16*)outputAcc->threatState[side];
    VecI16 registers[L1_ITERATIONS];

    for (int i = 0; i < L1_ITERATIONS; i++)
        registers[i] = inputVec[i];

    while (addFeatures.size() && subFeatures.size()) {
        VecI16s* addWeightsVec = (VecI16s*)&networkData->inputThreatWeights[addFeatures.remove(0) * L1_SIZE];
        VecI16s* subWeightsVec = (VecI16s*)&networkData->inputThreatWeights[subFeatures.remove(0) * L1_SIZE];
        for (int i = 0; i < L1_ITERATIONS; ++i) {
            VecI16 addWeights = convertEpi8Epi16(addWeightsVec[i]);
            VecI16 subWeights = convertEpi8Epi16(subWeightsVec[i]);
            registers[i] = subEpi16(addEpi16(registers[i], addWeights), subWeights);
        }
    }

    while (addFeatures.size()) {
        VecI16s* addWeightVec = (VecI16s*)&networkData->inputThreatWeights[addFeatures.remove(0) * L1_SIZE];
        for (int i = 0; i < L1_ITERATIONS; i++) {
            VecI16 addWeights = convertEpi8Epi16(addWeightVec[i]);
            registers[i] = addEpi16(registers[i], addWeights);
        }
    }

    while (subFeatures.size()) {
        VecI16s* subWeightVec = (VecI16s*)&networkData->inputThreatWeights[subFeatures.remove(0) * L1_SIZE];
        for (int i = 0; i < L1_ITERATIONS; i++) {
            VecI16 subWeights = convertEpi8Epi16(subWeightVec[i]);
            registers[i] = subEpi16(registers[i], subWeights);
        }
    }

    for (int i = 0; i < L1_ITERATIONS; i++)
        outputVec[i] = registers[i];

#else

    while (addFeatures.size() && subFeatures.size()) {
        addSubToAccumulator<true, side>(inputAcc->threatState, outputAcc->threatState, addFeatures.remove(0), subFeatures.remove(0));
        inputAcc = outputAcc;
    }

    while (addFeatures.size()) {
        addToAccumulator<true, side>(inputAcc->threatState, outputAcc->threatState, addFeatures.remove(0));
        inputAcc = outputAcc;
    }

    while (subFeatures.size()) {
        subFromAccumulator<true, side>(inputAcc->threatState, outputAcc->threatState, subFeatures.remove(0));
        inputAcc = outputAcc;
    }

#endif
}

template<bool I8, Color side>
void NNUE::addToAccumulator(int16_t(*inputData)[L1_SIZE], int16_t(*outputData)[L1_SIZE], int featureIndex) {
    VecI16* inputVec = (VecI16*)inputData[side];
    VecI16* outputVec = (VecI16*)outputData[side];

    if (I8) {
        VecI16s* weightsVec = (VecI16s*)&networkData->inputThreatWeights[featureIndex * L1_SIZE];

        for (int i = 0; i < L1_ITERATIONS; ++i) {
            VecI16 addWeights = convertEpi8Epi16(weightsVec[i]);
            outputVec[i] = addEpi16(inputVec[i], addWeights);
        }
    } else {
        VecI16* weightsVec = (VecI16*)&networkData->inputPsqWeights[featureIndex * L1_SIZE];

        for (int i = 0; i < L1_ITERATIONS; ++i) {
            outputVec[i] = addEpi16(inputVec[i], weightsVec[i]);
        }
    }
}

template<bool I8, Color side>
void NNUE::subFromAccumulator(int16_t(*inputData)[L1_SIZE], int16_t(*outputData)[L1_SIZE], int featureIndex) {
    VecI16* inputVec = (VecI16*)inputData[side];
    VecI16* outputVec = (VecI16*)outputData[side];

    if (I8) {
        VecI16s* weightsVec = (VecI16s*)&networkData->inputThreatWeights[featureIndex * L1_SIZE];

        for (int i = 0; i < L1_ITERATIONS; ++i) {
            VecI16 addWeights = convertEpi8Epi16(weightsVec[i]);
            outputVec[i] = subEpi16(inputVec[i], addWeights);
        }
    } else {
        VecI16* weightsVec = (VecI16*)&networkData->inputPsqWeights[featureIndex * L1_SIZE];
        
        for (int i = 0; i < L1_ITERATIONS; ++i) {
            outputVec[i] = subEpi16(inputVec[i], weightsVec[i]);
        }
    }
}

template<bool I8, Color side>
void NNUE::addSubToAccumulator(int16_t(*inputData)[L1_SIZE], int16_t(*outputData)[L1_SIZE], int addIndex, int subIndex) {
    VecI16* inputVec = (VecI16*)inputData[side];
    VecI16* outputVec = (VecI16*)outputData[side];

    if (I8) {
        VecI16s* addWeightsVec = (VecI16s*)&networkData->inputThreatWeights[addIndex * L1_SIZE];
        VecI16s* subWeightsVec = (VecI16s*)&networkData->inputThreatWeights[subIndex * L1_SIZE];

        for (int i = 0; i < L1_ITERATIONS; ++i) {
            VecI16 addWeights = convertEpi8Epi16(addWeightsVec[i]);
            VecI16 subWeights = convertEpi8Epi16(subWeightsVec[i]);
            outputVec[i] = subEpi16(addEpi16(inputVec[i], addWeights), subWeights);
        }
    } else {
        VecI16* addWeightsVec = (VecI16*)&networkData->inputPsqWeights[addIndex * L1_SIZE];
        VecI16* subWeightsVec = (VecI16*)&networkData->inputPsqWeights[subIndex * L1_SIZE];

        for (int i = 0; i < L1_ITERATIONS; ++i) {
            outputVec[i] = subEpi16(addEpi16(inputVec[i], addWeightsVec[i]), subWeightsVec[i]);
        }
    }
}

Eval NNUE::evaluate(Board* board) {
    // Make sure the current accumulators are up to date
    calculateAccumulators<Color::WHITE>();
    calculateAccumulators<Color::BLACK>();

    assert(accumulatorStack[currentAccumulator].computedPieces[Color::WHITE] && accumulatorStack[currentAccumulator].computedPieces[Color::BLACK]);
    assert(accumulatorStack[currentAccumulator].computedThreats[Color::WHITE] && accumulatorStack[currentAccumulator].computedThreats[Color::BLACK]);

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

    VecI32* pairwiseOutputsVecI32 = reinterpret_cast<VecI32*>(pairwiseOutputs);
    VecI16_v128 nnzZero = setZero_v128();
    VecI16_v128 nnzIncrement = set1Epi16_v128(8);
    for (int i = 0; i < L1_SIZE / INT8_PER_INT32 / 16; i++) {
        uint32_t nnz = 0;

        for (int j = 0; j < 16 / I32_VEC_SIZE; j++) {
            nnz |= vecNNZ(pairwiseOutputsVecI32[i * 16 / I32_VEC_SIZE + j]) << (j * I32_VEC_SIZE);
        }

        for (int j = 0; j < 16 / 8; j++) {
            uint16_t lookup = (nnz >> (j * 8)) & 0xFF;
            VecI16_v128 offsets = loadu_v128(nnzLookup[lookup]);
            storeu_v128(nnzIndices + nnzCount, addEpi16_v128(nnzZero, offsets));
            nnzCount += BB::popcount(lookup);
            nnzZero = addEpi16_v128(nnzZero, nnzIncrement);
        }
    }

    // ---------------------- SPARSE L1 PROPAGATION ----------------------

    int* pairwiseOutputsPacks = reinterpret_cast<int*>(pairwiseOutputs);
    VecI32* l1MatmulOutputsVec = reinterpret_cast<VecI32*>(l1MatmulOutputs);
    int8_t* l1Weights = networkData->l1Weights[bucket];

#if defined(__AVX512VNNI__)
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
