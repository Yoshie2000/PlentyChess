#pragma once

#include <cstdint>

#include "search.h"

int64_t getTime();
bool timeOver(SearchParameters& parameters, SearchData& data);
bool timeOverDepthCleared(SearchParameters& parameters, SearchData& data, double factor);
void initTimeManagement(Board& rootBoard, SearchParameters& parameters, SearchData& data);