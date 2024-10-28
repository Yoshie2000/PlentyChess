#include "thread.h"

#include <fstream>
#include <iostream>
#include <filesystem>

std::vector<Move> generateLegalMoves(Board* board) {
    Move moves[MAX_MOVES] = { MOVE_NONE };
    int m = 0;
    generateMoves(board, moves, &m);
    std::vector<Move> legalMoves;
    for (int i = 0; i < m; i++) {
        if (board->isLegal(moves[i])) {
            legalMoves.push_back(moves[i]);
        }
    }
    return legalMoves;
}

bool playRandomMoves(Board* board, Thread* thread, int remainingMoves) {
    std::vector<Move> legalMoves = generateLegalMoves(board);

    if (legalMoves.empty())
        return false;

    if (remainingMoves == 0) {

        // Do a verification search that this position isn't completely busted
        thread->ucinewgame();
        thread->rootMoves.clear();
        thread->searchParameters->depth = 10;
        thread->searchParameters->nodes = 1000000;
        thread->rootBoard = *board;
        thread->tdatagen();

        Eval verificationScore = thread->rootMoves[0].value;

        if (std::abs(verificationScore) >= 1000)
            return false;
        
        std::cout << "info string genfens " << board->fen() << std::endl;
        return true;
    }

    BoardStack boardStack;
    Move move = MOVE_NONE;
    while (move == MOVE_NONE) {
        int r = std::rand() % 100;
        Piece randomPiece = r < 35 ? Piece::PAWN : r < 50 ? Piece::KNIGHT : r < 65 ? Piece::BISHOP : r < 80 ? Piece::QUEEN : r < 95 ? Piece::KING : Piece::ROOK;
        std::vector<Move> pieceMoves;
        for (Move m : legalMoves) {
            if (board->pieces[moveOrigin(m)] == randomPiece)
                pieceMoves.push_back(m);
        }
        if (!pieceMoves.empty()) {
            move = pieceMoves[std::rand() % pieceMoves.size()];
        }
    }
    board->doMove(&boardStack, move, board->hashAfter(move), &thread->nnue);

    return playRandomMoves(board, thread, remainingMoves - 1);
}

std::string fenFromBook(std::string bookFile, size_t pos) {
    std::ifstream file(bookFile);
    if (file) {
        file.seekg(pos);
        std::string line;
        std::getline(file, line);
        return line;
    }
    return "";
}

inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

void Thread::tgenfens() {
    std::srand(searchParameters->genfensSeed);

    size_t bookSize = 0;
    std::vector<size_t> lineOffsets;
    rtrim(searchParameters->genfensBook);
    std::string bookPath = std::filesystem::current_path() / searchParameters->genfensBook;
    if (searchParameters->genfensBook.length() > 0 && searchParameters->genfensBook != "None") {
        std::ifstream f(bookPath);
        if (!f.good()) {
            std::cout << "info string unable to find genfens file" << std::endl;
            return;
        }
        {
            std::ifstream file(bookPath); 
            bookSize = std::count(std::istreambuf_iterator<char>(file), 
                std::istreambuf_iterator<char>(), '\n');
        }
        {
            std::ifstream file(bookPath);
            std::string line;
            lineOffsets.push_back(file.tellg());

            while (std::getline(file, line)) {
                lineOffsets.push_back(file.tellg());
            }
        }
    }

    int generatedFens = 0;
    while (generatedFens < searchParameters->genfensFens) {

        // Set up a fresh board
        Board board;
        BoardStack boardStack;
        board.stack = &boardStack;
        board.startpos();
        nnue.reset(&board);

        int randomMoves = 8 + std::rand() % 2;

        if (bookSize > 0) {
            size_t bookPosition = std::rand() % bookSize;
            std::string fen = fenFromBook(bookPath, lineOffsets[bookPosition]);
            randomMoves = 3 + std::rand() % 2;
            board.parseFen(fen, false);
        }

        // Play random moves
        if (playRandomMoves(&board, this, randomMoves)) {
            generatedFens++;
        }
    }
}