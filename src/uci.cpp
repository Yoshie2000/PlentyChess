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

void position(std::string line, Board* board, std::deque<BoardStack>* stackQueue) {
    *stackQueue = std::deque<BoardStack>(1);
    board->stack = &stackQueue->back();

    // Set up startpos or moves
    if (matchesToken(line, "startpos")) {
        startpos(board);
        line = line.substr(std::min(9, (int) line.length()));
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

    }

    searchThread->startSearching(*board, *stackQueue, parameters);
    if (parameters.movetime > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(parameters.movetime));
        searchThread->stopSearching();
    }
}

void uciLoop(Thread* searchThread) {
    Board board;
    std::deque<BoardStack> stackQueue = std::deque<BoardStack>(1);
    board.stack = &stackQueue.back();
    startpos(&board);

    printf("UCI thread running\n");

    for (std::string line; std::getline(std::cin, line);) {

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
        else if (matchesToken(line, "debug")) debugBoard(&board);
        else printf("Unknown command\n");
    }

    printf("UCI thread stopping\n");
}

/*
position startpos moves e2e3 d7d5 b2b3 e7e6 c1b2 g8h6 g1h3 b8c6 d1f3 e6e5 c2c4 c6b4
position startpos moves e2e3 d7d5 b2b3 e7e6 c1b2 g8h6 g1h3 b8c6 d1f3 e6e5 c2c4 c6b4 c4d5
position startpos moves e2e3 d7d5 b2b3 e7e6 c1b2 g8h6 g1h3 b8c6 d1f3 e6e5 c2c4 c6b4 c4d5 b4c2
position startpos moves e2e3 d7d5 b2b3 e7e6 c1b2 g8h6 g1h3 b8c6 d1f3 e6e5 c2c4 c6b4 c4d5 b4c2 e1d1
position startpos moves e2e3 d7d5 b2b3 e7e6 c1b2 g8h6 g1h3 b8c6 d1f3 e6e5 c2c4 c6b4 c4d5 b4c2 e1d1 c8g4
*/