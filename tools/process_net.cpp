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

constexpr int KING_BUCKETS = 12;
constexpr int OUTPUT_BUCKETS = 8;

constexpr int THREAT_INPUT_SIZE = 79856;
constexpr int INPUT_SIZE_BIG = THREAT_INPUT_SIZE + KING_BUCKETS * 768;
constexpr int INPUT_SIZE_SMALL = (KING_BUCKETS + 1) * 768;
constexpr int L1_SIZE_BIG = 640;
constexpr int L1_SIZE_SMALL = 128;
constexpr int L2_SIZE = 16;
constexpr int L3_SIZE = 32;

constexpr int INPUT_QUANT = 255;
constexpr int L1_QUANT = 64;

constexpr int ALIGNMENT = 64;
constexpr int INT8_PER_INT32 = sizeof(int32_t) / sizeof(int8_t);

struct RawNetworkDataBig {
    float inputWeights[(INPUT_SIZE_BIG + 768 /* factoriser bucket at the beginning */) * L1_SIZE_BIG];
    float inputBiases[L1_SIZE_BIG];
    float l1Weights[L1_SIZE_BIG][OUTPUT_BUCKETS][L2_SIZE];
    float l1Biases[OUTPUT_BUCKETS][L2_SIZE];
    float l2Weights[2 * L2_SIZE][OUTPUT_BUCKETS][L3_SIZE];
    float l2Biases[OUTPUT_BUCKETS][L3_SIZE];
    float l3Weights[L3_SIZE + 2 * L2_SIZE][OUTPUT_BUCKETS];
    float l3Biases[OUTPUT_BUCKETS];
};

struct RawNetworkDataSmall {
    float inputWeights[INPUT_SIZE_SMALL * L1_SIZE_SMALL];
    float inputBiases[L1_SIZE_SMALL];
    float l1Weights[L1_SIZE_SMALL][OUTPUT_BUCKETS][L2_SIZE];
    float l1Biases[OUTPUT_BUCKETS][L2_SIZE];
    float l2Weights[2 * L2_SIZE][OUTPUT_BUCKETS][L3_SIZE];
    float l2Biases[OUTPUT_BUCKETS][L3_SIZE];
    float l3Weights[L3_SIZE + 2 * L2_SIZE][OUTPUT_BUCKETS];
    float l3Biases[OUTPUT_BUCKETS];
};

struct NetworkDataBig {
    alignas(ALIGNMENT) int16_t inputPsqWeights[768 * KING_BUCKETS * L1_SIZE_BIG];
    alignas(ALIGNMENT) int8_t inputThreatWeights[THREAT_INPUT_SIZE * L1_SIZE_BIG];
    alignas(ALIGNMENT) int16_t inputBiases[L1_SIZE_BIG];
    alignas(ALIGNMENT) int8_t l1Weights[OUTPUT_BUCKETS][L1_SIZE_BIG * L2_SIZE];
    alignas(ALIGNMENT) float l1Biases[OUTPUT_BUCKETS][L2_SIZE];
    alignas(ALIGNMENT) float l2Weights[OUTPUT_BUCKETS][2 * L2_SIZE * L3_SIZE];
    alignas(ALIGNMENT) float l2Biases[OUTPUT_BUCKETS][L3_SIZE];
    alignas(ALIGNMENT) float l3Weights[OUTPUT_BUCKETS][L3_SIZE + 2 * L2_SIZE];
    alignas(ALIGNMENT) float l3Biases[OUTPUT_BUCKETS];
};

struct NetworkDataSmall {
    alignas(ALIGNMENT) int16_t inputWeights[768 * KING_BUCKETS * L1_SIZE_SMALL];
    alignas(ALIGNMENT) int16_t inputBiases[L1_SIZE_SMALL];
    alignas(ALIGNMENT) int8_t l1Weights[OUTPUT_BUCKETS][L1_SIZE_SMALL * L2_SIZE];
    alignas(ALIGNMENT) float l1Biases[OUTPUT_BUCKETS][L2_SIZE];
    alignas(ALIGNMENT) float l2Weights[OUTPUT_BUCKETS][2 * L2_SIZE * L3_SIZE];
    alignas(ALIGNMENT) float l2Biases[OUTPUT_BUCKETS][L3_SIZE];
    alignas(ALIGNMENT) float l3Weights[OUTPUT_BUCKETS][L3_SIZE + 2 * L2_SIZE];
    alignas(ALIGNMENT) float l3Biases[OUTPUT_BUCKETS];
};

struct NetworkData {
    NetworkDataBig bigNet;
    NetworkDataSmall smallNet;
};

RawNetworkDataBig rawBig;
RawNetworkDataSmall rawSmall;

NetworkDataBig tmpBig;
NetworkDataSmall tmpSmall;

NetworkDataBig outBig;
NetworkDataSmall outSmall;

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

void quantizeNetwork() {
    // Add factorized input weights to buckets, and quantize
    for (int w = 0; w < THREAT_INPUT_SIZE * L1_SIZE_BIG; w++) {
        tmpBig.inputThreatWeights[w] = std::clamp<int16_t>(quantize<INPUT_QUANT>(rawBig.inputWeights[w + 768 * L1_SIZE_BIG /* factoriser bucket at the beginning */]), -128, 127);
    }
    for (int kb = 0; kb < KING_BUCKETS; kb++) {
        for (int w = 0; w < 768 * L1_SIZE_BIG; w++) {
            int psqIdx = kb * 768 * L1_SIZE_BIG + w;
            int idx = THREAT_INPUT_SIZE * L1_SIZE_BIG + psqIdx;
            int rawIdx = 768 * L1_SIZE_BIG + idx; // skip factoriser
            int factoriserIdx = w;
            tmpBig.inputPsqWeights[psqIdx] = quantize<INPUT_QUANT>(rawBig.inputWeights[rawIdx]) + quantize<INPUT_QUANT>(rawBig.inputWeights[factoriserIdx]);
        }
    }

    // Quantize input biases
    for (int b = 0; b < L1_SIZE_BIG; b++) {
        tmpBig.inputBiases[b] = quantize<INPUT_QUANT>(rawBig.inputBiases[b]);
    }

    // Quantize L1 weights
    for (int b = 0; b < OUTPUT_BUCKETS; b++) {
        for (int l1 = 0; l1 < L1_SIZE_BIG; l1++) {
            for (int l2 = 0; l2 < L2_SIZE; l2++) {
                tmpBig.l1Weights[b][l2 * L1_SIZE_BIG + l1] = quantize<L1_QUANT>(((float*)rawBig.l1Weights)[b * L1_SIZE_BIG * L2_SIZE + l2 * L1_SIZE_BIG + l1]);
            }
        }
    }

    // std::memcpy the rest
    std::memcpy(tmpBig.l1Biases, rawBig.l1Biases, sizeof(rawBig.l1Biases));
    std::memcpy(tmpBig.l2Weights, rawBig.l2Weights, sizeof(rawBig.l2Weights));
    std::memcpy(tmpBig.l2Biases, rawBig.l2Biases, sizeof(rawBig.l2Biases));
    std::memcpy(tmpBig.l3Weights, rawBig.l3Weights, sizeof(rawBig.l3Weights));
    std::memcpy(tmpBig.l3Biases, rawBig.l3Biases, sizeof(rawBig.l3Biases));

    // Small net
    for (int kb = 0; kb < KING_BUCKETS; kb++) {
        for (int w = 0; w < 768 * L1_SIZE_SMALL; w++) {
            int psqIdx = kb * 768 * L1_SIZE_SMALL + w;
            tmpSmall.inputWeights[psqIdx] = quantize<INPUT_QUANT>(rawSmall.inputWeights[psqIdx + 768 * L1_SIZE_SMALL]) + quantize<INPUT_QUANT>(rawSmall.inputWeights[w]);
        }
    }

    // Quantize input biases
    for (int b = 0; b < L1_SIZE_SMALL; b++) {
        tmpSmall.inputBiases[b] = quantize<INPUT_QUANT>(rawSmall.inputBiases[b]);
    }

    // Quantize L1 weights
    for (int b = 0; b < OUTPUT_BUCKETS; b++) {
        for (int l1 = 0; l1 < L1_SIZE_SMALL; l1++) {
            for (int l2 = 0; l2 < L2_SIZE; l2++) {
                tmpSmall.l1Weights[b][l2 * L1_SIZE_SMALL + l1] = quantize<L1_QUANT>(((float*)rawSmall.l1Weights)[b * L1_SIZE_SMALL * L2_SIZE + l2 * L1_SIZE_SMALL + l1]);
            }
        }
    }

    // std::memcpy the rest
    std::memcpy(tmpSmall.l1Biases, rawSmall.l1Biases, sizeof(rawSmall.l1Biases));
    std::memcpy(tmpSmall.l2Weights, rawSmall.l2Weights, sizeof(rawSmall.l2Weights));
    std::memcpy(tmpSmall.l2Biases, rawSmall.l2Biases, sizeof(rawSmall.l2Biases));
    std::memcpy(tmpSmall.l3Weights, rawSmall.l3Weights, sizeof(rawSmall.l3Weights));
    std::memcpy(tmpSmall.l3Biases, rawSmall.l3Biases, sizeof(rawSmall.l3Biases));
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
        __m128i* vec = reinterpret_cast<__m128i*>(limit == 1 ? (int16_t*)tmpBig.inputBiases : (int16_t*)tmpBig.inputPsqWeights);

        for (int i = 0; i < limit * L1_SIZE_BIG / weightsPerBlock; i += packusBlocks) {
            for (int j = 0; j < packusBlocks; j++)
                regs[j] = vec[i + j];

            for (int j = 0; j < packusBlocks; j++)
                vec[i + j] = regs[permutation[j]];
        }
    }

    uint64_t* weightsVec = reinterpret_cast<uint64_t*>(tmpBig.inputThreatWeights);
    uint64_t weightsRegs[packusBlocks];
    for (int i = 0; i < THREAT_INPUT_SIZE * L1_SIZE_BIG / weightsPerBlock; i += packusBlocks) {
        for (int j = 0; j < packusBlocks; j++)
            weightsRegs[j] = weightsVec[i + j];

        for (int j = 0; j < packusBlocks; j++)
            weightsVec[i + j] = weightsRegs[permutation[j]];
    }
#endif

    // Transpose L1 / L2 / L3 weights
    for (int b = 0; b < OUTPUT_BUCKETS; b++) {
#if defined(__SSSE3__) || defined(__AVX2__) || (defined(__AVX512F__) && defined(__AVX512BW__)) || defined(ARCH_ARM)
        for (int l1 = 0; l1 < L1_SIZE_BIG / INT8_PER_INT32; l1++) {
            for (int l2 = 0; l2 < L2_SIZE; l2++) {
                for (int c = 0; c < INT8_PER_INT32; c++) {
                    outBig.l1Weights[b][l1 * INT8_PER_INT32 * L2_SIZE + l2 * INT8_PER_INT32 + c] = reinterpret_cast<int8_t*>(tmpBig.l1Weights)[(l1 * INT8_PER_INT32 + c) * OUTPUT_BUCKETS * L2_SIZE + b * L2_SIZE + l2];
                }
            }
        }
#else
        for (int l1 = 0; l1 < L1_SIZE_BIG; l1++) {
            for (int l2 = 0; l2 < L2_SIZE; l2++) {
                outBig.l1Weights[b][l1 * L2_SIZE + l2] = reinterpret_cast<int8_t*>(tmpBig.l1Weights)[l1 * OUTPUT_BUCKETS * L2_SIZE + b * L2_SIZE + l2];
            }
        }
#endif

        for (int l2 = 0; l2 < 2 * L2_SIZE; l2++) {
            for (int l3 = 0; l3 < L3_SIZE; l3++) {
                outBig.l2Weights[b][l2 * L3_SIZE + l3] = reinterpret_cast<float*>(tmpBig.l2Weights)[l2 * OUTPUT_BUCKETS * L3_SIZE + b * L3_SIZE + l3];
            }
        }

        for (int l3 = 0; l3 < L3_SIZE + 2 * L2_SIZE; l3++) {
            outBig.l3Weights[b][l3] = reinterpret_cast<float*>(tmpBig.l3Weights)[l3 * OUTPUT_BUCKETS + b];
        }
    }

    // std::memcpy the rest
    std::memcpy(outBig.inputPsqWeights, tmpBig.inputPsqWeights, sizeof(tmpBig.inputPsqWeights));
    std::memcpy(outBig.inputThreatWeights, tmpBig.inputThreatWeights, sizeof(tmpBig.inputThreatWeights));
    std::memcpy(outBig.inputBiases, tmpBig.inputBiases, sizeof(tmpBig.inputBiases));
    std::memcpy(outBig.l1Biases, tmpBig.l1Biases, sizeof(tmpBig.l1Biases));
    std::memcpy(outBig.l2Biases, tmpBig.l2Biases, sizeof(tmpBig.l2Biases));
    std::memcpy(outBig.l3Biases, tmpBig.l3Biases, sizeof(tmpBig.l3Biases));

    // Small net
#if defined(__AVX2__) || (defined(__AVX512F__) && defined(__AVX512BW__))
    // Transpose input weights for packus
    for (int limit : { 1, 768 * KING_BUCKETS }) {
        __m128i* vec = reinterpret_cast<__m128i*>(limit == 1 ? (int16_t*)tmpSmall.inputBiases : (int16_t*)tmpSmall.inputWeights);

        for (int i = 0; i < limit * L1_SIZE_SMALL / weightsPerBlock; i += packusBlocks) {
            for (int j = 0; j < packusBlocks; j++)
                regs[j] = vec[i + j];

            for (int j = 0; j < packusBlocks; j++)
                vec[i + j] = regs[permutation[j]];
        }
    }
#endif

    // Transpose L1 / L2 / L3 weights
    for (int b = 0; b < OUTPUT_BUCKETS; b++) {
#if defined(__SSSE3__) || defined(__AVX2__) || (defined(__AVX512F__) && defined(__AVX512BW__)) || defined(ARCH_ARM)
        for (int l1 = 0; l1 < L1_SIZE_SMALL / INT8_PER_INT32; l1++) {
            for (int l2 = 0; l2 < L2_SIZE; l2++) {
                for (int c = 0; c < INT8_PER_INT32; c++) {
                    outSmall.l1Weights[b][l1 * INT8_PER_INT32 * L2_SIZE + l2 * INT8_PER_INT32 + c] = reinterpret_cast<int8_t*>(tmpSmall.l1Weights)[(l1 * INT8_PER_INT32 + c) * OUTPUT_BUCKETS * L2_SIZE + b * L2_SIZE + l2];
                }
            }
        }
#else
        for (int l1 = 0; l1 < L1_SIZE_SMALL; l1++) {
            for (int l2 = 0; l2 < L2_SIZE; l2++) {
                outSmall.l1Weights[b][l1 * L2_SIZE + l2] = reinterpret_cast<int8_t*>(tmpSmall.l1Weights)[l1 * OUTPUT_BUCKETS * L2_SIZE + b * L2_SIZE + l2];
            }
        }
#endif

        for (int l2 = 0; l2 < 2 * L2_SIZE; l2++) {
            for (int l3 = 0; l3 < L3_SIZE; l3++) {
                outSmall.l2Weights[b][l2 * L3_SIZE + l3] = reinterpret_cast<float*>(tmpSmall.l2Weights)[l2 * OUTPUT_BUCKETS * L3_SIZE + b * L3_SIZE + l3];
            }
        }

        for (int l3 = 0; l3 < L3_SIZE + 2 * L2_SIZE; l3++) {
            outSmall.l3Weights[b][l3] = reinterpret_cast<float*>(tmpSmall.l3Weights)[l3 * OUTPUT_BUCKETS + b];
        }
    }

    // std::memcpy the rest
    std::memcpy(outSmall.inputWeights, tmpSmall.inputWeights, sizeof(tmpSmall.inputWeights));
    std::memcpy(outSmall.inputBiases, tmpSmall.inputBiases, sizeof(tmpSmall.inputBiases));
    std::memcpy(outSmall.l1Biases, tmpSmall.l1Biases, sizeof(tmpSmall.l1Biases));
    std::memcpy(outSmall.l2Biases, tmpSmall.l2Biases, sizeof(tmpSmall.l2Biases));
    std::memcpy(outSmall.l3Biases, tmpSmall.l3Biases, sizeof(tmpSmall.l3Biases));
}

void readFile(std::string file, char* target, size_t size) {
    std::ifstream infile(file, std::ios::binary);
    if (!infile) {
        std::cerr << "Error opening file for reading (" << file << ")" << std::endl;
        throw std::exception();
    }
    infile.read(target, size);
    infile.close();
}

void writeFile(std::string file, char* target, size_t size) {
    std::ofstream outfile(file, std::ios::binary);
    if (!outfile) {
        std::cerr << "Error opening file for writing (" << file << ")" << std::endl;
        throw std::exception();
    }
    outfile.write(target, size);
    outfile.close();
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
        readFile(infile_name + ".big", reinterpret_cast<char*>(&rawBig), sizeof(rawBig));
        readFile(infile_name + ".small", reinterpret_cast<char*>(&rawSmall), sizeof(rawSmall));

        quantizeNetwork();

        out.bigNet = tmpBig;
        out.smallNet = tmpSmall;
        writeFile("quantised.bin", reinterpret_cast<char*>(&out), sizeof(out));
    }
    else {
        readFile(infile_name, reinterpret_cast<char*>(&out), sizeof(out));
        tmpBig = out.bigNet;
        tmpSmall = out.smallNet;
    }

    transposePermuteNetwork();

    out.bigNet = outBig;
    out.smallNet = outSmall;
    writeFile(outfilefile_name, reinterpret_cast<char*>(&out), sizeof(out));
}