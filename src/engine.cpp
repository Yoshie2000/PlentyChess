#include "thread.h"
#include "uci.h"

int main() {
    Thread searchThread;
    uciLoop(&searchThread);
    return 0;
}