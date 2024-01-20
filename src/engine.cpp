#include "spsa.h"
#include "thread.h"
#include "uci.h"
#include "tt.h"
#include "magic.h"
#include "bitboard.h"

int main(int argc, char* argv[]) {
    generateMagics();

    initBitboard();
    initReductions();
    initZobrist();

    TT.clear();

    ThreadPool threads(1);

    uciLoop(&threads, argc, argv);
    return 0;
}