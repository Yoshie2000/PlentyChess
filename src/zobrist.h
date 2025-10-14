#include <cstdint>

#include "types.h"

namespace Zobrist {

    extern uint64_t PIECE_SQUARES[2][Piece::TOTAL][64];
    extern uint64_t STM_BLACK;
    extern uint64_t NO_PAWNS;
    extern uint64_t CASTLING[16]; // 2^4
    extern uint64_t ENPASSENT[8]; // 8 files

    extern uint64_t CUCKOO_HASHES[8192];
    extern Move CUCKOO_MOVES[8192];

    constexpr int H1(uint64_t h) { return h & 0x1fff; }
    constexpr int H2(uint64_t h) { return (h >> 16) & 0x1fff; }

    void init();

}
