/**
 * NNUE Evaluation is heavily inspired by Obsidian (https://github.com/gab8192/Obsidian/)
*/
#include <iostream>
#include <fstream>
#include <string.h>
#include <cassert>

#include "nnue.h"
#include "incbin/incbin.h"

// This will define the following variables:
// const unsigned char        gNETWORKData[];
// const unsigned char *const gNETWORKEnd;
// const unsigned int         gNETWORKSize;
INCBIN(NETWORK, NETWORK_FILE);

NetworkData networkData;
alignas(ALIGNMENT) int cachedFeatureOffsets[2][PIECE_TYPES * 2 + 1][64];

void initNetworkData() {
    FILE* nn = fopen(ALT_NETWORK_FILE, "rb");

    if (nn) {
        // Read network from file
        size_t read = 0;
        size_t fileSize = sizeof(networkData);
        size_t objectsExpected = fileSize / sizeof(int16_t);

        read += fread(networkData.featureWeights, sizeof(int16_t), INPUT_WIDTH * HIDDEN_WIDTH, nn);
        read += fread(networkData.featureBiases, sizeof(int16_t), HIDDEN_WIDTH, nn);
        read += fread(networkData.outputWeights, sizeof(int16_t), HIDDEN_WIDTH * 2, nn);
        read += fread(&networkData.outputBias, sizeof(int16_t), 1, nn);

        if (std::abs((int64_t)read - (int64_t)objectsExpected) >= ALIGNMENT) {
            std::cout << "Error loading the net, aborting ";
            std::cout << "Expected " << objectsExpected << " shorts, got " << read << "\n";
            exit(1);
        }

        fclose(nn);
    }
    else {
        memcpy(&networkData, gNETWORKData, sizeof(networkData));
    }

    // Cache feature indexes
    for (Color side = COLOR_WHITE; side <= COLOR_BLACK; side++) {
        for (int piece = PIECE_PAWN; piece < PIECE_TYPES; piece++) {
            for (Square square = 0; square < 64; square++) {
                int pieceIndex = piece * 2 + side;

                int relativeSquareUs = (square ^ (side * 56));
                cachedFeatureOffsets[side][pieceIndex][square] = 64 * piece + relativeSquareUs;
                cachedFeatureOffsets[side][pieceIndex][square] *= HIDDEN_WIDTH;

                int relativeSquareThem = (square ^ ((1 - side) * 56));
                cachedFeatureOffsets[1 - side][pieceIndex][square] = 64 * (piece + 6) + relativeSquareThem;
                cachedFeatureOffsets[1 - side][pieceIndex][square] *= HIDDEN_WIDTH;
            }
        }
    }
}

void resetAccumulators(Board* board, NNUE* nnue) {
    // Sets up the NNUE accumulator completely from scratch by adding each piece individually
    nnue->currentAccumulator = 0;
    nnue->lastCalculatedAccumulator = 0;
    for (size_t i = 0; i < sizeof(nnue->accumulatorStack) / sizeof(Accumulator); i++) {
        for (int side = COLOR_WHITE; side <= COLOR_BLACK; ++side) {
            memcpy(nnue->accumulatorStack[i].colors[side], networkData.featureBiases, sizeof(networkData.featureBiases));
        }
        nnue->accumulatorStack[i].numDirtyPieces = 0;
    }

    for (Square square = 0; square < 64; square++) {
        Piece piece = board->pieces[square];
        if (piece == NO_PIECE) continue;

        Color pieceColor = (board->byColor[COLOR_WHITE] & (C64(1) << square)) ? COLOR_WHITE : COLOR_BLACK;
        nnue->addPieceToAccumulator(&nnue->accumulatorStack[0], &nnue->accumulatorStack[0], square, piece, pieceColor);
    }
}

void NNUE::addPiece(Square square, Piece piece, Color pieceColor) {
    assert(piece < NO_PIECE);

    Accumulator* acc = &accumulatorStack[currentAccumulator];
    acc->dirtyPieces[acc->numDirtyPieces++] = { NO_SQUARE, square, piece, pieceColor };
}

void NNUE::removePiece(Square square, Piece piece, Color pieceColor) {
    assert(piece < NO_PIECE);

    Accumulator* acc = &accumulatorStack[currentAccumulator];
    acc->dirtyPieces[acc->numDirtyPieces++] = { square, NO_SQUARE, piece, pieceColor };
}

void NNUE::movePiece(Square origin, Square target, Piece piece, Color pieceColor) {
    assert(piece < NO_PIECE);

    Accumulator* acc = &accumulatorStack[currentAccumulator];
    acc->dirtyPieces[acc->numDirtyPieces++] = { origin, target, piece, pieceColor };
}

void NNUE::calculateAccumulators() {
    // Starting from the last calculated accumulator, calculate all incremental updates

    while (lastCalculatedAccumulator < currentAccumulator) {

        Accumulator* inputAcc = &accumulatorStack[lastCalculatedAccumulator];
        Accumulator* outputAcc = &accumulatorStack[lastCalculatedAccumulator + 1];

        // Incrementally update all the dirty pieces
        for (int dp = 0; dp < outputAcc->numDirtyPieces; dp++) {
            DirtyPiece dirtyPiece = outputAcc->dirtyPieces[dp];

            if (dirtyPiece.origin == NO_SQUARE) {
                addPieceToAccumulator(inputAcc, outputAcc, dirtyPiece.target, dirtyPiece.piece, dirtyPiece.pieceColor);
            }
            else if (dirtyPiece.target == NO_SQUARE) {
                removePieceFromAccumulator(inputAcc, outputAcc, dirtyPiece.origin, dirtyPiece.piece, dirtyPiece.pieceColor);
            }
            else {
                movePieceInAccumulator(inputAcc, outputAcc, dirtyPiece.origin, dirtyPiece.target, dirtyPiece.piece, dirtyPiece.pieceColor);
            }

            // After the input was used to calculate the next accumulator, that accumulator updates itself for the rest of the dirtyPieces
            inputAcc = outputAcc;
        }

        lastCalculatedAccumulator++;
    }
}

void NNUE::addPieceToAccumulator(Accumulator* inputAcc, Accumulator* outputAcc, Square square, Piece piece, Color pieceColor) {
    int pieceIndex = 2 * piece + pieceColor;
    for (int side = COLOR_WHITE; side <= COLOR_BLACK; side++) {
        // Get the index of the piece for this color in the input layer
        int weightOffset = cachedFeatureOffsets[side][pieceIndex][square] / WEIGHTS_PER_VEC;

        Vec* inputVec = (Vec*)inputAcc->colors[side];
        Vec* outputVec = (Vec*)outputAcc->colors[side];
        Vec* weightsVec = (Vec*)networkData.featureWeights;

        // The number of iterations to compute the hidden layer depends on the size of the vector registers
        for (int i = 0; i < HIDDEN_ITERATIONS; ++i)
            outputVec[i] = addEpi16(inputVec[i], weightsVec[weightOffset + i]);
    }
}

void NNUE::removePieceFromAccumulator(Accumulator* inputAcc, Accumulator* outputAcc, Square square, Piece piece, Color pieceColor) {
    int pieceIndex = 2 * piece + pieceColor;
    for (int side = COLOR_WHITE; side <= COLOR_BLACK; side++) {
        // Get the index of the piece for this color in the input layer
        int weightOffset = cachedFeatureOffsets[side][pieceIndex][square] / WEIGHTS_PER_VEC;

        Vec* inputVec = (Vec*)inputAcc->colors[side];
        Vec* outputVec = (Vec*)outputAcc->colors[side];
        Vec* weightsVec = (Vec*)networkData.featureWeights;

        // The number of iterations to compute the hidden layer depends on the size of the vector registers
        for (int i = 0; i < HIDDEN_ITERATIONS; ++i)
            outputVec[i] = subEpi16(inputVec[i], weightsVec[weightOffset + i]);
    }
}

void NNUE::movePieceInAccumulator(Accumulator* inputAcc, Accumulator* outputAcc, Square origin, Square target, Piece piece, Color pieceColor) {
    int pieceIndex = 2 * piece + pieceColor;
    for (int side = COLOR_WHITE; side <= COLOR_BLACK; side++) {
        // Get the index of the piece squares for this color in the input layer
        int subtractWeightOffset = cachedFeatureOffsets[side][pieceIndex][origin] / WEIGHTS_PER_VEC;
        int addWeightOffset = cachedFeatureOffsets[side][pieceIndex][target] / WEIGHTS_PER_VEC;

        Vec* inputVec = (Vec*)inputAcc->colors[side];
        Vec* outputVec = (Vec*)outputAcc->colors[side];
        Vec* weightsVec = (Vec*)networkData.featureWeights;

        // The number of iterations to compute the hidden layer depends on the size of the vector registers
        for (int i = 0; i < HIDDEN_ITERATIONS; ++i) {
            outputVec[i] = subEpi16(addEpi16(inputVec[i], weightsVec[addWeightOffset + i]), weightsVec[subtractWeightOffset + i]);
        }
    }
}

void NNUE::incrementAccumulator() {
    currentAccumulator++;
}

void NNUE::decrementAccumulator() {
    accumulatorStack[currentAccumulator].numDirtyPieces = 0;
    currentAccumulator--;
    lastCalculatedAccumulator = std::min(currentAccumulator, lastCalculatedAccumulator);
}

Eval NNUE::evaluate(Color sideToMove) {
    assert(currentAccumulator >= lastCalculatedAccumulator);

    // Make sure the current accumulator is up to date
    calculateAccumulators();

    assert(currentAccumulator == lastCalculatedAccumulator);

    Accumulator& accumulator = accumulatorStack[currentAccumulator];

    Vec* stmAcc = (Vec*)accumulator.colors[sideToMove];
    Vec* oppAcc = (Vec*)accumulator.colors[1 - sideToMove];

    Vec* stmWeights = (Vec*)&networkData.outputWeights[0];
    Vec* oppWeights = (Vec*)&networkData.outputWeights[HIDDEN_WIDTH];

    const Vec reluClipMin = vecSetZero();
    const Vec reluClipMax = vecSet1Epi16(NETWORK_QA);

    Vec sum = vecSetZero();
    Vec reg;

    for (int i = 0; i < HIDDEN_ITERATIONS; ++i) {
        // Side to move
        reg = maxEpi16(stmAcc[i], reluClipMin); // clip (screlu min)
        reg = minEpi16(reg, reluClipMax); // clip (screlu max)
        reg = mulloEpi16(reg, reg); // square (screlu square)
        reg = maddEpi16(reg, stmWeights[i]); // multiply with output layer
        sum = addEpi32(sum, reg); // collect the result

        // Non side to move
        reg = maxEpi16(oppAcc[i], reluClipMin);
        reg = minEpi16(reg, reluClipMax);
        reg = mulloEpi16(reg, reg);
        reg = maddEpi16(reg, oppWeights[i]);
        sum = addEpi32(sum, reg);
    }

    int unsquared = vecHaddEpi32(sum) / NETWORK_QA + networkData.outputBias;

    return (Eval)((unsquared * NETWORK_SCALE) / NETWORK_QAB);
}