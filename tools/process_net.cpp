#include <cstdio>
#include <iostream>
#include <cstring>
#include <cmath>
#include <fstream>
#include <cstdint>

#include <immintrin.h>
#include <xmmintrin.h>

constexpr int INPUT_SIZE = 80624;
constexpr int L1_SIZE = 384;

constexpr int OUTPUT_BUCKETS = 1;

constexpr int INPUT_QUANT = 510;
constexpr int L1_QUANT = 64;

constexpr int ALIGNMENT = 64;
constexpr int INT8_PER_INT32 = sizeof(int32_t) / sizeof(int8_t);

struct RawNetworkData {
    float inputWeights[INPUT_SIZE * L1_SIZE];
    float inputBiases[L1_SIZE];
    float l1Weights[2 * L1_SIZE][OUTPUT_BUCKETS];
    float l1Biases[OUTPUT_BUCKETS];
};

struct NetworkData {
    alignas(ALIGNMENT) int16_t inputWeights[INPUT_SIZE * L1_SIZE];
    alignas(ALIGNMENT) int16_t inputBiases[L1_SIZE];
    alignas(ALIGNMENT) int16_t l1Weights[OUTPUT_BUCKETS][2 * L1_SIZE];
    alignas(ALIGNMENT) int16_t l1Biases[OUTPUT_BUCKETS];
};

RawNetworkData raw;
NetworkData tmp;
NetworkData out;

template<int Q>
int16_t quantize(float number) {
    return static_cast<int16_t>(std::round(number * static_cast<float>(Q)));
}

void quantizeNetwork() {
    // Add factorized input weights to buckets, and quantize
    for (int w = 0; w < INPUT_SIZE * L1_SIZE; w++) {
        tmp.inputWeights[w] = quantize<INPUT_QUANT>(raw.inputWeights[w]);
    }

    // Quantize input biases
    for (int b = 0; b < L1_SIZE; b++) {
        tmp.inputBiases[b] = quantize<INPUT_QUANT>(raw.inputBiases[b]);
    }

    // Quantize L1 weights and biases
    for (int b = 0; b < OUTPUT_BUCKETS; b++) {
        for (int l1 = 0; l1 < L1_SIZE * 2; l1++) {
            tmp.l1Weights[b][l1] = quantize<L1_QUANT>(((float*)raw.l1Weights)[b * L1_SIZE * 2 + l1]);
        }
        tmp.l1Biases[b] = quantize<L1_QUANT * INPUT_QUANT>(raw.l1Biases[b]);

        // for (int i = 0; i < L1_SIZE; i++) {
        //     std::cout << "(" << int(tmp.l1Weights[b][i]) << " " << int(tmp.l1Weights[b][i + L1_SIZE]) << "), ";
        // }
        // std::cout << std::endl;
    }
}

void transposePermuteNetwork() {
    for (int b = 0; b < OUTPUT_BUCKETS; b++) {
        for (int l1 = 0; l1 < L1_SIZE * 2; l1++) {
            out.l1Weights[b][l1] = reinterpret_cast<int16_t*>(tmp.l1Weights)[l1 * OUTPUT_BUCKETS + b];
        }
        // for (int i = 0; i < L1_SIZE; i++) {
        //     std::cout << "(" << int(out.l1Weights[b][i]) << " " << int(out.l1Weights[b][i + L1_SIZE]) << "), ";
        // }
        // std::cout << std::endl;
    }

    // std::memcpy the rest
    std::memcpy(out.inputWeights, tmp.inputWeights, sizeof(tmp.inputWeights));
    std::memcpy(out.inputBiases, tmp.inputBiases, sizeof(tmp.inputBiases));
    std::memcpy(out.l1Biases, tmp.l1Biases, sizeof(tmp.l1Biases));
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <infile_is_floats> <infile> <outfile>\n";
        return -1;
    }
    std::string infile_is_floats = argv[1];
    bool in_is_floats = infile_is_floats == "true";
    std::string infile_name = argv[2];
    std::string outfilefile_name = argv[3];

    if (in_is_floats) {
        // Read the network
        std::ifstream infile(infile_name, std::ios::binary);
        if (!infile) {
            std::cerr << "Error opening float file for reading (" << infile_name << ")" << std::endl;
            return -1;
        }
        infile.read(reinterpret_cast<char*>(&raw), sizeof(raw));
        infile.close();

        quantizeNetwork();

        // Write the network
        std::ofstream outfile("quantised.bin", std::ios::binary);
        if (!outfile) {
            std::cerr << "Error opening quantised file for writing" << std::endl;
            return -1;
        }
        outfile.write(reinterpret_cast<char*>(&tmp), sizeof(tmp));
        outfile.close();
    }
    else {
        // Read the network
        std::ifstream infile(infile_name, std::ios::binary);
        if (!infile) {
            std::cerr << "Error opening quantised file for reading (" << infile_name << ")" << std::endl;
            return -1;
        }
        infile.read(reinterpret_cast<char*>(&tmp), sizeof(tmp));
        infile.close();
    }

    transposePermuteNetwork();

    // Write the network
    std::ofstream outfile(outfilefile_name, std::ios::binary);
    if (!outfile) {
        std::cerr << "Error opening file for writing" << std::endl;
        return -1;
    }
    outfile.write(reinterpret_cast<char*>(&out), sizeof(out));
    outfile.close();
}
