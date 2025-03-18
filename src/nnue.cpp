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
    // Reset finny tables
    for (int i = 0; i < 2; i++) {
        memset(finnyTable[i].byColor, 0, sizeof(finnyTable[i].byColor));
        memset(finnyTable[i].byPiece, 0, sizeof(finnyTable[i].byPiece));
        memset(finnyTable[i].mailbox, Piece::NONE, sizeof(finnyTable[i].mailbox));
        memcpy(finnyTable[i].colors[Color::WHITE], networkData->inputBiases, sizeof(networkData->inputBiases));
        memcpy(finnyTable[i].colors[Color::BLACK], networkData->inputBiases, sizeof(networkData->inputBiases));
        memset(finnyTable[i].threats, 0, sizeof(finnyTable[i].threats));
    }

    // Reset accumulator
    resetAccumulator<Color::WHITE>(board, &accumulatorStack[0]);
    resetAccumulator<Color::BLACK>(board, &accumulatorStack[0]);

    currentAccumulator = 0;
    lastCalculatedAccumulator[Color::WHITE] = 0;
    lastCalculatedAccumulator[Color::BLACK] = 0;
}

template<Color side>
void NNUE::resetAccumulator(Board* board, Accumulator* acc) {
    // Overwrite with biases
    memcpy(acc->colors[side], networkData->inputBiases, sizeof(networkData->inputBiases));

    ThreatInputs::FeatureList features;
    ThreatInputs::addSideFeatures(board, side, features);
    for (int featureIndex : features)
        addToAccumulator<side>(acc->colors, acc->colors, featureIndex);

    acc->kingBucketInfo[side] = getKingBucket(lsb(board->byColor[side] & board->byPiece[Piece::KING]));
    memcpy(acc->byColor[side], board->byColor, sizeof(board->byColor));
    memcpy(acc->byPiece[side], board->byPiece, sizeof(board->byPiece));
    memcpy(acc->mailbox[side], board->pieces, sizeof(board->pieces));
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

void NNUE::incrementAccumulator() {
    currentAccumulator++;
    accumulatorStack[currentAccumulator].numDirtyPieces = 0;
}

void NNUE::decrementAccumulator() {
    assert(currentAccumulator > 0);
    accumulatorStack[currentAccumulator].numDirtyPieces = 0;
    currentAccumulator--;
    lastCalculatedAccumulator[Color::WHITE] = std::min(lastCalculatedAccumulator[Color::WHITE], currentAccumulator);
    lastCalculatedAccumulator[Color::BLACK] = std::min(lastCalculatedAccumulator[Color::BLACK], currentAccumulator);
}

void NNUE::finalizeMove(Board* board) {
    Accumulator* accumulator = &accumulatorStack[currentAccumulator];

    for (Color side = Color::WHITE; side <= Color::BLACK; ++side) {
        accumulator->kingBucketInfo[side] = getKingBucket(lsb(board->byPiece[Piece::KING] & board->byColor[side]));
        memcpy(&accumulator->threats[side], &board->stack->threats, sizeof(board->stack->threats));
        memcpy(accumulator->byColor[side], board->byColor, sizeof(board->byColor));
        memcpy(accumulator->byPiece[side], board->byPiece, sizeof(board->byPiece));
        memcpy(accumulator->mailbox[side], board->pieces, sizeof(board->pieces));
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
    bool hm = acc->kingBucketInfo[side].mirrored;
    FinnyEntry* finnyEntry = &finnyTable[hm];

    // Update matching finny table with the changed pieces
    for (Color c = Color::WHITE; c <= Color::BLACK; ++c) {
        Color relativeColor = static_cast<Color>(c != side);

        for (Piece p = Piece::PAWN; p < Piece::TOTAL; ++p) {
            Bitboard finnyBB = finnyEntry->byColor[side][c] & finnyEntry->byPiece[side][p];
            Bitboard accBB = acc->byColor[side][c] & acc->byPiece[side][p];

            Bitboard addBB = accBB & ~finnyBB;
            Bitboard removeBB = ~accBB & finnyBB;

            while (addBB) {
                Square square = popLSB(&addBB);
                int featureIndex = ThreatInputs::getPieceFeature(p, square ^ (56 * side) ^ (7 * hm), relativeColor);
                addToAccumulator<side>(finnyEntry->colors, finnyEntry->colors, featureIndex);
            }
            while (removeBB) {
                Square square = popLSB(&removeBB);
                int featureIndex = ThreatInputs::getPieceFeature(p, square ^ (56 * side) ^ (7 * hm), relativeColor);
                subFromAccumulator<side>(finnyEntry->colors, finnyEntry->colors, featureIndex);
            }
        }
    }

    // Incrementally update threats via bitboard diffs
    for (Square square = 0; square < 64; square++) {
        Bitboard finnyBB = finnyEntry->threats[side].bySquare[square] & (finnyEntry->byColor[side][Color::WHITE] | finnyEntry->byColor[side][Color::BLACK]);
        Bitboard accBB = acc->threats[side].bySquare[square] & (acc->byColor[side][Color::WHITE] | acc->byColor[side][Color::BLACK]);

        Bitboard addBB = accBB & ~finnyBB;
        Bitboard removeBB = ~accBB & finnyBB;
        Bitboard updateBB = accBB & finnyBB;

        while (addBB) {
            Square attackedSquare = popLSB(&addBB);
            Piece piece = acc->mailbox[side][square];
            Color pieceColor = (bitboard(square) & acc->byColor[side][Color::WHITE]) ? Color::WHITE : Color::BLACK;

            Piece targetPiece = acc->mailbox[side][attackedSquare];
            Color relativeSide = static_cast<Color>(static_cast<bool>(acc->byColor[side][flip(side)] & bitboard(attackedSquare)));
            bool enemy = static_cast<bool>(acc->byColor[side][flip(pieceColor)] & bitboard(attackedSquare));
            int sideOffset = (pieceColor != side) * ThreatInputs::PieceOffsets::END;

            Square relativeSquare = square ^ (hm * 7) ^ (side * 56);
            Square relativeAttackedSquare = attackedSquare ^ (hm * 7) ^ (side * 56);

            int featureIndex = ThreatInputs::getThreatFeature(piece, relativeSquare, relativeAttackedSquare, targetPiece, relativeSide, enemy, sideOffset);
            if (featureIndex != -1)
                addToAccumulator<side>(finnyEntry->colors, finnyEntry->colors, featureIndex);
        }
        while (removeBB) {
            Square attackedSquare = popLSB(&removeBB);
            Piece piece = finnyEntry->mailbox[side][square];
            Color pieceColor = (bitboard(square) & finnyEntry->byColor[side][Color::WHITE]) ? Color::WHITE : Color::BLACK;

            Piece targetPiece = finnyEntry->mailbox[side][attackedSquare];
            Color relativeSide = static_cast<Color>(static_cast<bool>(finnyEntry->byColor[side][flip(side)] & bitboard(attackedSquare)));
            bool enemy = static_cast<bool>(finnyEntry->byColor[side][flip(pieceColor)] & bitboard(attackedSquare));
            int sideOffset = (pieceColor != side) * ThreatInputs::PieceOffsets::END;

            Square relativeSquare = square ^ (hm * 7) ^ (side * 56);
            Square relativeAttackedSquare = attackedSquare ^ (hm * 7) ^ (side * 56);

            int featureIndex = ThreatInputs::getThreatFeature(piece, relativeSquare, relativeAttackedSquare, targetPiece, relativeSide, enemy, sideOffset);
            if (featureIndex != -1)
                subFromAccumulator<side>(finnyEntry->colors, finnyEntry->colors, featureIndex);
        }
        while (updateBB) {
            Square attackedSquare = popLSB(&updateBB);

            // Feature index of finny entry
            Piece piece = finnyEntry->mailbox[side][square];
            Color pieceColor = (bitboard(square) & finnyEntry->byColor[side][Color::WHITE]) ? Color::WHITE : Color::BLACK;

            Piece targetPiece = finnyEntry->mailbox[side][attackedSquare];
            Color relativeSide = static_cast<Color>(static_cast<bool>(finnyEntry->byColor[side][flip(side)] & bitboard(attackedSquare)));
            bool enemy = static_cast<bool>(finnyEntry->byColor[side][flip(pieceColor)] & bitboard(attackedSquare));
            int sideOffset = (pieceColor != side) * ThreatInputs::PieceOffsets::END;

            Square relativeSquare = square ^ (hm * 7) ^ (side * 56);
            Square relativeAttackedSquare = attackedSquare ^ (hm * 7) ^ (side * 56);

            int finnyFeatureIndex = ThreatInputs::getThreatFeature(piece, relativeSquare, relativeAttackedSquare, targetPiece, relativeSide, enemy, sideOffset);

            // Feature index of accumulator
            piece = acc->mailbox[side][square];
            pieceColor = (bitboard(square) & acc->byColor[side][Color::WHITE]) ? Color::WHITE : Color::BLACK;

            targetPiece = acc->mailbox[side][attackedSquare];
            relativeSide = static_cast<Color>(static_cast<bool>(acc->byColor[side][flip(side)] & bitboard(attackedSquare)));
            enemy = static_cast<bool>(acc->byColor[side][flip(pieceColor)] & bitboard(attackedSquare));
            sideOffset = (pieceColor != side) * ThreatInputs::PieceOffsets::END;

            relativeSquare = square ^ (hm * 7) ^ (side * 56);
            relativeAttackedSquare = attackedSquare ^ (hm * 7) ^ (side * 56);

            int accumulatorFeatureIndex = ThreatInputs::getThreatFeature(piece, relativeSquare, relativeAttackedSquare, targetPiece, relativeSide, enemy, sideOffset);

            if (finnyFeatureIndex != -1 && accumulatorFeatureIndex != -1) {
                // Both have a valid feature => addsub if they are not the same
                if (accumulatorFeatureIndex != finnyFeatureIndex)
                    addSubToAccumulator<side>(finnyEntry->colors, finnyEntry->colors, accumulatorFeatureIndex, finnyFeatureIndex);
            }
            else if (finnyFeatureIndex == -1 && accumulatorFeatureIndex != -1) {
                // Only the accumulator feature is valid => add
                addToAccumulator<side>(finnyEntry->colors, finnyEntry->colors, accumulatorFeatureIndex);
            }
            else if (finnyFeatureIndex != -1 && accumulatorFeatureIndex == -1) {
                // Only the finny feature is valid => sub
                subFromAccumulator<side>(finnyEntry->colors, finnyEntry->colors, finnyFeatureIndex);
            }
            else {
                // Both features are not valid, do nothing
                assert(finnyFeatureIndex == -1 && accumulatorFeatureIndex == -1);
            }
        }
    }

    memcpy(finnyEntry->byColor[side], acc->byColor[side], sizeof(finnyEntry->byColor[side]));
    memcpy(finnyEntry->byPiece[side], acc->byPiece[side], sizeof(finnyEntry->byPiece[side]));
    memcpy(finnyEntry->mailbox[side], acc->mailbox[side], sizeof(finnyEntry->mailbox[side]));
    memcpy(&finnyEntry->threats[side], &acc->threats[side], sizeof(finnyEntry->threats[side]));

    // Copy result to the current accumulator
    memcpy(acc->colors[side], finnyEntry->colors[side], sizeof(acc->colors[side]));
}

template<Color side>
void NNUE::incrementallyUpdateAccumulator(Accumulator* inputAcc, Accumulator* outputAcc, KingBucketInfo* kingBucket) {
    ThreatInputs::FeatureList addFeatureList;
    ThreatInputs::FeatureList subFeatureList;

    calculatePieceFeatures<side>(outputAcc, kingBucket, addFeatureList, subFeatureList);
    calculateThreatFeatures<side>(inputAcc, outputAcc, kingBucket, addFeatureList, subFeatureList);

    applyAccumulatorUpdates<side>(inputAcc, outputAcc, addFeatureList, subFeatureList);
}

template<Color side>
void NNUE::calculatePieceFeatures(Accumulator* outputAcc, KingBucketInfo* kingBucket, ThreatInputs::FeatureList& addFeatureList, ThreatInputs::FeatureList& subFeatureList) {
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
__attribute__((always_inline)) inline void NNUE::addThreatFeatures(Square square, Bitboard threatsToAdd, KingBucketInfo* kingBucket, Accumulator* baseAcc, ThreatInputs::FeatureList& featureList) {
    while (threatsToAdd) {
        Square attackedSquare = popLSB(&threatsToAdd);
        Piece piece = baseAcc->mailbox[side][square];
        Color pieceColor = (bitboard(square) & baseAcc->byColor[side][Color::WHITE]) ? Color::WHITE : Color::BLACK;

        Piece targetPiece = baseAcc->mailbox[side][attackedSquare];
        Color relativeSide = static_cast<Color>(static_cast<bool>(baseAcc->byColor[side][flip(side)] & bitboard(attackedSquare)));
        bool enemy = static_cast<bool>(baseAcc->byColor[side][flip(pieceColor)] & bitboard(attackedSquare));
        int sideOffset = (pieceColor != side) * ThreatInputs::PieceOffsets::END;

        Square relativeSquare = square ^ (kingBucket->mirrored * 7) ^ (side * 56);
        Square relativeAttackedSquare = attackedSquare ^ (kingBucket->mirrored * 7) ^ (side * 56);

        int featureIndex = ThreatInputs::getThreatFeature(piece, relativeSquare, relativeAttackedSquare, targetPiece, relativeSide, enemy, sideOffset);
        if (featureIndex != -1)
            featureList.add(featureIndex);
    }
}

template<Color side>
__attribute__((always_inline)) inline void NNUE::removeThreatFeatures(Square square, Bitboard threatsToRemove, KingBucketInfo* kingBucket, Accumulator* baseAcc, ThreatInputs::FeatureList& featureList) {
    while (threatsToRemove) {
        Square attackedSquare = popLSB(&threatsToRemove);
        Piece piece = baseAcc->mailbox[side][square];
        Color pieceColor = (bitboard(square) & baseAcc->byColor[side][Color::WHITE]) ? Color::WHITE : Color::BLACK;

        Piece targetPiece = baseAcc->mailbox[side][attackedSquare];
        Color relativeSide = static_cast<Color>(static_cast<bool>(baseAcc->byColor[side][flip(side)] & bitboard(attackedSquare)));
        bool enemy = static_cast<bool>(baseAcc->byColor[side][flip(pieceColor)] & bitboard(attackedSquare));
        int sideOffset = (pieceColor != side) * ThreatInputs::PieceOffsets::END;

        Square relativeSquare = square ^ (kingBucket->mirrored * 7) ^ (side * 56);
        Square relativeAttackedSquare = attackedSquare ^ (kingBucket->mirrored * 7) ^ (side * 56);

        int featureIndex = ThreatInputs::getThreatFeature(piece, relativeSquare, relativeAttackedSquare, targetPiece, relativeSide, enemy, sideOffset);
        if (featureIndex != -1)
            featureList.add(featureIndex);
    }
}

template<Color side>
__attribute__((always_inline)) inline void NNUE::updateThreatFeatures(Square square, Bitboard threatsToUpdate, KingBucketInfo* kingBucket, Accumulator* inputAcc, Accumulator* outputAcc, ThreatInputs::FeatureList& addFeatureList, ThreatInputs::FeatureList& subFeatureList) {
    while (threatsToUpdate) {
        Square attackedSquare = popLSB(&threatsToUpdate);

        // Feature index of finny entry
        Piece piece1 = inputAcc->mailbox[side][square];
        Color pieceColor1 = (bitboard(square) & inputAcc->byColor[side][Color::WHITE]) ? Color::WHITE : Color::BLACK;

        Piece targetPiece1 = inputAcc->mailbox[side][attackedSquare];
        Color relativeSide1 = static_cast<Color>(static_cast<bool>(inputAcc->byColor[side][flip(side)] & bitboard(attackedSquare)));
        bool enemy1 = static_cast<bool>(inputAcc->byColor[side][flip(pieceColor1)] & bitboard(attackedSquare));
        int sideOffset1 = (pieceColor1 != side) * ThreatInputs::PieceOffsets::END;

        Square relativeSquare1 = square ^ (kingBucket->mirrored * 7) ^ (side * 56);
        Square relativeAttackedSquare1 = attackedSquare ^ (kingBucket->mirrored * 7) ^ (side * 56);

        // Feature index of accumulator
        Piece piece2 = outputAcc->mailbox[side][square];
        Color pieceColor2 = (bitboard(square) & outputAcc->byColor[side][Color::WHITE]) ? Color::WHITE : Color::BLACK;

        Piece targetPiece2 = outputAcc->mailbox[side][attackedSquare];
        Color relativeSide2 = static_cast<Color>(static_cast<bool>(outputAcc->byColor[side][flip(side)] & bitboard(attackedSquare)));
        bool enemy2 = static_cast<bool>(outputAcc->byColor[side][flip(pieceColor2)] & bitboard(attackedSquare));
        int sideOffset2 = (pieceColor2 != side) * ThreatInputs::PieceOffsets::END;

        Square relativeSquare2 = square ^ (kingBucket->mirrored * 7) ^ (side * 56);
        Square relativeAttackedSquare2 = attackedSquare ^ (kingBucket->mirrored * 7) ^ (side * 56);

        // Continue if all values are equal
        if (piece1 == piece2 && relativeSquare1 == relativeSquare2 && relativeAttackedSquare1 == relativeAttackedSquare2 && targetPiece1 == targetPiece2 && relativeSide1 == relativeSide2 && enemy1 == enemy2 && sideOffset1 == sideOffset2)
            continue;

        // Actually calculate the indices if the parameters are not identical
        int finnyFeatureIndex = ThreatInputs::getThreatFeature(piece1, relativeSquare1, relativeAttackedSquare1, targetPiece1, relativeSide1, enemy1, sideOffset1);
        int accumulatorFeatureIndex = ThreatInputs::getThreatFeature(piece2, relativeSquare2, relativeAttackedSquare2, targetPiece2, relativeSide2, enemy2, sideOffset2);

        if (finnyFeatureIndex != -1 && accumulatorFeatureIndex != -1) {
            assert(accumulatorFeatureIndex != finnyFeatureIndex);

            // Both have a valid feature
            addFeatureList.add(accumulatorFeatureIndex);
            subFeatureList.add(finnyFeatureIndex);
        }
        else if (finnyFeatureIndex == -1 && accumulatorFeatureIndex != -1) {
            // Only the accumulator feature is valid => add
            addFeatureList.add(accumulatorFeatureIndex);
        }
        else if (finnyFeatureIndex != -1 && accumulatorFeatureIndex == -1) {
            // Only the finny feature is valid => sub
            subFeatureList.add(finnyFeatureIndex);
        }
        else {
            // Both features are not valid, do nothing
            assert(finnyFeatureIndex == -1 && accumulatorFeatureIndex == -1);
        }
    }
}

template<Color side>
void NNUE::calculateThreatFeatures(Accumulator* inputAcc, Accumulator* outputAcc, KingBucketInfo* kingBucket, ThreatInputs::FeatureList& addFeatureList, ThreatInputs::FeatureList& subFeatureList) {
    Bitboard originalInputOccupancy = inputAcc->byColor[side][Color::WHITE] | inputAcc->byColor[side][Color::BLACK];
    Bitboard outputOccupancy = outputAcc->byColor[side][Color::WHITE] | outputAcc->byColor[side][Color::BLACK];

    // Incrementally update threats via bitboard diffs
    for (Square square = 0; square < 64; square++) {
        Bitboard finnyBB = inputAcc->threats[side].bySquare[square] & originalInputOccupancy;
        Bitboard accBB = outputAcc->threats[side].bySquare[square] & outputOccupancy;

        Bitboard addBB = accBB & ~finnyBB;
        Bitboard removeBB = ~accBB & finnyBB;
        Bitboard updateBB = accBB & finnyBB;

        addThreatFeatures<side>(square, addBB, kingBucket, outputAcc, addFeatureList);
        removeThreatFeatures<side>(square, removeBB, kingBucket, inputAcc, subFeatureList);
        updateThreatFeatures<side>(square, updateBB, kingBucket, inputAcc, outputAcc, addFeatureList, subFeatureList);
    }
}

template<Color side>
void NNUE::applyAccumulatorUpdates(Accumulator* inputAcc, Accumulator* outputAcc, ThreatInputs::FeatureList& addFeatureList, ThreatInputs::FeatureList& subFeatureList) {
    int commonEnd = std::min(addFeatureList.count(), subFeatureList.count());
    VecI16* inputVec = (VecI16*)inputAcc->colors[side];
    VecI16* outputVec = (VecI16*)outputAcc->colors[side];
    VecI16* weightsVec = (VecI16*)networkData->inputWeights;

    constexpr int UNROLL_REGISTERS = 16;
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

alignas(ALIGNMENT) uint8_t l1Neurons[2 * L1_SIZE];

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

    VecI16* stmAcc = reinterpret_cast<VecI16*>(accumulator->colors[board->stm]);
    VecI16* oppAcc = reinterpret_cast<VecI16*>(accumulator->colors[1 - board->stm]);

    VecI16 i16Zero = set1Epi16(0);
    VecI16 i16Quant = set1Epi16(INPUT_QUANT);

    VecIu8* l1NeuronsVec = reinterpret_cast<VecIu8*>(l1Neurons);

    // No pairwise on this net
    for (int l1 = 0; l1 < L1_ITERATIONS * 2; l1 += 2) {
        // STM
        VecI16 clipped1 = minEpi16(maxEpi16(stmAcc[l1], i16Zero), i16Quant);
        VecI16 clipped2 = minEpi16(maxEpi16(stmAcc[l1 + 1], i16Zero), i16Quant);

        l1NeuronsVec[l1 / 2] = packusEpi16(clipped1, clipped2);

        // NSTM
        clipped1 = minEpi16(maxEpi16(oppAcc[l1], i16Zero), i16Quant);
        clipped2 = minEpi16(maxEpi16(oppAcc[l1 + 1], i16Zero), i16Quant);

        l1NeuronsVec[l1 / 2 + L1_ITERATIONS] = packusEpi16(clipped1, clipped2);
    }

    for (int i = 0; i < 2 * L1_SIZE; i++)
        std::cout << int(l1Neurons[i]) << " ";
    std::cout << std::endl;

// #if defined(PROCESS_NET)
//     nnz.addActivations(l1Neurons);
// #endif

    alignas(ALIGNMENT) int l2Neurons[L2_SIZE] = {};
#if defined(__SSSE3__) || defined(__AVX2__) || (defined(__AVX512F__) && defined(__AVX512BW__)) || defined(ARCH_ARM)
    int nnzCount = 0;
    alignas(ALIGNMENT) uint16_t nnzIndices[2 * L1_SIZE / INT8_PER_INT32];

#if defined(ARCH_X86)
    __m128i nnzZero = _mm_setzero_si128();
    __m128i nnzIncrement = _mm_set1_epi16(8);
    for (int i = 0; i < 2 * L1_SIZE / INT8_PER_INT32 / 16; i++) {
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

    for (int i = 0; i < 2 * L1_SIZE / INT8_PER_INT32 / 16; i++) {
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
    for (int l1 = 0; l1 < 2 * L1_SIZE; l1++) {
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