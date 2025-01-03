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
constexpr int KING_BUCKETS = 9;
constexpr int OUTPUT_BUCKETS = 8;

constexpr int NETWORK_SCALE = 400;
constexpr int INPUT_QUANT = 362;
constexpr int L1_QUANT = 64;

constexpr int ALIGNMENT = 64;
constexpr int INT8_PER_INT32 = sizeof(int32_t) / sizeof(int8_t);

struct RawNetworkData {
    float inputWeights[KING_BUCKETS + static_cast<int>(KING_BUCKETS_FACTORIZED)][INPUT_SIZE * L1_SIZE];
    float inputBiases[L1_SIZE];
    float l1Weights[L1_SIZE][OUTPUT_BUCKETS][L2_SIZE];
    float l1Biases[OUTPUT_BUCKETS][L2_SIZE];
    float l2Weights[L2_SIZE][OUTPUT_BUCKETS][L3_SIZE];
    float l2Biases[OUTPUT_BUCKETS][L3_SIZE];
    float l3Weights[L3_SIZE][OUTPUT_BUCKETS];
    float l3Biases[OUTPUT_BUCKETS];
};

struct NetworkData {
    alignas(ALIGNMENT) int16_t inputWeights[KING_BUCKETS][INPUT_SIZE * L1_SIZE];
    alignas(ALIGNMENT) int16_t inputBiases[L1_SIZE];
    alignas(ALIGNMENT) int8_t l1Weights[OUTPUT_BUCKETS][L1_SIZE * L2_SIZE];
    alignas(ALIGNMENT) float l1Biases[OUTPUT_BUCKETS][L2_SIZE];
    alignas(ALIGNMENT) float l2Weights[OUTPUT_BUCKETS][L2_SIZE * L3_SIZE];
    alignas(ALIGNMENT) float l2Biases[OUTPUT_BUCKETS][L3_SIZE];
    alignas(ALIGNMENT) float l3Weights[OUTPUT_BUCKETS][L3_SIZE];
    alignas(ALIGNMENT) float l3Biases[OUTPUT_BUCKETS];
};

struct Networks {
    NetworkData middlegame;
    NetworkData endgame;
};

// static_assert(sizeof(RawNetworkData) % 64 == 0);
static_assert(sizeof(NetworkData) % 64 == 0);
static_assert(sizeof(Networks) % 64 == 0);

template<int Q>
int16_t quantize(float number) {
    return static_cast<int16_t>(std::round(number * static_cast<float>(Q)));
}

template<typename T>
T* loadBinary(std::string path, int alignment = ALIGNMENT) {
    T* data = reinterpret_cast<T*>(std::aligned_alloc(alignment, sizeof(T)));

    std::ifstream infile(path, std::ios::binary);
    if (!infile) {
        std::cerr << "Error opening file for reading (" << path << ")" << std::endl;
        return nullptr;
    }
    infile.read(reinterpret_cast<char*>(data), sizeof(T));
    infile.close();

    return data;
}

template<typename T>
void writeBinary(T* data, std::string path) {
    std::ofstream outfile(path, std::ios::binary);
    if (!outfile) {
        std::cerr << "Error opening file for writing" << std::endl;
        return;
    }
    outfile.write(reinterpret_cast<char*>(data), sizeof(T));
    outfile.close();
}

NetworkData* quantizeNetwork(RawNetworkData* raw) {
    NetworkData* out = reinterpret_cast<NetworkData*>(std::aligned_alloc(ALIGNMENT, sizeof(NetworkData)));

    // Add factorized input weights to buckets, and quantize
    for (int n = 0; n < KING_BUCKETS; n++) {
        for (int w = 0; w < INPUT_SIZE * L1_SIZE; w++) {
            if (KING_BUCKETS_FACTORIZED) {
                out->inputWeights[n][w] = quantize<INPUT_QUANT>(raw->inputWeights[n + 1][w] + raw->inputWeights[0][w]);
            }
            else {
                out->inputWeights[n][w] = quantize<INPUT_QUANT>(raw->inputWeights[n][w]);
            }
        }
    }

    // Quantize input biases
    for (int b = 0; b < L1_SIZE; b++) {
        out->inputBiases[b] = quantize<INPUT_QUANT>(raw->inputBiases[b]);
    }

    // Quantize L1 weights
    for (int b = 0; b < OUTPUT_BUCKETS; b++) {
        for (int l1 = 0; l1 < L1_SIZE; l1++) {
            for (int l2 = 0; l2 < L2_SIZE; l2++) {
                out->l1Weights[b][l2 * L1_SIZE + l1] = quantize<L1_QUANT>(((float*)raw->l1Weights)[b * L1_SIZE * L2_SIZE + l2 * L1_SIZE + l1]);
            }
        }
    }

    // std::memcpy the rest
    std::memcpy(out->l1Biases, raw->l1Biases, sizeof(raw->l1Biases));
    std::memcpy(out->l2Weights, raw->l2Weights, sizeof(raw->l2Weights));
    std::memcpy(out->l2Biases, raw->l2Biases, sizeof(raw->l2Biases));
    std::memcpy(out->l3Weights, raw->l3Weights, sizeof(raw->l3Weights));
    std::memcpy(out->l3Biases, raw->l3Biases, sizeof(raw->l3Biases));

    return out;
}

NetworkData* transposePermuteNetwork(NetworkData* tmp) {
    NetworkData* out = reinterpret_cast<NetworkData*>(std::aligned_alloc(ALIGNMENT, sizeof(NetworkData)));

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
        __m128i* vec = reinterpret_cast<__m128i*>(limit == 1 ? (int16_t*)tmp->inputBiases : (int16_t*)tmp->inputWeights);

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
#if defined(__SSSE3__) || defined(__AVX2__) || (defined(__AVX512F__) && defined(__AVX512BW__)) || defined(ARCH_ARM)
        for (int l1 = 0; l1 < L1_SIZE / INT8_PER_INT32; l1++) {
            for (int l2 = 0; l2 < L2_SIZE; l2++) {
                for (int c = 0; c < INT8_PER_INT32; c++) {
                    out->l1Weights[b][l1 * INT8_PER_INT32 * L2_SIZE + l2 * INT8_PER_INT32 + c] = reinterpret_cast<int8_t*>(tmp->l1Weights)[(l1 * INT8_PER_INT32 + c) * OUTPUT_BUCKETS * L2_SIZE + b * L2_SIZE + l2];
                }
            }
        }
#else
        for (int l1 = 0; l1 < L1_SIZE; l1++) {
            for (int l2 = 0; l2 < L2_SIZE; l2++) {
                out->l1Weights[b][l1 * L2_SIZE + l2] = reinterpret_cast<int8_t*>(tmp->l1Weights)[l1 * OUTPUT_BUCKETS * L2_SIZE + b * L2_SIZE + l2];
            }
        }
#endif

        for (int l2 = 0; l2 < L2_SIZE; l2++) {
            for (int l3 = 0; l3 < L3_SIZE; l3++) {
                out->l2Weights[b][l2 * L3_SIZE + l3] = reinterpret_cast<float*>(tmp->l2Weights)[l2 * OUTPUT_BUCKETS * L3_SIZE + b * L3_SIZE + l3];
            }
        }

        for (int l3 = 0; l3 < L3_SIZE; l3++) {
            out->l3Weights[b][l3] = reinterpret_cast<float*>(tmp->l3Weights)[l3 * OUTPUT_BUCKETS + b];
        }
    }

    // std::memcpy the rest
    std::memcpy(out->inputWeights, tmp->inputWeights, sizeof(tmp->inputWeights));
    std::memcpy(out->inputBiases, tmp->inputBiases, sizeof(tmp->inputBiases));
    std::memcpy(out->l1Biases, tmp->l1Biases, sizeof(tmp->l1Biases));
    std::memcpy(out->l2Biases, tmp->l2Biases, sizeof(tmp->l2Biases));
    std::memcpy(out->l3Biases, tmp->l3Biases, sizeof(tmp->l3Biases));

    return out;
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <infile_is_floats> <outfile> <infile> <infile2?>\n";
        return -1;
    }

    std::string infile_is_floats = argv[1];
    bool in_is_floats = infile_is_floats == "true";

    std::string path_out = argv[2];
    std::string path_in1 = argv[3];
    std::string path_in2 = in_is_floats ? argv[4] : "";

    NetworkData* quantised1, *quantised2;
    Networks* networks;
    if (in_is_floats) {
        RawNetworkData* in_raw = loadBinary<RawNetworkData>(path_in1, 1);
        RawNetworkData* in2_raw = loadBinary<RawNetworkData>(path_in2, 1);

        quantised1 = quantizeNetwork(in_raw);
        std::free(in_raw);
        quantised2 = quantizeNetwork(in2_raw);
        std::free(in2_raw);

        Networks* combined = reinterpret_cast<Networks*>(std::aligned_alloc(ALIGNMENT, sizeof(Networks)));
        std::memcpy(&combined->middlegame, quantised1, sizeof(NetworkData));
        std::memcpy(&combined->endgame, quantised2, sizeof(NetworkData));

        writeBinary<Networks>(combined, "quantised.bin");
        std::free(combined);
    }
    else {
        networks = loadBinary<Networks>(path_in1);

        quantised1 = &networks->middlegame;
        quantised2 = &networks->endgame;
    }

    NetworkData* final1 = transposePermuteNetwork(quantised1);
    NetworkData* final2 = transposePermuteNetwork(quantised2);

    Networks* combined = reinterpret_cast<Networks*>(std::aligned_alloc(ALIGNMENT, sizeof(Networks)));
    std::memcpy(&combined->middlegame, final1, sizeof(NetworkData));
    std::memcpy(&combined->endgame, final2, sizeof(NetworkData));

    writeBinary<Networks>(combined, path_out);

    if (in_is_floats) {
        std::free(quantised1);
        std::free(quantised2);
    } else {
        std::free(networks);
    }
    std::free(final1);
    std::free(final2);
    std::free(combined);
}
