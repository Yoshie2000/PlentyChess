#include "thread.h"
#include "uci.h"
#include "tt.h"
#include "magic.h"

int main() {
    initZobrist();
    generateMagics();

    Thread searchThread;
    uciLoop(&searchThread);
    return 0;
}