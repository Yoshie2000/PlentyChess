#pragma once

#include <cstdint>

#include "types.h"

namespace Zobrist {

    constexpr int FMR_GRANULARITY = 10;

    extern Hash PIECE_SQUARES[2][Piece::TOTAL][64];
    extern Hash STM_BLACK;
    extern Hash NO_PAWNS;
    extern Hash CASTLING[16]; // 2^4
    extern Hash ENPASSENT[8]; // 8 files
    extern Hash FMR[110 / FMR_GRANULARITY];

    extern Hash CUCKOO_HASHES[8192];
    extern Move CUCKOO_MOVES[8192];

    constexpr int H1(Hash h) { return h & 0x1fff; }
    constexpr int H2(Hash h) { return (h >> 16) & 0x1fff; }

    void init();

}
