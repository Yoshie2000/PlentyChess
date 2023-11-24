#ifndef ENGINE_H_INCLUDED
    #define ENGINE_H_INCLUDED

    #include <stdbool.h>

    extern pthread_t searchThread;
    extern pthread_cond_t searchCond;
    extern pthread_mutex_t searchMutex;
    extern bool searchStopped;

#endif