#include <chrono>

#include "time.h"
#include "uci.h"

int64_t getTime() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

bool timeOver(SearchParameters* parameters, SearchData* data) {
    return (data->maxTime && (data->nodesSearched % 1024) == 0 && getTime() >= data->maxTime) || (parameters->nodes && data->nodesSearched >= parameters->nodes);
}

bool timeOverDepthCleared(SearchParameters* parameters, SearchData* data, double factor) {
    int64_t adjustedOptTime = (int64_t)(data->startTime + (double)(data->optTime - data->startTime) * factor);
    int64_t currentTime = getTime();
    return (data->maxTime && (currentTime >= adjustedOptTime || currentTime >= data->maxTime)) || (parameters->nodes && data->nodesSearched >= parameters->nodes);
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
    time -= std::min((int64_t)UCI::Options.moveOverhead.value, time / 2);

    // Figure out how we should spend this time
    if (parameters->movetime) {
        data->optTime = data->startTime + time;
        data->maxTime = data->startTime + time;
    }
    else if (parameters->movestogo) {
        int64_t totalTime = time / parameters->movestogo;
        int64_t maxTime = 3 * time / 4;

        data->optTime = data->startTime + std::min<int64_t>(maxTime, 0.8 * totalTime);
        data->maxTime = data->startTime + std::min<int64_t>(maxTime, 2.5 * totalTime);
    }
    else if (parameters->wtime && parameters->btime) {
        int64_t totalTime = time / 20 + increment / 2;
        int64_t maxTime = 3 * time / 4;

        data->optTime = data->startTime + std::min<int64_t>(maxTime, 0.8 * totalTime);
        data->maxTime = data->startTime + std::min<int64_t>(maxTime, 2.5 * totalTime);
    }
}