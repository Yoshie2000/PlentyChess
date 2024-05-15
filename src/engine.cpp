#include "spsa.h"
#include "uci.h"
#include "tt.h"
#include "magic.h"
#include "bitboard.h"
#include "nnue.h"
#include "debug.h"

int main(int argc, char* argv[]) {
    generateMagics();

    initBitboard();
    initReductions();
    initZobrist();

    initNetworkData();

    TT.clear();

    uciLoop(argc, argv);

    Debug::printMean();

    return 0;
}