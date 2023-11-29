#include "thread.h"
#include "uci.h"
#include "tt.h"
#include "magic.h"

int main() {
    initZobrist();

    // Thread searchThread;
    // uciLoop(&searchThread);
    generateMagics();
    return 0;
}