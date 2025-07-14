#include <chrono>

#include "time.h"
#include "uci.h"
#include "spsa.h"

TUNE_INT_DISABLED(maxTimeFactor, 729, 500, 1000);
TUNE_INT_DISABLED(totalTimeDivisor, 134, 50, 500);
TUNE_INT_DISABLED(totalTimeIncrementDivisor, 15, 5, 50);
TUNE_FLOAT_DISABLED(optTimeFactor, 0.8462773005555602f, 0.5f, 1.5f);
TUNE_FLOAT_DISABLED(maxTimeFactor2, 2.8100754942943835f, 1.5f, 3.5f);

int64_t getTime() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

bool timeOver(SearchParameters& parameters, SearchData& data) {
    if (parameters.ponder)
        return false;
    return (data.maxTime && (data.nodesSearched % 1024) == 0 && getTime() >= data.maxTime) || (parameters.nodes && data.nodesSearched >= parameters.nodes * (1 + 9 * UCI::Options.datagen.value));
}

bool timeOverDepthCleared(SearchParameters& parameters, SearchData& data, double factor) {
    if (parameters.ponder)
        return false;
    int64_t adjustedOptTime = (int64_t)(data.startTime + (double)(data.optTime - data.startTime) * factor);
    int64_t currentTime = getTime();
    return (data.maxTime && (currentTime >= adjustedOptTime || currentTime >= data.maxTime)) || (parameters.nodes && data.nodesSearched >= parameters.nodes * (1 + 9 * UCI::Options.datagen.value));
}

void initTimeManagement(Board& rootBoard, SearchParameters& parameters, SearchData& data) {
    data.startTime = getTime();
    data.maxTime = 0;

    if (UCI::Options.datagen.value)
        return;

    int64_t time = -1;
    int64_t increment = 0;

    // Figure out time / increment
    if (parameters.movetime) {
        time = parameters.movetime;
    }
    else if (parameters.wtime && parameters.btime) {
        time = rootBoard.stm == Color::WHITE ? parameters.wtime : parameters.btime;
        increment = rootBoard.stm == Color::WHITE ? parameters.winc : parameters.binc;
    }

    // Safety in case nothing (or something negative) was specified
    if (time < 0)
        time = 1000;
    // Subtract some time for communication overhead
    time -= std::min((int64_t)UCI::Options.moveOverhead.value, time / 2);

    // Figure out how we should spend this time
    if (parameters.movetime) {
        data.optTime = data.startTime + time;
        data.maxTime = data.startTime + time;
    }
    else if (parameters.movestogo) {
        int64_t totalTime = time / parameters.movestogo;
        int64_t maxTime = maxTimeFactor * time / 1000;

        data.optTime = data.startTime + std::min<int64_t>(maxTime, optTimeFactor * totalTime);
        data.maxTime = data.startTime + std::min<int64_t>(maxTime, maxTimeFactor2 * totalTime);
    }
    else if (parameters.wtime && parameters.btime) {
        int64_t totalTime = 10 * time / totalTimeDivisor + 10 * increment / totalTimeIncrementDivisor;
        int64_t maxTime = maxTimeFactor * time / 1000;

        data.optTime = data.startTime + std::min<int64_t>(maxTime, optTimeFactor * totalTime);
        data.maxTime = data.startTime + std::min<int64_t>(maxTime, maxTimeFactor2 * totalTime);
    }
}