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

void initNetworkData() {
    networkData = (NetworkData*)gNETWORKData;
}

void NNUE::reset(Board* board) {
    // Reset accumulator
    resetAccumulator<Color::WHITE>(board, &accumulatorStack[0]);
    resetAccumulator<Color::BLACK>(board, &accumulatorStack[0]);
    accumulatorStack[0].numDirtyPieces = 0;
    accumulatorStack[0].numDirtyThreats = 0;

    currentAccumulator = 0;
}

template<Color side>
__attribute_noinline__ void NNUE::resetAccumulator(Board* board, Accumulator* acc) {
    // Overwrite with biases
    memcpy(acc->colors[side], networkData->inputBiases, sizeof(networkData->inputBiases));

    ThreatInputs::FeatureList features;
    ThreatInputs::addSideFeatures(board, side, features);
    for (int featureIndex : features)
        addToAccumulator<side>(acc->colors, acc->colors, featureIndex);

    acc->kingBucketInfo[side] = getKingBucket(lsb(board->byColor[side] & board->byPiece[Piece::KING]));
    memcpy(acc->byColor[side], board->byColor, sizeof(board->byColor));
    memcpy(acc->byPiece[side], board->byPiece, sizeof(board->byPiece));
    acc->updated[side] = true;
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
    accumulatorStack[currentAccumulator].updated[Color::WHITE] = false;
    accumulatorStack[currentAccumulator].updated[Color::BLACK] = false;
}

void NNUE::decrementAccumulator() {
    assert(currentAccumulator > 0);
    currentAccumulator--;
}

void NNUE::finalizeMove(Board* board) {
    Accumulator* accumulator = &accumulatorStack[currentAccumulator];

    for (Color side = Color::WHITE; side <= Color::BLACK; ++side) {
        accumulator->kingBucketInfo[side] = getKingBucket(lsb(board->byPiece[Piece::KING] & board->byColor[side]));
        memcpy(accumulator->byColor[side], board->byColor, sizeof(board->byColor));
        memcpy(accumulator->byPiece[side], board->byPiece, sizeof(board->byPiece));
    }
}

template<Color side>
void NNUE::calculateAccumulators(Board* board) {
    Accumulator* current = &accumulatorStack[currentAccumulator];

    if (current->updated[side])
        return;
    
    Accumulator* previous = current;
    KingBucketInfo* currentKingBucket = &current->kingBucketInfo[side];

    while (true) {
        previous--;
        KingBucketInfo* prevKingBucket = &previous->kingBucketInfo[side];

        if (needsRefresh(currentKingBucket, prevKingBucket)) {
            resetAccumulator<side>(board, current);
            break;
        }

        if (previous->updated[side]) {
            while (previous != current) {
                incrementallyUpdateAccumulator<side>(previous, previous + 1, currentKingBucket);
                previous++;
            }
            break;
        }
    }

    assert(current->updated[side]);
}

template<Color side>
__attribute_noinline__ void NNUE::incrementallyUpdateAccumulator(Accumulator* inputAcc, Accumulator* outputAcc, KingBucketInfo* kingBucket) {
    ThreatInputs::FeatureList addFeatureList;
    ThreatInputs::FeatureList subFeatureList;

    calculatePieceFeatures<side>(outputAcc, kingBucket, addFeatureList, subFeatureList);
    calculateThreatFeatures<side>(outputAcc, kingBucket, addFeatureList, subFeatureList);

    applyAccumulatorUpdates<side>(inputAcc, outputAcc, addFeatureList, subFeatureList);

    outputAcc->updated[side] = true;
}

template<Color side>
__attribute_noinline__ void NNUE::calculatePieceFeatures(Accumulator* outputAcc, KingBucketInfo* kingBucket, ThreatInputs::FeatureList& addFeatureList, ThreatInputs::FeatureList& subFeatureList) {
    for (int dp = 0; dp < outputAcc->numDirtyPieces; dp++) {
        DirtyPiece dirtyPiece = outputAcc->dirtyPieces[dp];

        Color relativeColor = static_cast<Color>(dirtyPiece.pieceColor != side);

        if (dirtyPiece.origin == NO_SQUARE) {
            int featureIndex = ThreatInputs::getPieceFeature(dirtyPiece.piece, dirtyPiece.target ^ (56 * side) ^ (7 * kingBucket->mirrored), relativeColor);
            addFeatureList.add(featureIndex);
        }
        else if (dirtyPiece.target == NO_SQUARE) {
            int featureIndex = ThreatInputs::getPieceFeature(dirtyPiece.piece, dirtyPiece.origin ^ (56 * side) ^ (7 * kingBucket->mirrored), relativeColor);
            subFeatureList.add(featureIndex);
        }
        else {
            int subIndex = ThreatInputs::getPieceFeature(dirtyPiece.piece, dirtyPiece.origin ^ (56 * side) ^ (7 * kingBucket->mirrored), relativeColor);
            int addIndex = ThreatInputs::getPieceFeature(dirtyPiece.piece, dirtyPiece.target ^ (56 * side) ^ (7 * kingBucket->mirrored), relativeColor);
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
        bool enemy = static_cast<bool>(dirtyThreat.pieceColor != dirtyThreat.attackedColor);
        int sideOffset = (dirtyThreat.pieceColor != side) * ThreatInputs::PieceOffsets::END;

        int featureIndex = ThreatInputs::getThreatFeature(piece, square, attackedSquare, attackedPiece, relativeSide, enemy, sideOffset);

        if (featureIndex != -1) {
            if (dirtyThreat.add) {
                int subIndex = subFeatureList.indexOf(featureIndex);
                if (subIndex == -1)
                    addFeatureList.add(featureIndex);
                else
                    subFeatureList.remove(subIndex);
            }
            else {
                int addIndex = addFeatureList.indexOf(featureIndex);
                if (addIndex == -1)
                    subFeatureList.add(featureIndex);
                else
                    addFeatureList.remove(addIndex);
            }
        }
    }
}

template<Color side>
__attribute_noinline__ void NNUE::applyAccumulatorUpdates(Accumulator* inputAcc, Accumulator* outputAcc, ThreatInputs::FeatureList& addFeatureList, ThreatInputs::FeatureList& subFeatureList) {
    int commonEnd = std::min(addFeatureList.count(), subFeatureList.count());
    VecI16* inputVec = (VecI16*)inputAcc->colors[side];
    VecI16* outputVec = (VecI16*)outputAcc->colors[side];
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

Eval NNUE::evaluate(Board* board) {
    // Make sure the current accumulators are up to date
    calculateAccumulators<Color::WHITE>(board);
    calculateAccumulators<Color::BLACK>(board);

    assert(accumulatorStack[currentAccumulator].updated[Color::WHITE] && accumulatorStack[currentAccumulator].updated[Color::BLACK]);

    // Calculate output bucket based on piece count
    int pieceCount = BB::popcount(board->byColor[Color::WHITE] | board->byColor[Color::BLACK]);
    int bucket = (pieceCount - 2) / 4;
    assert(0 <= bucket && bucket < OUTPUT_BUCKETS);

    Accumulator* accumulator = &accumulatorStack[currentAccumulator];

    VecI16* stmAcc = (VecI16*)accumulator->colors[board->stm];
    VecI16* oppAcc = (VecI16*)accumulator->colors[flip(board->stm)];

    VecI16* stmWeights = (VecI16*)&networkData->l1Weights[bucket][0];
    VecI16* oppWeights = (VecI16*)&networkData->l1Weights[bucket][L1_SIZE];

    VecI16 reluClipMin = set1Epi16(0);
    VecI16 reluClipMax = set1Epi16(INPUT_QUANT);

    VecI16 sum = set1Epi16(0);
    VecI16 vec0, vec1;

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

    int unsquared = vecHaddEpi32(sum) / INPUT_QUANT + networkData->l1Biases[bucket];
    return (Eval)((unsquared * NETWORK_SCALE) / (INPUT_QUANT * L1_QUANT));
}