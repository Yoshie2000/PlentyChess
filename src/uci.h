#pragma once

#include <tuple>

#include "nnue.h"

constexpr auto VERSION = "7.0.36";

template<int... Is>
struct seq { };

template<int N, int... Is>
struct gen_seq : gen_seq<N - 1, N - 1, Is...> { };

template<int... Is>
struct gen_seq<0, Is...> : seq<Is...> { };

template<typename T, typename Func, int... Is>
void for_each(T&& t, Func f, seq<Is...>) {
    auto l __attribute((unused)) = { (f(std::get<Is>(t)), 0)... };
}

template<typename... Ts, typename Func>
void for_each_in_tuple(std::tuple<Ts...> const& t, Func f) {
    for_each(t, f, gen_seq<sizeof...(Ts)>());
}

namespace UCI {

    extern bool optionsDirty;

    enum UCIOptionType {
        UCI_SPIN,
        UCI_STRING,
        UCI_CHECK
    };

    template <UCIOptionType OptionType>
    struct UCIOption {};

    template <>
    struct UCIOption<UCI_SPIN> {
        std::string name;
        int defaultValue;
        int value;
        int minValue;
        int maxValue;
    };

    template <>
    struct UCIOption<UCI_STRING> {
        std::string name;
        std::string defaultValue;
        std::string value;
    };

    template <>
    struct UCIOption<UCI_CHECK> {
        std::string name;
        bool defaultValue;
        bool value;
    };

    struct UCIOptions {
        UCIOption<UCI_SPIN> hash = {
            "Hash",
#ifdef PROFILE_GENERATE
            64,
            64,
#else
            16,
            16,
#endif
            1,
            1048576
        };

        UCIOption<UCI_SPIN> threads = {
            "Threads",
            1,
            1,
            1,
            4096
        };

        UCIOption<UCI_SPIN> multiPV = {
            "MultiPV",
            1,
            1,
            1,
            218
        };

        UCIOption<UCI_CHECK> chess960 = {
            "UCI_Chess960",
            false,
            false
        };

        UCIOption<UCI_SPIN> moveOverhead = {
            "MoveOverhead",
            100,
            100,
            10,
            10000
        };

        UCIOption<UCI_CHECK> ponder = {
            "Ponder",
            false,
            false
        };

        UCIOption<UCI_CHECK> datagen = {
            "Datagen",
            false,
            false
        };

        UCIOption<UCI_CHECK> minimal = {
            "Minimal",
            false,
            false
        };

        UCIOption<UCI_STRING> syzygyPath = {
            "SyzygyPath",
            "",
            ""
        };

        UCIOption<UCI_SPIN> syzygyProbeLimit = {
            "SyzygyProbeLimit",
            7,
            7,
            0,
            7
        };

        template <typename Func>
        void forEach(Func&& f) {
            auto optionsTuple = std::make_tuple(&hash, &threads, &multiPV, &moveOverhead, &chess960, &ponder, &datagen, &minimal, &syzygyPath, &syzygyProbeLimit);
            for_each_in_tuple(optionsTuple, f);
        }
    };

    extern UCIOptions Options;
    extern NNUE nnue;

}

void uciLoop(int argc, char* argv[]);
