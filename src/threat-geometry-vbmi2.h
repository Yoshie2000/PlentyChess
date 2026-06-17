#pragma once

#include <immintrin.h>
#include <utility>

namespace ThreatGeometry {

    struct Vector {
        __m512i raw;
        Vector flip() const { return {_mm512_shuffle_i64x2(raw, raw, 0b01001110)}; }
    };

    struct Permutation {
        Vector indexes;
        Bitrays valid;
    };

    inline Permutation permutationFor(Square focus) {
        __m512i indexes = _mm512_loadu_si512(PERMUTATION_TABLE[focus].data());
        return {{indexes}, _mm512_testn_epi8_mask(indexes, _mm512_set1_epi8(0x80))};
    }

    inline __m512i colouredMailbox(const uint8_t* pieces, uint64_t black) {
        __m512i types = _mm512_loadu_si512(pieces);
        return _mm512_mask_add_epi8(types, black, types, _mm512_set1_epi8(8));
    }

    inline std::pair<Vector, Vector> permuteMailbox(const Permutation& permutation, __m512i mailbox) {
        __m512i lut = _mm512_broadcast_i32x4(_mm_loadu_si128(reinterpret_cast<const __m128i*>(PIECE_TO_BIT.data())));
        __m512i permuted = _mm512_permutexvar_epi8(permutation.indexes.raw, mailbox);
        __m512i bits = _mm512_maskz_shuffle_epi8(permutation.valid, lut, permuted);
        return {{permuted}, {bits}};
    }

    inline Bitrays closestOccupied(Vector bits) {
        Bitrays occupied = _mm512_test_epi8_mask(bits.raw, bits.raw);
        Bitrays o = occupied | 0x8181818181818181ULL;
        return (o ^ (o - 0x0303030303030303ULL)) & occupied;
    }

    inline Bitrays rayFill(Bitrays br) {
        br = (br + 0x7E7E7E7E7E7E7E7EULL) & 0x8080808080808080ULL;
        return br - (br >> 7);
    }

    inline Bitrays outgoingThreats(uint8_t colouredPiece, Bitrays closest) {
        return OUTGOING_THREATS[colouredPiece] & closest;
    }

    inline Bitrays incomingAttackers(Vector bits, Bitrays closest) {
        return _mm512_test_epi8_mask(bits.raw, _mm512_loadu_si512(INCOMING_THREATS_MASK.data())) & closest;
    }

    inline Bitrays incomingSliders(Vector bits, Bitrays closest) {
        return _mm512_test_epi8_mask(bits.raw, _mm512_loadu_si512(INCOMING_SLIDERS_MASK.data())) & closest & 0xFEFEFEFEFEFEFEFEULL;
    }

    template<bool outgoing>
    inline int pushFocusThreats(void* dst, Vector indexes, Vector pieces, Bitrays br, uint8_t focusPiece, uint8_t focusSquare) {
        const __m512i pair2Shuffle = _mm512_set_epi8(
            79, 15, 79, 15, 78, 14, 78, 14, 77, 13, 77, 13, 76, 12, 76, 12, 75, 11, 75, 11,
            74, 10, 74, 10, 73, 9, 73, 9, 72, 8, 72, 8, 71, 7, 71, 7, 70, 6, 70, 6, 69, 5,
            69, 5, 68, 4, 68, 4, 67, 3, 67, 3, 66, 2, 66, 2, 65, 1, 65, 1, 64, 0, 64, 0
        );
        __m512i focus = _mm512_set1_epi16(static_cast<int16_t>(focusPiece | (focusSquare << 8)));
        __m512i otherSq = _mm512_maskz_compress_epi8(br, indexes.raw);
        __m512i otherPiece = _mm512_maskz_compress_epi8(br, pieces.raw);
        __m512i other = _mm512_permutex2var_epi8(otherPiece, pair2Shuffle, otherSq);
        constexpr uint64_t mask = outgoing ? 0xCCCCCCCCCCCCCCCCULL : 0x3333333333333333ULL;
        _mm512_storeu_si512(dst, _mm512_mask_mov_epi8(focus, mask, other));
        return __builtin_popcountll(br);
    }

    inline int pushDiscoveredThreats(void* dst, Vector indexes, Vector pieces, Bitrays sliders, Bitrays victims) {
        __m128i sliderPiece = _mm512_castsi512_si128(_mm512_maskz_compress_epi8(sliders, pieces.raw));
        __m128i sliderSq = _mm512_castsi512_si128(_mm512_maskz_compress_epi8(sliders, indexes.raw));
        __m128i victimPiece = _mm512_castsi512_si128(_mm512_maskz_compress_epi8(victims, pieces.flip().raw));
        __m128i victimSq = _mm512_castsi512_si128(_mm512_maskz_compress_epi8(victims, indexes.flip().raw));
        __m128i sliderPair = _mm_unpacklo_epi8(sliderPiece, sliderSq);
        __m128i victimPair = _mm_unpacklo_epi8(victimPiece, victimSq);
        _mm_storeu_si128(static_cast<__m128i*>(dst), _mm_unpacklo_epi16(sliderPair, victimPair));
        _mm_storeu_si128(static_cast<__m128i*>(dst) + 1, _mm_unpackhi_epi16(sliderPair, victimPair));
        return __builtin_popcountll(victims);
    }

}
