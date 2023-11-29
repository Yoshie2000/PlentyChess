#include "thread.h"
#include "uci.h"
#include "tt.h"

int main() {
    initZobrist();

    Thread searchThread;
    uciLoop(&searchThread);
    return 0;
}