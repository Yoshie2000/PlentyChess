#include <vector>
#include <iostream>

#include "spsa.h"

void SPSA::trySetParam(std::string varName, std::string value) {
    SPSAValue* entry = {};
    bool found = false;
    for (SPSAValue& other : instance().tuneValues) {
        if (other.varName == varName) {
            entry = &other;
            found = true;
            break;
        }
    }
    if (!found) return;

    bool isFloat = value.find(".") <= value.size();
    if (isFloat) {
        float* floatVariable = (float*)entry->varPointer;
        *floatVariable = std::stof(value);
    }
    else {
        int* intVariable = (int*)entry->varPointer;
        *intVariable = std::stoi(value);
    }
}