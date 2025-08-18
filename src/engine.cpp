#include "spsa.h"
#include "uci.h"
#include "zobrist.h"
#include "magic.h"
#include "bitboard.h"
#include "nnue.h"

int main(int argc, char* argv[]) {
    generateMagics();

    BB::init();
    initZobrist();

    initNetworkData();

    uciLoop(argc, argv);
    return 0;
}