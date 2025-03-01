#pragma once

#include <type_traits>
#include <string>
#include <vector>
#include <iostream>

template<
    typename T,
    typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type
>
struct SPSAValue {
    std::string varName;
    bool isFloat;
    T* varPointer;
    T minimumValue;
    T maximumValue;

    void printParam() {
        if (isFloat)
            printf("%s, float, %f, %f, %f, %f, 0.002\n", varName.c_str(), (float)*varPointer, (float)minimumValue, (float)maximumValue, (float)((maximumValue - minimumValue) / 20.0));
        else
            printf("%s, int, %d, %d, %d, %f, 0.002\n", varName.c_str(), (int)*varPointer, (int)minimumValue, (int)maximumValue, (float)((maximumValue - minimumValue) / 20.0));
    }

    void printUCIOption() {
        if (isFloat)
            printf("option name %s type string default %f\n", varName.c_str(), (float)*varPointer);
        else
            printf("option name %s type spin default %d min %d max %d\n", varName.c_str(), (int)*varPointer, (int)minimumValue, (int)maximumValue);
    }

};

class SPSA {

public:
    std::vector<SPSAValue<int>> intValues;
    std::vector<SPSAValue<float>> floatValues;

    static SPSA& instance() {
        static SPSA t;
        return t;
    }  // Singleton

    int tune(std::string varName, float* initialValue, float minimumValue, float maximumValue) {
        SPSAValue<float> value;
        value.varName = varName;
        value.isFloat = true;
        value.varPointer = initialValue;
        value.minimumValue = minimumValue;
        value.maximumValue = maximumValue;

        floatValues.push_back(static_cast<SPSAValue<float>>(value));
        value.printParam();

        return 0;
    }

    int tune(std::string varName, int* initialValue, int minimumValue, int maximumValue) {
        SPSAValue<int> value;
        value.varName = varName;
        value.isFloat = false;
        value.varPointer = initialValue;
        value.minimumValue = minimumValue;
        value.maximumValue = maximumValue;

        intValues.push_back(static_cast<SPSAValue<int>>(value));
        value.printParam();

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

    static void printUCI() {
        for (SPSAValue<int>& value : instance().intValues) {
            value.printUCIOption();
        }
        for (SPSAValue<float>& value : instance().floatValues) {
            value.printUCIOption();
        }
    }

};

#define TUNE_ENABLED false

// Some fancy macro stuff to call the tune() methods inline from anywhere
#define STRINGIFY(x) #x
#define UNIQUE2(x, y) x##y
#define UNIQUE(x, y) UNIQUE2(x, y)
#define GET_FIRST_ARG(arg1, ...) arg1

#if TUNE_ENABLED == true
    #define TUNE_FLOAT(arg1, arg2, ...) float arg1 = arg2; int UNIQUE(SPSA, arg1) = SPSA::tuneStatic(STRINGIFY(arg1), &arg1, __VA_ARGS__)
    #define TUNE_INT(arg1, arg2, ...) int arg1 = arg2; int UNIQUE(SPSA, arg1) = SPSA::tuneStatic(STRINGIFY(arg1), &arg1, __VA_ARGS__)

    #define TUNE_FLOAT_DISABLED(arg1, arg2, ...) constexpr float arg1 = arg2
    #define TUNE_INT_DISABLED(arg1, arg2, ...) constexpr int arg1 = arg2
#else
    #define TUNE_FLOAT(arg1, arg2, ...) constexpr float arg1 = arg2
    #define TUNE_INT(arg1, arg2, ...) constexpr int arg1 = arg2

    #define TUNE_FLOAT_DISABLED(arg1, arg2, ...) constexpr float arg1 = arg2
    #define TUNE_INT_DISABLED(arg1, arg2, ...) constexpr int arg1 = arg2
#endif