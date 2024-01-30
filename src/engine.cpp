#include "spsa.h"
#include "uci.h"
#include "tt.h"
#include "magic.h"
#include "bitboard.h"
#include "nnue.h"

int main(int argc, char* argv[]) {
    generateMagics();

    initBitboard();
    initReductions();
    initZobrist();

    initNetworkData();

    TT.clear();

    uciLoop(argc, argv);
    return 0;
}