#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <deque>

#include "board.h"
#include "uci.h"
#include "move.h"
#include "thread.h"

bool matchesToken(std::string line, std::string token) {
    return line.rfind(token, 0) == 0;
}

void position(std::string line, Board* board, std::deque<BoardStack>* stackQueue) {
    *stackQueue = std::deque<BoardStack>(1);
    board->stack = &stackQueue->back();

    // Set up startpos or moves
    if (matchesToken(line, "startpos")) {
        startpos(board);
        line = line.substr(9);
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
            while (line[i] != ' ' && i < 5 && i < line.length()) {
                move[i] = line[i];
                i++;
            }
            Move m = stringToMove(move);

            stackQueue->emplace_back();
            doMove(board, &stackQueue->back(), m);
            // TODO do something here

            if (line.length() > i)
                line = line.substr(i + 1);
            if (line.length() >= lastStrlen) break;
        }
    }
}

void uciLoop(Thread* searchThread) {
    Board board;
    std::deque<BoardStack> stackQueue = std::deque<BoardStack>(1);
    board.stack = &stackQueue.back();
    startpos(&board);

    printf("UCI thread running\n");

    char line[UCI_LINE_LENGTH];
    while (fgets(line, sizeof(line), stdin)) {

        if (matchesToken(line, "quit") || matchesToken(line, "stop")) {
            searchThread->exit();
            break;
        }

        else if (matchesToken(line, "search")) searchThread->startSearching(&board);
        else if (matchesToken(line, "uci")) printf("id name yoshie2000-chess-engine\nuciok\n");
        else if (matchesToken(line, "go")) printf("TODO\n");
        else if (matchesToken(line, "position")) position(line + 9, &board, &stackQueue);
        else if (matchesToken(line, "ucinewgame")) printf("TODO\n");
        else if (matchesToken(line, "isready")) printf("readyok\n");

        /* NON UCI COMMANDS */
        else if (matchesToken(line, "debug")) debugBoard(&board);
        else printf("Unknown command\n");
    }

    printf("UCI thread stopping\n");
}