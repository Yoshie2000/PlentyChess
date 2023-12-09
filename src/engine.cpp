#include "thread.h"
#include "uci.h"
#include "tt.h"
#include "magic.h"
#include "bitboard.h"

int main(int argc, char* argv[]) {
    initZobrist();
    generateMagics();
    initBitboard();

    Thread searchThread;
    uciLoop(&searchThread, argc, argv);
    return 0;
}