#pragma once

#include <string>
#include <map>
#include <cstdint>
#include <vector>

namespace Debug {

    void hitrate(std::string name, bool condition);
    void average(std::string name, int64_t value);
    void extreme(std::string name, int64_t value);

    void show();

}
