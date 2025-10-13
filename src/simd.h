#pragma once

#if defined(ARCH_X86)
#include <immintrin.h>
#include <xmmintrin.h>
#else
#include <arm_neon.h>
#endif

#if defined(__AVX512F__) && defined(__AVX512BW__)

using VecI8 = __m512i;
using VecIu8 = __m512i;
using VecI16 = __m512i;
using VecIu16 = __m512i;
using VecI32 = __m512i;
using VecF = __m512;

inline VecI16 addEpi16(VecI16 x, VecI16 y) {
  return _mm512_add_epi16(x, y);
}

inline VecI16 subEpi16(VecI16 x, VecI16 y) {
  return _mm512_sub_epi16(x, y);
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

inline VecI16 set1Epi32(int i) {
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
using VecIu16 = __m256i;
using VecI32 = __m256i;
using VecF = __m256;

inline VecI16 addEpi16(VecI16 x, VecI16 y) {
  return _mm256_add_epi16(x, y);
}

inline VecI16 subEpi16(VecI16 x, VecI16 y) {
  return _mm256_sub_epi16(x, y);
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

inline VecI16 set1Epi32(int i) {
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
using VecIu16 = __m128i;
using VecI32 = __m128i;
using VecF = __m128;

inline VecI16 addEpi16(VecI16 x, VecI16 y) {
  return _mm_add_epi16(x, y);
}

inline VecI16 subEpi16(VecI16 x, VecI16 y) {
  return _mm_sub_epi16(x, y);
}

inline VecI16 minEpi16(VecI16 x, VecI16 y) {
  return _mm_min_epi16(x, y);
}

inline VecI16 maxEpi16(VecI16 x, VecI16 y) {
  return _mm_max_epi16(x, y);
}

inline VecI16 set1Epi16(int i) {
  return _mm_set1_epi16(i);
}

inline VecI16 set1Epi32(int i) {
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

#else

using VecI8 = int8x16_t;
using VecIu8 = uint8x16_t;
using VecI16 = int16x8_t;
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

inline VecI16 minEpi16(VecI16 x, VecI16 y) {
  return vminq_s16(x, y);
}

inline VecI16 maxEpi16(VecI16 x, VecI16 y) {
  return vmaxq_s16(x, y);
}

inline VecI16 set1Epi16(int i) {
  return vdupq_n_s16(i);
}

inline VecI32 set1Epi32(int i) {
  return vdupq_n_s32(i);
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