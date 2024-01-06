#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <deque>
#include <sstream>
#include <algorithm>

#include "board.h"
#include "uci.h"
#include "move.h"
#include "thread.h"
#include "search.h"
#include "tt.h"

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

const std::vector<std::string> seeTest = {
    "6k1/1pp4p/p1pb4/6q1/3P1pRr/2P4P/PP1Br1P1/5RKN w - - | f1f4 | -100 | P - R + B",
    "5rk1/1pp2q1p/p1pb4/8/3P1NP1/2P5/1P1BQ1P1/5RK1 b - - | d6f4 | 0 | -N + B",
    "4R3/2r3p1/5bk1/1p1r3p/p2PR1P1/P1BK1P2/1P6/8 b - - | h5g4 | 0",
    "4R3/2r3p1/5bk1/1p1r1p1p/p2PR1P1/P1BK1P2/1P6/8 b - - | h5g4 | 0",
    "4r1k1/5pp1/nbp4p/1p2p2q/1P2P1b1/1BP2N1P/1B2QPPK/3R4 b - - | g4f3 | 0",
    "2r1r1k1/pp1bppbp/3p1np1/q3P3/2P2P2/1P2B3/P1N1B1PP/2RQ1RK1 b - - | d6e5 | 100 | P",
    "7r/5qpk/p1Qp1b1p/3r3n/BB3p2/5p2/P1P2P2/4RK1R w - - | e1e8 | 0",
    "6rr/6pk/p1Qp1b1p/2n5/1B3p2/5p2/P1P2P2/4RK1R w - - | e1e8 | -500 | -R",
    "7r/5qpk/2Qp1b1p/1N1r3n/BB3p2/5p2/P1P2P2/4RK1R w - - | e1e8 | -500 | -R",
    "6RR/4bP2/8/8/5r2/3K4/5p2/4k3 w - - | f7f8q | 200 | B - P",
    "6RR/4bP2/8/8/5r2/3K4/5p2/4k3 w - - | f7f8n | 200 | N - P",
    "7R/5P2/8/8/6r1/3K4/5p2/4k3 w - - | f7f8q | 800 | Q - P",
    "7R/5P2/8/8/6r1/3K4/5p2/4k3 w - - | f7f8b | 200 | B - P",
    "7R/4bP2/8/8/1q6/3K4/5p2/4k3 w - - | f7f8r | -100 | -P",
    "8/4kp2/2npp3/1Nn5/1p2PQP1/7q/1PP1B3/4KR1r b - - | h1f1 | 0",
    "8/4kp2/2npp3/1Nn5/1p2P1P1/7q/1PP1B3/4KR1r b - - | h1f1 | 0",
    "2r2r1k/6bp/p7/2q2p1Q/3PpP2/1B6/P5PP/2RR3K b - - | c5c1 | 100 | R - Q + R",
    "r2qk1nr/pp2ppbp/2b3p1/2p1p3/8/2N2N2/PPPP1PPP/R1BQR1K1 w kq - | f3e5 | 100 | P",
    "6r1/4kq2/b2p1p2/p1pPb3/p1P2B1Q/2P4P/2B1R1P1/6K1 w - - | f4e5 | 0",
    "3q2nk/pb1r1p2/np6/3P2Pp/2p1P3/2R4B/PQ3P1P/3R2K1 w - h6 | g5h6 | 0",
    "3q2nk/pb1r1p2/np6/3P2Pp/2p1P3/2R1B2B/PQ3P1P/3R2K1 w - h6 | g5h6 | 100 | P",
    "2r4r/1P4pk/p2p1b1p/7n/BB3p2/2R2p2/P1P2P2/4RK2 w - - | c3c8 | 500 | R",
    "2r5/1P4pk/p2p1b1p/5b1n/BB3p2/2R2p2/P1P2P2/4RK2 w - - | c3c8 | 500 | R",
    "2r4k/2r4p/p7/2b2p1b/4pP2/1BR5/P1R3PP/2Q4K w - - | c3c5 | 300 | B",
    "8/pp6/2pkp3/4bp2/2R3b1/2P5/PP4B1/1K6 w - - | g2c6 | -200 | P - B",
    "4q3/1p1pr1k1/1B2rp2/6p1/p3PP2/P3R1P1/1P2R1K1/4Q3 b - - | e6e4 | -400 | P - R",
    "4q3/1p1pr1kb/1B2rp2/6p1/p3PP2/P3R1P1/1P2R1K1/4Q3 b - - | h7e4 | 100 | P",
    "3r3k/3r4/2n1n3/8/3p4/2PR4/1B1Q4/3R3K w - - | d3d4 | -100 | P - R + N - P + N - B + R - Q + R",
    "1k1r4/1ppn3p/p4b2/4n3/8/P2N2P1/1PP1R1BP/2K1Q3 w - - | d3e5 | 100 | N - N + B - R + N",
    "1k1r3q/1ppn3p/p4b2/4p3/8/P2N2P1/1PP1R1BP/2K1Q3 w - - | d3e5 | -200 | P - N",
    "rnb2b1r/ppp2kpp/5n2/4P3/q2P3B/5R2/PPP2PPP/RN1QKB2 w Q - | h4f6 | 100 | N - B + P",
    "r2q1rk1/2p1bppp/p2p1n2/1p2P3/4P1b1/1nP1BN2/PP3PPP/RN1QR1K1 b - - | g4f3 | 0 | N - B",
    "r1bqkb1r/2pp1ppp/p1n5/1p2p3/3Pn3/1B3N2/PPP2PPP/RNBQ1RK1 b kq - | c6d4 | 0 | P - N + N - P",
    "r1bq1r2/pp1ppkbp/4N1p1/n3P1B1/8/2N5/PPP2PPP/R2QK2R w KQ - | e6g7 | 0 | B - N",
    "r1bq1r2/pp1ppkbp/4N1pB/n3P3/8/2N5/PPP2PPP/R2QK2R w KQ - | e6g7 | 300 | B",
    "rnq1k2r/1b3ppp/p2bpn2/1p1p4/3N4/1BN1P3/PPP2PPP/R1BQR1K1 b kq - | d6h2 | -200 | P - B",
    "rn2k2r/1bq2ppp/p2bpn2/1p1p4/3N4/1BN1P3/PPP2PPP/R1BQR1K1 b kq - | d6h2 | 100 | P",
    "r2qkbn1/ppp1pp1p/3p1rp1/3Pn3/4P1b1/2N2N2/PPP2PPP/R1BQKB1R b KQq - | g4f3 | 100 | N - B + P",
    "rnbq1rk1/pppp1ppp/4pn2/8/1bPP4/P1N5/1PQ1PPPP/R1B1KBNR b KQ - | b4c3 | 0 | N - B",
    "r4rk1/3nppbp/bq1p1np1/2pP4/8/2N2NPP/PP2PPB1/R1BQR1K1 b - - | b6b2 | -800 | P - Q",
    "r4rk1/1q1nppbp/b2p1np1/2pP4/8/2N2NPP/PP2PPB1/R1BQR1K1 b - - | f6d5 | -200 | P - N",
    "1r3r2/5p2/4p2p/2k1n1P1/2PN1nP1/1P3P2/8/2KR1B1R b - - | b8b3 | -400 | P - R",
    "1r3r2/5p2/4p2p/4n1P1/kPPN1nP1/5P2/8/2KR1B1R b - - | b8b4 | 100 | P",
    "2r2rk1/5pp1/pp5p/q2p4/P3n3/1Q3NP1/1P2PP1P/2RR2K1 b - - | c8c1 | 0 | R - R",
    "5rk1/5pp1/2r4p/5b2/2R5/6Q1/R1P1qPP1/5NK1 b - - | f5c2 | -100 | P - B + R - Q + R",
    "1r3r1k/p4pp1/2p1p2p/qpQP3P/2P5/3R4/PP3PP1/1K1R4 b - - | a5a2 | -800 | P - Q",
    "1r5k/p4pp1/2p1p2p/qpQP3P/2P2P2/1P1R4/P4rP1/1K1R4 b - - | a5a2 | 100 | P",
    "r2q1rk1/1b2bppp/p2p1n2/1ppNp3/3nP3/P2P1N1P/BPP2PP1/R1BQR1K1 w - - | d5e7 | 0 | B - N",
    "rnbqrbn1/pp3ppp/3p4/2p2k2/4p3/3B1K2/PPP2PPP/RNB1Q1NR w - - | d3e4 | 100 | P",
    "rnb1k2r/p3p1pp/1p3p1b/7n/1N2N3/3P1PB1/PPP1P1PP/R2QKB1R w KQkq - | e4d6 | -200 | -N + P",
    "r1b1k2r/p4npp/1pp2p1b/7n/1N2N3/3P1PB1/PPP1P1PP/R2QKB1R w KQkq - | e4d6 | 0 | -N + N",
    "2r1k2r/pb4pp/5p1b/2KB3n/4N3/2NP1PB1/PPP1P1PP/R2Q3R w k - | d5c6 | -300 | -B",
    "2r1k2r/pb4pp/5p1b/2KB3n/1N2N3/3P1PB1/PPP1P1PP/R2Q3R w k - | d5c6 | 0 | -B + B",
    "2r1k3/pbr3pp/5p1b/2KB3n/1N2N3/3P1PB1/PPP1P1PP/R2Q3R w - - | d5c6 | -300 | -B + B - N",
    "5k2/p2P2pp/8/1pb5/1Nn1P1n1/6Q1/PPP4P/R3K1NR w KQ - | d7d8q | 800 | (Q - P)",
    "r4k2/p2P2pp/8/1pb5/1Nn1P1n1/6Q1/PPP4P/R3K1NR w KQ - | d7d8q | -100 | (Q - P) - Q",
    "5k2/p2P2pp/1b6/1p6/1Nn1P1n1/8/PPP4P/R2QK1NR w KQ - | d7d8q | 200 | (Q - P) - Q + B",
    "4kbnr/p1P1pppp/b7/4q3/7n/8/PP1PPPPP/RNBQKBNR w KQk - | c7c8q | -100 | (Q - P) - Q",
    "4kbnr/p1P1pppp/b7/4q3/7n/8/PPQPPPPP/RNB1KBNR w KQk - | c7c8q | 200 | (Q - P) - Q + B",
    "4kbnr/p1P1pppp/b7/4q3/7n/8/PPQPPPPP/RNB1KBNR w KQk - | c7c8q | 200 | (Q - P)",
    "4kbnr/p1P4p/b1q5/5pP1/4n3/5Q2/PP1PPP1P/RNB1KBNR w KQk f6 | g5f6 | 0 | P - P",
    "4kbnr/p1P4p/b1q5/5pP1/4n3/5Q2/PP1PPP1P/RNB1KBNR w KQk f6 | g5f6 | 0 | P - P",
    "4kbnr/p1P4p/b1q5/5pP1/4n2Q/8/PP1PPP1P/RNB1KBNR w KQk f6 | g5f6 | 0 | P - P",
    "1n2kb1r/p1P4p/2qb4/5pP1/4n2Q/8/PP1PPP1P/RNB1KBNR w KQk - | c7b8q | 200 | N + (Q - P) - Q",
    "rnbqk2r/pp3ppp/2p1pn2/3p4/3P4/N1P1BN2/PPB1PPPb/R2Q1RK1 w kq - | g1h2 | 300 | B",
    "3N4/2K5/2n5/1k6/8/8/8/8 b - - | c6d8 | 0 | N - N",
    "3n3r/2P5/8/1k6/8/8/3Q4/4K3 w - - | c7d8q | 700 | (N + Q - P) - Q + R",
    "r2n3r/2P1P3/4N3/1k6/8/8/8/4K3 w - - | e6d8 | 300 | N",
    "8/8/8/1k6/6b1/4N3/2p3K1/3n4 w - - | e3d1 | 0 | N - N",
    "8/8/1k6/8/8/2N1N3/4p1K1/3n4 w - - | c3d1 | 100 | N - (N + Q - P) + Q",
    "r1bqk1nr/pppp1ppp/2n5/1B2p3/1b2P3/5N2/PPPP1PPP/RNBQK2R w KQkq - | e1g1 | 0"
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
        parameters.depth = 12;

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

std::vector<std::string> splitString(const std::string& input, char delimiter) {
    std::vector<std::string> tokens;
    std::istringstream tokenStream(input);
    std::string token;

    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }

    return tokens;
}

void seetest(Board* board) {
    int passed = 0, failed = 0;

    for (const std::string line : seeTest) {
        std::stringstream iss(line);
        std::vector<std::string> tokens = splitString(line, '|');

        std::string fen = tokens[0];
        std::string uciMove = tokens[1];
        int gain = stoi(tokens[2]);
        bool expected = gain >= 0;

        parseFen(board, fen);
        char charMove[6] = { '\0' };
        uciMove.copy(charMove, uciMove.length() - 2, 1);
        Move move = stringToMove(charMove, board);
        bool result = SEE(board, move, 0);

        if (result == expected)
            passed++;
        else
        {
            std::cout << "FAILED " << fen << "|" << uciMove << "(" << moveToString(move) << ")" << " | Expected: " << expected << std::endl;
            failed++;
        }

    }

    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << failed << std::endl;
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

void setoption(std::string line) {
    std::string name, value;

    if (matchesToken(line, "name")) {
        line = line.substr(5);
        name = line.substr(0, line.find(' '));
        line = line.substr(name.length() + 1);
    }

    if (matchesToken(line, "value")) {
        line = line.substr(6);
        value = line.find(' ') != std::string::npos ? line.substr(0, line.find(' ')) : line;
    }

    if (name == "Hash") {
        size_t hashSize = std::stoi(value);
        TT.resize(hashSize);
    }
}

void go(std::string line, Thread* searchThread, Board* board, std::deque<BoardStack>* stackQueue) {
    SearchParameters parameters;

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
        else if (matchesToken(line, "uci")) printf("id name yoshie2000-chess-engine\nid author Yoshie2000\n\noption name Hash type spin default 1 min 1 max 4096\nuciok\n");
        else if (matchesToken(line, "ucinewgame")) printf("TODO\n");
        else if (matchesToken(line, "go")) go(line.substr(3), searchThread, &board, &stackQueue);
        else if (matchesToken(line, "position")) position(line.substr(9), &board, &stackQueue);
        else if (matchesToken(line, "setoption")) setoption(line.substr(10));

        /* NON UCI COMMANDS */
        else if (matchesToken(line, "bench")) bench(searchThread, &stackQueue, &board);
        else if (matchesToken(line, "perfttest")) perfttest(searchThread, &stackQueue, &board);
        else if (matchesToken(line, "debug")) debugBoard(&board);
        else if (matchesToken(line, "eval")) debugEval(&board);
        else if (matchesToken(line, "seetest")) seetest(&board);
        else if (matchesToken(line, "see")) debugSEE(&board);
        else printf("Unknown command\n");
    }

    printf("UCI thread stopping\n");
}
