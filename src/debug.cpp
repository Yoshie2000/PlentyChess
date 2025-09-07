#include "debug.h"

#include <iostream>
#include <numeric>

namespace Debug {
    std::map<std::string, std::pair<uint64_t, uint64_t>> hitrates;
    std::map<std::string, std::vector<int64_t>> averages;
    std::map<std::string, std::pair<int64_t, int64_t>> extremes;
}

void Debug::hitrate(std::string name, bool condition) {
    if (hitrates.count(name)) {
        auto [calls, hits] = hitrates[name];
        calls++;
        hits += static_cast<uint64_t>(condition);
        hitrates[name] = std::make_pair(calls, hits);
    }
    else {
        hitrates[name] = std::make_pair(1, static_cast<uint64_t>(condition));
    }
}

void Debug::average(std::string name, int64_t value) {
    if (averages.count(name)) {
        averages[name].push_back(value);
    }
    else {
        averages[name] = {};
        averages[name].push_back(value);
    }
}

void Debug::extreme(std::string name, int64_t value) {
    if (extremes.count(name)) {
        auto [min, max] = extremes[name];
        min = std::min(min, value);
        max = std::max(max, value);
        extremes[name] = std::make_pair(min, max);
    }
    else {
        extremes[name] = std::make_pair(value, value);
    }
}

void Debug::show() {
    if (hitrates.size()) {
        std::cout << "\n--- Hit rates ---" << std::endl;
        for (auto element = hitrates.begin(); element != hitrates.end(); element++) {
            auto name = element->first;
            auto [calls, hits] = element->second;
            float hitrate = 100.0f * float(hits) / float(calls);
            std::cout << "\t" << name << ": " << hitrate << "%" << std::endl;
        }
    }

    if (averages.size()) {
        std::cout << "\n--- Averages ---" << std::endl;
        for (auto element = averages.begin(); element != averages.end(); element++) {
            auto name = element->first;
            auto values = element->second;
            float average = static_cast<float>(std::accumulate(values.begin(), values.end(), 0)) / values.size();
            std::cout << "\t" << name << ": " << average << std::endl;
        }
    }

    if (extremes.size()) {
        std::cout << "\n--- Extremes ---" << std::endl;
        for (auto element = extremes.begin(); element != extremes.end(); element++) {
            auto name = element->first;
            auto [min, max] = element->second;
            std::cout << "\t" << name << ": [" << min << "; " << max << "]" << std::endl;
        }
    }
}