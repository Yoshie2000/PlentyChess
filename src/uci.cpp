#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <deque>
#include <sstream>
#include <algorithm>
#include <tuple>

#include "board.h"
#include "uci.h"
#include "move.h"
#include "thread.h"
#include "search.h"
#include "tt.h"
#include "spsa.h"
#include "nnue.h"
#include "spsa.h"
#include "history.h"
#include "fathom/src/tbprobe.h"
#include "debug.h"
#include "bench.h"

namespace UCI {
    UCIOptions Options;
    NNUE nnue;
    bool optionsDirty = false;
}

ThreadPool threads;

bool matchesToken(std::string line, std::string token) {
    return line.rfind(token, 0) == 0;
}

bool nextToken(std::string* line, std::string* token) {
    if (line->length() == 0) return false;
    std::string::size_type tokenEnd = line->find(' ');
    if (tokenEnd != std::string::npos) {
        *token = line->substr(0, tokenEnd);
        *line = line->substr(1 + tokenEnd);
        return true;
    }
    *token = line->substr(0);
    *line = line->substr(1);
    return true;
}

void bench(Board& board, std::vector<Hash>& boardHistory) {
    boardHistory.clear();
    boardHistory.push_back(0);

    bool minimal = UCI::Options.minimal.value;
    UCI::Options.minimal.value = true;

    uint64_t nodes = 0;
    int64_t elapsed = 0;
    int position = 1;
    int totalPositions = Bench::BENCH_POSITIONS.size();

    threads.waitForSearchFinished();
    threads.ucinewgame();

    int i = 0;
    for (const std::string& fen : Bench::BENCH_POSITIONS) {
        board.parseFen(fen, i++ >= 44);
        boardHistory[0] = board.hashes.hash;
        SearchParameters parameters;

#ifdef PROFILE_GENERATE
        TT.newSearch();
        parameters.depth = 20;
#else
        parameters.depth = 13;
#endif

        std::cerr << "\nPosition: " << position++ << '/' << totalPositions << " (" << fen << ")" << std::endl;

        std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

        threads.startSearching(board, boardHistory, parameters);
        threads.waitForSearchFinished();
        nodes += threads.nodesSearched();

        std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
        elapsed += std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
    }

    std::cerr << "\n==========================="
        << "\nTotal time (ms) : " << elapsed
        << "\nNodes searched  : " << nodes
        << "\nNodes/second    : " << 1000 * nodes / elapsed << std::endl;

    UCI::Options.minimal.value = minimal;
}

void perfttest(Board& board, std::vector<Hash>& boardHistory) {
    boardHistory.clear();
    boardHistory.push_back(0);

    threads.waitForSearchFinished();
    SearchParameters parameters;
    parameters.depth = 5;
    parameters.perft = true;

    board.startpos();
    boardHistory[0] = board.hashes.hash;
    threads.startSearching(board, boardHistory, parameters);
    threads.waitForSearchFinished();
    if (threads.nodesSearched() != 4865609) std::cout << "Failed perft for startpos" << std::endl;

    board.parseFen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", false);
    boardHistory[0] = board.hashes.hash;
    threads.startSearching(board, boardHistory, parameters);
    threads.waitForSearchFinished();
    if (threads.nodesSearched() != 193690690) std::cout << "Failed perft for r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1" << std::endl;

    board.parseFen("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", false);
    boardHistory[0] = board.hashes.hash;
    parameters.depth = 6;
    threads.startSearching(board, boardHistory, parameters);
    threads.waitForSearchFinished();
    if (threads.nodesSearched() != 11030083) std::cout << "Failed perft for 8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1" << std::endl;

    board.parseFen("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", false);
    boardHistory[0] = board.hashes.hash;
    parameters.depth = 5;
    threads.startSearching(board, boardHistory, parameters);
    threads.waitForSearchFinished();
    if (threads.nodesSearched() != 15833292) std::cout << "Failed perft for r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1" << std::endl;

    board.parseFen("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", false);
    boardHistory[0] = board.hashes.hash;
    threads.startSearching(board, boardHistory, parameters);
    threads.waitForSearchFinished();
    if (threads.nodesSearched() != 89941194) std::cout << "Failed perft for rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8" << std::endl;

    board.parseFen("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", false);
    boardHistory[0] = board.hashes.hash;
    threads.startSearching(board, boardHistory, parameters);
    threads.waitForSearchFinished();
    if (threads.nodesSearched() != 164075551) std::cout << "Failed perft for r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10" << std::endl;

    board.parseFen("1rqbkrbn/1ppppp1p/1n6/p1N3p1/8/2P4P/PP1PPPP1/1RQBKRBN w FBfb - 0 9", true);
    boardHistory[0] = board.hashes.hash;
    threads.startSearching(board, boardHistory, parameters);
    threads.waitForSearchFinished();
    if (threads.nodesSearched() != 8652810) std::cout << "Failed perft for 1rqbkrbn/1ppppp1p/1n6/p1N3p1/8/2P4P/PP1PPPP1/1RQBKRBN w FBfb - 0 9" << std::endl;

    board.parseFen("rbbqn1kr/pp2p1pp/6n1/2pp1p2/2P4P/P7/BP1PPPP1/R1BQNNKR w HAha - 0 9", true);
    boardHistory[0] = board.hashes.hash;
    threads.startSearching(board, boardHistory, parameters);
    threads.waitForSearchFinished();
    if (threads.nodesSearched() != 26302461) std::cout << "Failed perft for rbbqn1kr/pp2p1pp/6n1/2pp1p2/2P4P/P7/BP1PPPP1/R1BQNNKR w HAha - 0 9" << std::endl;

    board.parseFen("rqbbknr1/1ppp2pp/p5n1/4pp2/P7/1PP5/1Q1PPPPP/R1BBKNRN w GAga - 0 9", true);
    boardHistory[0] = board.hashes.hash;
    threads.startSearching(board, boardHistory, parameters);
    threads.waitForSearchFinished();
    if (threads.nodesSearched() != 11029596) std::cout << "Failed perft for rqbbknr1/1ppp2pp/p5n1/4pp2/P7/1PP5/1Q1PPPPP/R1BBKNRN w GAga - 0 9" << std::endl;

    std::cout << "Perfttest done" << std::endl;
}

std::vector<std::string> splitString(const std::string& input, char delimiter) {
    std::vector<std::string> tokens;
    std::istringstream tokenStream(input);
    std::string token;

    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }

    return tokens;
}

void position(std::string line, Board& board, std::vector<Hash>& boardHistory) {
    std::istringstream iss(line);
    std::string token;
    iss >> token;

    // Set up startpos or moves
    if (token == "startpos") {
        board.startpos();
    }
    else if (token == "fen") {
        board.parseFen(iss, UCI::Options.chess960.value);
    }
    else {
        std::cout << "Not a valid position, exiting" << std::endl;
        exit(-1);
    }

    boardHistory.clear();
    boardHistory.reserve(MAX_PLY);
    boardHistory.push_back(board.hashes.hash);

    // Make further moves
    iss >> token;
    if (token != "moves")
        return;

    UCI::nnue.reset(&board);
    int moveCount = 0;

    while (iss >> token) {
        Move m = stringToMove(token.c_str(), &board);
        
        assert(board.isLegal(m));

        Board boardCopy = board;
        boardCopy.doMove(m, board.hashAfter(m), &UCI::nnue);
        boardHistory.push_back(boardCopy.hashes.hash);

        if (moveCount++ > 200) {
            UCI::nnue.reset(&boardCopy);
        }

        board = boardCopy;
    }
}

struct setUciOption
{
    std::string name;
    std::string value;

    setUciOption(std::string n, std::string v) {
        name = n;
        value = v;
    }

    template<UCI::UCIOptionType OptionType>
    void operator () (UCI::UCIOption<OptionType>* option) {
        if (option->name != name) return;
        if constexpr (OptionType == UCI::UCI_SPIN) {
            option->value = std::stoi(value);
        }
        else if constexpr (OptionType == UCI::UCI_STRING) {
            option->value = value;
        }
        else if constexpr (OptionType == UCI::UCI_CHECK) {
            option->value = value == "true";
        }
    }
};

void setoption(std::string line) {
    std::string name, value;

    if (matchesToken(line, "name")) {
        line = line.substr(5);
        name = line.substr(0, line.find(' '));
        line = line.substr(std::min(line.size(), name.length() + 1));
    }

    if (matchesToken(line, "value")) {
        line = line.substr(6);
        value = line.find(' ') != std::string::npos ? line.substr(0, line.find(' ')) : line;
    }

    if (name == "Hash" || name == "Threads") {
        UCI::optionsDirty = true;
    }

    UCI::Options.forEach(setUciOption(name, value));
    SPSA::trySetParam(name, value);

    if (name == "SyzygyPath") {
        std::string path = UCI::Options.syzygyPath.value;
        tb_init(path.c_str());
        if (!TB_LARGEST)
            std::cout << "info string Tablebases failed to load" << std::endl;
    }
}

void go(std::string line, Board& board, std::vector<Hash>& boardHistory) {
    SearchParameters parameters;
    if (line.size() == 2) {
        line = "";
        parameters.infinite = true;
    }
    else {
        line = line.substr(3);
    }

    std::string token;
    while (nextToken(&line, &token)) {

        if (matchesToken(token, "perft")) {
            parameters.perft = true;
            nextToken(&line, &token);
            parameters.depth = std::stoi(token);
        }

        if (matchesToken(token, "depth")) {
            nextToken(&line, &token);
            parameters.depth = std::stoi(token);
        }

        if (matchesToken(token, "nodes")) {
            nextToken(&line, &token);
            parameters.nodes = std::stoi(token);
        }

        if (matchesToken(token, "infinite")) {
            parameters.infinite = true;
        }

        if (matchesToken(token, "ponder")) {
            parameters.ponder = true;
        }

        if (matchesToken(token, "movetime")) {
            nextToken(&line, &token);
            parameters.movetime = std::stoi(token);
        }
        if (matchesToken(token, "wtime")) {
            nextToken(&line, &token);
            parameters.wtime = std::stoi(token);
        }
        if (matchesToken(token, "btime")) {
            nextToken(&line, &token);
            parameters.btime = std::stoi(token);
        }
        if (matchesToken(token, "winc")) {
            nextToken(&line, &token);
            parameters.winc = std::stoi(token);
        }
        if (matchesToken(token, "binc")) {
            nextToken(&line, &token);
            parameters.binc = std::stoi(token);
        }

        if (matchesToken(token, "movestogo")) {
            nextToken(&line, &token);
            parameters.movestogo = std::stoi(token);
        }

    }

    TT.newSearch();
    threads.startSearching(board, boardHistory, parameters);
}

void speedtest(Board& board, std::vector<Hash>& boardHistory) {
    int numThreads = std::thread::hardware_concurrency();
    int numHash = 16 * numThreads;
    int numSeconds = 150;

    UCI::Options.minimal.value = true;
    UCI::Options.moveOverhead.value = 0;
    UCI::Options.hash.value = numHash;
    UCI::Options.threads.value = numThreads;
    TT.resize(numHash);
    threads.resize(numThreads);

    // Scale time per move based on fishtest LTC model
    auto getCorrectedTime = [&](int ply) {
        return 50000.0 / (static_cast<double>(ply) + 15.0);
    };
    float totalTime = 0;
    int totalPositions = 0;
    for (auto& game : Bench::SPEEDTEST_POSITIONS) {
        for (size_t i = 0; i < game.size(); i++) {
            totalTime += getCorrectedTime(i + 1);
            totalPositions++;
        }
    }
    float timeScaleFactor = static_cast<float>(numSeconds) * 1000 / totalTime;

    // Play the games using the computed time scale
    uint64_t nodes = 0;
    int64_t time = 0;
    int64_t hashfull = 0;
    int maxHashfull = 0;
    SearchParameters parameters;
    for (auto& game : Bench::SPEEDTEST_POSITIONS) {
        TT.clear();
        threads.ucinewgame();

        for (size_t i = 0; i < game.size(); i++) {
            board.parseFen(game[i], false);
            TT.newSearch();

            parameters.movetime = getCorrectedTime(i + 1) * timeScaleFactor;
            
            std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

            threads.startSearching(board, boardHistory, parameters);
            threads.waitForSearchFinished();
            nodes += threads.nodesSearched();
            hashfull += TT.hashfull();
            maxHashfull = std::max(maxHashfull, TT.hashfull());

            std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
            time += std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
        }
    }

    // Compute total nps
    std::cout << std::endl << "--- Speedtest finished ---" << std::endl;
    std::cout << "Threads: " << numThreads << std::endl;
    std::cout << "Hash: " << numHash << " MB" << std::endl;
    std::cout << "Average hashfull: " << (hashfull / totalPositions) << std::endl;
    std::cout << "Max hashfull: " << maxHashfull << std::endl;
    std::cout << "Nodes: " << nodes << std::endl;
    std::cout << "Time: " << time << std::endl;
    std::cout << "NPS: " << (1000ULL * nodes / time) << std::endl;
}

void genfens(std::string params, Board& board, std::vector<Hash>& boardHistory) {
    std::string token;
    SearchParameters parameters;
    parameters.genfens = true;

    while (nextToken(&params, &token)) {
        if (matchesToken(token, "genfens")) {
            nextToken(&params, &token);
            parameters.genfensFens = std::stoi(token);
        }
        if (matchesToken(token, "seed")) {
            nextToken(&params, &token);
            parameters.genfensSeed = std::stoi(token);
        }
    }

    TT.newSearch();
    threads.startSearching(board, boardHistory, parameters);
    threads.waitForSearchFinished();
}

struct printOptions
{
    template<UCI::UCIOptionType OptionType>
    void operator () (UCI::UCIOption<OptionType>* option) {
        if constexpr (OptionType == UCI::UCI_SPIN) {
            std::cout << "option name " << option->name << " type spin default " << option->defaultValue << " min " << option->minValue << " max " << option->maxValue << std::endl;
        }
        else if constexpr (OptionType == UCI::UCI_STRING) {
            std::cout << "option name " << option->name << " type string default " << option->defaultValue << std::endl;
        }
        else if constexpr (OptionType == UCI::UCI_CHECK) {
            std::cout << "option name " << option->name << " type check default " << (option->defaultValue ? "true" : "false") << std::endl;
        }
    }
};

void uciLoop(int argc, char* argv[]) {
    threads.resize(1);
    std::vector<Hash> boardHistory;
    Board board;
    board.startpos();
    boardHistory.push_back(board.hashes.hash);

    std::cout << "UCI thread running" << std::endl;

#if defined(PROCESS_NET)
    bench(board, boardHistory);
    nnz.permuteNetwork();
    return;
#endif

    if (argc > 1 && matchesToken(argv[1], "genfens")) {
        std::cout << "starting fen generation" << std::endl;
        std::string params(argv[1]);
        genfens(params, board, boardHistory);

        if (argc > 2 && matchesToken(argv[2], "quit"))
            return;
    }
    if (argc > 1 && matchesToken(argv[1], "bench")) {
        bench(board, boardHistory);
        Debug::show();
        return;
    }
    if (argc > 1 && matchesToken(argv[1], "speedtest")) {
        speedtest(board, boardHistory);
        return;
    }
    for (std::string line = {};std::getline(std::cin, line);) {

        if (matchesToken(line, "quit")) {
            threads.stopSearching();
            threads.exit();
            break;
        }
        else if (matchesToken(line, "stop")) {
            threads.searchParameters.ponderhit = false;
            threads.stopSearching();
        }
        else if (matchesToken(line, "ponderhit")) {
            threads.searchParameters.ponderhit = true;
            threads.stopSearching();
        }
        else if (matchesToken(line, "wait")) {
            threads.waitForSearchFinished();
        }

        else if (matchesToken(line, "isready")) {
            threads.waitForSearchFinished();
            std::cout << "readyok" << std::endl;
        }
        else if (matchesToken(line, "ucinewgame")) {
            threads.resize(UCI::Options.threads.value);
            TT.resize(UCI::Options.hash.value);
            threads.ucinewgame();
            UCI::optionsDirty = false;
        }
        else if (matchesToken(line, "uci")) {
            std::cout << "id name PlentyChess " << static_cast<std::string>(VERSION) << "\nid author Yoshie2000\n" << std::endl;
            UCI::Options.forEach(printOptions());
            SPSA::printUCI();
            std::cout << std::endl << "uciok" << std::endl;
        }
        else if (matchesToken(line, "go")) {
            if (UCI::optionsDirty) {
                threads.resize(UCI::Options.threads.value);
                TT.resize(UCI::Options.hash.value);
                UCI::optionsDirty = false;
            }
            go(line, board, boardHistory);
        }
        else if (matchesToken(line, "position")) position(line.substr(9), board, boardHistory);
        else if (matchesToken(line, "setoption")) setoption(line.substr(10));

        /* NON UCI COMMANDS */
        else if (matchesToken(line, "bench")) bench(board, boardHistory);
        else if (matchesToken(line, "perfttest")) perfttest(board, boardHistory);
        else if (matchesToken(line, "debug")) board.debugBoard();
        else if (matchesToken(line, "eval")) {
            UCI::nnue.reset(&board);
            std::cout << UCI::nnue.evaluate(&board) << std::endl;
        }
        else std::cout << "Unknown command" << std::endl;
    }

    std::cout << "UCI thread stopping" << std::endl;
}
