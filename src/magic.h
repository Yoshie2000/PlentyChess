#pragma once

#include <cstddef>
#include <immintrin.h>

#include "types.h"

#if defined(USE_BMI2)

struct MagicEntry {
    Bitboard mask;
    Bitboard* tableIndex;

    MagicEntry() {}
};

extern MagicEntry ROOK_MAGICS[64];
extern MagicEntry BISHOP_MAGICS[64];

extern Bitboard ROOK_MOVES[102400];
extern Bitboard BISHOP_MOVES[5248];

#else

struct MagicEntry {
    Bitboard mask;
    uint64_t magic;
    uint8_t shift;

    MagicEntry() {}
};

extern MagicEntry ROOK_MAGICS[64];
extern MagicEntry BISHOP_MAGICS[64];

extern Bitboard ROOK_MOVES[64][4096];
extern Bitboard BISHOP_MOVES[64][4096];

inline size_t magicIndex(MagicEntry entry, Bitboard occupied) {
    Bitboard blockers = occupied & entry.mask;
    blockers *= entry.magic;
    return blockers >> entry.shift;
}

#endif

inline size_t magicIndexRook(Square square, Bitboard occupied) {
    MagicEntry entry = ROOK_MAGICS[square];
#if defined(USE_BMI2)
    return (size_t)_pext_u64(occupied, entry.mask);
#else
    Bitboard blockers = occupied & entry.mask;
    blockers *= entry.magic;
    return blockers >> entry.shift;
#endif
}

inline size_t magicIndexBishop(Square square, Bitboard occupied) {
    MagicEntry entry = BISHOP_MAGICS[square];
#if defined(USE_BMI2)
    return (size_t)_pext_u64(occupied, entry.mask);
#else
    Bitboard blockers = occupied & entry.mask;
    blockers *= entry.magic;
    return blockers >> entry.shift;
#endif
}

inline Bitboard getRookMoves(Square square, Bitboard occupied) {
#if defined(USE_BMI2)
    return ROOK_MAGICS[square].tableIndex[magicIndexRook(square, occupied)];
#else
    return ROOK_MOVES[square][magicIndexRook(square, occupied)];
#endif
}

inline Bitboard getBishopMoves(Square square, Bitboard occupied) {
#if defined(USE_BMI2)
    return BISHOP_MAGICS[square].tableIndex[magicIndexBishop(square, occupied)];
#else
    return BISHOP_MOVES[square][magicIndexBishop(square, occupied)];
#endif
}

void generateMagics();