#pragma once

#include <cstdint>

#include "search.h"

int64_t getTime();
bool timeOver(SearchParameters* parameters, SearchData* data);
void initTimeManagement(Board* rootBoard, SearchParameters* parameters, SearchData* data);