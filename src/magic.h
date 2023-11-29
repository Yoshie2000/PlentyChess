#include "types.h"
#include <cstddef>
#ifndef MAGIC_H_INCLUDED
#define MAGIC_H_INCLUDED

struct MagicEntry {
    Bitboard mask;
    uint64_t magic;
    uint8_t shift;

    MagicEntry() {}
};

extern const MagicEntry ROOK_MAGICS[64];
extern const MagicEntry BISHOP_MAGICS[64];

extern const Bitboard ROOK_MOVES[64][10];
extern const Bitboard BISHOP_MOVES[64][10];

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

#endif