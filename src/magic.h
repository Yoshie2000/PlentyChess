#pragma once

#include <cstddef>

#include "types.h"

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

inline Bitboard getRookMoves(Square square, Bitboard occupied) {
    MagicEntry magic = ROOK_MAGICS[square];
    const Bitboard* moves = ROOK_MOVES[square];
    return moves[magicIndex(magic, occupied)];
}

inline Bitboard getBishopMoves(Square square, Bitboard occupied) {
    MagicEntry magic = BISHOP_MAGICS[square];
    const Bitboard* moves = BISHOP_MOVES[square];
    return moves[magicIndex(magic, occupied)];
}

void generateMagics();