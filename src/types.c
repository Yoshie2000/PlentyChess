#include "types.h"

Square lsb(Bitboard bb) {
    return __builtin_ctzll(bb);
}

Square popLSB(Bitboard* bb) {
    Square l = lsb(*bb);
    *bb &= *bb - 1;
    return l;
}