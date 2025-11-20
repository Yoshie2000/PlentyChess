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

namespace UCI {
    UCIOptions Options;
    NNUE nnue;
    bool optionsDirty = false;
}

ThreadPool threads;

const std::vector<std::string> benchPositions = {
    //   "setoption name UCI_Chess960 value false",
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
      "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 10",
      "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 11",
      "4rrk1/pp1n3p/3q2pQ/2p1pb2/2PP4/2P3N1/P2B2PP/4RRK1 b - - 7 19",
      "rq3rk1/ppp2ppp/1bnpb3/3N2B1/3NP3/7P/PPPQ1PP1/2KR3R w - - 7 14 moves d4e6",
      "r1bq1r1k/1pp1n1pp/1p1p4/4p2Q/4Pp2/1BNP4/PPP2PPP/3R1RK1 w - - 2 14 moves g2g4",
      "r3r1k1/2p2ppp/p1p1bn2/8/1q2P3/2NPQN2/PPP3PP/R4RK1 b - - 2 15",
      "r1bbk1nr/pp3p1p/2n5/1N4p1/2Np1B2/8/PPP2PPP/2KR1B1R w kq - 0 13",
      "r1bq1rk1/ppp1nppp/4n3/3p3Q/3P4/1BP1B3/PP1N2PP/R4RK1 w - - 1 16",
      "4r1k1/r1q2ppp/ppp2n2/4P3/5Rb1/1N1BQ3/PPP3PP/R5K1 w - - 1 17",
      "2rqkb1r/ppp2p2/2npb1p1/1N1Nn2p/2P1PP2/8/PP2B1PP/R1BQK2R b KQ - 0 11",
      "r1bq1r1k/b1p1npp1/p2p3p/1p6/3PP3/1B2NN2/PP3PPP/R2Q1RK1 w - - 1 16",
      "3r1rk1/p5pp/bpp1pp2/8/q1PP1P2/b3P3/P2NQRPP/1R2B1K1 b - - 6 22",
      "r1q2rk1/2p1bppp/2Pp4/p6b/Q1PNp3/4B3/PP1R1PPP/2K4R w - - 2 18",
      "4k2r/1pb2ppp/1p2p3/1R1p4/3P4/2r1PN2/P4PPP/1R4K1 b - - 3 22",
      "3q2k1/pb3p1p/4pbp1/2r5/PpN2N2/1P2P2P/5PP1/Q2R2K1 b - - 4 26",
      "6k1/6p1/6Pp/ppp5/3pn2P/1P3K2/1PP2P2/3N4 b - - 0 1",
      "3b4/5kp1/1p1p1p1p/pP1PpP1P/P1P1P3/3KN3/8/8 w - - 0 1",
      "2K5/p7/7P/5pR1/8/5k2/r7/8 w - - 0 1 moves g5g6 f3e3 g6g5 e3f3",
      "8/6pk/1p6/8/PP3p1p/5P2/4KP1q/3Q4 w - - 0 1",
      "7k/3p2pp/4q3/8/4Q3/5Kp1/P6b/8 w - - 0 1",
      "8/2p5/8/2kPKp1p/2p4P/2P5/3P4/8 w - - 0 1",
      "8/1p3pp1/7p/5P1P/2k3P1/8/2K2P2/8 w - - 0 1",
      "8/pp2r1k1/2p1p3/3pP2p/1P1P1P1P/P5KR/8/8 w - - 0 1",
      "8/3p4/p1bk3p/Pp6/1Kp1PpPp/2P2P1P/2P5/5B2 b - - 0 1",
      "5k2/7R/4P2p/5K2/p1r2P1p/8/8/8 b - - 0 1",
      "6k1/6p1/P6p/r1N5/5p2/7P/1b3PP1/4R1K1 w - - 0 1",
      "1r3k2/4q3/2Pp3b/3Bp3/2Q2p2/1p1P2P1/1P2KP2/3N4 w - - 0 1",
      "6k1/4pp1p/3p2p1/P1pPb3/R7/1r2P1PP/3B1P2/6K1 w - - 0 1",
      "8/3p3B/5p2/5P2/p7/PP5b/k7/6K1 w - - 0 1",
      "5rk1/q6p/2p3bR/1pPp1rP1/1P1Pp3/P3B1Q1/1K3P2/R7 w - - 93 90",
      "4rrk1/1p1nq3/p7/2p1P1pp/3P2bp/3Q1Bn1/PPPB4/1K2R1NR w - - 40 21",
      "r3k2r/3nnpbp/q2pp1p1/p7/Pp1PPPP1/4BNN1/1P5P/R2Q1RK1 w kq - 0 16",
      "3Qb1k1/1r2ppb1/pN1n2q1/Pp1Pp1Pr/4P2p/4BP2/4B1R1/1R5K b - - 11 40",
      "4k3/3q1r2/1N2r1b1/3ppN2/2nPP3/1B1R2n1/2R1Q3/3K4 w - - 5 1",

      // 5-man positions
      "8/8/8/8/5kp1/P7/8/1K1N4 w - - 0 1",     // Kc2 - mate
      "8/8/8/5N2/8/p7/8/2NK3k w - - 0 1",      // Na2 - mate
      "8/3k4/8/8/8/4B3/4KB2/2B5 w - - 0 1",    // draw

      // 6-man positions
      "8/8/1P6/5pr1/8/4R3/7k/2K5 w - - 0 1",   // Re5 - mate
      "8/2p4P/8/kr6/6R1/8/8/1K6 w - - 0 1",    // Ka2 - mate
      "8/8/3P3k/8/1p6/8/1P6/1K3n2 b - - 0 1",  // Nd2 - draw

      // 7-man positions
      "8/R7/2q5/8/6k1/8/1P5p/K6R w - - 0 124", // Draw

      // Mate and stalemate positions
      "6k1/3b3r/1p1p4/p1n2p2/1PPNpP1q/P3Q1p1/1R1RB1P1/5K2 b - - 0 1",
      "r2r1n2/pp2bk2/2p1p2p/3q4/3PN1QP/2P3R1/P4PP1/5RK1 w - - 0 1",

      // Chess 960
      "bb1n1rkr/ppp1Q1pp/3n1p2/3p4/3P4/6Pq/PPP1PP1P/BB1NNRKR w HFhf - 0 5",
      "nqbnrkrb/pppppppp/8/8/8/8/PPPPPPPP/NQBNRKRB w EGeg - 0 1",
};

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

void bench(Board& board, std::vector<uint64_t>& boardHistory) {
    boardHistory.clear();
    boardHistory.push_back(0);

    bool minimal = UCI::Options.minimal.value;
    UCI::Options.minimal.value = true;

    uint64_t nodes = 0;
    int64_t elapsed = 0;
    int position = 1;
    int totalPositions = benchPositions.size();

    threads.waitForSearchFinished();
    threads.resize(std::thread::hardware_concurrency());

    int i = 0;
    for (const std::string& fen : benchPositions) {
        board.parseFen(fen, i++ >= 44);
        boardHistory[0] = board.hashes.hash;
        SearchParameters parameters;

#ifdef PROFILE_GENERATE
        TT.newSearch();
        parameters.depth = 20;
#else
        parameters.depth = 25;
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

void perfttest(Board& board, std::vector<uint64_t>& boardHistory) {
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

void position(std::string line, Board& board, std::vector<uint64_t>& boardHistory) {
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

void go(std::string line, Board& board, std::vector<uint64_t>& boardHistory) {
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

void genfens(std::string params, Board& board, std::vector<uint64_t>& boardHistory) {
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
    std::vector<uint64_t> boardHistory;
    Board board;
    board.startpos();

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

        else if (matchesToken(line, "isready")) std::cout << "readyok" << std::endl;
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
