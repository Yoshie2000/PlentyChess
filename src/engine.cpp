#include "thread.h"
#include "uci.h"
#include "tt.h"
#include "magic.h"

int main(int argc, char* argv[]) {
    initZobrist();
    generateMagics();

    Thread searchThread;
    uciLoop(&searchThread, argc, argv);
    return 0;
}