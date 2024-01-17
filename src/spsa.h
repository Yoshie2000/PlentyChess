#pragma once

#include <type_traits>
#include <string>
#include <vector>
#include <iostream>

struct SPSAValue {
    std::string varName;
    void* varPointer;
};

class SPSA {

public:
    std::vector<SPSAValue> tuneValues;

    static SPSA& instance() {
        static SPSA t;
        return t;
    }  // Singleton

    template<
        typename T,
        typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type
    >
    int tune(std::string varName, T* initialValue, T minimumValue, T maximumValue) {
        bool isFloat = std::is_floating_point<T>::value;

        if (isFloat)
            printf("%s, float, %f, %f, %f, %f, 0.002\n", varName.c_str(), (float)*initialValue, (float)minimumValue, (float)maximumValue, (float)((maximumValue - minimumValue) / 20.0));
        else
            printf("%s, int, %d, %d, %d, %f, 0.002\n", varName.c_str(), (int)*initialValue, (int)minimumValue, (int)maximumValue, (float)((maximumValue - minimumValue) / 20.0));

        SPSAValue value;
        value.varName = varName;
        value.varPointer = (void*)initialValue;
        tuneValues.push_back(value);

        return 0;
    }

    template<
        typename T,
        typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type
    >
    static int tuneStatic(std::string varName, T* initialValue, T minimumValue, T maximumValue) {
        return instance().tune(varName, initialValue, minimumValue, maximumValue);
    }

    static void trySetParam(std::string varName, std::string value);

};

// Some fancy macro stuff to call the tune() methods inline from anywhere
#define STRINGIFY(x) #x
#define UNIQUE2(x, y) x##y
#define UNIQUE(x, y) UNIQUE2(x, y)
#define GET_FIRST_ARG(arg1, ...) arg1
#define TUNE(arg1, ...) int UNIQUE(p, __LINE__) = SPSA::tuneStatic(STRINGIFY(arg1), &arg1, __VA_ARGS__)
