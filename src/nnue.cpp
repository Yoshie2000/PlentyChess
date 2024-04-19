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

void initNetworkData() {
    FILE* nn = fopen(ALT_NETWORK_FILE, "rb");

    if (nn) {
        // Read network from file
        size_t read = 0;
        size_t fileSize = sizeof(networkData);
        size_t objectsExpected = fileSize / sizeof(int16_t);

        read += fread(networkData.featureWeights, sizeof(int16_t), INPUT_WIDTH * HIDDEN_WIDTH, nn);
        read += fread(networkData.featureBiases, sizeof(int16_t), HIDDEN_WIDTH, nn);
        read += fread(networkData.outputWeights, sizeof(int16_t), OUTPUT_BUCKETS * 2 * HIDDEN_WIDTH, nn);
        read += fread(&networkData.outputBiases, sizeof(int16_t), OUTPUT_BUCKETS, nn);

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

    // Transpose output weights
    int16_t transposed[OUTPUT_BUCKETS][2 * HIDDEN_WIDTH];
    for (int n = 0; n < OUTPUT_BUCKETS * 2 * HIDDEN_WIDTH; n++) {
        transposed[n % OUTPUT_BUCKETS][n / OUTPUT_BUCKETS] = networkData.outputWeights[n / (2 * HIDDEN_WIDTH)][n % (2 * HIDDEN_WIDTH)];
    }
    memcpy(networkData.outputWeights, transposed, sizeof(transposed));

}

inline int getFeatureOffset(Color side, Piece piece, Color pieceColor, Square square) {
    int relativeSquare = (square ^ (side * 56));
    if (side == pieceColor)
        return (64 * piece + relativeSquare) * HIDDEN_WIDTH / WEIGHTS_PER_VEC;
    else
        return (64 * (piece + 6) + relativeSquare) * HIDDEN_WIDTH / WEIGHTS_PER_VEC;
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
        nnue->accumulatorStack[i].calculatedDirtyPieces = 0;
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
    // calculateOneAccumulator();

    int i = 0;
    while (lastCalculatedAccumulator < currentAccumulator) {

        Accumulator* outputAcc = &accumulatorStack[lastCalculatedAccumulator + 1];
        Accumulator* inputAcc = &accumulatorStack[lastCalculatedAccumulator + (outputAcc->calculatedDirtyPieces != 0)];

        // Incrementally update all the dirty pieces
        for (int dp = outputAcc->calculatedDirtyPieces; dp < outputAcc->numDirtyPieces; dp++) {
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
            outputAcc->calculatedDirtyPieces++;
        }

        lastCalculatedAccumulator++;
        i++;
    }
}

void NNUE::calculateOneAccumulator() {
    Accumulator* outputAcc = &accumulatorStack[lastCalculatedAccumulator + 1];
    Accumulator* inputAcc = &accumulatorStack[lastCalculatedAccumulator + (outputAcc->calculatedDirtyPieces != 0)];

    if (outputAcc->numDirtyPieces < 1) {
        if (lastCalculatedAccumulator != currentAccumulator)
            lastCalculatedAccumulator++;
        return;
    }

    // Incrementally update all the next dirty pieece
    DirtyPiece dirtyPiece = outputAcc->dirtyPieces[outputAcc->calculatedDirtyPieces++];

    if (dirtyPiece.origin == NO_SQUARE) {
        addPieceToAccumulator(inputAcc, outputAcc, dirtyPiece.target, dirtyPiece.piece, dirtyPiece.pieceColor);
    }
    else if (dirtyPiece.target == NO_SQUARE) {
        removePieceFromAccumulator(inputAcc, outputAcc, dirtyPiece.origin, dirtyPiece.piece, dirtyPiece.pieceColor);
    }
    else {
        movePieceInAccumulator(inputAcc, outputAcc, dirtyPiece.origin, dirtyPiece.target, dirtyPiece.piece, dirtyPiece.pieceColor);
    }

    if (outputAcc->calculatedDirtyPieces == outputAcc->numDirtyPieces)
        lastCalculatedAccumulator++;
}

void NNUE::addPieceToAccumulator(Accumulator* inputAcc, Accumulator* outputAcc, Square square, Piece piece, Color pieceColor) {
    for (int side = COLOR_WHITE; side <= COLOR_BLACK; side++) {
        // Get the index of the piece for this color in the input layer
        int weightOffset = getFeatureOffset(side, piece, pieceColor, square);

        Vec* inputVec = (Vec*)inputAcc->colors[side];
        Vec* outputVec = (Vec*)outputAcc->colors[side];
        Vec* weightsVec = (Vec*)networkData.featureWeights;

        // The number of iterations to compute the hidden layer depends on the size of the vector registers
        for (int i = 0; i < HIDDEN_ITERATIONS; ++i)
            outputVec[i] = addEpi16(inputVec[i], weightsVec[weightOffset + i]);
    }
}

void NNUE::removePieceFromAccumulator(Accumulator* inputAcc, Accumulator* outputAcc, Square square, Piece piece, Color pieceColor) {
    for (int side = COLOR_WHITE; side <= COLOR_BLACK; side++) {
        // Get the index of the piece for this color in the input layer
        int weightOffset = getFeatureOffset(side, piece, pieceColor, square);

        Vec* inputVec = (Vec*)inputAcc->colors[side];
        Vec* outputVec = (Vec*)outputAcc->colors[side];
        Vec* weightsVec = (Vec*)networkData.featureWeights;

        // The number of iterations to compute the hidden layer depends on the size of the vector registers
        for (int i = 0; i < HIDDEN_ITERATIONS; ++i)
            outputVec[i] = subEpi16(inputVec[i], weightsVec[weightOffset + i]);
    }
}

void NNUE::movePieceInAccumulator(Accumulator* inputAcc, Accumulator* outputAcc, Square origin, Square target, Piece piece, Color pieceColor) {
    for (int side = COLOR_WHITE; side <= COLOR_BLACK; side++) {
        // Get the index of the piece squares for this color in the input layer
        int subtractWeightOffset = getFeatureOffset(side, piece, pieceColor, origin);
        int addWeightOffset = getFeatureOffset(side, piece, pieceColor, target);

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
    accumulatorStack[currentAccumulator].calculatedDirtyPieces = 0;
    currentAccumulator--;
    lastCalculatedAccumulator = std::min(currentAccumulator, lastCalculatedAccumulator);
}

Eval NNUE::evaluate(Board* board) {
    assert(currentAccumulator >= lastCalculatedAccumulator);

    // Make sure the current accumulator is up to date
    calculateAccumulators();
    assert(currentAccumulator == lastCalculatedAccumulator);

    int pieceCount = __builtin_popcountll(board->byColor[COLOR_WHITE] | board->byColor[COLOR_BLACK]);
    constexpr int divisor = ((32 + OUTPUT_BUCKETS - 1) / OUTPUT_BUCKETS);
    int bucket = (pieceCount - 2) / divisor;
    assert(0 <= bucket && bucket < OUTPUT_BUCKETS);

    Accumulator& accumulator = accumulatorStack[currentAccumulator];

    Vec* stmAcc = (Vec*)accumulator.colors[board->stm];
    Vec* oppAcc = (Vec*)accumulator.colors[1 - board->stm];

    Vec* stmWeights = (Vec*)&networkData.outputWeights[bucket][0];
    Vec* oppWeights = (Vec*)&networkData.outputWeights[bucket][HIDDEN_WIDTH];

    Vec reluClipMin = vecSetZero();
    Vec reluClipMax = vecSet1Epi16(NETWORK_QA);

    Vec sum = vecSetZero();
    Vec vec0, vec1;

    for (int i = 0; i < HIDDEN_ITERATIONS; ++i) {
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

    int unsquared = vecHaddEpi32(sum) / NETWORK_QA + networkData.outputBiases[bucket];

    return (Eval)((unsquared * NETWORK_SCALE) / NETWORK_QAB);
}