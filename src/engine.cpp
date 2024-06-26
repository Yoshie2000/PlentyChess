#include "spsa.h"
#include "uci.h"
#include "tt.h"
#include "zobrist.h"
#include "magic.h"
#include "bitboard.h"
#include "nnue.h"

int main(int argc, char* argv[]) {
    generateMagics();

    BB::init();
    initReductions();
    initZobrist();

    initNetworkData();

    TT.clear();

    uciLoop(argc, argv);
    return 0;
}