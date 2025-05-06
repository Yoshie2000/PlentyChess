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

constexpr int INPUT_SIZE = 768;
constexpr int L1_SIZE = 1536;
constexpr int L2_SIZE = 16;
constexpr int L3_SIZE = 32;

constexpr bool KING_BUCKETS_FACTORIZED = true;
constexpr int KING_BUCKETS = 13;
constexpr int OUTPUT_BUCKETS = 8;

constexpr int INPUT_QUANT = 255;
constexpr int L1_QUANT = 128;

constexpr int ALIGNMENT = 64;
constexpr int INT8_PER_INT32 = sizeof(int32_t) / sizeof(int8_t);

struct RawNetworkData {
    float inputWeights[KING_BUCKETS + static_cast<int>(KING_BUCKETS_FACTORIZED)][INPUT_SIZE * L1_SIZE];
    float inputBiases[L1_SIZE];
    float l1Weights[L1_SIZE][OUTPUT_BUCKETS][L2_SIZE];
    float l1Biases[OUTPUT_BUCKETS][L2_SIZE];
    float l2Weights[2 * L2_SIZE][OUTPUT_BUCKETS][L3_SIZE];
    float l2Biases[OUTPUT_BUCKETS][L3_SIZE];
    float l3Weights[L3_SIZE][OUTPUT_BUCKETS];
    float l3Biases[OUTPUT_BUCKETS];
};

struct NetworkData {
    alignas(ALIGNMENT) int16_t inputWeights[KING_BUCKETS][INPUT_SIZE * L1_SIZE];
    alignas(ALIGNMENT) int16_t inputBiases[L1_SIZE];
    alignas(ALIGNMENT) int8_t l1Weights[OUTPUT_BUCKETS][L1_SIZE * L2_SIZE];
    alignas(ALIGNMENT) float l1Biases[OUTPUT_BUCKETS][L2_SIZE];
    alignas(ALIGNMENT) float l2Weights[OUTPUT_BUCKETS][2 * L2_SIZE * L3_SIZE];
    alignas(ALIGNMENT) float l2Biases[OUTPUT_BUCKETS][L3_SIZE];
    alignas(ALIGNMENT) float l3Weights[OUTPUT_BUCKETS][L3_SIZE];
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
    for (int n = 0; n < KING_BUCKETS; n++) {
        for (int w = 0; w < INPUT_SIZE * L1_SIZE; w++) {
            if (KING_BUCKETS_FACTORIZED) {
                tmp.inputWeights[n][w] = quantize<INPUT_QUANT>(raw.inputWeights[n + 1][w] + raw.inputWeights[0][w]);
            }
            else {
                tmp.inputWeights[n][w] = quantize<INPUT_QUANT>(raw.inputWeights[n][w]);
            }
        }
    }

    // Quantize input biases
    for (int b = 0; b < L1_SIZE; b++) {
        tmp.inputBiases[b] = quantize<INPUT_QUANT>(raw.inputBiases[b]);
    }

    // Quantize L1 weights
    for (int b = 0; b < OUTPUT_BUCKETS; b++) {
        for (int l1 = 0; l1 < L1_SIZE; l1++) {
            for (int l2 = 0; l2 < L2_SIZE; l2++) {
                tmp.l1Weights[b][l2 * L1_SIZE + l1] = quantize<L1_QUANT>(((float*)raw.l1Weights)[b * L1_SIZE * L2_SIZE + l2 * L1_SIZE + l1]);
            }
        }
    }

    // std::memcpy the rest
    std::memcpy(tmp.l1Biases, raw.l1Biases, sizeof(raw.l1Biases));
    std::memcpy(tmp.l2Weights, raw.l2Weights, sizeof(raw.l2Weights));
    std::memcpy(tmp.l2Biases, raw.l2Biases, sizeof(raw.l2Biases));
    std::memcpy(tmp.l3Weights, raw.l3Weights, sizeof(raw.l3Weights));
    std::memcpy(tmp.l3Biases, raw.l3Biases, sizeof(raw.l3Biases));
}

void transposePermuteNetwork() {
#if defined(__AVX2__) || (defined(__AVX512F__) && defined(__AVX512BW__))
    // Transpose input weights for packus
    constexpr int weightsPerBlock = sizeof(__m128i) / sizeof(int16_t);
#if (defined(__AVX512F__) && defined(__AVX512BW__))
    constexpr int packusBlocks = 8;
    constexpr int permutation[packusBlocks] = { 0, 2, 4, 6, 1, 3, 5, 7 };
#else
    constexpr int packusBlocks = 4;
    constexpr int permutation[packusBlocks] = { 0, 2, 1, 3 };
#endif
    __m128i regs[packusBlocks];

    for (int limit : { 1, KING_BUCKETS* INPUT_SIZE }) {
        __m128i* vec = reinterpret_cast<__m128i*>(limit == 1 ? (int16_t*) tmp.inputBiases : (int16_t*) tmp.inputWeights);

        for (int i = 0; i < limit * L1_SIZE / weightsPerBlock; i += packusBlocks) {
            for (int j = 0; j < packusBlocks; j++)
                regs[j] = vec[i + j];

            for (int j = 0; j < packusBlocks; j++)
                vec[i + j] = regs[permutation[j]];
        }
    }
#endif

    // Transpose L1 / L2 / L3 weights
    for (int b = 0; b < OUTPUT_BUCKETS; b++) {
        for (int l1 = 0; l1 < L1_SIZE; l1 += INT8_PER_INT32) {
            for (int l2 = 0; l2 < L2_SIZE; l2++) {
                for (int c = 0; c < INT8_PER_INT32; c++) {
                    out.l1Weights[b][l1 * L2_SIZE + l2 * INT8_PER_INT32 + c] = reinterpret_cast<int8_t*>(tmp.l1Weights[b])[(l1 + c) * L2_SIZE + l2];
                }
            }
        }
    }

    // std::memcpy the rest
    std::memcpy(out.inputWeights, tmp.inputWeights, sizeof(tmp.inputWeights));
    std::memcpy(out.inputBiases, tmp.inputBiases, sizeof(tmp.inputBiases));
    std::memcpy(out.l1Biases, tmp.l1Biases, sizeof(tmp.l1Biases));
    std::memcpy(out.l2Weights, tmp.l2Weights, sizeof(tmp.l2Weights));
    std::memcpy(out.l2Biases, tmp.l2Biases, sizeof(tmp.l2Biases));
    std::memcpy(out.l3Weights, tmp.l3Weights, sizeof(tmp.l3Weights));
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
