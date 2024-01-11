/**
 * NNUE Evaluation is heavily inspired by Alexandria (https://github.com/PGG106/Alexandria/) and Obsidian (https://github.com/gab8192/Obsidian/)
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

namespace NNUE {

    alignas(ALIGNMENT) int FEATURES[2][PIECE_TYPES * 2 + 1][64];

    struct {
        alignas(ALIGNMENT) int16_t featureWeights[INPUT_WIDTH * HIDDEN_WIDTH];
        alignas(ALIGNMENT) int16_t featureBiases[HIDDEN_WIDTH];
        alignas(ALIGNMENT) int16_t outputWeights[2 * HIDDEN_WIDTH];
        int16_t outputBias;
    } network;

    template <int inputSize>
    inline void addAll(int16_t* output, int16_t* input, int offset) {
        offset /= WEIGHTS_PER_VEC;

        Vec* inputVec = (Vec*)input;
        Vec* outputVec = (Vec*)output;
        Vec* weightsVec = (Vec*)network.featureWeights;

        for (int i = 0; i < inputSize / WEIGHTS_PER_VEC; ++i)
            outputVec[i] = addEpi16(inputVec[i], weightsVec[offset + i]);
    }

    template <int inputSize>
    inline void subAll(int16_t* output, int16_t* input, int offset) {
        offset /= WEIGHTS_PER_VEC;

        Vec* inputVec = (Vec*)input;
        Vec* outputVec = (Vec*)output;
        Vec* weightsVec = (Vec*)network.featureWeights;

        for (int i = 0; i < inputSize / WEIGHTS_PER_VEC; ++i)
            outputVec[i] = subEpi16(inputVec[i], weightsVec[offset + i]);
    }

    template <int inputSize>
    inline void addSubAll(int16_t* output, int16_t* input, int addOff, int subtractOff) {
        addOff /= WEIGHTS_PER_VEC;
        subtractOff /= WEIGHTS_PER_VEC;

        Vec* inputVec = (Vec*)input;
        Vec* outputVec = (Vec*)output;
        Vec* weightsVec = (Vec*)network.featureWeights;

        for (int i = 0; i < inputSize / WEIGHTS_PER_VEC; ++i)
            outputVec[i] = subEpi16(addEpi16(inputVec[i], weightsVec[addOff + i]), weightsVec[subtractOff + i]);
    }


    void Accumulator::clear() {
        for (int c = COLOR_WHITE; c <= COLOR_BLACK; ++c)
            memcpy(colors[c], network.featureBiases, sizeof(network.featureBiases));
    }

    void Accumulator::addFeature(Square square, Piece piece, Color pieceColor, Accumulator* input) {
        assert(piece < NO_PIECE);

        int pieceIndex = 2 * piece + pieceColor;
        for (int c = COLOR_WHITE; c <= COLOR_BLACK; ++c)
            addAll<HIDDEN_WIDTH>(colors[c], input->colors[c], FEATURES[c][pieceIndex][square]);
    }

    void Accumulator::removeFeature(Square square, Piece piece, Color pieceColor, Accumulator* input) {
        assert(piece < NO_PIECE);

        int pieceIndex = 2 * piece + pieceColor;
        for (int c = COLOR_WHITE; c <= COLOR_BLACK; ++c)
            subAll<HIDDEN_WIDTH>(colors[c], input->colors[c], FEATURES[c][pieceIndex][square]);
    }

    void Accumulator::moveFeature(Square origin, Square target, Piece piece, Color pieceColor, Accumulator* input) {
        assert(piece < NO_PIECE);

        int pieceIndex = 2 * piece + pieceColor;
        for (int c = COLOR_WHITE; c <= COLOR_BLACK; ++c)
            addSubAll<HIDDEN_WIDTH>(colors[c], input->colors[c], FEATURES[c][pieceIndex][target], FEATURES[c][pieceIndex][origin]);
    }

    void init() {
        // open the nn file
        FILE* nn = fopen(ALT_NETWORK_FILE, "rb");

        // if it's not invalid read the config values from it
        if (nn) {
            // initialize an accumulator for every input of the second layer
            size_t read = 0;
            size_t fileSize = sizeof(network);
            size_t objectsExpected = fileSize / sizeof(int16_t);

            read += fread(network.featureWeights, sizeof(int16_t), INPUT_WIDTH * HIDDEN_WIDTH, nn);
            read += fread(network.featureBiases, sizeof(int16_t), HIDDEN_WIDTH, nn);
            read += fread(network.outputWeights, sizeof(int16_t), HIDDEN_WIDTH * 2, nn);
            read += fread(&network.outputBias, sizeof(int16_t), 1, nn);

            if (std::abs((int64_t) read - (int64_t) objectsExpected) >= ALIGNMENT) {
                std::cout << "Error loading the net, aborting ";
                std::cout << "Expected " << objectsExpected << " shorts, got " << read << "\n";
                exit(1);
            }

            // after reading the config we can close the file
            fclose(nn);
        }
        else {
            memcpy(&network, gNETWORKData, sizeof(network));
        }

        // Cache feature indexes
        for (Color c = COLOR_WHITE; c <= COLOR_BLACK; c++) {
            for (int piece = PIECE_PAWN; piece < PIECE_TYPES; piece++) {
                for (Square square = 0; square < 64; square++) {
                    int pieceIndex = piece * 2 + c;

                    int relativeSquareUs = (square ^ (c * 56));
                    FEATURES[c][pieceIndex][square] = 64 * piece + relativeSquareUs;
                    int relativeSquareThem = (square ^ ((1 - c) * 56));
                    FEATURES[1 - c][pieceIndex][square] = 64 * (piece + 6) + relativeSquareThem;

                    FEATURES[c][pieceIndex][square] *= HIDDEN_WIDTH;
                    FEATURES[1 - c][pieceIndex][square] *= HIDDEN_WIDTH;
                }
            }
        }

    }

    Eval evaluate(Accumulator& accumulator, Color sideToMove) {

        Vec* stmAcc = (Vec*)accumulator.colors[sideToMove];
        Vec* oppAcc = (Vec*)accumulator.colors[1 - sideToMove];

        Vec* stmWeights = (Vec*)&network.outputWeights[0];
        Vec* oppWeights = (Vec*)&network.outputWeights[HIDDEN_WIDTH];

        const Vec reluClipMin = vecSetZero();
        const Vec reluClipMax = vecSet1Epi16(NETWORK_QA);

        Vec sum = vecSetZero();
        Vec reg;

        for (int i = 0; i < HIDDEN_WIDTH / WEIGHTS_PER_VEC; ++i) {
            // Side to move
            reg = maxEpi16(stmAcc[i], reluClipMin); // clip
            reg = minEpi16(reg, reluClipMax); // clip
            reg = mulloEpi16(reg, reg); // square
            reg = maddEpi16(reg, stmWeights[i]); // multiply with output layer
            sum = addEpi32(sum, reg); // collect the result

            // Non side to move
            reg = maxEpi16(oppAcc[i], reluClipMin);
            reg = minEpi16(reg, reluClipMax);
            reg = mulloEpi16(reg, reg);
            reg = maddEpi16(reg, oppWeights[i]);
            sum = addEpi32(sum, reg);
        }

        int unsquared = vecHaddEpi32(sum) / NETWORK_QA + network.outputBias;

        return (Eval)((unsquared * NETWORK_SCALE) / NETWORK_QAB);
    }

}

NNUE::Accumulator accumulatorStack[MAX_PLY];
int accumulatorStackHead;