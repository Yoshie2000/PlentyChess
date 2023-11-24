#ifndef ENGINE_H_INCLUDED
    #define ENGINE_H_INCLUDED

    #include <stdbool.h>
    #include <thread>
    #include <mutex>
    #include <condition_variable>

    extern std::thread searchThread;
    extern std::mutex searchMutex;
    extern std::condition_variable searchCv;
    extern bool searchStopped;

#endif