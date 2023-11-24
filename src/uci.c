#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "board.h"
#include "engine.h"
#include "uci.h"
#include "move.h"

void notifySearchThread() {
    if (pthread_cond_signal(&searchCond) != 0) {
        perror("pthread_cond_signal() error");
        exit(4);
    }
}

bool matchesToken(char* line, char* token) {
    return strncmp(line, token, strlen(token)) == 0;
}

void position(char* line) {
    struct Board board;
    struct BoardStack stack;
    board.stack = &stack;

    // Set up startpos or moves
    if (matchesToken(line, "startpos")) {
        startpos(&board);
        line += 9;
    }
    else if (matchesToken(line, "fen")) {
        line += 4;
        line += parseFen(&board, line) + 1;
    }
    else {
        printf("Not a valid position, exiting\n");
        exit(-1);
    }

    // Make further moves
    if (matchesToken(line, "moves")) {
        line += 6;

        char move[5];
        size_t lastStrlen = strlen(line);
        while (strlen(line) >= 4) {
            lastStrlen = strlen(line);

            size_t i = 0;
            while (line[i] != ' ' && i < 5 && i < strlen(line)) {
                move[i] = line[i];
                i++;
            }
            Move m = stringToMove(move);
            // TODO do something here

            line += i + 1;
            if (strlen(line) >= lastStrlen) break;
        }
    }

    debugBoard(&board);
}

void uciLoop() {
    printf("UCI thread running\n");

    char line[UCI_LINE_LENGTH];
    while (fgets(line, sizeof(line), stdin)) {

        if (matchesToken(line, "quit") || matchesToken(line, "stop")) {
            searchStopped = true;
            notifySearchThread();
            break;
        }

        else if (matchesToken(line, "search")) notifySearchThread();
        else if (matchesToken(line, "uci")) printf("id name yoshie2000-chess-engine\nuciok\n");
        else if (matchesToken(line, "go")) printf("TODO\n");
        else if (matchesToken(line, "position")) position(line + 9);
        else if (matchesToken(line, "ucinewgame")) printf("TODO\n");
        else if (matchesToken(line, "isready")) printf("readyok\n");
        else printf("Unknown command\n");
    }

    printf("UCI thread stopping\n");
}