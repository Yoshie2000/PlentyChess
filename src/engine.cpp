#include "thread.h"
#include "uci.h"
#include "tt.h"
#include "magic.h"
#include "bitboard.h"
#include "history.h"
#include "nnue.h"

int main(int argc, char* argv[]) {
    generateMagics();

    initBitboard();
    initReductions();
    initHistory();
    initZobrist();

    TT.clear();
    nnue.initNetwork();

    Thread searchThread;
    uciLoop(&searchThread, argc, argv);
    return 0;
}