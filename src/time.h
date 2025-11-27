#pragma once

#include <cstdint>

#include "search.h"

namespace TimeManagement {

    int64_t getTime();
    bool isHardLimitReached(SearchParameters& parameters, SearchData& data);
    bool isSoftLimitReached(SearchParameters& parameters, SearchData& data, double factor);
    void init(Board& rootBoard, SearchParameters& parameters, SearchData& data);

}