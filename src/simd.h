#pragma once

#if defined(ARCH_X86)

#include <immintrin.h>
#include <xmmintrin.h>

using VecI16_v128 = __m128i;

inline VecI16_v128 setZero_v128() {
  return _mm_setzero_si128();
}

inline VecI16_v128 set1Epi16_v128(int i) {
  return _mm_set1_epi16(i);
}

inline VecI16_v128 loadu_v128(const uint16_t* addr) {
  return _mm_loadu_si128(reinterpret_cast<const VecI16_v128*>(addr));
}

inline void storeu_v128(uint16_t* addr, VecI16_v128 x) {
  _mm_storeu_si128(reinterpret_cast<VecI16_v128*>(addr), x);
}

inline VecI16_v128 addEpi16_v128(VecI16_v128 x, VecI16_v128 y) {
  return _mm_add_epi16(x, y);
}

#elif defined(ARCH_WASM_SIMD)

#include <wasm_simd128.h>

using VecI16_v128 = v128_t;

inline VecI16_v128 setZero_v128() {
  return wasm_i16x8_splat(0);
}

inline VecI16_v128 set1Epi16_v128(int i) {
  return wasm_i16x8_splat(i);
}

inline VecI16_v128 loadu_v128(const uint16_t* addr) {
  return wasm_v128_load(addr);
}

inline void storeu_v128(uint16_t* addr, VecI16_v128 x) {
  wasm_v128_store(addr, x);
}

inline VecI16_v128 addEpi16_v128(VecI16_v128 x, VecI16_v128 y) {
  return wasm_i16x8_add(x, y);
}

#elif defined(ARCH_WASM)

// Scalar fallback for Wasm without SIMD
#include <cstdint>
#include <cstring>

struct VecI16_v128 {
  uint16_t data[8];
};

inline VecI16_v128 setZero_v128() {
  VecI16_v128 result;
  for (int i = 0; i < 8; i++) result.data[i] = 0;
  return result;
}

inline VecI16_v128 set1Epi16_v128(int i) {
  VecI16_v128 result;
  for (int j = 0; j < 8; j++) result.data[j] = static_cast<uint16_t>(i);
  return result;
}

inline VecI16_v128 loadu_v128(const uint16_t* addr) {
  VecI16_v128 result;
  std::memcpy(result.data, addr, sizeof(result.data));
  return result;
}

inline void storeu_v128(uint16_t* addr, VecI16_v128 x) {
  std::memcpy(addr, x.data, sizeof(x.data));
}

inline VecI16_v128 addEpi16_v128(VecI16_v128 x, VecI16_v128 y) {
  VecI16_v128 result;
  for (int i = 0; i < 8; i++) result.data[i] = x.data[i] + y.data[i];
  return result;
}

#else

#include <arm_neon.h>

using VecI16_v128 = uint16x8_t;

inline VecI16_v128 setZero_v128() {
  return vdupq_n_u16(0);
}

inline VecI16_v128 set1Epi16_v128(int i) {
  return vdupq_n_u16(i);
}

inline VecI16_v128 loadu_v128(const uint16_t* addr) {
  return vld1q_u16(addr);
}

inline void storeu_v128(uint16_t* addr, VecI16_v128 x) {
  vst1q_u16(addr, x);
}

inline VecI16_v128 addEpi16_v128(VecI16_v128 x, VecI16_v128 y) {
  return vaddq_u16(x, y);
}

#endif

#if defined(__AVX512F__) && defined(__AVX512BW__)

using VecI8 = __m512i;
using VecIu8 = __m512i;
using VecI16 = __m512i;
using VecI16s = __m256i;
using VecIu16 = __m512i;
using VecI32 = __m512i;
using VecF = __m512;

inline VecI16 addEpi16(VecI16 x, VecI16 y) {
  return _mm512_add_epi16(x, y);
}

inline VecI16 subEpi16(VecI16 x, VecI16 y) {
  return _mm512_sub_epi16(x, y);
}

inline VecI16 convertEpi8Epi16(VecI16s x) {
  return _mm512_cvtepi8_epi16(x);
}

inline VecI16 minEpi16(VecI16 x, VecI16 y) {
  return _mm512_min_epi16(x, y);
}

inline VecI16 maxEpi16(VecI16 x, VecI16 y) {
  return _mm512_max_epi16(x, y);
}

inline VecI16 set1Epi16(int i) {
  return _mm512_set1_epi16(i);
}

inline VecIu8 set1Epi32(int i) {
  return _mm512_set1_epi32(i);
}

inline VecI16 srliEpi16(VecI16 x, int shift) {
  return _mm512_srli_epi16(x, shift);
}

inline VecI16 slliEpi16(VecI16 x, int shift) {
  return _mm512_slli_epi16(x, shift);
}

inline VecI16 mulhiEpi16(VecI16 x, VecI16 y) {
  return _mm512_mulhi_epi16(x, y);
}

#if defined(__AVX512VNNI__)
inline VecI32 dpbusdEpi32(VecI32 sum, VecIu8 u, VecI8 i) {
  return _mm512_dpbusd_epi32(sum, u, i);
}
inline VecI32 dpbusdEpi32x2(VecI32 sum, VecIu8 u, VecI8 i, VecIu8 u2, VecI8 i2) {
  return _mm512_dpbusd_epi32(_mm512_dpbusd_epi32(sum, u, i), u2, i2);
}
#else
inline VecI32 dpbusdEpi32(VecI32 sum, VecIu8 u, VecI8 i) {
  VecI32 sum32 = _mm512_madd_epi16(_mm512_maddubs_epi16(u, i), _mm512_set1_epi16(1));
  return _mm512_add_epi32(sum32, sum);
}
inline VecI32 dpbusdEpi32x2(VecI32 sum, VecIu8 u, VecI8 i, VecIu8 u2, VecI8 i2) {
  VecI16 mul1 = _mm512_maddubs_epi16(u, i);
  VecI16 mul2 = _mm512_maddubs_epi16(u2, i2);
  VecI32 sum32 = _mm512_madd_epi16(_mm512_add_epi16(mul1, mul2), _mm512_set1_epi16(1));
  return _mm512_add_epi32(sum32, sum);
}
#endif

inline VecIu8 packusEpi16(VecI16 x, VecI16 y) {
  return _mm512_packus_epi16(x, y);
}

inline void vecStoreI(VecI16* dest, VecI16 x) {
  _mm512_store_si512(dest, x);
}

inline VecF cvtepi32Ps(VecI32 x) {
  return _mm512_cvtepi32_ps(x);
}

inline VecF addPs(VecF x, VecF y) {
  return _mm512_add_ps(x, y);
}

inline VecF mulPs(VecF x, VecF y) {
  return _mm512_mul_ps(x, y);
}

inline VecF set1Ps(float x) {
  return _mm512_set1_ps(x);
}

inline VecF minPs(VecF x, VecF y) {
  return _mm512_min_ps(x, y);
}

inline VecF maxPs(VecF x, VecF y) {
  return _mm512_max_ps(x, y);
}

inline VecF fmaddPs(VecF a, VecF b, VecF c) {
  return _mm512_fmadd_ps(a, b, c);
}

inline float reduceAddPs(VecF* v) {
  return _mm512_reduce_add_ps(v[0]);
}

inline uint32_t vecNNZ(VecI32 chunk) {
  return _mm512_cmpgt_epi32_mask(chunk, _mm512_setzero_si512());
}

#elif defined(__AVX2__)

using VecI8 = __m256i;
using VecIu8 = __m256i;
using VecI16 = __m256i;
using VecI16s = __m128i;
using VecIu16 = __m256i;
using VecI32 = __m256i;
using VecF = __m256;

inline VecI16 addEpi16(VecI16 x, VecI16 y) {
  return _mm256_add_epi16(x, y);
}

inline VecI16 subEpi16(VecI16 x, VecI16 y) {
  return _mm256_sub_epi16(x, y);
}

inline VecI16 convertEpi8Epi16(VecI16s x) {
  return _mm256_cvtepi8_epi16(x);
}

inline VecI16 minEpi16(VecI16 x, VecI16 y) {
  return _mm256_min_epi16(x, y);
}

inline VecI16 maxEpi16(VecI16 x, VecI16 y) {
  return _mm256_max_epi16(x, y);
}

inline VecI16 set1Epi16(int i) {
  return _mm256_set1_epi16(i);
}

inline VecIu8 set1Epi32(int i) {
  return _mm256_set1_epi32(i);
}

inline VecI16 srliEpi16(VecI16 x, int shift) {
  return _mm256_srli_epi16(x, shift);
}

inline VecI16 slliEpi16(VecI16 x, int shift) {
  return _mm256_slli_epi16(x, shift);
}

inline VecI16 mulhiEpi16(VecI16 x, VecI16 y) {
  return _mm256_mulhi_epi16(x, y);
}

inline VecIu8 packusEpi16(VecI16 x, VecI16 y) {
  return _mm256_packus_epi16(x, y);
}

inline void vecStoreI(VecI16* dest, VecI16 x) {
  _mm256_store_si256(dest, x);
}

inline VecI32 dpbusdEpi32(VecI32 sum, VecIu8 u, VecI8 i) {
  VecI32 sum32 = _mm256_madd_epi16(_mm256_maddubs_epi16(u, i), _mm256_set1_epi16(1));
  return _mm256_add_epi32(sum32, sum);
}

inline VecI32 dpbusdEpi32x2(VecI32 sum, VecIu8 u, VecI8 i, VecIu8 u2, VecI8 i2) {
  VecI16 mul1 = _mm256_maddubs_epi16(u, i);
  VecI16 mul2 = _mm256_maddubs_epi16(u2, i2);
  VecI32 sum32 = _mm256_madd_epi16(_mm256_add_epi16(mul1, mul2), _mm256_set1_epi16(1));
  return _mm256_add_epi32(sum32, sum);
}

inline VecF cvtepi32Ps(VecI32 x) {
  return _mm256_cvtepi32_ps(x);
}

inline VecF addPs(VecF x, VecF y) {
  return _mm256_add_ps(x, y);
}

inline VecF mulPs(VecF x, VecF y) {
  return _mm256_mul_ps(x, y);
}

inline VecF set1Ps(float x) {
  return _mm256_set1_ps(x);
}

inline VecF minPs(VecF x, VecF y) {
  return _mm256_min_ps(x, y);
}

inline VecF maxPs(VecF x, VecF y) {
  return _mm256_max_ps(x, y);
}

inline VecF fmaddPs(VecF a, VecF b, VecF c) {
  return _mm256_fmadd_ps(a, b, c);
}

inline float reduceAddPs(VecF* v) {
  v[0] = _mm256_add_ps(v[0], v[1]);
  __m128 high = _mm256_extractf128_ps(v[0], 1);
  __m128 low = _mm256_castps256_ps128(v[0]);
  __m128 sum = _mm_add_ps(high, low);
  __m128 high64 = _mm_movehl_ps(sum, sum);
  __m128 sum64 = _mm_add_ps(sum, high64);

  return ((float*)&sum64)[0] + ((float*)&sum64)[1];
}

inline uint32_t vecNNZ(VecI32 chunk) {
  return _mm256_movemask_ps(_mm256_castsi256_ps(_mm256_cmpgt_epi32(chunk, _mm256_setzero_si256())));
}

#elif defined(ARCH_X86)

using VecI8 = __m128i;
using VecIu8 = __m128i;
using VecI16 = __m128i;
using VecI16s = uint64_t;
using VecIu16 = __m128i;
using VecI32 = __m128i;
using VecF = __m128;

inline VecI16 addEpi16(VecI16 x, VecI16 y) {
  return _mm_add_epi16(x, y);
}

inline VecI16 subEpi16(VecI16 x, VecI16 y) {
  return _mm_sub_epi16(x, y);
}

#if defined(__FMA__)
inline VecI16 convertEpi8Epi16(VecI16s x) {
  return _mm_cvtepi8_epi16(_mm_set_epi64x(0, x));
}
#else
inline VecI16 convertEpi8Epi16(VecI16s x) {
  __m128i v8 = _mm_cvtsi64_si128(x);
  v8 = _mm_slli_si128(v8, 8);
  v8 = _mm_srli_si128(v8, 8);
  __m128i sign = _mm_cmpgt_epi8(_mm_setzero_si128(), v8);
  return _mm_unpacklo_epi8(v8, sign);
}
#endif

inline VecI16 minEpi16(VecI16 x, VecI16 y) {
  return _mm_min_epi16(x, y);
}

inline VecI16 maxEpi16(VecI16 x, VecI16 y) {
  return _mm_max_epi16(x, y);
}

inline VecI16 set1Epi16(int i) {
  return _mm_set1_epi16(i);
}

inline VecIu8 set1Epi32(int i) {
  return _mm_set1_epi32(i);
}

inline VecI16 srliEpi16(VecI16 x, int shift) {
  return _mm_srli_epi16(x, shift);
}

inline VecI16 slliEpi16(VecI16 x, int shift) {
  return _mm_slli_epi16(x, shift);
}

inline VecI16 mulhiEpi16(VecI16 x, VecI16 y) {
  return _mm_mulhi_epi16(x, y);
}

inline VecIu8 packusEpi16(VecI16 x, VecI16 y) {
  return _mm_packus_epi16(x, y);
}

inline void vecStoreI(VecI16* dest, VecI16 x) {
  _mm_store_si128(dest, x);
}

#if defined(__SSSE3__)
inline VecI32 dpbusdEpi32(VecI32 sum, VecIu8 u, VecI8 i) {
  VecI32 sum32 = _mm_madd_epi16(_mm_maddubs_epi16(u, i), _mm_set1_epi16(1));
  return _mm_add_epi32(sum32, sum);
}
inline VecI32 dpbusdEpi32x2(VecI32 sum, VecIu8 u, VecI8 i, VecIu8 u2, VecI8 i2) {
  VecI16 mul1 = _mm_maddubs_epi16(u, i);
  VecI16 mul2 = _mm_maddubs_epi16(u2, i2);
  VecI32 sum32 = _mm_madd_epi16(_mm_add_epi16(mul1, mul2), _mm_set1_epi16(1));
  return _mm_add_epi32(sum32, sum);
}
#endif

inline VecF cvtepi32Ps(VecI32 x) {
  return _mm_cvtepi32_ps(x);
}

inline VecF addPs(VecF x, VecF y) {
  return _mm_add_ps(x, y);
}

inline VecF mulPs(VecF x, VecF y) {
  return _mm_mul_ps(x, y);
}

inline VecF set1Ps(float x) {
  return _mm_set1_ps(x);
}

inline VecF minPs(VecF x, VecF y) {
  return _mm_min_ps(x, y);
}

inline VecF maxPs(VecF x, VecF y) {
  return _mm_max_ps(x, y);
}

#if defined(__FMA__)
inline VecF fmaddPs(VecF a, VecF b, VecF c) {
  return _mm_fmadd_ps(a, b, c);
}
#endif

inline float reduceAddPsR(float* sums, int length) {
  if (length == 2) return sums[0] + sums[1];
  length /= 2;
  for (int i = 0; i < length; ++i)
    sums[i] += sums[i + length];
  return reduceAddPsR(sums, length);
}

inline float reduceAddPs(VecF* sums) {
  return reduceAddPsR((float*)sums, 64 / sizeof(float));
}

inline uint32_t vecNNZ(VecI32 chunk) {
  return _mm_movemask_ps(_mm_castsi128_ps(_mm_cmpgt_epi32(chunk, _mm_setzero_si128())));
}

#elif defined(ARCH_WASM_SIMD)

#include <wasm_simd128.h>

using VecI8 = v128_t;
using VecIu8 = v128_t;
using VecI16 = v128_t;
using VecI16s = int64_t; // 8 bytes for 8 int8 values
using VecIu16 = v128_t;
using VecI32 = v128_t;
using VecF = v128_t;

// Integer operations
inline VecI16 addEpi16(VecI16 x, VecI16 y) {
  return wasm_i16x8_add(x, y);
}

inline VecI16 subEpi16(VecI16 x, VecI16 y) {
  return wasm_i16x8_sub(x, y);
}

inline VecI16 convertEpi8Epi16(VecI16s x) {
  // Load 8 bytes and sign-extend to 16-bit
  v128_t v8 = wasm_i64x2_splat(x);
  return wasm_i16x8_extend_low_i8x16(v8);
}

inline VecI16 minEpi16(VecI16 x, VecI16 y) {
  return wasm_i16x8_min(x, y);
}

inline VecI16 maxEpi16(VecI16 x, VecI16 y) {
  return wasm_i16x8_max(x, y);
}

inline VecI16 set1Epi16(int i) {
  return wasm_i16x8_splat(i);
}

inline VecIu8 set1Epi32(int i) {
  return wasm_i32x4_splat(i);
}

inline VecI16 srliEpi16(VecI16 x, int shift) {
  return wasm_u16x8_shr(x, shift);
}

inline VecI16 slliEpi16(VecI16 x, int shift) {
  return wasm_i16x8_shl(x, shift);
}

inline VecI16 mulhiEpi16(VecI16 x, VecI16 y) {
  // Multiply and keep high 16 bits of each 32-bit result
  v128_t lo = wasm_i32x4_extmul_low_i16x8(x, y);
  v128_t hi = wasm_i32x4_extmul_high_i16x8(x, y);
  // Shift right by 16 to get high halves
  lo = wasm_i32x4_shr(lo, 16);
  hi = wasm_i32x4_shr(hi, 16);
  // Pack back to i16x8
  return wasm_i16x8_narrow_i32x4(lo, hi);
}

// Emulate maddubs: multiply unsigned bytes by signed bytes, add adjacent pairs with saturation
inline v128_t wasm_maddubs(v128_t a, v128_t b) {
  // Extract low and high halves
  v128_t a_lo = wasm_u16x8_extend_low_u8x16(a);
  v128_t b_lo = wasm_i16x8_extend_low_i8x16(b);
  v128_t a_hi = wasm_u16x8_extend_high_u8x16(a);
  v128_t b_hi = wasm_i16x8_extend_high_i8x16(b);

  // Multiply to get i16x8
  v128_t mul_lo = wasm_i16x8_mul(wasm_i16x8_add(a_lo, wasm_i16x8_splat(0)), b_lo); // reinterpret as signed
  v128_t mul_hi = wasm_i16x8_mul(wasm_i16x8_add(a_hi, wasm_i16x8_splat(0)), b_hi);

  // Add adjacent pairs with saturation
  // mul_lo has 8 i16 values, we need pairs [0]+[1], [2]+[3], [4]+[5], [6]+[7]
  v128_t lo_even = wasm_i16x8_shuffle(mul_lo, mul_lo, 0, 2, 4, 6, 0, 2, 4, 6);
  v128_t lo_odd = wasm_i16x8_shuffle(mul_lo, mul_lo, 1, 3, 5, 7, 1, 3, 5, 7);
  v128_t hi_even = wasm_i16x8_shuffle(mul_hi, mul_hi, 0, 2, 4, 6, 0, 2, 4, 6);
  v128_t hi_odd = wasm_i16x8_shuffle(mul_hi, mul_hi, 1, 3, 5, 7, 1, 3, 5, 7);

  v128_t sum_lo = wasm_i16x8_add_sat(lo_even, lo_odd);
  v128_t sum_hi = wasm_i16x8_add_sat(hi_even, hi_odd);

  // Combine lower 4 elements from each
  return wasm_i16x8_shuffle(sum_lo, sum_hi, 0, 1, 2, 3, 8, 9, 10, 11);
}

// Emulate madd: multiply i16 pairs and add adjacent to get i32
inline v128_t wasm_madd(v128_t a, v128_t b) {
  v128_t lo = wasm_i32x4_extmul_low_i16x8(a, b);
  v128_t hi = wasm_i32x4_extmul_high_i16x8(a, b);
  // Add pairs: [lo0+lo1, lo2+lo3, hi0+hi1, hi2+hi3]
  v128_t lo_even = wasm_i32x4_shuffle(lo, lo, 0, 2, 0, 2);
  v128_t lo_odd = wasm_i32x4_shuffle(lo, lo, 1, 3, 1, 3);
  v128_t hi_even = wasm_i32x4_shuffle(hi, hi, 0, 2, 0, 2);
  v128_t hi_odd = wasm_i32x4_shuffle(hi, hi, 1, 3, 1, 3);
  v128_t sum_lo = wasm_i32x4_add(lo_even, lo_odd);
  v128_t sum_hi = wasm_i32x4_add(hi_even, hi_odd);
  return wasm_i32x4_shuffle(sum_lo, sum_hi, 0, 1, 4, 5);
}

// SIMD dot-product
inline VecI32 dpbusdEpi32(VecI32 sum, VecIu8 u, VecI8 i) {
  v128_t maddubs_result = wasm_maddubs(u, i);
  v128_t sum32 = wasm_madd(maddubs_result, wasm_i16x8_splat(1));
  return wasm_i32x4_add(sum32, sum);
}

inline VecI32 dpbusdEpi32x2(VecI32 sum, VecIu8 u, VecI8 i, VecIu8 u2, VecI8 i2) {
  v128_t mul1 = wasm_maddubs(u, i);
  v128_t mul2 = wasm_maddubs(u2, i2);
  v128_t sum32 = wasm_madd(wasm_i16x8_add(mul1, mul2), wasm_i16x8_splat(1));
  return wasm_i32x4_add(sum32, sum);
}

// Pack and store
inline VecIu8 packusEpi16(VecI16 x, VecI16 y) {
  return wasm_u8x16_narrow_i16x8(x, y);
}

inline void vecStoreI(VecI16* dest, VecI16 x) {
  wasm_v128_store(dest, x);
}

// Floating-point operations
inline VecF cvtepi32Ps(VecI32 x) {
  return wasm_f32x4_convert_i32x4(x);
}

inline VecF addPs(VecF x, VecF y) {
  return wasm_f32x4_add(x, y);
}

inline VecF mulPs(VecF x, VecF y) {
  return wasm_f32x4_mul(x, y);
}

inline VecF set1Ps(float x) {
  return wasm_f32x4_splat(x);
}

inline VecF minPs(VecF x, VecF y) {
  return wasm_f32x4_min(x, y);
}

inline VecF maxPs(VecF x, VecF y) {
  return wasm_f32x4_max(x, y);
}

// No FMA in Wasm SIMD, emulate with mul+add
inline VecF fmaddPs(VecF a, VecF b, VecF c) {
  return wasm_f32x4_add(wasm_f32x4_mul(a, b), c);
}

inline float reduceAddPsR(float* sums, int length) {
  if (length == 2) return sums[0] + sums[1];
  length /= 2;
  for (int i = 0; i < length; ++i)
    sums[i] += sums[i + length];
  return reduceAddPsR(sums, length);
}

inline float reduceAddPs(VecF* sums) {
  return reduceAddPsR((float*)sums, 64 / sizeof(float));
}

inline uint32_t vecNNZ(VecI32 chunk) {
  v128_t cmp = wasm_i32x4_gt(chunk, wasm_i32x4_splat(0));
  // Extract the sign bits
  return wasm_i8x16_bitmask(cmp) & 0x1111; // Get bits 0, 4, 8, 12
}

#elif defined(ARCH_WASM)

// Scalar fallback for Wasm without SIMD
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <cmath>

// Using 128-bit emulated vector types for consistency
struct VecI8_scalar {
  int8_t data[16];
};
struct VecIu8_scalar {
  uint8_t data[16];
};
struct VecI16_scalar {
  int16_t data[8];
};
struct VecIu16_scalar {
  uint16_t data[8];
};
struct VecI32_scalar {
  int32_t data[4];
};
struct VecF_scalar {
  float data[4];
};

using VecI8 = VecI8_scalar;
using VecIu8 = VecIu8_scalar;
using VecI16 = VecI16_scalar;
using VecI16s = int64_t;
using VecIu16 = VecIu16_scalar;
using VecI32 = VecI32_scalar;
using VecF = VecF_scalar;

// Integer operations
inline VecI16 addEpi16(VecI16 x, VecI16 y) {
  VecI16 result;
  for (int i = 0; i < 8; i++) result.data[i] = x.data[i] + y.data[i];
  return result;
}

inline VecI16 subEpi16(VecI16 x, VecI16 y) {
  VecI16 result;
  for (int i = 0; i < 8; i++) result.data[i] = x.data[i] - y.data[i];
  return result;
}

inline VecI16 convertEpi8Epi16(VecI16s x) {
  VecI16 result;
  int8_t* bytes = reinterpret_cast<int8_t*>(&x);
  for (int i = 0; i < 8; i++) result.data[i] = bytes[i];
  return result;
}

inline VecI16 minEpi16(VecI16 x, VecI16 y) {
  VecI16 result;
  for (int i = 0; i < 8; i++) result.data[i] = std::min(x.data[i], y.data[i]);
  return result;
}

inline VecI16 maxEpi16(VecI16 x, VecI16 y) {
  VecI16 result;
  for (int i = 0; i < 8; i++) result.data[i] = std::max(x.data[i], y.data[i]);
  return result;
}

inline VecI16 set1Epi16(int i) {
  VecI16 result;
  for (int j = 0; j < 8; j++) result.data[j] = static_cast<int16_t>(i);
  return result;
}

inline VecIu8 set1Epi32(int i) {
  VecIu8 result;
  for (int j = 0; j < 4; j++) {
    result.data[j*4+0] = (i >> 0) & 0xFF;
    result.data[j*4+1] = (i >> 8) & 0xFF;
    result.data[j*4+2] = (i >> 16) & 0xFF;
    result.data[j*4+3] = (i >> 24) & 0xFF;
  }
  return result;
}

inline VecI16 srliEpi16(VecI16 x, int shift) {
  VecI16 result;
  for (int i = 0; i < 8; i++) result.data[i] = static_cast<uint16_t>(x.data[i]) >> shift;
  return result;
}

inline VecI16 slliEpi16(VecI16 x, int shift) {
  VecI16 result;
  for (int i = 0; i < 8; i++) result.data[i] = x.data[i] << shift;
  return result;
}

inline VecI16 mulhiEpi16(VecI16 x, VecI16 y) {
  VecI16 result;
  for (int i = 0; i < 8; i++) {
    int32_t product = static_cast<int32_t>(x.data[i]) * static_cast<int32_t>(y.data[i]);
    result.data[i] = product >> 16;
  }
  return result;
}

inline VecIu8 packusEpi16(VecI16 x, VecI16 y) {
  VecIu8 result;
  for (int i = 0; i < 8; i++) {
    result.data[i] = static_cast<uint8_t>(std::clamp(x.data[i], (int16_t)0, (int16_t)255));
    result.data[i+8] = static_cast<uint8_t>(std::clamp(y.data[i], (int16_t)0, (int16_t)255));
  }
  return result;
}

inline void vecStoreI(VecI16* dest, VecI16 x) {
  *dest = x;
}

// Floating-point operations
inline VecF cvtepi32Ps(VecI32 x) {
  VecF result;
  for (int i = 0; i < 4; i++) result.data[i] = static_cast<float>(x.data[i]);
  return result;
}

inline VecF addPs(VecF x, VecF y) {
  VecF result;
  for (int i = 0; i < 4; i++) result.data[i] = x.data[i] + y.data[i];
  return result;
}

inline VecF mulPs(VecF x, VecF y) {
  VecF result;
  for (int i = 0; i < 4; i++) result.data[i] = x.data[i] * y.data[i];
  return result;
}

inline VecF set1Ps(float x) {
  VecF result;
  for (int i = 0; i < 4; i++) result.data[i] = x;
  return result;
}

inline VecF minPs(VecF x, VecF y) {
  VecF result;
  for (int i = 0; i < 4; i++) result.data[i] = std::min(x.data[i], y.data[i]);
  return result;
}

inline VecF maxPs(VecF x, VecF y) {
  VecF result;
  for (int i = 0; i < 4; i++) result.data[i] = std::max(x.data[i], y.data[i]);
  return result;
}

inline VecF fmaddPs(VecF a, VecF b, VecF c) {
  VecF result;
  for (int i = 0; i < 4; i++) result.data[i] = std::fma(a.data[i], b.data[i], c.data[i]);
  return result;
}

inline float reduceAddPsR(float* sums, int length) {
  if (length == 2) return sums[0] + sums[1];
  length /= 2;
  for (int i = 0; i < length; ++i)
    sums[i] += sums[i + length];
  return reduceAddPsR(sums, length);
}

inline float reduceAddPs(VecF* sums) {
  return reduceAddPsR((float*)sums, 64 / sizeof(float));
}

inline uint32_t vecNNZ(VecI32 chunk) {
  uint32_t mask = 0;
  for (int i = 0; i < 4; i++) {
    if (chunk.data[i] > 0) mask |= (1u << i);
  }
  return mask;
}

#else

using VecI8 = int8x16_t;
using VecIu8 = uint8x16_t;
using VecI16 = int16x8_t;
using VecI16s = int8x8_t;
using VecIu16 = uint16x8_t;
using VecI32 = int32x4_t;
using VecF = float32x4_t;

// Integer operations
inline VecI16 addEpi16(VecI16 x, VecI16 y) {
  return vaddq_s16(x, y);
}

inline VecI16 subEpi16(VecI16 x, VecI16 y) {
  return vsubq_s16(x, y);
}

inline VecI16 convertEpi8Epi16(VecI16s x) {
  return vshll_n_s8(x, 0);
}

inline VecI16 minEpi16(VecI16 x, VecI16 y) {
  return vminq_s16(x, y);
}

inline VecI16 maxEpi16(VecI16 x, VecI16 y) {
  return vmaxq_s16(x, y);
}

inline VecI16 set1Epi16(int i) {
  return vdupq_n_s16(i);
}

inline VecIu8 set1Epi32(int i) {
  return vreinterpretq_u8_s32(vdupq_n_s32(i));
}

inline VecI16 srliEpi16(VecI16 x, int shift) {
  return vshlq_s16(x, vdupq_n_s16(-shift));
}

inline VecI16 slliEpi16(VecI16 x, int shift) {
  return vshlq_s16(x, vdupq_n_s16(shift));
}

inline VecI16 mulhiEpi16(VecI16 x, VecI16 y) {
  VecI32 lo = vmull_s16(vget_low_s16(x), vget_low_s16(y));
  VecI32 hi = vmull_s16(vget_high_s16(x), vget_high_s16(y));
  return vcombine_s16(vshrn_n_s32(lo, 16), vshrn_n_s32(hi, 16));
}

inline int16x8_t maddubs(uint8x16_t a, int8x16_t b) {
  int16x8_t tl = vmulq_s16(vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(a))), vmovl_s8(vget_low_s8(b)));
  int16x8_t th = vmulq_s16(vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(a))), vmovl_s8(vget_high_s8(b)));
  return vqaddq_s16(vuzp1q_s16(tl, th), vuzp2q_s16(tl, th));
}

inline int32x4_t madd(int16x8_t a, int16x8_t b) {
  int32x4_t low = vmull_s16(vget_low_s16(a), vget_low_s16(b));
  int32x4_t high = vmull_high_s16(a, b);
  return vpaddq_s32(low, high);
}

// SIMD dot-product
inline VecI32 dpbusdEpi32(VecI32 sum, VecIu8 u, VecI8 i) {
  int32x4_t sum32 = madd(maddubs(u, i), vdupq_n_s16(1));
  return vaddq_s32(sum32, sum);
}

inline VecI32 dpbusdEpi32x2(VecI32 sum, VecIu8 u, VecI8 i, VecIu8 u2, VecI8 i2) {
  int16x8_t mul1 = maddubs(u, i);
  int16x8_t mul2 = maddubs(u2, i2);
  VecI32 sum32 = madd(vaddq_s16(mul1, mul2), vdupq_n_s16(1));
  return vaddq_s32(sum32, sum);
}

// Pack and store
inline VecIu8 packusEpi16(VecI16 x, VecI16 y) {
  return vcombine_u8(vqmovun_s16(x), vqmovun_s16(y));
}

inline void vecStoreI(VecI16* dest, VecI16 x) {
  vst1q_s16(reinterpret_cast<int16_t*>(dest), x);
}

// Floating-point operations
inline VecF cvtepi32Ps(VecI32 x) {
  return vcvtq_f32_s32(x);
}

inline VecF addPs(VecF x, VecF y) {
  return vaddq_f32(x, y);
}

inline VecF mulPs(VecF x, VecF y) {
  return vmulq_f32(x, y);
}

inline VecF set1Ps(float x) {
  return vdupq_n_f32(x);
}

inline VecF minPs(VecF x, VecF y) {
  return vminq_f32(x, y);
}

inline VecF maxPs(VecF x, VecF y) {
  return vmaxq_f32(x, y);
}

inline VecF fmaddPs(VecF a, VecF b, VecF c) {
  return vfmaq_f32(c, a, b);
}

inline float reduceAddPsR(float* sums, int length) {
  if (length == 2) return sums[0] + sums[1];
  length /= 2;
  for (int i = 0; i < length; ++i)
    sums[i] += sums[i + length];
  return reduceAddPsR(sums, length);
}

inline float reduceAddPs(VecF* sums) {
  return reduceAddPsR((float*)sums, 64 / sizeof(float));
}

inline uint32_t vecNNZ(VecI32 chunk) {
  uint32x4_t mask = vcgtq_s32(chunk, vdupq_n_s32(0));
  uint16x4_t narrowed_mask = vmovn_u32(mask);
  uint64_t packed_mask = vget_lane_u64(vreinterpret_u64_u16(narrowed_mask), 0);
  return ((packed_mask & (1ULL << 0)) >> 0) |
    ((packed_mask & (1ULL << 16)) >> 15) |
    ((packed_mask & (1ULL << 32)) >> 30) |
    ((packed_mask & (1ULL << 48)) >> 45);
}

#endif