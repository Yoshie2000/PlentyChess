#include <chrono>

#include "time.h"

int64_t getTime() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

bool timeOver(SearchParameters* parameters, SearchData* data) {
    return (data->maxTime && getTime() >= data->maxTime) || (parameters->nodes && data->nodesSearched >= parameters->nodes);
}

void initTimeManagement(Board* rootBoard, SearchParameters* parameters, SearchData* data) {
    data->startTime = getTime();
    data->maxTime = 0;

    int64_t time = -1;
    int64_t increment = 0;

    // Figure out time / increment
    if (parameters->movetime) {
        time = parameters->movetime;
    }
    else if (parameters->wtime && parameters->btime) {
        time = rootBoard->stm == COLOR_WHITE ? parameters->wtime : parameters->btime;
        increment = rootBoard->stm == COLOR_WHITE ? parameters->winc : parameters->binc;
    }

    // Safety in case nothing (or something negative) was specified
    if (time < 0)
        time = 1000;
    // Subtract some time for communication overhead
    time -= std::min((int64_t)100, time / 2);

    // Figure out how we should spend this time
    if (parameters->movetime) {
        data->maxTime = data->startTime + time;
    }
    else if (parameters->wtime && parameters->btime) {
        int64_t totalTime = time / 20 + increment / 2;

        data->maxTime = data->startTime + totalTime;
    }
}