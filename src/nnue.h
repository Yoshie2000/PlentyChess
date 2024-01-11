/**
 * NNUE Evaluation is heavily inspired by Alexandria (https://github.com/PGG106/Alexandria/) and Obsidian (https://github.com/gab8192/Obsidian/)
*/
#pragma once

#include <algorithm>
#include <cstdint>
#include <immintrin.h>

#include "types.h"

#define NETWORK_FILE "network.bin"
#define ALT_NETWORK_FILE "alt-network.bin"

struct DirtyPiece {
  Square origin;
  Square target;
  Piece piece;
  Color pieceColor;
};

namespace NNUE {

  constexpr int INPUT_WIDTH = 768;
  constexpr int HIDDEN_WIDTH = 1024;

  constexpr int NETWORK_SCALE = 400;
  constexpr int NETWORK_QA = 181;
  constexpr int NETWORK_QB = 64;
  constexpr int NETWORK_QAB = NETWORK_QA * NETWORK_QB;

#if defined(__AVX512F__) && defined(__AVX512BW__)

  using Vec = __m512i;

  inline Vec addEpi16(Vec x, Vec y) {
    return _mm512_add_epi16(x, y);
  }

  inline Vec addEpi32(Vec x, Vec y) {
    return _mm512_add_epi32(x, y);
  }

  inline Vec subEpi16(Vec x, Vec y) {
    return _mm512_sub_epi16(x, y);
  }

  inline Vec minEpi16(Vec x, Vec y) {
    return _mm512_min_epi16(x, y);
  }

  inline Vec maxEpi16(Vec x, Vec y) {
    return _mm512_max_epi16(x, y);
  }

  inline Vec mulloEpi16(Vec x, Vec y) {
    return _mm512_mullo_epi16(x, y);
  }

  inline Vec maddEpi16(Vec x, Vec y) {
    return _mm512_madd_epi16(x, y);
  }

  inline Vec vecSetZero() {
    return _mm512_setzero_si512();
  }

  inline Vec vecSet1Epi16(int16_t x) {
    return _mm512_set1_epi16(x);
  }

  inline int vecHaddEpi32(Vec vec) {
    return _mm512_reduce_add_epi32(vec);
  }

#elif defined(__AVX2__)

  using Vec = __m256i;

  inline Vec addEpi16(Vec x, Vec y) {
    return _mm256_add_epi16(x, y);
  }

  inline Vec addEpi32(Vec x, Vec y) {
    return _mm256_add_epi32(x, y);
  }

  inline Vec subEpi16(Vec x, Vec y) {
    return _mm256_sub_epi16(x, y);
  }

  inline Vec minEpi16(Vec x, Vec y) {
    return _mm256_min_epi16(x, y);
  }

  inline Vec maxEpi16(Vec x, Vec y) {
    return _mm256_max_epi16(x, y);
  }

  inline Vec mulloEpi16(Vec x, Vec y) {
    return _mm256_mullo_epi16(x, y);
  }

  inline Vec maddEpi16(Vec x, Vec y) {
    return _mm256_madd_epi16(x, y);
  }

  inline Vec vecSetZero() {
    return _mm256_setzero_si256();
  }

  inline Vec vecSet1Epi16(int16_t x) {
    return _mm256_set1_epi16(x);
  }

  inline int vecHaddEpi32(Vec vec) {
    __m128i xmm0;
    __m128i xmm1;

    // Get the lower and upper half of the register:
    xmm0 = _mm256_castsi256_si128(vec);
    xmm1 = _mm256_extracti128_si256(vec, 1);

    // Add the lower and upper half vertically:
    xmm0 = _mm_add_epi32(xmm0, xmm1);

    // Get the upper half of the result:
    xmm1 = _mm_unpackhi_epi64(xmm0, xmm0);

    // Add the lower and upper half vertically:
    xmm0 = _mm_add_epi32(xmm0, xmm1);

    // Shuffle the result so that the lower 32-bits are directly above the second-lower 32-bits:
    xmm1 = _mm_shuffle_epi32(xmm0, _MM_SHUFFLE(2, 3, 0, 1));

    // Add the lower 32-bits to the second-lower 32-bits vertically:
    xmm0 = _mm_add_epi32(xmm0, xmm1);

    // Cast the result to the 32-bit integer type and return it:
    return _mm_cvtsi128_si32(xmm0);
  }

#else

  using Vec = __m128i;

  inline Vec addEpi16(Vec x, Vec y) {
    return _mm_add_epi16(x, y);
  }

  inline Vec addEpi32(Vec x, Vec y) {
    return _mm_add_epi32(x, y);
  }

  inline Vec subEpi16(Vec x, Vec y) {
    return _mm_sub_epi16(x, y);
  }

  inline Vec minEpi16(Vec x, Vec y) {
    return _mm_min_epi16(x, y);
  }

  inline Vec maxEpi16(Vec x, Vec y) {
    return _mm_max_epi16(x, y);
  }

  inline Vec mulloEpi16(Vec x, Vec y) {
    return _mm_mullo_epi16(x, y);
  }

  inline Vec maddEpi16(Vec x, Vec y) {
    return _mm_madd_epi16(x, y);
  }

  inline Vec vecSetZero() {
    return _mm_setzero_si128();
  }

  inline Vec vecSet1Epi16(int16_t x) {
    return _mm_set1_epi16(x);
  }

  inline int vecHaddEpi32(Vec vec) {
    int* asArray = (int*) &vec;
    return asArray[0] + asArray[1] + asArray[2] + asArray[3];
  }

#endif

  constexpr int ALIGNMENT = std::max<int>(8, sizeof(Vec));
  constexpr int WEIGHTS_PER_VEC = sizeof(Vec) / sizeof(int16_t);

  struct Accumulator {
    alignas(ALIGNMENT) int16_t colors[2][HIDDEN_WIDTH];

    void clear();

    void addFeature(Square square, Piece piece, Color pieceColor, Accumulator* input);

    void removeFeature(Square square, Piece piece, Color pieceColor, Accumulator* input);

    void moveFeature(Square origin, Square target, Piece piece, Color pieceColor, Accumulator* input);
  };

  void init();

  Eval evaluate(Accumulator& accumulator, Color sideToMove);
}

extern NNUE::Accumulator accumulatorStack[MAX_PLY];
extern int accumulatorStackHead;