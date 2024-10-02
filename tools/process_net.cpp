#include <stdio.h>
#include <cstdio>
#include <iostream>
#include <string.h>
#include <cmath>

constexpr int INPUT_SIZE = 768;
constexpr int L1_SIZE = 1536;
constexpr int L2_SIZE = 16;
constexpr int L3_SIZE = 32;

constexpr bool KING_BUCKETS_FACTORIZED = true;
constexpr int KING_BUCKETS = 7;
constexpr int OUTPUT_BUCKETS = 8;

constexpr int NETWORK_SCALE = 400;
constexpr int INPUT_QUANT = 362;
constexpr int L1_QUANT = 32;

struct Network {
    float inputWeights[KING_BUCKETS][INPUT_SIZE * L1_SIZE];
    float inputBiases [L1_SIZE];
    float l1Weights   [L1_SIZE][OUTPUT_BUCKETS][L2_SIZE];
    float l1Biases    [OUTPUT_BUCKETS][L2_SIZE];
    float l2Weights   [L2_SIZE][OUTPUT_BUCKETS][L3_SIZE];
    float l2Biases    [OUTPUT_BUCKETS][L3_SIZE];
    float l3Weights   [L3_SIZE][OUTPUT_BUCKETS];
    float l3Biases    [OUTPUT_BUCKETS];
};

struct QuantizedNetwork {
  int16_t inputWeights[KING_BUCKETS][INPUT_SIZE * L1_SIZE];
  int16_t inputBiases [L1_SIZE];
  int8_t  l1Weights   [OUTPUT_BUCKETS][L1_SIZE * L2_SIZE];
  float   l1Biases    [OUTPUT_BUCKETS][L2_SIZE];
  float   l2Weights   [OUTPUT_BUCKETS][L2_SIZE * L3_SIZE];
  float   l2Biases    [OUTPUT_BUCKETS][L3_SIZE];
  float   l3Weights   [OUTPUT_BUCKETS][L3_SIZE];
  float   l3Biases    [OUTPUT_BUCKETS];
};

Network network;
QuantizedNetwork out;

template<int Q>
int16_t quantize(float number) {
    return static_cast<int16_t>(std::round(number * static_cast<float>(Q)));
}

int main(void) {
    FILE* nn = fopen("../params.bin", "rb");

    if (nn) {
        // Read network from file
        size_t read = 0;
        size_t objectsExpected = sizeof(network) / sizeof(float);

        read += fread(network.inputWeights, sizeof(float), sizeof(network.inputWeights) / sizeof(float), nn);
        read += fread(network.inputBiases, sizeof(float), sizeof(network.inputBiases) / sizeof(float), nn);
        read += fread(network.l1Weights, sizeof(float), sizeof(network.l1Weights) / sizeof(float), nn);
        read += fread(network.l1Biases, sizeof(float), sizeof(network.l1Biases) / sizeof(float), nn);
        read += fread(network.l2Weights, sizeof(float), sizeof(network.l2Weights) / sizeof(float), nn);
        read += fread(network.l2Biases, sizeof(float), sizeof(network.l2Biases) / sizeof(float), nn);
        read += fread(network.l3Weights, sizeof(float), sizeof(network.l3Weights) / sizeof(float), nn);
        read += fread(network.l3Biases, sizeof(float), sizeof(network.l3Biases) / sizeof(float), nn);

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

    // Transpose L1/L2/L3 weights
    float transposedL1Weights[OUTPUT_BUCKETS][L1_SIZE * L2_SIZE];
    for (int b = 0; b < OUTPUT_BUCKETS; b++) {
        for (int l1 = 0; l1 < L1_SIZE; l1++) {
            for (int l2 = 0; l2 < L2_SIZE; l2++) {
                transposedL1Weights[b][l2 * L1_SIZE + l1] = network.l1Weights[l1][b][l2];
            }
        }
    }
    memcpy(network.l1Weights, transposedL1Weights, sizeof(transposedL1Weights));

    float transposedL2Weights[OUTPUT_BUCKETS][L2_SIZE * L3_SIZE];
    for (int b = 0; b < OUTPUT_BUCKETS; b++) {
        for (int l2 = 0; l2 < L2_SIZE; l2++) {
            for (int l3 = 0; l3 < L3_SIZE; l3++) {
                transposedL2Weights[b][l2 * L3_SIZE + l3] = network.l2Weights[l2][b][l3];
            }
        }
    }
    memcpy(network.l2Weights, transposedL2Weights, sizeof(transposedL2Weights));

    float transposedL3Weights[OUTPUT_BUCKETS][L3_SIZE];
    for (int b = 0; b < OUTPUT_BUCKETS; b++) {
        for (int l3 = 0; l3 < L3_SIZE; l3++) {
            transposedL3Weights[b][l3] = network.l3Weights[l3][b];
        }
    }
    memcpy(network.l3Weights, transposedL3Weights, sizeof(transposedL3Weights));

    // Add factorized input weights to buckets, and quantize
    for (int n = 0; n < KING_BUCKETS; n++) {
        for (int w = 0; w < INPUT_SIZE * L1_SIZE; w++) {
            if (KING_BUCKETS_FACTORIZED) {
                out.inputWeights[n][w] = quantize<INPUT_QUANT>(network.inputWeights[n + 1][w] + network.inputWeights[0][w]);
            } else {
                out.inputWeights[n][w] = quantize<INPUT_QUANT>(network.inputWeights[n][w]);
            }
        }
    }

    // Quantize input biases
    for (int b = 0; b < L1_SIZE; b++) {
        out.inputBiases[b] = quantize<INPUT_QUANT>(network.inputBiases[b]);
    }

    // Quantize L1 weights
    for (int b = 0; b < OUTPUT_BUCKETS; b++) {
        for (int l1 = 0; l1 < L1_SIZE; l1++) {
            for (int l2 = 0; l2 < L2_SIZE; l2++) {
                out.l1Weights[b][l2 * L1_SIZE + l1] = quantize<L1_QUANT>(((float*) network.l1Weights)[b * L1_SIZE * L2_SIZE + l2 * L1_SIZE + l1]);
            }
        }
    }

    // Memcpy the rest
    memcpy(out.l1Biases, network.l1Biases, sizeof(network.l1Biases));
    memcpy(out.l2Weights, network.l2Weights, sizeof(network.l2Weights));
    memcpy(out.l2Biases, network.l2Biases, sizeof(network.l2Biases));
    memcpy(out.l3Weights, network.l3Weights, sizeof(network.l3Weights));
    memcpy(out.l3Biases, network.l3Biases, sizeof(network.l3Biases));

    // Write the network
    nn = fopen("../network.bin", "wb");
    if (nn) {
        // Read network from file
        size_t written = 0;
        size_t objectsExpected = sizeof(out);

        written += sizeof(int16_t) * fwrite(out.inputWeights, sizeof(int16_t), sizeof(out.inputWeights) / sizeof(int16_t), nn);
        written += sizeof(int16_t) * fwrite(out.inputBiases, sizeof(int16_t), sizeof(out.inputBiases) / sizeof(int16_t), nn);
        written += sizeof(int8_t) * fwrite(out.l1Weights, sizeof(int8_t), sizeof(out.l1Weights) / sizeof(int8_t), nn);
        written += sizeof(float) * fwrite(out.l1Biases, sizeof(float), sizeof(out.l1Biases) / sizeof(float), nn);
        written += sizeof(float) * fwrite(out.l2Weights, sizeof(float), sizeof(out.l2Weights) / sizeof(float), nn);
        written += sizeof(float) * fwrite(out.l2Biases, sizeof(float), sizeof(out.l2Biases) / sizeof(float), nn);
        written += sizeof(float) * fwrite(out.l3Weights, sizeof(float), sizeof(out.l3Weights) / sizeof(float), nn);
        written += sizeof(float) * fwrite(out.l3Biases, sizeof(float), sizeof(out.l3Biases) / sizeof(float), nn);

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
