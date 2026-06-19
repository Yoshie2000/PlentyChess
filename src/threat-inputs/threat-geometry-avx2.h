#pragma once

#include <immintrin.h>
#include <array>
#include <tuple>
#include <cstring>
#include "threat-geometry.h"

namespace ThreatGeometry {

    struct Vector {
        std::array<__m256i, 2> raw;

        Vector flip() const {
            return Vector{ { raw[1], raw[0] } };
        }

        Bitrays toMask() const {
            return static_cast<uint32_t>(_mm256_movemask_epi8(raw[0]))
                | (static_cast<Bitrays>(_mm256_movemask_epi8(raw[1])) << 32);
        }

        static Vector load(const void* ptr) {
            return Vector{ {
                _mm256_loadu_si256(static_cast<const __m256i*>(ptr) + 0),
                _mm256_loadu_si256(static_cast<const __m256i*>(ptr) + 1),
            } };
        }

        template <typename T>
        static Vector cast(const T& v) {
            static_assert(sizeof(T) == sizeof(__m256i) * 2);
            return load(&v);
        }
    };

    struct Permutation {
        Vector indexes;
        Vector invalid;
    };

    inline Permutation permutationFor(Square focus) {
        const Vector indexes = Vector::cast(PERMUTATION_TABLE[focus]);
        const Vector invalid{ {
            _mm256_cmpeq_epi8(indexes.raw[0], _mm256_set1_epi8(0x80)),
            _mm256_cmpeq_epi8(indexes.raw[1], _mm256_set1_epi8(0x80)),
        } };
        return Permutation{ indexes, invalid };
    }

    inline __m256i colourAddend(uint32_t blackHalf) {
        const __m256i spread = _mm256_setr_epi64x(
            0x0000000000000000LL,
            0x0101010101010101LL,
            0x0202020202020202LL,
            0x0303030303030303LL
        );
        const __m256i bit = _mm256_set1_epi64x(0x8040201008040201ULL);
        __m256i v = _mm256_shuffle_epi8(_mm256_set1_epi32(blackHalf), spread);
        v = _mm256_cmpeq_epi8(_mm256_and_si256(v, bit), bit);
        return _mm256_and_si256(v, _mm256_set1_epi8(8));
    }

    inline Vector colouredMailbox(const uint8_t* pieces, uint64_t black) {
        const Vector types = Vector::load(pieces);
        return Vector{ {
            _mm256_add_epi8(types.raw[0], colourAddend(static_cast<uint32_t>(black))),
            _mm256_add_epi8(types.raw[1], colourAddend(static_cast<uint32_t>(black >> 32))),
        } };
    }

    inline std::tuple<Vector, Vector> permuteMailbox(const Permutation& permutation, Vector maskedMailbox) {
        const __m256i lut = _mm256_broadcastsi128_si256(_mm_loadu_si128(
            reinterpret_cast<const __m128i*>(PIECE_TO_BIT.data())
        ));

        const auto halfSwizzler = [](__m256i bytes0, __m256i bytes1, __m256i idxs) {
            const __m256i mask0 = _mm256_slli_epi64(idxs, 2);
            const __m256i mask1 = _mm256_slli_epi64(idxs, 3);

            const __m256i lolo0 = _mm256_shuffle_epi8(_mm256_permute2x128_si256(bytes0, bytes0, 0x00), idxs);
            const __m256i hihi0 = _mm256_shuffle_epi8(_mm256_permute2x128_si256(bytes0, bytes0, 0x11), idxs);
            const __m256i x = _mm256_blendv_epi8(lolo0, hihi0, mask1);

            const __m256i lolo1 = _mm256_shuffle_epi8(_mm256_permute2x128_si256(bytes1, bytes1, 0x00), idxs);
            const __m256i hihi1 = _mm256_shuffle_epi8(_mm256_permute2x128_si256(bytes1, bytes1, 0x11), idxs);
            const __m256i y = _mm256_blendv_epi8(lolo1, hihi1, mask1);

            return _mm256_blendv_epi8(x, y, mask0);
            };

        const Vector permuted{ {
            halfSwizzler(maskedMailbox.raw[0], maskedMailbox.raw[1], permutation.indexes.raw[0]),
            halfSwizzler(maskedMailbox.raw[0], maskedMailbox.raw[1], permutation.indexes.raw[1]),
        } };
        const Vector bits{ {
            _mm256_andnot_si256(permutation.invalid.raw[0], _mm256_shuffle_epi8(lut, permuted.raw[0])),
            _mm256_andnot_si256(permutation.invalid.raw[1], _mm256_shuffle_epi8(lut, permuted.raw[1])),
        } };

        return { permuted, bits };
    }

    inline Vector ignoreSquare(Vector mailbox, Square ignore) {
        const Vector iota = Vector::cast(std::array<uint8_t, 64>{{
            0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
            17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
            33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
            49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63
        }});
        const __m256i ignoreVec = _mm256_set1_epi8(static_cast<int8_t>(ignore));
        const __m256i noneVec = _mm256_set1_epi8(static_cast<int8_t>(Piece::NONE));
        return Vector{ {
            _mm256_blendv_epi8(mailbox.raw[0], noneVec, _mm256_cmpeq_epi8(iota.raw[0], ignoreVec)),
            _mm256_blendv_epi8(mailbox.raw[1], noneVec, _mm256_cmpeq_epi8(iota.raw[1], ignoreVec)),
        } };
    }

    inline Bitrays closestOccupied(Vector bits) {
        const Vector unoccupied{ {
            _mm256_cmpeq_epi8(bits.raw[0], _mm256_setzero_si256()),
            _mm256_cmpeq_epi8(bits.raw[1], _mm256_setzero_si256()),
        } };
        const Bitrays occupied = ~unoccupied.toMask();
        const Bitrays o = occupied | 0x8181818181818181ULL;
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
        const Vector mask = Vector::cast(INCOMING_THREATS_MASK);
        const Vector v{ {
            _mm256_cmpeq_epi8(_mm256_and_si256(bits.raw[0], mask.raw[0]), _mm256_setzero_si256()),
            _mm256_cmpeq_epi8(_mm256_and_si256(bits.raw[1], mask.raw[1]), _mm256_setzero_si256()),
        } };
        return ~v.toMask() & closest;
    }

    inline Bitrays incomingSliders(Vector bits, Bitrays closest) {
        const Vector mask = Vector::cast(INCOMING_SLIDERS_MASK);
        const Vector v{ {
            _mm256_cmpeq_epi8(_mm256_and_si256(bits.raw[0], mask.raw[0]), _mm256_setzero_si256()),
            _mm256_cmpeq_epi8(_mm256_and_si256(bits.raw[1], mask.raw[1]), _mm256_setzero_si256()),
        } };
        return ~v.toMask() & closest & 0xFEFEFEFEFEFEFEFEULL;
    }

    template<bool outgoing>
    inline int pushFocusThreats(void* dst, Vector indexes, Vector pieces, Bitrays br, uint8_t focusPiece, uint8_t focusSquare) {
        uint8_t pieceArr[64], sqArr[64];
        std::memcpy(pieceArr, pieces.raw.data(), sizeof(pieceArr));
        std::memcpy(sqArr, indexes.raw.data(), sizeof(sqArr));

        uint8_t* out = static_cast<uint8_t*>(dst);
        const int count = BB::popcount(br);
        while (br) {
            const Square i = popLSB(&br);
            const uint8_t otherPiece = pieceArr[i];
            const uint8_t otherSquare = sqArr[i];
            if constexpr (outgoing) {
                out[0] = focusPiece;
                out[1] = focusSquare;
                out[2] = otherPiece;
                out[3] = otherSquare;
            }
            else {
                out[0] = otherPiece;
                out[1] = otherSquare;
                out[2] = focusPiece;
                out[3] = focusSquare;
            }
            out += 4;
        }
        return count;
    }

    inline int pushDiscoveredThreats(void* dst, Vector indexes, Vector pieces, Bitrays sliders, Bitrays victims) {
        uint8_t pieceArr[64], sqArr[64];
        std::memcpy(pieceArr, pieces.raw.data(), sizeof(pieceArr));
        std::memcpy(sqArr, indexes.raw.data(), sizeof(sqArr));

        uint8_t* out = static_cast<uint8_t*>(dst);
        const int count = BB::popcount(victims);
        while (sliders) {
            const Square s = popLSB(&sliders);
            const Square v = popLSB(&victims) ^ 32;
            out[0] = pieceArr[s];
            out[1] = sqArr[s];
            out[2] = pieceArr[v];
            out[3] = sqArr[v];
            out += 4;
        }
        return count;
    }

}
