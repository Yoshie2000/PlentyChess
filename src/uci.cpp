#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <deque>
#include <algorithm>

#include "board.h"
#include "uci.h"
#include "move.h"
#include "thread.h"
#include "search.h"

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
    //   "setoption name UCI_Chess960 value true",
    //   "bbqnnrkr/pppppppp/8/8/8/8/PPPPPPPP/BBQNNRKR w HFhf - 0 1 moves g2g3 d7d5 d2d4 c8h3 c1g5 e8d6 g5e7 f7f6",
    //   "nqbnrkrb/pppppppp/8/8/8/8/PPPPPPPP/NQBNRKRB w KQkq - 0 1",
    //   "setoption name UCI_Chess960 value false"
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

void bench(Thread* searchThread, std::deque<BoardStack>* stackQueue, Board* board) {
    uint64_t nodes = 0;
    int position = 1;
    int totalPositions = benchPositions.size();

    searchThread->waitForSearchFinished();

    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    for (const std::string fen : benchPositions) {
        parseFen(board, fen);
        SearchParameters parameters;
        parameters.depth = 5;

        std::cerr << "\nPosition: " << position++ << '/' << totalPositions << " (" << fen << ")" << std::endl;

        searchThread->startSearching(*board, *stackQueue, parameters);
        searchThread->waitForSearchFinished();
        nodes += searchThread->nodesSearched;
    }
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();

    int64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
    std::cerr << "\n==========================="
        << "\nTotal time (ms) : " << elapsed
        << "\nNodes searched  : " << nodes
        << "\nNodes/second    : " << 1000 * nodes / elapsed << std::endl;
}

void perfttest(Thread* searchThread, std::deque<BoardStack>* stackQueue, Board* board) {

    searchThread->waitForSearchFinished();
    SearchParameters parameters;
    parameters.depth = 5;
    parameters.perft = true;

    startpos(board);
    searchThread->startSearching(*board, *stackQueue, parameters);
    searchThread->waitForSearchFinished();
    if (searchThread->nodesSearched != 4865609) std::cout << "Failed perft for startpos" << std::endl;

    parseFen(board, "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
    searchThread->startSearching(*board, *stackQueue, parameters);
    searchThread->waitForSearchFinished();
    if (searchThread->nodesSearched != 193690690) std::cout << "Failed perft for r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1" << std::endl;

    parseFen(board, "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1");
    parameters.depth = 6;
    searchThread->startSearching(*board, *stackQueue, parameters);
    searchThread->waitForSearchFinished();
    if (searchThread->nodesSearched != 11030083) std::cout << "Failed perft for 8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1" << std::endl;

    parseFen(board, "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1");
    parameters.depth = 5;
    searchThread->startSearching(*board, *stackQueue, parameters);
    searchThread->waitForSearchFinished();
    if (searchThread->nodesSearched != 15833292) std::cout << "Failed perft for r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1" << std::endl;

    parseFen(board, "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8");
    searchThread->startSearching(*board, *stackQueue, parameters);
    searchThread->waitForSearchFinished();
    if (searchThread->nodesSearched != 89941194) std::cout << "Failed perft for rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8" << std::endl;

    parseFen(board, "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10");
    searchThread->startSearching(*board, *stackQueue, parameters);
    searchThread->waitForSearchFinished();
    if (searchThread->nodesSearched != 164075551) std::cout << "Failed perft for r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10" << std::endl;

    std::cout << "Perfttest done" << std::endl;
}

void position(std::string line, Board* board, std::deque<BoardStack>* stackQueue) {
    *stackQueue = std::deque<BoardStack>(1);
    board->stack = &stackQueue->back();

    // Set up startpos or moves
    if (matchesToken(line, "startpos")) {
        startpos(board);
        line = line.substr(std::min(9, (int)line.length()));
    }
    else if (matchesToken(line, "fen")) {
        line = line.substr(4);
        size_t fenLength = parseFen(board, line) + 1;
        if (line.length() > fenLength)
            line = line.substr(fenLength);
    }
    else {
        printf("Not a valid position, exiting\n");
        exit(-1);
    }

    // Make further moves
    if (matchesToken(line, "moves")) {
        line = line.substr(6);

        char move[5];
        size_t lastStrlen = line.length();
        while (line.length() >= 4) {
            lastStrlen = line.length();

            size_t i = 0;
            move[4] = ' ';
            while (line[i] != ' ' && i < 5 && i < line.length()) {
                move[i] = line[i];
                i++;
            }
            Move m = stringToMove(move, board);

            stackQueue->emplace_back();
            doMove(board, &stackQueue->back(), m);

            if (line.length() > i)
                line = line.substr(i + 1);
            if (line.length() >= lastStrlen) break;
        }
    }
}

void go(std::string line, Thread* searchThread, Board* board, std::deque<BoardStack>* stackQueue) {
    SearchParameters parameters;

    std::string token;
    while (nextToken(&line, &token)) {
        // std::cout << token << std::endl;

        if (matchesToken(token, "perft")) {
            parameters.perft = true;
            nextToken(&line, &token);
            parameters.depth = std::stoi(token);
        }

        if (matchesToken(token, "depth")) {
            nextToken(&line, &token);
            parameters.depth = std::stoi(token);
        }

        if (matchesToken(token, "infinite")) {
            parameters.infinite = true;
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

    }

    searchThread->startSearching(*board, *stackQueue, parameters);
    if (parameters.movetime > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(parameters.movetime));
        searchThread->stopSearching();
    }
    if (parameters.wtime > 0 && parameters.btime > 0) {
        uint64_t time = board->stm == COLOR_WHITE ? parameters.wtime : parameters.btime;
        uint64_t inc = board->stm == COLOR_WHITE ? parameters.winc : parameters.binc;

        uint64_t totalTime = time / 20 + inc / 2;

        std::this_thread::sleep_for(std::chrono::milliseconds(totalTime));
        searchThread->stopSearching();
    }
}

void uciLoop(Thread* searchThread, int argc, char* argv[]) {
    Board board;
    std::deque<BoardStack> stackQueue = std::deque<BoardStack>(1);
    board.stack = &stackQueue.back();
    startpos(&board);

    printf("UCI thread running\n");

    if (argc > 1 && matchesToken(argv[1], "bench")) {
        bench(searchThread, &stackQueue, &board);
        return;
    }
    for (std::string line;std::getline(std::cin, line);) {

        if (matchesToken(line, "quit")) {
            searchThread->exit();
            break;
        }
        else if (matchesToken(line, "stop")) searchThread->stopSearching();

        else if (matchesToken(line, "isready")) printf("readyok\n");
        else if (matchesToken(line, "uci")) printf("id name yoshie2000-chess-engine\nuciok\n");
        else if (matchesToken(line, "ucinewgame")) printf("TODO\n");
        else if (matchesToken(line, "go")) go(line.substr(3), searchThread, &board, &stackQueue);
        else if (matchesToken(line, "position")) position(line.substr(9), &board, &stackQueue);

        /* NON UCI COMMANDS */
        else if (matchesToken(line, "bench")) bench(searchThread, &stackQueue, &board);
        else if (matchesToken(line, "perfttest")) perfttest(searchThread, &stackQueue, &board);
        else if (matchesToken(line, "debug")) debugBoard(&board);
        else if (matchesToken(line, "eval")) debugEval(&board);
        else printf("Unknown command\n");
    }

    printf("UCI thread stopping\n");
}
