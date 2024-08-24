#include <iostream>
#include <cstdint>
#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>

#include "rescore.h"
#include "bitboard.h"
#include "board.h"
#include "types.h"
#include "zobrist.h"
#include "tt.h"
#include "thread.h"

struct Bulletformat {
    uint64_t occ;
    uint8_t pcs[16];
    int16_t score;
    uint8_t result;
    uint8_t ksq;
    uint8_t opp_ksq;
    uint8_t extra[3];
};

static_assert(sizeof(Bulletformat) == 32);

void rescore(std::string path, ThreadPool& threads) {
    auto start = std::chrono::high_resolution_clock::now();
    
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "Unable to open file!" << std::endl;
        exit(1);
    }

    std::string outputPath = path;
    size_t pos = outputPath.find_last_of('.');
    if (pos != std::string::npos)
        outputPath.insert(pos, "r");
    else {
        std::cerr << "Unable to name the output file!" << std::endl;
        exit(1);
    }
    std::ofstream outputFile(outputPath, std::ios::binary);
    if (!outputFile) {
        std::cerr << "Unable to open output file!" << std::endl;
        exit(1);
    }

    SearchParameters parameters;
    parameters.nodes = 5000;

    Bulletformat entry;
    long positions = 0;
    
    long researchedPositions = 0;
    double meanDiff = 0;
    long changedWdls = 0;
    while (file.read(reinterpret_cast<char*>(&entry), sizeof(Bulletformat))) {
        positions++;

        if (BB::popcount(entry.occ) <= 9) {

            Board board;
            std::deque<BoardStack> stackQueue = std::deque<BoardStack>(1);
            board.stack = &stackQueue.back();
            board.stm = Color::WHITE; // All pieces are from STM perspective, so this should work

            for (Square s = 0; s < 64; s++) {
                board.pieces[s] = Piece::NONE;
            }
            for (Color c = Color::WHITE; c <= Color::BLACK; ++c) {
                board.byColor[c] = bitboard(0);
                for (Piece p = Piece::PAWN; p < Piece::TOTAL; ++p) {
                    board.stack->pieceCount[c][p] = 0;
                    board.byPiece[p] = bitboard(0);
                }
            }
            board.ply = 0;
            board.chess960 = false;
            board.castlingSquares[0] = NO_SQUARE;
            board.castlingSquares[1] = NO_SQUARE;
            board.castlingSquares[2] = NO_SQUARE;
            board.castlingSquares[3] = NO_SQUARE;
            board.stack->checkers = bitboard(0);
            board.stack->capturedPiece = Piece::NONE;
            board.stack->hash = 0;
            board.stack->pawnHash = ZOBRIST_NO_PAWNS;
            board.stack->nullmove_ply = 0;
            board.stack->enpassantTarget = 0;
            board.stack->rule50_ply = 0;
            board.stack->nullmove_ply = 0;
            board.stack->castling = 0;
            board.stack->move = MOVE_NONE;

            uint64_t occ = entry.occ;
            int idx = 0;
            while (occ) {
                uint8_t square = lsb(occ);
                uint8_t coloredPiece = (entry.pcs[idx / 2] >> (4 * (idx & 1))) & 0b1111;
                Piece p = Piece(coloredPiece & 0b0111);
                uint8_t color = (coloredPiece & 0b1000) >> 3;

                // if (color == Color::BLACK)
                //     square ^= 56;
                assert(p >= 0 && p < Piece::TOTAL);

                uint64_t bb = 1ULL << square;
                board.byColor[color] |= bb;
                board.byPiece[p] |= bb;
                board.pieces[square] = p;
                board.stack->pieceCount[color][p]++;
                board.stack->hash ^= ZOBRIST_PIECE_SQUARES[color][p][square];
                if (p == Piece::PAWN)
                    board.stack->pawnHash ^= ZOBRIST_PIECE_SQUARES[color][p][square];

                occ &= occ - 1;
                idx++;
            }

            // Update king checking stuff
            Square enemyKing = lsb(board.byColor[Color::BLACK] & board.byPiece[Piece::KING]);
            board.stack->checkers = board.attackersTo(enemyKing, board.byColor[Color::WHITE] | board.byColor[Color::BLACK]) & board.byColor[Color::BLACK];
            board.stack->checkerCount = BB::popcount(board.stack->checkers);

            board.updateSliderPins(Color::WHITE);
            board.updateSliderPins(Color::BLACK);

            board.calculateThreats();

            threads.ucinewgame();
            TT.newSearch();
            threads.startSearching(board, stackQueue, parameters);
            threads.waitForSearchFinished();

            Eval rescoreResult = threads.threads[0]->rootMoves[0].value;
            researchedPositions++;
            
            if (rescoreResult >= EVAL_TB_WIN_IN_MAX_PLY && entry.result != 2)
                entry.result = 2, changedWdls++; // "Us" win
            else if (rescoreResult <= -EVAL_TB_WIN_IN_MAX_PLY && entry.result != 0)
                entry.result = 0, changedWdls++; // "Them" win
            
            if (std::abs(rescoreResult) <= EVAL_TB_WIN_IN_MAX_PLY) {
                int newScore = 100 * rescoreResult / 266;
                meanDiff = ((researchedPositions - 1) * meanDiff + std::abs(newScore - entry.score)) / researchedPositions;
                entry.score = newScore;
            } else {
                continue;
            }
        }

        outputFile.write(reinterpret_cast<const char*>(&entry), sizeof(Bulletformat));

        if (positions % 1000 == 0) {
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> duration = end - start;
            std::cout << positions << " in " << long(duration.count()) << "s (" << long(positions / duration.count()) <<  " pos/s), rescored " << (100 * researchedPositions / positions) << "% (mean diff " << meanDiff << ", changed wdls " << (100 * changedWdls / researchedPositions) << "%)" << std::endl;
        }
    }

    file.close();
}