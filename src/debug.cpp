#include "debug.h"

#include <vector>
#include <iostream>

namespace Debug {

    std::vector<std::vector<int>> meanValues;

    void printMean() {
        for (size_t slot = 0; slot < meanValues.size(); slot++) {
            int64_t sum = 0;
            for (size_t i = 0; i < meanValues[slot].size(); i++)
                sum += meanValues[slot][i];
            std::cout << "Mean #" << slot << ": " << ((double) (sum) / meanValues[slot].size()) << "\n";
            std::cout << "Sum  #" << slot << ": " << sum << "\n";
        }
        std::cout << std::endl;
    }

    void mean(int value, int slot) {
        while ((size_t) slot >= meanValues.size()) {
            std::vector<int> newVec;
            meanValues.push_back(newVec);
        }

        meanValues[slot].push_back(value);
        if (meanValues[slot].size() % 100000 == 0)
            printMean();
    }

}