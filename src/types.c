#include "types.h"

inline Square lsb(Bitboard bb) {
    return __builtin_ctzll(bb);
}

inline Square popLSB(Bitboard* bb) {
    Square l = lsb(*bb);
    *bb &= *bb - 1;
    return l;
}