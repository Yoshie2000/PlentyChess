/**
 * NNUE Evaluation is heavily inspired by Obsidian (https://github.com/gab8192/Obsidian/)
*/
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
INCBIN(NETWORK, NETWORK_FILE);

NetworkData networkData;
FinnyEntry finnyTable[2][KING_BUCKETS];

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

inline int getFeatureOffset(Color side, Piece piece, Color pieceColor, Square square, KingBucketInfo* kingBucket) {
    square ^= kingBucket->mirrored * 7;
    int relativeSquare = square ^ (side * 56);
    if (side == pieceColor)
        return (64 * piece + relativeSquare) * HIDDEN_WIDTH / WEIGHTS_PER_VEC;
    else
        return (64 * (piece + 6) + relativeSquare) * HIDDEN_WIDTH / WEIGHTS_PER_VEC;
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
            memcpy(finnyTable[i][j].colors[Color::WHITE], networkData.featureBiases, sizeof(networkData.featureBiases));
            memcpy(finnyTable[i][j].colors[Color::BLACK], networkData.featureBiases, sizeof(networkData.featureBiases));
        }
    }
}

template<Color side>
void NNUE::resetAccumulator(Board* board, Accumulator* acc) {
    // Overwrite with biases
    memcpy(acc->colors[side], networkData.featureBiases, sizeof(networkData.featureBiases));

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

    int fullRefreshCost = BB::popcount(acc->byColor[side][Color::WHITE] | acc->byColor[side][Color::BLACK]);
    int finnyRefreshCost = 0;
    for (Color c = Color::WHITE; c <= Color::BLACK; ++c) {
        for (Piece p = Piece::PAWN; p < Piece::TOTAL; ++p) {
            Bitboard finnyBB = finnyEntry->byColor[side][c] & finnyEntry->byPiece[side][p];
            Bitboard accBB = acc->byColor[side][c] & acc->byPiece[side][p];

            Bitboard addBB = accBB & ~finnyBB;
            Bitboard removeBB = ~accBB & finnyBB;

            finnyRefreshCost += BB::popcount(addBB | removeBB);
        }
    }

    if (fullRefreshCost > finnyRefreshCost) {
        // Do update based on finny tables

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
    else {
        // Do update completely from scratch
        
        // Full refresh using the bitboards in the accumulator
        memcpy(acc->colors[side], networkData.featureBiases, sizeof(networkData.featureBiases));
        for (Color c = Color::WHITE; c <= Color::BLACK; ++c) {
            for (Piece p = Piece::PAWN; p < Piece::TOTAL; ++p) {
                Bitboard pieceBB = acc->byColor[side][c] & acc->byPiece[side][p];
                while (pieceBB) {
                    Square square = popLSB(&pieceBB);
                    addPieceToAccumulator<side>(acc->colors, acc->colors, &acc->kingBucketInfo[side], square, p, c);
                }
            }
        }

        // Write updates to finny table
        memcpy(finnyEntry->byColor[side], acc->byColor[side], sizeof(finnyEntry->byColor[side]));
        memcpy(finnyEntry->byPiece[side], acc->byPiece[side], sizeof(finnyEntry->byPiece[side]));

        // Copy result to the current accumulator
        memcpy(finnyEntry->colors[side], acc->colors[side], sizeof(acc->colors[side]));
    }
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
void NNUE::addPieceToAccumulator(int16_t(*inputData)[HIDDEN_WIDTH], int16_t(*outputData)[HIDDEN_WIDTH], KingBucketInfo* kingBucket, Square square, Piece piece, Color pieceColor) {
    // Get the index of the piece for this color in the input layer
    int weightOffset = getFeatureOffset(side, piece, pieceColor, square, kingBucket);

    Vec* inputVec = (Vec*)inputData[side];
    Vec* outputVec = (Vec*)outputData[side];
    Vec* weightsVec = (Vec*)networkData.featureWeights[kingBucket->index];

    // The number of iterations to compute the hidden layer depends on the size of the vector registers
    for (int i = 0; i < HIDDEN_ITERATIONS; ++i)
        outputVec[i] = addEpi16(inputVec[i], weightsVec[weightOffset + i]);
}

template<Color side>
void NNUE::removePieceFromAccumulator(int16_t(*inputData)[HIDDEN_WIDTH], int16_t(*outputData)[HIDDEN_WIDTH], KingBucketInfo* kingBucket, Square square, Piece piece, Color pieceColor) {
    // Get the index of the piece for this color in the input layer
    int weightOffset = getFeatureOffset(side, piece, pieceColor, square, kingBucket);

    Vec* inputVec = (Vec*)inputData[side];
    Vec* outputVec = (Vec*)outputData[side];
    Vec* weightsVec = (Vec*)networkData.featureWeights[kingBucket->index];

    // The number of iterations to compute the hidden layer depends on the size of the vector registers
    for (int i = 0; i < HIDDEN_ITERATIONS; ++i)
        outputVec[i] = subEpi16(inputVec[i], weightsVec[weightOffset + i]);
}

template<Color side>
void NNUE::movePieceInAccumulator(int16_t(*inputData)[HIDDEN_WIDTH], int16_t(*outputData)[HIDDEN_WIDTH], KingBucketInfo* kingBucket, Square origin, Square target, Piece piece, Color pieceColor) {
    // Get the index of the piece squares for this color in the input layer
    int subtractWeightOffset = getFeatureOffset(side, piece, pieceColor, origin, kingBucket);
    int addWeightOffset = getFeatureOffset(side, piece, pieceColor, target, kingBucket);

    Vec* inputVec = (Vec*)inputData[side];
    Vec* outputVec = (Vec*)outputData[side];
    Vec* weightsVec = (Vec*)networkData.featureWeights[kingBucket->index];

    // The number of iterations to compute the hidden layer depends on the size of the vector registers
    for (int i = 0; i < HIDDEN_ITERATIONS; ++i) {
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

    Vec* stmAcc = (Vec*)accumulator->colors[board->stm];
    Vec* oppAcc = (Vec*)accumulator->colors[1 - board->stm];

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