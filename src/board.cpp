#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <cassert>

#include "move.h"
#include "types.h"
#include "board.h"
#include "bitboard.h"
#include "tt.h"
#include "magic.h"
#include "evaluation.h"

const Bitboard FILE_A = 0x0101010101010101;
const Bitboard FILE_B = 0x0202020202020202;
const Bitboard FILE_C = 0x0404040404040404;
const Bitboard FILE_D = 0x0808080808080808;
const Bitboard FILE_E = 0x1010101010101010;
const Bitboard FILE_F = 0x2020202020202020;
const Bitboard FILE_G = 0x4040404040404040;
const Bitboard FILE_H = 0x8080808080808080;

const Bitboard RANK_1 = 0x00000000000000FF;
const Bitboard RANK_2 = 0x000000000000FF00;
const Bitboard RANK_3 = 0x0000000000FF0000;
const Bitboard RANK_4 = 0x00000000FF000000;
const Bitboard RANK_5 = 0x000000FF00000000;
const Bitboard RANK_6 = 0x0000FF0000000000;
const Bitboard RANK_7 = 0x00FF000000000000;
const Bitboard RANK_8 = 0xFF00000000000000;

void startpos(Board* result) {
    parseFen(result, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

size_t parseFen(Board* board, std::string fen) {
    Square currentSquare = 56;
    uint8_t currentRank = 7;
    size_t i = 0;

    // Reset everything (there might be garbage data)
    for (Square s = 0; s < 64; s++) {
        board->pieces[s] = NO_PIECE;
    }
    for (Color c = 0; c <= 1; c++) {
        board->byColor[c] = C64(0);
        for (Piece p = 0; p < PIECE_TYPES; p++) {
            board->stack->pieceCount[c][p] = 0;
            board->byPiece[p] = C64(0);
        }
    }
    board->stack->checkers = C64(0);
    board->stack->capturedPiece = NO_PIECE;
    board->stack->hash = 0;
    board->stack->pawnHash = ZOBRIST_NO_PAWNS;
    board->stack->nullmove_ply = 0;

    // Board position and everything
    Bitboard currentSquareBB = C64(1) << currentSquare;
    for (i = 0; i < fen.length(); i++) {
        char c = fen.at(i);
        if (c == ' ') {
            i++;
            break;
        }

        if (currentSquare < 64)
            currentSquareBB = C64(1) << currentSquare;
        switch (c) {
        case 'p':
            board->byColor[COLOR_BLACK] |= currentSquareBB;
            board->byPiece[PIECE_PAWN] |= currentSquareBB;
            board->stack->pieceCount[COLOR_BLACK][PIECE_PAWN]++;
            board->pieces[currentSquare] = PIECE_PAWN;
            board->stack->hash ^= ZOBRIST_PIECE_SQUARES[COLOR_BLACK][PIECE_PAWN][currentSquare];
            board->stack->pawnHash ^= ZOBRIST_PIECE_SQUARES[COLOR_BLACK][PIECE_PAWN][currentSquare];
            currentSquare++;
            break;
        case 'P':
            board->byColor[COLOR_WHITE] |= currentSquareBB;
            board->byPiece[PIECE_PAWN] |= currentSquareBB;
            board->stack->pieceCount[COLOR_WHITE][PIECE_PAWN]++;
            board->pieces[currentSquare] = PIECE_PAWN;
            board->stack->hash ^= ZOBRIST_PIECE_SQUARES[COLOR_WHITE][PIECE_PAWN][currentSquare];
            board->stack->pawnHash ^= ZOBRIST_PIECE_SQUARES[COLOR_WHITE][PIECE_PAWN][currentSquare];
            currentSquare++;
            break;
        case 'n':
            board->byColor[COLOR_BLACK] |= currentSquareBB;
            board->byPiece[PIECE_KNIGHT] |= currentSquareBB;
            board->stack->pieceCount[COLOR_BLACK][PIECE_KNIGHT]++;
            board->pieces[currentSquare] = PIECE_KNIGHT;
            board->stack->hash ^= ZOBRIST_PIECE_SQUARES[COLOR_BLACK][PIECE_KNIGHT][currentSquare];
            currentSquare++;
            break;
        case 'N':
            board->byColor[COLOR_WHITE] |= currentSquareBB;
            board->byPiece[PIECE_KNIGHT] |= currentSquareBB;
            board->stack->pieceCount[COLOR_WHITE][PIECE_KNIGHT]++;
            board->pieces[currentSquare] = PIECE_KNIGHT;
            board->stack->hash ^= ZOBRIST_PIECE_SQUARES[COLOR_WHITE][PIECE_KNIGHT][currentSquare];
            currentSquare++;
            break;
        case 'b':
            board->byColor[COLOR_BLACK] |= currentSquareBB;
            board->byPiece[PIECE_BISHOP] |= currentSquareBB;
            board->stack->pieceCount[COLOR_BLACK][PIECE_BISHOP]++;
            board->pieces[currentSquare] = PIECE_BISHOP;
            board->stack->hash ^= ZOBRIST_PIECE_SQUARES[COLOR_BLACK][PIECE_BISHOP][currentSquare];
            currentSquare++;
            break;
        case 'B':
            board->byColor[COLOR_WHITE] |= currentSquareBB;
            board->byPiece[PIECE_BISHOP] |= currentSquareBB;
            board->stack->pieceCount[COLOR_WHITE][PIECE_BISHOP]++;
            board->pieces[currentSquare] = PIECE_BISHOP;
            board->stack->hash ^= ZOBRIST_PIECE_SQUARES[COLOR_WHITE][PIECE_BISHOP][currentSquare];
            currentSquare++;
            break;
        case 'r':
            board->byColor[COLOR_BLACK] |= currentSquareBB;
            board->byPiece[PIECE_ROOK] |= currentSquareBB;
            board->stack->pieceCount[COLOR_BLACK][PIECE_ROOK]++;
            board->pieces[currentSquare] = PIECE_ROOK;
            board->stack->hash ^= ZOBRIST_PIECE_SQUARES[COLOR_BLACK][PIECE_ROOK][currentSquare];
            currentSquare++;
            break;
        case 'R':
            board->byColor[COLOR_WHITE] |= currentSquareBB;
            board->byPiece[PIECE_ROOK] |= currentSquareBB;
            board->stack->pieceCount[COLOR_WHITE][PIECE_ROOK]++;
            board->pieces[currentSquare] = PIECE_ROOK;
            board->stack->hash ^= ZOBRIST_PIECE_SQUARES[COLOR_WHITE][PIECE_ROOK][currentSquare];
            currentSquare++;
            break;
        case 'q':
            board->byColor[COLOR_BLACK] |= currentSquareBB;
            board->byPiece[PIECE_QUEEN] |= currentSquareBB;
            board->stack->pieceCount[COLOR_BLACK][PIECE_QUEEN]++;
            board->pieces[currentSquare] = PIECE_QUEEN;
            board->stack->hash ^= ZOBRIST_PIECE_SQUARES[COLOR_BLACK][PIECE_QUEEN][currentSquare];
            currentSquare++;
            break;
        case 'Q':
            board->byColor[COLOR_WHITE] |= currentSquareBB;
            board->byPiece[PIECE_QUEEN] |= currentSquareBB;
            board->stack->pieceCount[COLOR_WHITE][PIECE_QUEEN]++;
            board->pieces[currentSquare] = PIECE_QUEEN;
            board->stack->hash ^= ZOBRIST_PIECE_SQUARES[COLOR_WHITE][PIECE_QUEEN][currentSquare];
            currentSquare++;
            break;
        case 'k':
            board->byColor[COLOR_BLACK] |= currentSquareBB;
            board->byPiece[PIECE_KING] |= currentSquareBB;
            board->stack->pieceCount[COLOR_BLACK][PIECE_KING]++;
            board->pieces[currentSquare] = PIECE_KING;
            board->stack->hash ^= ZOBRIST_PIECE_SQUARES[COLOR_BLACK][PIECE_KING][currentSquare];
            currentSquare++;
            break;
        case 'K':
            board->byColor[COLOR_WHITE] |= currentSquareBB;
            board->byPiece[PIECE_KING] |= currentSquareBB;
            board->stack->pieceCount[COLOR_WHITE][PIECE_KING]++;
            board->pieces[currentSquare] = PIECE_KING;
            board->stack->hash ^= ZOBRIST_PIECE_SQUARES[COLOR_WHITE][PIECE_KING][currentSquare];
            currentSquare++;
            break;
        case '/':
            currentRank--;
            currentSquare = 8 * currentRank;
            break;
        default: // Number
            currentSquare += (int)(c)-48;
            break;
        }
    }

    // Side to move
    board->stm = fen.at(i) == 'w' ? COLOR_WHITE : COLOR_BLACK;
    if (board->stm == COLOR_BLACK)
        board->stack->hash ^= ZOBRIST_STM_BLACK;
    i += 2;

    // Castling
    board->stack->castling = 0;
    for (; i < fen.length(); i++) {
        char c = fen.at(i);

        if (c == '-') {
            i += 2;
            break;
        }
        else if (c == ' ') {
            i++;
            break;
        }

        switch (c) {
        case 'k':
            board->stack->castling |= 0x4;
            break;
        case 'K':
            board->stack->castling |= 0x1;
            break;
        case 'q':
            board->stack->castling |= 0x8;
            break;
        case 'Q':
            board->stack->castling |= 0x2;
            break;
        case ' ':
        default:
            std::cout << "Weird char in fen castling: " << c << " (index " << i << ")" << std::endl;
            exit(-1);
            break;
        }
    }
    board->stack->hash ^= ZOBRIST_CASTLING[board->stack->castling & 0xF];

    // en passent
    if (fen.at(i) == '-') {
        board->stack->enpassantTarget = C64(0);
        i += 2;
    }
    else {
        char epTargetString[2] = { fen.at(i), fen.at(i + 1) };
        Square epTargetSquare = stringToSquare(epTargetString);
        board->stack->enpassantTarget = C64(1) << epTargetSquare;
        i += 3;

        // Check if there's *actually* a pawn that can do enpassent
        Bitboard enemyPawns = board->byColor[1 - board->stm] & board->byPiece[PIECE_PAWN];
        Bitboard epRank = board->stm == COLOR_WHITE ? RANK_4 : RANK_5;
        Bitboard epPawns = ((C64(1) << (epTargetSquare + 1 + UP[board->stm])) | (C64(1) << (epTargetSquare - 1 + UP[board->stm]))) & epRank & enemyPawns;
        if (epPawns)
            board->stack->hash ^= ZOBRIST_ENPASSENT[epTargetSquare % 8];
    }

    if (fen.length() <= i) {
        board->stack->rule50_ply = 0;
        board->ply = 0;
    }
    else {
        // 50 move rule
        std::string rule50String = "--";
        int rule50tmp = 0;
        while (fen.at(i) != ' ') {
            rule50String.replace(rule50tmp++, 1, 1, fen.at(i++));
        }
        if (rule50String.at(1) == '-') {
            board->stack->rule50_ply = (int)(rule50String.at(0)) - 48;
        }
        else {
            board->stack->rule50_ply = 10 * ((int)(rule50String.at(0)) - 48) + ((int)(rule50String.at(1)) - 48);
        }
        i++;

        // Move number
        std::string plyString = "---";
        int plyTmp = 0;
        while (i < fen.length() && fen.at(i) != ' ') {
            plyString.replace(plyTmp++, 1, 1, fen.at(i++));
        }
        if (plyString.at(1) == '-') {
            board->ply = (int)(plyString.at(0)) - 48;
        }
        else if (plyString.at(2) == '-') {
            board->ply = 10 * ((int)(plyString.at(0)) - 48) + ((int)(plyString.at(1)) - 48);
        }
        else {
            board->ply = 100 * ((int)(plyString.at(0)) - 48) + 10 * ((int)(plyString.at(1)) - 48) + ((int)(plyString.at(2)) - 48);
        }
    }

    // Update king checking stuff
    Square enemyKing = lsb(board->byColor[board->stm] & board->byPiece[PIECE_KING]);
    board->stack->checkers = attackersTo(board, enemyKing, board->byColor[COLOR_WHITE] | board->byColor[COLOR_BLACK]) & board->byColor[1 - board->stm];
    board->stack->checkerCount = __builtin_popcountll(board->stack->checkers);

    updateSliderPins(board, COLOR_WHITE);
    updateSliderPins(board, COLOR_BLACK);

    return i;
}

void castlingRookSquares(Board* board, Square origin, Square target, Square* rookOrigin, Square* rookTarget, uint8_t* castling) {
    if (board->stm == COLOR_WHITE) {
        if (target > origin) { // Kingside White
            *rookOrigin = 7;
            *rookTarget = 5;
        }
        else { // Queenside White
            *rookOrigin = 0;
            *rookTarget = 3;
        }
        *castling &= 0xC; // Clear flags for white
    }
    else {
        if (target > origin) { // Kingside Black
            *rookOrigin = 63;
            *rookTarget = 61;
        }
        else { // Queenside Black
            *rookOrigin = 56;
            *rookTarget = 59;
        }
        *castling &= 0x3; // Clear flags for black
    }
}

void doMove(Board* board, BoardStack* newStack, Move move, uint64_t newHash, NNUE* nnue) {
    newStack->previous = board->stack;
    board->stack = newStack;
    memcpy(newStack->pieceCount, newStack->previous->pieceCount, sizeof(int) * 12 + sizeof(uint8_t));

    newStack->hash = newHash;
    newStack->pawnHash = newStack->previous->pawnHash;
    newStack->rule50_ply = newStack->previous->rule50_ply + 1;
    newStack->nullmove_ply = newStack->previous->nullmove_ply + 1;

    Square origin = moveOrigin(move);
    Square target = moveTarget(move);
    Move specialMove = move & 0x3000;

    assert(origin < 64 && target < 64);

    newStack->capturedPiece = board->pieces[target];
    Square captureTarget = target;
    Bitboard captureTargetBB = C64(1) << captureTarget;

    Piece piece = board->pieces[origin];
    Piece promotionPiece = NO_PIECE;

    Bitboard originBB = C64(1) << origin;
    Bitboard targetBB = C64(1) << target;
    Bitboard fromTo = originBB | targetBB;

    board->byColor[board->stm] ^= fromTo;
    board->byPiece[piece] ^= fromTo;

    board->pieces[origin] = NO_PIECE;
    board->pieces[target] = piece;

    // Increment the NNUE accumulator to indicate that we're on a different move now
    nnue->incrementAccumulator();

    newStack->enpassantTarget = 0;

    if (piece == PIECE_PAWN) {
        newStack->rule50_ply = 0;

        // This move is en passent
        if (specialMove == MOVE_ENPASSANT) {
            newStack->capturedPiece = PIECE_PAWN;
            captureTarget = target - UP[board->stm];

            assert(captureTarget < 64);

            captureTargetBB = C64(1) << captureTarget;
            board->pieces[captureTarget] = NO_PIECE; // remove the captured pawn

            newStack->pawnHash ^= ZOBRIST_PIECE_SQUARES[1 - board->stm][PIECE_PAWN][captureTarget];
        }


        if ((origin ^ target) == 16) {
            assert(target - UP[board->stm] < 64);

            Bitboard enemyPawns = board->byColor[1 - board->stm] & board->byPiece[PIECE_PAWN];
            Bitboard epRank = board->stm == COLOR_WHITE ? RANK_4 : RANK_5;
            Bitboard epPawns = ((C64(1) << (target + 1)) | (C64(1) << (target - 1))) & epRank & enemyPawns;
            if (epPawns)
                newStack->enpassantTarget = C64(1) << (target - UP[board->stm]);

        }

        newStack->pawnHash ^= ZOBRIST_PIECE_SQUARES[board->stm][PIECE_PAWN][origin] ^ ZOBRIST_PIECE_SQUARES[board->stm][PIECE_PAWN][target];
    }

    // Handle capture
    if (newStack->capturedPiece != NO_PIECE) {
        board->byColor[1 - board->stm] ^= captureTargetBB; // take away the captured piece
        board->byPiece[newStack->capturedPiece] ^= captureTargetBB;

        nnue->removePiece(captureTarget, newStack->capturedPiece, (Color)(1 - board->stm));

        newStack->pieceCount[1 - board->stm][newStack->capturedPiece]--;
        newStack->rule50_ply = 0;
    }

    // This move is castling
    if (specialMove == MOVE_CASTLING) {
        Square rookOrigin, rookTarget;
        castlingRookSquares(board, origin, target, &rookOrigin, &rookTarget, &newStack->castling);
        assert(rookOrigin < 64 && rookTarget < 64);

        Bitboard rookFromToBB = (C64(1) << rookOrigin) | (C64(1) << rookTarget);
        board->pieces[rookOrigin] = NO_PIECE;
        board->pieces[rookTarget] = PIECE_ROOK;
        board->byColor[board->stm] ^= rookFromToBB;
        board->byPiece[PIECE_ROOK] ^= rookFromToBB;

        nnue->movePiece(rookOrigin, rookTarget, PIECE_ROOK, board->stm);
    }

    // This move is promotion
    if (specialMove == MOVE_PROMOTION) {
        promotionPiece = PROMOTION_PIECE[move >> 14];
        board->byPiece[piece] ^= targetBB;
        board->byPiece[promotionPiece] ^= targetBB;

        board->pieces[target] = promotionPiece;
        newStack->pawnHash ^= ZOBRIST_PIECE_SQUARES[board->stm][PIECE_PAWN][target];

        // Promotion, we don't move the current piece, instead we remove it from the origin square
        // and place the promotionPiece on the target square. This saves one accumulator update
        nnue->removePiece(origin, piece, board->stm);
        nnue->addPiece(target, promotionPiece, board->stm);

        newStack->pieceCount[board->stm][PIECE_PAWN]--;
        newStack->pieceCount[board->stm][promotionPiece]++;
        newStack->rule50_ply = 0;
    }
    else {
        // No promotion, we can normally move the piece in the accumulator
        nnue->movePiece(origin, target, piece, board->stm);
    }

    // Unset castling flags if necessary
    if (piece == PIECE_KING) {
        if (board->stm == COLOR_WHITE)
            newStack->castling &= 0xC;
        else
            newStack->castling &= 0x3;
    }
    if (piece == PIECE_ROOK || newStack->capturedPiece == PIECE_ROOK) {
        switch (piece == PIECE_ROOK ? origin : captureTarget) {
        case 0:
            newStack->castling &= ~0x2; // Queenside castle white
            break;
        case 7:
            newStack->castling &= ~0x1; // Kingside castle white
            break;
        case 56:
            newStack->castling &= ~0x8; // Queenside castle black
            break;
        case 63:
            newStack->castling &= ~0x4; // Kingside castle black
            break;
        default:
            break;
        }
    }

    // Update king checking stuff
    assert((board->byColor[1 - board->stm] & board->byPiece[PIECE_KING]) > 0);

    Square enemyKing = lsb(board->byColor[1 - board->stm] & board->byPiece[PIECE_KING]);
    newStack->checkers = attackersTo(board, enemyKing, board->byColor[COLOR_WHITE] | board->byColor[COLOR_BLACK]) & board->byColor[board->stm];
    newStack->checkerCount = newStack->checkers ? __builtin_popcountll(newStack->checkers) : 0;
    updateSliderPins(board, COLOR_WHITE);
    updateSliderPins(board, COLOR_BLACK);

    board->stm = 1 - board->stm;
    newStack->move = move;
}

void undoMove(Board* board, Move move, NNUE* nnue) {
    board->stm = 1 - board->stm;

    Square origin = moveOrigin(move);
    Square target = moveTarget(move);
    Square captureTarget = target;

    assert(origin < 64 && target < 64);

    Bitboard originBB = C64(1) << origin;
    Bitboard targetBB = C64(1) << target;
    Bitboard fromTo = originBB | targetBB;
    Bitboard captureTargetBB = C64(1) << captureTarget;

    board->byColor[board->stm] ^= fromTo;

    Piece piece = board->pieces[target];

    board->byPiece[piece] ^= fromTo;

    board->pieces[target] = NO_PIECE;
    board->pieces[origin] = piece;

    // This move is en passent
    Move specialMove = move & 0x3000;
    if (specialMove == MOVE_ENPASSANT) {
        captureTarget = target - UP[board->stm];

        assert(captureTarget < 64);

        captureTargetBB = C64(1) << captureTarget;
    }

    // Handle capture
    if (board->stack->capturedPiece != NO_PIECE) {
        board->pieces[captureTarget] = board->stack->capturedPiece;
        board->byColor[1 - board->stm] ^= captureTargetBB;
        board->byPiece[board->stack->capturedPiece] ^= captureTargetBB;
    }

    // Castling
    if (specialMove == MOVE_CASTLING) {
        Square rookOrigin, rookTarget;
        castlingRookSquares(board, origin, target, &rookOrigin, &rookTarget, &board->stack->castling);

        assert(rookOrigin < 64 && rookTarget < 64);

        Bitboard rookFromToBB = (C64(1) << rookOrigin) | (C64(1) << rookTarget);
        board->pieces[rookOrigin] = PIECE_ROOK;
        board->pieces[rookTarget] = NO_PIECE;
        board->byColor[board->stm] ^= rookFromToBB;
        board->byPiece[PIECE_ROOK] ^= rookFromToBB;
    }

    // This move is promotion
    if (specialMove == MOVE_PROMOTION) {
        board->byPiece[piece] ^= originBB;
        board->byPiece[PIECE_PAWN] ^= originBB;

        board->pieces[origin] = PIECE_PAWN;
    }

    nnue->decrementAccumulator();

    board->stack = board->stack->previous;
}

void doNullMove(Board* board, BoardStack* newStack) {
    assert(!board->stack->checkers);

    newStack->previous = board->stack;
    board->stack = newStack;
    memcpy(newStack->pieceCount, newStack->previous->pieceCount, sizeof(int) * 12 + sizeof(uint8_t));

    newStack->hash = newStack->previous->hash ^ ZOBRIST_STM_BLACK;
    newStack->pawnHash = newStack->previous->pawnHash;
    newStack->rule50_ply = newStack->previous->rule50_ply + 1;
    newStack->nullmove_ply = 0;

    // En passent square
    if (newStack->previous->enpassantTarget != 0) {
        newStack->hash ^= ZOBRIST_ENPASSENT[lsb(newStack->previous->enpassantTarget) % 8];
    }
    newStack->enpassantTarget = 0;

    // Update king checking stuff
    assert((board->byColor[1 - board->stm] & board->byPiece[PIECE_KING]) > 0);

    newStack->checkers = C64(0);
    newStack->checkerCount = 0;
    updateSliderPins(board, COLOR_WHITE);
    updateSliderPins(board, COLOR_BLACK);

    board->stm = 1 - board->stm;
    newStack->move = MOVE_NULL;
}

void undoNullMove(Board* board) {
    board->stm = 1 - board->stm;
    board->stack = board->stack->previous;
}

uint64_t hashAfter(Board* board, Move move) {
    uint64_t hash = board->stack->hash ^ ZOBRIST_STM_BLACK;

    Square origin = moveOrigin(move);
    Square target = moveTarget(move);
    Square captureTarget = target;

    Piece piece = board->pieces[origin];
    Piece capturedPiece = board->pieces[target];

    Move specialMove = move & 0x3000;

    uint8_t newCastling = board->stack->castling;

    hash ^= ZOBRIST_PIECE_SQUARES[board->stm][piece][origin] ^ ZOBRIST_PIECE_SQUARES[board->stm][piece][target];

    if (board->stack->enpassantTarget != 0)
        hash ^= ZOBRIST_ENPASSENT[lsb(board->stack->enpassantTarget) % 8];

    if (piece == PIECE_PAWN) {
        if (specialMove == MOVE_ENPASSANT) {
            capturedPiece = PIECE_PAWN;
            captureTarget = target - UP[board->stm];
        }

        if ((origin ^ target) == 16) {
            Bitboard enemyPawns = board->byColor[1 - board->stm] & board->byPiece[PIECE_PAWN];
            Bitboard epRank = board->stm == COLOR_WHITE ? RANK_4 : RANK_5;
            Bitboard epPawns = ((C64(1) << (target + 1)) | (C64(1) << (target - 1))) & epRank & enemyPawns;
            if (epPawns)
                hash ^= ZOBRIST_ENPASSENT[origin % 8];
        }
    }

    if (capturedPiece != NO_PIECE)
        hash ^= ZOBRIST_PIECE_SQUARES[1 - board->stm][capturedPiece][captureTarget];

    if (specialMove == MOVE_CASTLING) {
        Square rookOrigin, rookTarget;

        hash ^= ZOBRIST_CASTLING[newCastling];
        castlingRookSquares(board, origin, target, &rookOrigin, &rookTarget, &newCastling);
        hash ^= ZOBRIST_CASTLING[newCastling];
        hash ^= ZOBRIST_PIECE_SQUARES[board->stm][PIECE_ROOK][rookOrigin] ^ ZOBRIST_PIECE_SQUARES[board->stm][PIECE_ROOK][rookTarget];
    }

    if (specialMove == MOVE_PROMOTION) {
        Piece promotionPiece = PROMOTION_PIECE[move >> 14];
        hash ^= ZOBRIST_PIECE_SQUARES[board->stm][piece][target] ^ ZOBRIST_PIECE_SQUARES[board->stm][promotionPiece][target];
    }

    if (piece == PIECE_KING) {
        hash ^= ZOBRIST_CASTLING[newCastling & 0xF];
        if (board->stm == COLOR_WHITE)
            newCastling &= 0xC;
        else
            newCastling &= 0x3;
        hash ^= ZOBRIST_CASTLING[newCastling & 0xF];
    }
    if (piece == PIECE_ROOK || capturedPiece == PIECE_ROOK) {
        hash ^= ZOBRIST_CASTLING[newCastling & 0xF];
        switch (piece == PIECE_ROOK ? origin : captureTarget) {
        case 0:
            newCastling &= ~0x2; // Queenside castle white
            break;
        case 7:
            newCastling &= ~0x1; // Kingside castle white
            break;
        case 56:
            newCastling &= ~0x8; // Queenside castle black
            break;
        case 63:
            newCastling &= ~0x4; // Kingside castle black
            break;
        default:
            break;
        }
        hash ^= ZOBRIST_CASTLING[newCastling & 0xF];
    }

    return hash;
}

void updateSliderPins(Board* board, Color side) {
    assert((board->byColor[side] & board->byPiece[PIECE_KING]) > 0);

    Square king = lsb(board->byColor[side] & board->byPiece[PIECE_KING]);

    board->stack->blockers[side] = 0;
    board->stack->pinners[1 - side] = 0;

    Bitboard possiblePinnersRook = getRookMoves(king, C64(0)) & (board->byPiece[PIECE_ROOK] | board->byPiece[PIECE_QUEEN]);
    Bitboard possiblePinnersBishop = getBishopMoves(king, C64(0)) & (board->byPiece[PIECE_BISHOP] | board->byPiece[PIECE_QUEEN]);
    Bitboard possiblePinners = (possiblePinnersBishop | possiblePinnersRook) & board->byColor[1 - side];
    Bitboard occupied = (board->byColor[side] | board->byColor[1 - side]) ^ possiblePinners;

    // Go through all pieces that could potentially pin the king
    while (possiblePinners) {
        Square pinnerSquare = popLSB(&possiblePinners);
        Bitboard blockerBB = BETWEEN[king][pinnerSquare] & occupied;

        if (__builtin_popcountll(blockerBB) == 1) {
            // We have exactly one blocker for this pinner
            board->stack->blockers[side] |= blockerBB;
            if (blockerBB & board->byColor[side])
                board->stack->pinners[1 - side] |= C64(1) << pinnerSquare;
        }
    }
}

// Using cuckoo tables, check if the side to move has any move that would lead to a repetition
bool hasUpcomingRepetition(Board* board, int ply) {

    int maxPlyOffset = std::min(board->stack->rule50_ply, board->stack->nullmove_ply);
    if (maxPlyOffset < 3)
        return false;

    uint64_t hash = board->stack->hash;
    BoardStack* stack = board->stack->previous;

    int j = 0;
    for (int i = 3; i <= maxPlyOffset; i += 2) {
        stack = stack->previous->previous;

        uint64_t moveHash = hash ^ stack->hash;
        if ((j = H1(moveHash), CUCKOO_HASHES[j] == moveHash) || (j = H2(moveHash), CUCKOO_HASHES[j] == moveHash)) {
            Move move = CUCKOO_MOVES[j];
            Square origin = moveOrigin(move);
            Square target = moveTarget(move);

            if (BETWEEN[origin][target] & (board->byColor[COLOR_WHITE] | board->byColor[COLOR_BLACK]))
                continue;

            if (ply > i)
                return true;

            Square pieceSquare = board->pieces[origin] == NO_PIECE ? target : origin;
            Color pieceColor = (board->byColor[COLOR_WHITE] & (C64(1) << pieceSquare)) ? COLOR_WHITE : COLOR_BLACK;
            if (pieceColor != board->stm)
                continue;

            // Check for 2-fold repetition
            BoardStack* stack2 = stack;
            for (int k = i + 4; k <= maxPlyOffset; k += 2) {
                if (k == i + 4)
                    stack2 = stack2->previous->previous;
                stack2 = stack->previous->previous;
                if (stack2->hash == stack->hash)
                    return true;
            }
        }
    }

    return false;
}

// Checks for 2-fold repetition and rule50 draw
bool isDraw(Board* board) {

    // The stack needs to go back far enough
    if (!board->stack->previous || !board->stack->previous->previous)
        return false;

    // 2-fold repetition
    int maxPlyOffset = std::min(board->stack->rule50_ply, board->stack->nullmove_ply);
    BoardStack* stack = board->stack->previous->previous;

    for (int i = 4; i <= maxPlyOffset; i += 2) {
        stack = stack->previous->previous;
        if (board->stack->hash == stack->hash) {
            return true;
        }
    }

    // 50 move rule draw
    if (board->stack->rule50_ply > 99) {
        if (!board->stack->checkers)
            return true;

        // If in check, it might be checkmate
        Move moves[MAX_MOVES] = { MOVE_NONE };
        int moveCount = 0;
        int legalMoveCount = 0;
        generateMoves(board, moves, &moveCount);
        for (int i = 0; i < moveCount; i++) {
            Move move = moves[i];
            if (!isLegal(board, move))
                continue;
            legalMoveCount++;
        }

        return legalMoveCount > 0;
    }

    // Otherwise, no draw
    return false;
}

Bitboard attackersTo(Board* board, Square s, Bitboard occupied) {
    Bitboard sBB = C64(1) << s;

    Bitboard pawnAtks = board->byPiece[PIECE_PAWN] & ((board->byColor[COLOR_BLACK] & pawnAttacks(sBB, COLOR_WHITE)) | (board->byColor[COLOR_WHITE] & pawnAttacks(sBB, COLOR_BLACK)));
    Bitboard knightAtks = board->byPiece[PIECE_KNIGHT] & knightAttacks(sBB);
    Bitboard bishopAtks = (board->byPiece[PIECE_BISHOP] | board->byPiece[PIECE_QUEEN]) & getBishopMoves(s, occupied);
    Bitboard rookAtks = (board->byPiece[PIECE_ROOK] | board->byPiece[PIECE_QUEEN]) & getRookMoves(s, occupied);
    Bitboard kingAtks = board->byPiece[PIECE_KING] & KING_ATTACKS[s];
    return pawnAtks | knightAtks | bishopAtks | rookAtks | kingAtks;
}

Bitboard knightAttacksAll(Board* board, Color side) {
    Bitboard knights = board->byPiece[PIECE_KNIGHT] & board->byColor[side];
    return knightAttacks(knights);
}

Bitboard kingAttacks(Board* board, Color color) {
    assert((board->byColor[color] & board->byPiece[PIECE_KING]) > 0);

    Square origin = lsb(board->byPiece[PIECE_KING] & board->byColor[color]);
    return KING_ATTACKS[origin];
}

Bitboard slidingPieceAttacksAll(Board* board, Color side, Piece pieceType) {
    Bitboard attacksBB = C64(0);

    Bitboard pieces = board->byPiece[pieceType] & board->byColor[side];
    while (pieces) {
        Square pieceSquare = popLSB(&pieces);
        if (pieceType == PIECE_BISHOP || pieceType == PIECE_QUEEN)
            attacksBB |= getBishopMoves(pieceSquare, board->byColor[COLOR_WHITE] | board->byColor[COLOR_BLACK]);
        if (pieceType == PIECE_ROOK || pieceType == PIECE_QUEEN)
            attacksBB |= getRookMoves(pieceSquare, board->byColor[COLOR_WHITE] | board->byColor[COLOR_BLACK]);
    }
    return attacksBB;
}

Bitboard attackedSquares(Board* board, Color side) {
    return
        pawnAttacks(board, side) |
        knightAttacksAll(board, side) |
        kingAttacks(board, side) |
        slidingPieceAttacksAll(board, side, PIECE_BISHOP) |
        slidingPieceAttacksAll(board, side, PIECE_ROOK) |
        slidingPieceAttacksAll(board, side, PIECE_QUEEN);
}

Bitboard attackedSquaresByPiece(Board* board, Color side, Piece pieceType) {
    switch (pieceType) {
    case PIECE_PAWN:
        return pawnAttacks(board, side);
    case PIECE_KNIGHT:
        return knightAttacksAll(board, side);
    case PIECE_KING:
        return kingAttacks(board, side);
    case PIECE_BISHOP:
        return slidingPieceAttacksAll(board, side, PIECE_BISHOP);
    case PIECE_ROOK:
        return slidingPieceAttacksAll(board, side, PIECE_ROOK);
    default:
        return slidingPieceAttacksAll(board, side, PIECE_QUEEN);
    }
}

Bitboard attackedSquaresByPiece(Piece pieceType, Square square, Bitboard occupied, Color stm) {
    switch (pieceType) {
    case PIECE_PAWN:
        return pawnAttacks(C64(1) << square, stm);
    case PIECE_KNIGHT:
        return knightAttacks(C64(1) << square);
    case PIECE_KING:
        return KING_ATTACKS[square];
    case PIECE_BISHOP:
        return getBishopMoves(square, occupied);
    case PIECE_ROOK:
        return getRookMoves(square, occupied);
    default:
        return getBishopMoves(square, occupied) | getRookMoves(square, occupied);
    }
}

void debugBoard(Board* board) {
    for (int rank = 7; rank >= 0; rank--) {

        printf("-");
        for (int file = 0; file <= 15; file++) {
            printf(" -");
        }
        printf("\n");

        for (int file = 0; file <= 7; file++) {

            // Get piece at index
            int idx = file + 8 * rank;
            Bitboard mask = C64(1) << idx;
            if ((board->stack->enpassantTarget & mask) != 0)
                printf("| E ");
            else if (((board->byColor[COLOR_WHITE] | board->byColor[COLOR_BLACK]) & mask) == 0)
                printf("|   ");
            else if ((board->byColor[COLOR_WHITE] & board->byPiece[PIECE_PAWN] & mask) != 0)
                printf("| ♙ ");
            else if ((board->byColor[COLOR_BLACK] & board->byPiece[PIECE_PAWN] & mask) != 0)
                printf("| ♟︎ ");
            else if ((board->byColor[COLOR_WHITE] & board->byPiece[PIECE_KNIGHT] & mask) != 0)
                printf("| ♘ ");
            else if ((board->byColor[COLOR_BLACK] & board->byPiece[PIECE_KNIGHT] & mask) != 0)
                printf("| ♞ ");
            else if ((board->byColor[COLOR_WHITE] & board->byPiece[PIECE_BISHOP] & mask) != 0)
                printf("| ♗ ");
            else if ((board->byColor[COLOR_BLACK] & board->byPiece[PIECE_BISHOP] & mask) != 0)
                printf("| ♝ ");
            else if ((board->byColor[COLOR_WHITE] & board->byPiece[PIECE_ROOK] & mask) != 0)
                printf("| ♖ ");
            else if ((board->byColor[COLOR_BLACK] & board->byPiece[PIECE_ROOK] & mask) != 0)
                printf("| ♜ ");
            else if ((board->byColor[COLOR_WHITE] & board->byPiece[PIECE_QUEEN] & mask) != 0)
                printf("| ♕ ");
            else if ((board->byColor[COLOR_BLACK] & board->byPiece[PIECE_QUEEN] & mask) != 0)
                printf("| ♛ ");
            else if ((board->byColor[COLOR_WHITE] & board->byPiece[PIECE_KING] & mask) != 0)
                printf("| ♔ ");
            else if ((board->byColor[COLOR_BLACK] & board->byPiece[PIECE_KING] & mask) != 0)
                printf("| ♚ ");
            else
                printf("| ? ");
        }
        printf("|\n");
    }
    printf("-");
    for (int file = 0; file <= 15; file++) {
        printf(" -");
    }
    printf("\n%" PRIu64 "\n", board->stack->hash);
}

int validateBoard(Board* board) {
    for (int rank = 7; rank >= 0; rank--) {
        for (int file = 0; file <= 7; file++) {

            // Get piece at index
            int idx = file + 8 * rank;
            Bitboard mask = C64(1) << idx;
            int first = 0;
            int second = 0;
            if ((board->stack->enpassantTarget & mask) != 0)
                first = 1;
            else if (board->pieces[idx] == NO_PIECE)
                first = 2;
            else if (board->pieces[idx] == PIECE_PAWN && (board->byColor[COLOR_WHITE] & mask) != 0)
                first = 3;
            else if (board->pieces[idx] == PIECE_PAWN && (board->byColor[COLOR_BLACK] & mask) != 0)
                first = 4;
            else if (board->pieces[idx] == PIECE_KNIGHT && (board->byColor[COLOR_WHITE] & mask) != 0)
                first = 5;
            else if (board->pieces[idx] == PIECE_KNIGHT && (board->byColor[COLOR_BLACK] & mask) != 0)
                first = 6;
            else if (board->pieces[idx] == PIECE_BISHOP && (board->byColor[COLOR_WHITE] & mask) != 0)
                first = 7;
            else if (board->pieces[idx] == PIECE_BISHOP && (board->byColor[COLOR_BLACK] & mask) != 0)
                first = 8;
            else if (board->pieces[idx] == PIECE_ROOK && (board->byColor[COLOR_WHITE] & mask) != 0)
                first = 9;
            else if (board->pieces[idx] == PIECE_ROOK && (board->byColor[COLOR_BLACK] & mask) != 0)
                first = 10;
            else if (board->pieces[idx] == PIECE_QUEEN && (board->byColor[COLOR_WHITE] & mask) != 0)
                first = 11;
            else if (board->pieces[idx] == PIECE_QUEEN && (board->byColor[COLOR_BLACK] & mask) != 0)
                first = 12;
            else if (board->pieces[idx] == PIECE_KING && (board->byColor[COLOR_WHITE] & mask) != 0)
                first = 13;
            else if (board->pieces[idx] == PIECE_KING && (board->byColor[COLOR_BLACK] & mask) != 0)
                first = 14;

            if ((board->stack->enpassantTarget & mask) != 0)
                second = 1;
            else if (((board->byColor[COLOR_WHITE] | board->byColor[COLOR_BLACK]) & mask) == 0)
                second = 2;
            else if ((board->byColor[COLOR_WHITE] & board->byPiece[PIECE_PAWN] & mask) != 0)
                second = 3;
            else if ((board->byColor[COLOR_BLACK] & board->byPiece[PIECE_PAWN] & mask) != 0)
                second = 4;
            else if ((board->byColor[COLOR_WHITE] & board->byPiece[PIECE_KNIGHT] & mask) != 0)
                second = 5;
            else if ((board->byColor[COLOR_BLACK] & board->byPiece[PIECE_KNIGHT] & mask) != 0)
                second = 6;
            else if ((board->byColor[COLOR_WHITE] & board->byPiece[PIECE_BISHOP] & mask) != 0)
                second = 7;
            else if ((board->byColor[COLOR_BLACK] & board->byPiece[PIECE_BISHOP] & mask) != 0)
                second = 8;
            else if ((board->byColor[COLOR_WHITE] & board->byPiece[PIECE_ROOK] & mask) != 0)
                second = 9;
            else if ((board->byColor[COLOR_BLACK] & board->byPiece[PIECE_ROOK] & mask) != 0)
                second = 10;
            else if ((board->byColor[COLOR_WHITE] & board->byPiece[PIECE_QUEEN] & mask) != 0)
                second = 11;
            else if ((board->byColor[COLOR_BLACK] & board->byPiece[PIECE_QUEEN] & mask) != 0)
                second = 12;
            else if ((board->byColor[COLOR_WHITE] & board->byPiece[PIECE_KING] & mask) != 0)
                second = 13;
            else if ((board->byColor[COLOR_BLACK] & board->byPiece[PIECE_KING] & mask) != 0)
                second = 14;

            if (first != second)
                return idx;
        }
    }
    return -1;
}

void debugBitboard(Bitboard bb) {
    for (int rank = 7; rank >= 0; rank--) {

        printf("-");
        for (int file = 0; file <= 15; file++) {
            printf(" -");
        }
        printf("\n");

        for (int file = 0; file <= 7; file++) {

            // Get piece at index
            int idx = file + 8 * rank;
            Bitboard mask = C64(1) << idx;
            if ((bb & mask) == 0)
                printf("|   ");
            else
                printf("| X ");
        }
        printf("|\n");
    }
    printf("-");
    for (int file = 0; file <= 15; file++) {
        printf(" -");
    }
    printf("\n");
}