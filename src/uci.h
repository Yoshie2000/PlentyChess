#ifndef UCI_H_INCLUDED
    #define UCI_H_INCLUDED

    #include "thread.h"

    #define UCI_LINE_LENGTH 256

    void uciLoop(Thread* searchThread, int argc, char* argv[]);

#endif