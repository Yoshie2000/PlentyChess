#include <cstdio>
#include <iostream>
#include <cstring>
#include <cmath>
#include <fstream>
#include <cstdint>
#include <algorithm>

#if defined(ARCH_X86)
#include <immintrin.h>
#include <xmmintrin.h>
#endif

constexpr int KING_BUCKETS = 10;
constexpr int OUTPUT_BUCKETS = 1;

constexpr int THREAT_INPUT_SIZE = 79856;
constexpr int INPUT_SIZE = THREAT_INPUT_SIZE + KING_BUCKETS * 768;
constexpr int L1_SIZE = 512;
constexpr int L2_SIZE = 16;
constexpr int L3_SIZE = 32;

constexpr int INPUT_QUANT = 255;
constexpr int L1_QUANT = 64;

constexpr int ALIGNMENT = 64;
constexpr int INT8_PER_INT32 = sizeof(int32_t) / sizeof(int8_t);

struct RawNetworkData {
    float inputWeights[(INPUT_SIZE + 768 /* factoriser bucket at the beginning */) * L1_SIZE];
    float inputBiases[L1_SIZE];
    float l1Weights[L1_SIZE][OUTPUT_BUCKETS][L2_SIZE];
    float l1Biases[OUTPUT_BUCKETS][L2_SIZE];
    float l2Weights[2 * L2_SIZE][OUTPUT_BUCKETS][L3_SIZE];
    float l2Biases[OUTPUT_BUCKETS][L3_SIZE];
    float l3Weights[L3_SIZE + 2 * L2_SIZE][OUTPUT_BUCKETS];
    float l3Biases[OUTPUT_BUCKETS];
};

struct NetworkData {
    alignas(ALIGNMENT) int8_t inputThreatWeights[THREAT_INPUT_SIZE * L1_SIZE];
    alignas(ALIGNMENT) int16_t inputPsqWeights[768 * KING_BUCKETS * L1_SIZE];
    alignas(ALIGNMENT) int16_t inputBiases[L1_SIZE];
    alignas(ALIGNMENT) int8_t l1Weights[OUTPUT_BUCKETS][L1_SIZE * L2_SIZE];
    alignas(ALIGNMENT) float l1Biases[OUTPUT_BUCKETS][L2_SIZE];
    alignas(ALIGNMENT) float l2Weights[OUTPUT_BUCKETS][L2_SIZE * L3_SIZE];
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

uint64_t readULEB128(std::istream& is) {
    constexpr int MAX_ITER = 1000000;
    int iter = 0;

    uint64_t result = 0;
    int shift = 0;
    while (true) {
        uint8_t byte = static_cast<uint8_t>(is.get());
        result |= (uint64_t)(byte & 0x7F) << shift;
        if ((byte & 0x80) == 0 || ++iter >= MAX_ITER) break;
        shift += 7;
    }
    return result;
}

int64_t readSLEB128(std::istream& is) {
    constexpr int MAX_ITER = 1000000;
    int iter = 0;

    int64_t result = 0;
    int shift = 0;
    uint8_t byte;
    do {
        byte = static_cast<uint8_t>(is.get());
        result |= (int64_t)(byte & 0x7F) << shift;
        shift += 7;
    } while ((byte & 0x80) && ++iter < MAX_ITER);
    if ((shift < 64) && (byte & 0x40))
        result |= -((int64_t)1 << shift);
    return result;
}

float readFloat(std::istream& is) {
    uint32_t v = static_cast<uint32_t>(readULEB128(is));
    float f;
    std::memcpy(&f, &v, sizeof(f));
    return f;
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

    for (int limit : { 1, 768 * KING_BUCKETS }) {
        __m128i* vec = reinterpret_cast<__m128i*>(limit == 1 ? (int16_t*)tmp.inputBiases : (int16_t*)tmp.inputPsqWeights);

        for (int i = 0; i < limit * L1_SIZE / weightsPerBlock; i += packusBlocks) {
            for (int j = 0; j < packusBlocks; j++)
                regs[j] = vec[i + j];

            for (int j = 0; j < packusBlocks; j++)
                vec[i + j] = regs[permutation[j]];
        }
    }

    uint64_t* weightsVec = reinterpret_cast<uint64_t*>(tmp.inputThreatWeights);
    uint64_t weightsRegs[packusBlocks];
    for (int i = 0; i < THREAT_INPUT_SIZE * L1_SIZE / weightsPerBlock; i += packusBlocks) {
        for (int j = 0; j < packusBlocks; j++)
            weightsRegs[j] = weightsVec[i + j];

        for (int j = 0; j < packusBlocks; j++)
            weightsVec[i + j] = weightsRegs[permutation[j]];
    }
#endif

    // std::memcpy the rest
    std::memcpy(out.inputPsqWeights, tmp.inputPsqWeights, sizeof(tmp.inputPsqWeights));
    std::memcpy(out.inputThreatWeights, tmp.inputThreatWeights, sizeof(tmp.inputThreatWeights));
    std::memcpy(out.inputBiases, tmp.inputBiases, sizeof(tmp.inputBiases));
    std::memcpy(out.l1Weights, tmp.l1Weights, sizeof(tmp.l1Weights));
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
        // Read the compressed network
        std::ifstream infile(infile_name, std::ios::binary);
        if (!infile) {
            std::cerr << "Error opening compressed file for reading (" << infile_name << ")" << std::endl;
            return -1;
        }
        infile.read(reinterpret_cast<char*>(&tmp), sizeof(tmp));
        infile.close();
    }

    transposePermuteNetwork();

    // Write the network
    std::ofstream outfile(outfilefile_name, std::ios::binary);
    if (!outfile) {
        std::cerr << "Error opening output file for writing" << std::endl;
        return -1;
    }
    outfile.write(reinterpret_cast<char*>(&out), sizeof(out));
    outfile.close();
}