#include <stdio.h>
#include <cstdio>
#include <iostream>
#include <string.h>

constexpr int INPUT_WIDTH = 768;
constexpr int HIDDEN_WIDTH = 768;

constexpr bool KING_BUCKETS_FACTORIZED = false;
constexpr int KING_BUCKETS = 1;
constexpr int OUTPUT_BUCKETS = 1;

constexpr int NETWORK_SCALE = 400;
constexpr int NETWORK_QA = 255;
constexpr int NETWORK_QB = 64;
constexpr int NETWORK_QAB = NETWORK_QA * NETWORK_QB;

struct Network {
    float featureWeights[KING_BUCKETS + static_cast<size_t>(KING_BUCKETS_FACTORIZED)][INPUT_WIDTH * HIDDEN_WIDTH];
    float featureBiases[HIDDEN_WIDTH];
    float outputWeights[OUTPUT_BUCKETS][2 * HIDDEN_WIDTH];
    float outputBiases[OUTPUT_BUCKETS];
};

struct QuantizedNetwork {
  int16_t featureWeights[KING_BUCKETS][INPUT_WIDTH * HIDDEN_WIDTH];
  int16_t featureBiases[HIDDEN_WIDTH];
  int16_t outputWeights[OUTPUT_BUCKETS][2 * HIDDEN_WIDTH];
  int16_t outputBiases[OUTPUT_BUCKETS];
};

Network network;
QuantizedNetwork out;

template<int Q>
int16_t quantize(float number) {
    return static_cast<int16_t>(static_cast<double>(number) * static_cast<double>(Q));
}

int main(void) {
    FILE* nn = fopen("../params.bin", "rb");

    if (nn) {
        // Read network from file
        size_t read = 0;
        size_t objectsExpected = sizeof(network) / sizeof(float);

        read += fread(network.featureWeights, sizeof(float), sizeof(network.featureWeights) / sizeof(float), nn);
        read += fread(network.featureBiases, sizeof(float), sizeof(network.featureBiases) / sizeof(float), nn);
        read += fread(network.outputWeights, sizeof(float), sizeof(network.outputWeights) / sizeof(float), nn);
        read += fread(&network.outputBiases, sizeof(float), sizeof(network.outputBiases) / sizeof(float), nn);

        if (std::abs((int64_t)read - (int64_t)objectsExpected) > 0) {
            std::cout << "Error loading the net, aborting ";
            std::cout << "Expected " << objectsExpected << " floats, got " << read << "\n";
            return -1;
        }

        fclose(nn);
    }
    else {
        std::cout << "Network file could not be found" << std::endl;
        return -1;
    }

    // Transpose output weights
    float transposed[OUTPUT_BUCKETS][2 * HIDDEN_WIDTH];
    for (int n = 0; n < OUTPUT_BUCKETS * 2 * HIDDEN_WIDTH; n++) {
        transposed[n % OUTPUT_BUCKETS][n / OUTPUT_BUCKETS] = network.outputWeights[n / (2 * HIDDEN_WIDTH)][n % (2 * HIDDEN_WIDTH)];
    }
    memcpy(network.outputWeights, transposed, sizeof(transposed));

    // Add factorized weights to buckets, and quantize
    for (int n = 0; n < KING_BUCKETS; n++) {
        for (int w = 0; w < INPUT_WIDTH * HIDDEN_WIDTH; w++) {
            if (KING_BUCKETS_FACTORIZED) {
                out.featureWeights[n][w] = quantize<NETWORK_QA>(network.featureWeights[n + 1][w] + network.featureWeights[0][w]);
            } else {
                out.featureWeights[n][w] = quantize<NETWORK_QA>(network.featureWeights[n][w]);
            }
        }
    }

    // Quantize feature biases
    for (int b = 0; b < HIDDEN_WIDTH; b++) {
        out.featureBiases[b] = quantize<NETWORK_QA>(network.featureBiases[b]);
    }

    // Quantize output weights and biases
    for (int b = 0; b < OUTPUT_BUCKETS; b++) {
        out.outputBiases[b] = quantize<NETWORK_QAB>(network.outputBiases[b]);
        for (int w = 0; w < 2 * HIDDEN_WIDTH; w++) {
            out.outputWeights[b][w] = quantize<NETWORK_QB>(network.outputWeights[b][w]);
        }
    }

    // Write the network
    nn = fopen("../network.bin", "wb");
    if (nn) {
        // Read network from file
        size_t written = 0;
        size_t objectsExpected = sizeof(out) / sizeof(int16_t);

        written += fwrite(out.featureWeights, sizeof(int16_t), sizeof(out.featureWeights) / sizeof(int16_t), nn);
        written += fwrite(out.featureBiases, sizeof(int16_t), sizeof(out.featureBiases) / sizeof(int16_t), nn);
        written += fwrite(out.outputWeights, sizeof(int16_t), sizeof(out.outputWeights) / sizeof(int16_t), nn);
        written += fwrite(&out.outputBiases, sizeof(int16_t), sizeof(out.outputBiases) / sizeof(int16_t), nn);

        if (std::abs((int64_t)written - (int64_t)objectsExpected) > 0) {
            std::cout << "Error writing the net, aborting ";
            std::cout << "Expected " << objectsExpected << " shorts, got " << written << "\n";
            return -1;
        }

        fclose(nn);
    }
    else {
        std::cout << "Network file could not be created" << std::endl;
        return -1;
    }

}