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
            
            // std::cout << "T " << int(piece) << " " << int(relativeSquare) << " " << int(relativeAttackedSquare) << " " << int(targetPiece) << " " << int(relativeSide) << " " << enemy << std::endl;
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
            } else if (finnyFeatureIndex == -1 && accumulatorFeatureIndex != -1) {
                // Only the accumulator feature is valid => add
                addToAccumulator<side>(finnyEntry->colors, finnyEntry->colors, accumulatorFeatureIndex);
            } else if (finnyFeatureIndex != -1 && accumulatorFeatureIndex == -1) {
                // Only the finny feature is valid => sub
                subFromAccumulator<side>(finnyEntry->colors, finnyEntry->colors, finnyFeatureIndex);
            } else {
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
    Accumulator* originalInputAcc = inputAcc;
    bool hm = kingBucket->mirrored;

    // Incrementally update all the dirty pieces
    // Input and output are the same king bucket so it's all good
    for (int dp = 0; dp < outputAcc->numDirtyPieces; dp++) {
        DirtyPiece dirtyPiece = outputAcc->dirtyPieces[dp];

        Color relativeColor = static_cast<Color>(dirtyPiece.pieceColor != side);

        if (dirtyPiece.origin == NO_SQUARE) {
            int featureIndex = ThreatInputs::getPieceFeature(dirtyPiece.piece, dirtyPiece.target ^ (56 * side) ^ (7 * hm), relativeColor);
            addToAccumulator<side>(inputAcc->colors, outputAcc->colors, featureIndex);
        }
        else if (dirtyPiece.target == NO_SQUARE) {
            int featureIndex = ThreatInputs::getPieceFeature(dirtyPiece.piece, dirtyPiece.origin ^ (56 * side) ^ (7 * hm), relativeColor);
            subFromAccumulator<side>(inputAcc->colors, outputAcc->colors, featureIndex);
        }
        else {
            int subIndex = ThreatInputs::getPieceFeature(dirtyPiece.piece, dirtyPiece.origin ^ (56 * side) ^ (7 * hm), relativeColor);
            int addIndex = ThreatInputs::getPieceFeature(dirtyPiece.piece, dirtyPiece.target ^ (56 * side) ^ (7 * hm), relativeColor);
            addSubToAccumulator<side>(inputAcc->colors, outputAcc->colors, addIndex, subIndex);
        }

        // After the input was used to calculate the next accumulator, that accumulator updates itself for the rest of the dirtyPieces
        inputAcc = outputAcc;
    }

    // Incrementally update threats via bitboard diffs
    for (Square square = 0; square < 64; square++) {
        Bitboard finnyBB = originalInputAcc->threats[side].bySquare[square] & (originalInputAcc->byColor[side][Color::WHITE] | originalInputAcc->byColor[side][Color::BLACK]);
        Bitboard accBB = outputAcc->threats[side].bySquare[square] & (outputAcc->byColor[side][Color::WHITE] | outputAcc->byColor[side][Color::BLACK]);

        Bitboard addBB = accBB & ~finnyBB;
        Bitboard removeBB = ~accBB & finnyBB;
        Bitboard updateBB = accBB & finnyBB;

        while (addBB) {
            Square attackedSquare = popLSB(&addBB);
            Piece piece = outputAcc->mailbox[side][square];
            Color pieceColor = (bitboard(square) & outputAcc->byColor[side][Color::WHITE]) ? Color::WHITE : Color::BLACK;

            Piece targetPiece = outputAcc->mailbox[side][attackedSquare];
            Color relativeSide = static_cast<Color>(static_cast<bool>(outputAcc->byColor[side][flip(side)] & bitboard(attackedSquare)));
            bool enemy = static_cast<bool>(outputAcc->byColor[side][flip(pieceColor)] & bitboard(attackedSquare));
            int sideOffset = (pieceColor != side) * ThreatInputs::PieceOffsets::END;

            Square relativeSquare = square ^ (hm * 7) ^ (side * 56);
            Square relativeAttackedSquare = attackedSquare ^ (hm * 7) ^ (side * 56);

            int featureIndex = ThreatInputs::getThreatFeature(piece, relativeSquare, relativeAttackedSquare, targetPiece, relativeSide, enemy, sideOffset);
            if (featureIndex != -1) {
                addToAccumulator<side>(inputAcc->colors, outputAcc->colors, featureIndex);

                // After the input was used to calculate the next accumulator, that accumulator updates itself for the rest of the dirtyPieces
                inputAcc = outputAcc;
            }
        }
        while (removeBB) {
            Square attackedSquare = popLSB(&removeBB);
            Piece piece = originalInputAcc->mailbox[side][square];
            Color pieceColor = (bitboard(square) & originalInputAcc->byColor[side][Color::WHITE]) ? Color::WHITE : Color::BLACK;

            Piece targetPiece = originalInputAcc->mailbox[side][attackedSquare];
            Color relativeSide = static_cast<Color>(static_cast<bool>(originalInputAcc->byColor[side][flip(side)] & bitboard(attackedSquare)));
            bool enemy = static_cast<bool>(originalInputAcc->byColor[side][flip(pieceColor)] & bitboard(attackedSquare));
            int sideOffset = (pieceColor != side) * ThreatInputs::PieceOffsets::END;

            Square relativeSquare = square ^ (hm * 7) ^ (side * 56);
            Square relativeAttackedSquare = attackedSquare ^ (hm * 7) ^ (side * 56);

            int featureIndex = ThreatInputs::getThreatFeature(piece, relativeSquare, relativeAttackedSquare, targetPiece, relativeSide, enemy, sideOffset);
            if (featureIndex != -1) {
                subFromAccumulator<side>(inputAcc->colors, outputAcc->colors, featureIndex);

                // After the input was used to calculate the next accumulator, that accumulator updates itself for the rest of the dirtyPieces
                inputAcc = outputAcc;
            }
        }
        while (updateBB) {
            Square attackedSquare = popLSB(&updateBB);

            // Feature index of finny entry
            Piece piece = originalInputAcc->mailbox[side][square];
            Color pieceColor = (bitboard(square) & originalInputAcc->byColor[side][Color::WHITE]) ? Color::WHITE : Color::BLACK;

            Piece targetPiece = originalInputAcc->mailbox[side][attackedSquare];
            Color relativeSide = static_cast<Color>(static_cast<bool>(originalInputAcc->byColor[side][flip(side)] & bitboard(attackedSquare)));
            bool enemy = static_cast<bool>(originalInputAcc->byColor[side][flip(pieceColor)] & bitboard(attackedSquare));
            int sideOffset = (pieceColor != side) * ThreatInputs::PieceOffsets::END;

            Square relativeSquare = square ^ (hm * 7) ^ (side * 56);
            Square relativeAttackedSquare = attackedSquare ^ (hm * 7) ^ (side * 56);

            int finnyFeatureIndex = ThreatInputs::getThreatFeature(piece, relativeSquare, relativeAttackedSquare, targetPiece, relativeSide, enemy, sideOffset);

            // Feature index of accumulator
            piece = outputAcc->mailbox[side][square];
            pieceColor = (bitboard(square) & outputAcc->byColor[side][Color::WHITE]) ? Color::WHITE : Color::BLACK;

            targetPiece = outputAcc->mailbox[side][attackedSquare];
            relativeSide = static_cast<Color>(static_cast<bool>(outputAcc->byColor[side][flip(side)] & bitboard(attackedSquare)));
            enemy = static_cast<bool>(outputAcc->byColor[side][flip(pieceColor)] & bitboard(attackedSquare));
            sideOffset = (pieceColor != side) * ThreatInputs::PieceOffsets::END;

            relativeSquare = square ^ (hm * 7) ^ (side * 56);
            relativeAttackedSquare = attackedSquare ^ (hm * 7) ^ (side * 56);
            
            int accumulatorFeatureIndex = ThreatInputs::getThreatFeature(piece, relativeSquare, relativeAttackedSquare, targetPiece, relativeSide, enemy, sideOffset);

            if (finnyFeatureIndex != -1 && accumulatorFeatureIndex != -1) {
                // Both have a valid feature => addsub if they are not the same
                if (accumulatorFeatureIndex != finnyFeatureIndex) {
                    addSubToAccumulator<side>(inputAcc->colors, outputAcc->colors, accumulatorFeatureIndex, finnyFeatureIndex);
                    // After the input was used to calculate the next accumulator, that accumulator updates itself for the rest of the dirtyPieces
                    inputAcc = outputAcc;
                }
            } else if (finnyFeatureIndex == -1 && accumulatorFeatureIndex != -1) {
                // Only the accumulator feature is valid => add
                addToAccumulator<side>(inputAcc->colors, outputAcc->colors, accumulatorFeatureIndex);
                // After the input was used to calculate the next accumulator, that accumulator updates itself for the rest of the dirtyPieces
                inputAcc = outputAcc;
            } else if (finnyFeatureIndex != -1 && accumulatorFeatureIndex == -1) {
                // Only the finny feature is valid => sub
                subFromAccumulator<side>(inputAcc->colors, outputAcc->colors, finnyFeatureIndex);
                // After the input was used to calculate the next accumulator, that accumulator updates itself for the rest of the dirtyPieces
                inputAcc = outputAcc;
            } else {
                // Both features are not valid, do nothing
                assert(finnyFeatureIndex == -1 && accumulatorFeatureIndex == -1);
            }
        }
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