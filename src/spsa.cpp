#include <vector>
#include <iostream>

#include "spsa.h"

void SPSA::trySetParam(std::string varName, std::string value) {
    
    SPSAValue<int>* intEntry = {};
    SPSAValue<float>* floatEntry = {};
    bool foundInt = false;
    bool foundFloat = false;
    for (SPSAValue<int>& other : instance().intValues) {
        if (other.varName == varName) {
            intEntry = &other;
            foundInt = true;
            break;
        }
    }
    for (SPSAValue<float>& other : instance().floatValues) {
        if (other.varName == varName) {
            floatEntry = &other;
            foundFloat = true;
            break;
        }
    }

    bool isFloat = value.find(".") <= value.size();
    if (isFloat && foundFloat) {
        float* floatVariable = (float*)floatEntry->varPointer;
        *floatVariable = std::stof(value);
    }
    else if (foundInt) {
        int* intVariable = (int*)intEntry->varPointer;
        *intVariable = std::stoi(value);
    }
}