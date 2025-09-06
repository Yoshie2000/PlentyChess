#include <cstdio>
#include <iostream>
#include <cstring>
#include <cmath>
#include <fstream>
#include <cstdint>

#if defined(ARCH_X86)
#include <immintrin.h>
#include <xmmintrin.h>
#else
#include <arm_neon.h>
#endif

constexpr int KING_BUCKETS = 12;
constexpr int OUTPUT_BUCKETS = 8;

constexpr int THREAT_INPUT_SIZE = 79856;
constexpr int INPUT_SIZE = THREAT_INPUT_SIZE + KING_BUCKETS * 768;
constexpr int L1_SIZE = 256;
constexpr int L2_SIZE = 16;
constexpr int L3_SIZE = 32;

constexpr int INPUT_QUANT = 255;
constexpr int L1_QUANT = 64;

constexpr int ALIGNMENT = 64;
constexpr int INT8_PER_INT32 = sizeof(int32_t) / sizeof(int8_t);

struct RawNetworkData {
    float inputWeights[(INPUT_SIZE + 768 /* factoriser bucket at the beginning */) * L1_SIZE];
    float inputBiases[L1_SIZE];
    float l1Weights[2 * L1_SIZE][OUTPUT_BUCKETS][L2_SIZE];
    float l1Biases[OUTPUT_BUCKETS][L2_SIZE];
    float l2Weights[2 * L2_SIZE][OUTPUT_BUCKETS][L3_SIZE];
    float l2Biases[OUTPUT_BUCKETS][L3_SIZE];
    float l3Weights[L3_SIZE + 2 * L2_SIZE][OUTPUT_BUCKETS];
    float l3Biases[OUTPUT_BUCKETS];
};

struct NetworkData {
    alignas(ALIGNMENT) int16_t inputWeights[INPUT_SIZE * L1_SIZE];
    alignas(ALIGNMENT) int16_t inputBiases[L1_SIZE];
    alignas(ALIGNMENT) int8_t l1Weights[OUTPUT_BUCKETS][2 * L1_SIZE * L2_SIZE];
    alignas(ALIGNMENT) float l1Biases[OUTPUT_BUCKETS][L2_SIZE];
    alignas(ALIGNMENT) float l2Weights[OUTPUT_BUCKETS][2 * L2_SIZE * L3_SIZE];
    alignas(ALIGNMENT) float l2Biases[OUTPUT_BUCKETS][L3_SIZE];
    alignas(ALIGNMENT) float l3Weights[OUTPUT_BUCKETS][L3_SIZE + 2 * L2_SIZE];
    alignas(ALIGNMENT) float l3Biases[OUTPUT_BUCKETS];
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
    for (int w = 0; w < THREAT_INPUT_SIZE * L1_SIZE; w++) {
        tmp.inputWeights[w] = quantize<INPUT_QUANT>(raw.inputWeights[w + 768 * L1_SIZE /* factoriser bucket at the beginning */]);
    }
    for (int kb = 0; kb < KING_BUCKETS; kb++) {
        for (int w = 0; w < 768 * L1_SIZE; w++) {
            int idx = THREAT_INPUT_SIZE * L1_SIZE + kb * 768 * L1_SIZE + w;
            int rawIdx = 768 * L1_SIZE + idx; // skip factoriser
            int factoriserIdx = w;
            tmp.inputWeights[idx] = quantize<INPUT_QUANT>(raw.inputWeights[rawIdx]) + quantize<INPUT_QUANT>(raw.inputWeights[factoriserIdx]);
        }
    }

    // Quantize input biases
    for (int b = 0; b < L1_SIZE; b++) {
        tmp.inputBiases[b] = quantize<INPUT_QUANT>(raw.inputBiases[b]);
    }

    // // Quantize L1 weights
    for (int b = 0; b < OUTPUT_BUCKETS; b++) {
        for (int l1 = 0; l1 < 2 * L1_SIZE; l1++) {
            for (int l2 = 0; l2 < L2_SIZE; l2++) {
                tmp.l1Weights[b][l2 * 2 * L1_SIZE + l1] = quantize<L1_QUANT>(((float*)raw.l1Weights)[b * 2 * L1_SIZE * L2_SIZE + l2 * 2 * L1_SIZE + l1]);
            }
        }
    }

    // std::memcpy the rest
    // std::memcpy(tmp.inputWeights, raw.inputWeights, sizeof(raw.inputWeights));
    // std::memcpy(tmp.inputBiases, raw.inputBiases, sizeof(raw.inputBiases));
    // std::memcpy(tmp.l1Weights, raw.l1Weights, sizeof(raw.l1Weights));
    std::memcpy(tmp.l1Biases, raw.l1Biases, sizeof(raw.l1Biases));
    std::memcpy(tmp.l2Weights, raw.l2Weights, sizeof(raw.l2Weights));
    std::memcpy(tmp.l2Biases, raw.l2Biases, sizeof(raw.l2Biases));
    std::memcpy(tmp.l3Weights, raw.l3Weights, sizeof(raw.l3Weights));
    std::memcpy(tmp.l3Biases, raw.l3Biases, sizeof(raw.l3Biases));
}

void transposePermuteNetwork() {
    // Transpose L1 / L2 / L3 weights
    for (int b = 0; b < OUTPUT_BUCKETS; b++) {
#if defined(__SSSE3__) || defined(__AVX2__) || (defined(__AVX512F__) && defined(__AVX512BW__)) || defined(ARCH_ARM)
        for (int l1 = 0; l1 < 2 * L1_SIZE / INT8_PER_INT32; l1++) {
            for (int l2 = 0; l2 < L2_SIZE; l2++) {
                for (int c = 0; c < INT8_PER_INT32; c++) {
                    out.l1Weights[b][l1 * INT8_PER_INT32 * L2_SIZE + l2 * INT8_PER_INT32 + c] = reinterpret_cast<int8_t*>(tmp.l1Weights)[(l1 * INT8_PER_INT32 + c) * OUTPUT_BUCKETS * L2_SIZE + b * L2_SIZE + l2];
                }
            }
        }
#else
        for (int l1 = 0; l1 < 2 * L1_SIZE; l1++) {
            for (int l2 = 0; l2 < L2_SIZE; l2++) {
                out.l1Weights[b][l1 * L2_SIZE + l2] = reinterpret_cast<int8_t*>(tmp.l1Weights)[l1 * OUTPUT_BUCKETS * L2_SIZE + b * L2_SIZE + l2];
            }
        }
#endif

        for (int l2 = 0; l2 < 2 * L2_SIZE; l2++) {
            for (int l3 = 0; l3 < L3_SIZE; l3++) {
                out.l2Weights[b][l2 * L3_SIZE + l3] = reinterpret_cast<float*>(tmp.l2Weights)[l2 * OUTPUT_BUCKETS * L3_SIZE + b * L3_SIZE + l3];
            }
        }

        for (int l3 = 0; l3 < L3_SIZE + 2 * L2_SIZE; l3++) {
            out.l3Weights[b][l3] = reinterpret_cast<float*>(tmp.l3Weights)[l3 * OUTPUT_BUCKETS + b];
        }
    }

    // std::memcpy the rest
    std::memcpy(out.inputWeights, tmp.inputWeights, sizeof(tmp.inputWeights));
    std::memcpy(out.inputBiases, tmp.inputBiases, sizeof(tmp.inputBiases));
    std::memcpy(out.l1Biases, tmp.l1Biases, sizeof(tmp.l1Biases));
    std::memcpy(out.l2Biases, tmp.l2Biases, sizeof(tmp.l2Biases));
    std::memcpy(out.l3Biases, tmp.l3Biases, sizeof(tmp.l3Biases));
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