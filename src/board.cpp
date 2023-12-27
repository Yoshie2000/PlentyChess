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
        board->stack->psq[c][PHASE_MG] = 0;
        board->stack->psq[c][PHASE_EG] = 0;
        for (Piece p = 0; p < PIECE_TYPES; p++) {
            board->stack->pieceCount[c][p] = 0;
            board->byPiece[p] = C64(0);
        }
    }
    board->stack->checkers = C64(0);
    board->stack->capturedPiece = NO_PIECE;
    board->stack->hash = 0;
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
            board->stack->hash ^= ZOBRIST_PIECE_SQUARES[PIECE_PAWN][currentSquare];
            currentSquare++;
            break;
        case 'P':
            board->byColor[COLOR_WHITE] |= currentSquareBB;
            board->byPiece[PIECE_PAWN] |= currentSquareBB;
            board->stack->pieceCount[COLOR_WHITE][PIECE_PAWN]++;
            board->pieces[currentSquare] = PIECE_PAWN;
            board->stack->hash ^= ZOBRIST_PIECE_SQUARES[PIECE_PAWN][currentSquare];
            currentSquare++;
            break;
        case 'n':
            board->byColor[COLOR_BLACK] |= currentSquareBB;
            board->byPiece[PIECE_KNIGHT] |= currentSquareBB;
            board->stack->pieceCount[COLOR_BLACK][PIECE_KNIGHT]++;
            board->pieces[currentSquare] = PIECE_KNIGHT;
            board->stack->hash ^= ZOBRIST_PIECE_SQUARES[PIECE_KNIGHT][currentSquare];
            currentSquare++;
            break;
        case 'N':
            board->byColor[COLOR_WHITE] |= currentSquareBB;
            board->byPiece[PIECE_KNIGHT] |= currentSquareBB;
            board->stack->pieceCount[COLOR_WHITE][PIECE_KNIGHT]++;
            board->pieces[currentSquare] = PIECE_KNIGHT;
            board->stack->hash ^= ZOBRIST_PIECE_SQUARES[PIECE_KNIGHT][currentSquare];
            currentSquare++;
            break;
        case 'b':
            board->byColor[COLOR_BLACK] |= currentSquareBB;
            board->byPiece[PIECE_BISHOP] |= currentSquareBB;
            board->stack->pieceCount[COLOR_BLACK][PIECE_BISHOP]++;
            board->pieces[currentSquare] = PIECE_BISHOP;
            board->stack->hash ^= ZOBRIST_PIECE_SQUARES[PIECE_BISHOP][currentSquare];
            currentSquare++;
            break;
        case 'B':
            board->byColor[COLOR_WHITE] |= currentSquareBB;
            board->byPiece[PIECE_BISHOP] |= currentSquareBB;
            board->stack->pieceCount[COLOR_WHITE][PIECE_BISHOP]++;
            board->pieces[currentSquare] = PIECE_BISHOP;
            board->stack->hash ^= ZOBRIST_PIECE_SQUARES[PIECE_BISHOP][currentSquare];
            currentSquare++;
            break;
        case 'r':
            board->byColor[COLOR_BLACK] |= currentSquareBB;
            board->byPiece[PIECE_ROOK] |= currentSquareBB;
            board->stack->pieceCount[COLOR_BLACK][PIECE_ROOK]++;
            board->pieces[currentSquare] = PIECE_ROOK;
            board->stack->hash ^= ZOBRIST_PIECE_SQUARES[PIECE_ROOK][currentSquare];
            currentSquare++;
            break;
        case 'R':
            board->byColor[COLOR_WHITE] |= currentSquareBB;
            board->byPiece[PIECE_ROOK] |= currentSquareBB;
            board->stack->pieceCount[COLOR_WHITE][PIECE_ROOK]++;
            board->pieces[currentSquare] = PIECE_ROOK;
            board->stack->hash ^= ZOBRIST_PIECE_SQUARES[PIECE_ROOK][currentSquare];
            currentSquare++;
            break;
        case 'q':
            board->byColor[COLOR_BLACK] |= currentSquareBB;
            board->byPiece[PIECE_QUEEN] |= currentSquareBB;
            board->stack->pieceCount[COLOR_BLACK][PIECE_QUEEN]++;
            board->pieces[currentSquare] = PIECE_QUEEN;
            board->stack->hash ^= ZOBRIST_PIECE_SQUARES[PIECE_QUEEN][currentSquare];
            currentSquare++;
            break;
        case 'Q':
            board->byColor[COLOR_WHITE] |= currentSquareBB;
            board->byPiece[PIECE_QUEEN] |= currentSquareBB;
            board->stack->pieceCount[COLOR_WHITE][PIECE_QUEEN]++;
            board->pieces[currentSquare] = PIECE_QUEEN;
            board->stack->hash ^= ZOBRIST_PIECE_SQUARES[PIECE_QUEEN][currentSquare];
            currentSquare++;
            break;
        case 'k':
            board->byColor[COLOR_BLACK] |= currentSquareBB;
            board->byPiece[PIECE_KING] |= currentSquareBB;
            board->stack->pieceCount[COLOR_BLACK][PIECE_KING]++;
            board->pieces[currentSquare] = PIECE_KING;
            board->stack->hash ^= ZOBRIST_PIECE_SQUARES[PIECE_KING][currentSquare];
            currentSquare++;
            break;
        case 'K':
            board->byColor[COLOR_WHITE] |= currentSquareBB;
            board->byPiece[PIECE_KING] |= currentSquareBB;
            board->stack->pieceCount[COLOR_WHITE][PIECE_KING]++;
            board->pieces[currentSquare] = PIECE_KING;
            board->stack->hash ^= ZOBRIST_PIECE_SQUARES[PIECE_KING][currentSquare];
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
            printf("Weird char in fen castling: %c (index %ld)\n", c, i);
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

        board->stack->hash ^= ZOBRIST_ENPASSENT[epTargetSquare % 8];
    }

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

    // Update king checking stuff
    Square enemyKing = lsb(board->byColor[board->stm] & board->byPiece[PIECE_KING]);
    board->stack->checkers = attackersTo(board, enemyKing, board->byColor[COLOR_WHITE] | board->byColor[COLOR_BLACK]) & board->byColor[1 - board->stm];
    board->stack->checkerCount = __builtin_popcountll(board->stack->checkers);

    updateSliderPins(board, COLOR_WHITE);
    updateSliderPins(board, COLOR_BLACK);

    // Set up piece values
    for (Square s = 0; s < 64; s++) {
        Piece p = board->pieces[s];
        if (p != NO_PIECE) {
            Color pieceColor = ((C64(1) << s) & board->byColor[COLOR_WHITE]) ? COLOR_WHITE : COLOR_BLACK;
            board->stack->psq[pieceColor][PHASE_MG] += PSQ[PIECE_PAWN][psqIndex(s, pieceColor)].mg;
            board->stack->psq[pieceColor][PHASE_EG] += PSQ[PIECE_PAWN][psqIndex(s, pieceColor)].eg;
        }
    }

    return i;
}

void castlingRookSquares(Board* board, Square origin, Square target, Square* rookOrigin, Square* rookTarget) {
    if (board->stm == COLOR_WHITE) {
        if (target > origin) { // Kingside White
            *rookOrigin = 7;
            *rookTarget = 5;
        }
        else { // Queenside White
            *rookOrigin = 0;
            *rookTarget = 3;
        }
        board->stack->castling &= 0xC; // Clear flags for white
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
        board->stack->castling &= 0x3; // Clear flags for black
    }
}

void doMove(Board* board, BoardStack* newStack, Move move) {
    newStack->previous = board->stack;
    board->stack = newStack;
    memcpy(newStack->pieceCount, newStack->previous->pieceCount, sizeof(int) * 12 + sizeof(Eval) * 4 + sizeof(uint8_t));

    newStack->hash = newStack->previous->hash ^ ZOBRIST_STM_BLACK;
    newStack->rule50_ply = newStack->previous->rule50_ply + 1;
    newStack->nullmove_ply = newStack->previous->nullmove_ply + 1;

    if (move == MOVE_NULL)
        newStack->nullmove_ply = 0;

    Square origin = moveOrigin(move);
    Square target = moveTarget(move);

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

    if (piece == PIECE_PAWN)
        newStack->rule50_ply = 0;

    // This move is en passent
    Move specialMove = move & 0x3000;
    if (specialMove == MOVE_ENPASSANT) {
        newStack->capturedPiece = PIECE_PAWN;
        captureTarget = target - UP[board->stm];

        assert(captureTarget < 64);

        captureTargetBB = C64(1) << captureTarget;
        board->pieces[captureTarget] = NO_PIECE; // remove the captured pawn
    }

    // Handle capture
    if (newStack->capturedPiece != NO_PIECE) {
        board->byColor[1 - board->stm] ^= captureTargetBB; // take away the captured piece
        board->byPiece[newStack->capturedPiece] ^= captureTargetBB;

        newStack->hash ^= ZOBRIST_PIECE_SQUARES[newStack->capturedPiece][captureTarget];
        newStack->psq[1 - board->stm][PHASE_MG] -= PSQ[newStack->capturedPiece][psqIndex(captureTarget, 1 - board->stm)].mg;
        newStack->psq[1 - board->stm][PHASE_EG] -= PSQ[newStack->capturedPiece][psqIndex(captureTarget, 1 - board->stm)].eg;

        newStack->pieceCount[1 - board->stm][newStack->capturedPiece]--;
        newStack->rule50_ply = 0;
    }

    // En passent square
    if (newStack->previous->enpassantTarget != 0) {
        newStack->hash ^= ZOBRIST_ENPASSENT[lsb(newStack->previous->enpassantTarget) % 8];
    }
    newStack->enpassantTarget = 0;
    if (piece == PIECE_PAWN && (origin ^ target) == 16) {
        assert(target - UP[board->stm] < 64);
        newStack->enpassantTarget = C64(1) << (target - UP[board->stm]);
        newStack->hash ^= ZOBRIST_ENPASSENT[origin % 8];
    }

    // This move is castling
    if (specialMove == MOVE_CASTLING) {
        Square rookOrigin, rookTarget;
        newStack->hash ^= ZOBRIST_CASTLING[newStack->castling & 0xF];
        castlingRookSquares(board, origin, target, &rookOrigin, &rookTarget);
        newStack->hash ^= ZOBRIST_CASTLING[newStack->castling & 0xF];
        assert(rookOrigin < 64 && rookTarget < 64);

        Bitboard rookFromToBB = (C64(1) << rookOrigin) | (C64(1) << rookTarget);
        board->pieces[rookOrigin] = NO_PIECE;
        board->pieces[rookTarget] = PIECE_ROOK;
        board->byColor[board->stm] ^= rookFromToBB;
        board->byPiece[PIECE_ROOK] ^= rookFromToBB;

        newStack->hash ^= ZOBRIST_PIECE_SQUARES[PIECE_ROOK][rookOrigin] ^ ZOBRIST_PIECE_SQUARES[PIECE_ROOK][rookTarget];
        newStack->psq[board->stm][PHASE_MG] -= PSQ[PIECE_ROOK][psqIndex(rookOrigin, board->stm)].mg;
        newStack->psq[board->stm][PHASE_EG] -= PSQ[PIECE_ROOK][psqIndex(rookOrigin, board->stm)].eg;
        newStack->psq[board->stm][PHASE_MG] += PSQ[PIECE_ROOK][psqIndex(rookTarget, board->stm)].mg;
        newStack->psq[board->stm][PHASE_EG] += PSQ[PIECE_ROOK][psqIndex(rookTarget, board->stm)].eg;
    }

    // This move is promotion
    if (specialMove == MOVE_PROMOTION) {
        promotionPiece = PROMOTION_PIECE[move >> 14];
        board->byPiece[piece] ^= targetBB;
        board->byPiece[promotionPiece] ^= targetBB;

        board->pieces[target] = promotionPiece;

        newStack->hash ^= ZOBRIST_PIECE_SQUARES[piece][target] ^ ZOBRIST_PIECE_SQUARES[promotionPiece][target];

        newStack->psq[board->stm][PHASE_MG] -= PSQ[piece][psqIndex(target, board->stm)].mg;
        newStack->psq[board->stm][PHASE_EG] -= PSQ[piece][psqIndex(target, board->stm)].eg;
        newStack->psq[board->stm][PHASE_MG] += PSQ[promotionPiece][psqIndex(target, board->stm)].mg;
        newStack->psq[board->stm][PHASE_EG] += PSQ[promotionPiece][psqIndex(target, board->stm)].eg;

        newStack->pieceCount[board->stm][PIECE_PAWN]--;
        newStack->pieceCount[board->stm][promotionPiece]++;
        newStack->rule50_ply = 0;
    }

    newStack->hash ^= ZOBRIST_PIECE_SQUARES[piece][origin] ^ ZOBRIST_PIECE_SQUARES[piece][target];

    newStack->psq[board->stm][PHASE_MG] -= PSQ[piece][psqIndex(origin, board->stm)].mg;
    newStack->psq[board->stm][PHASE_EG] -= PSQ[piece][psqIndex(origin, board->stm)].eg;
    newStack->psq[board->stm][PHASE_MG] += PSQ[piece][psqIndex(target, board->stm)].mg;
    newStack->psq[board->stm][PHASE_EG] += PSQ[piece][psqIndex(target, board->stm)].eg;

    // Unset castling flags if necessary
    if (piece == PIECE_KING) {
        newStack->hash ^= ZOBRIST_CASTLING[newStack->castling & 0xF];
        if (board->stm == COLOR_WHITE)
            newStack->castling &= 0xC;
        else
            newStack->castling &= 0x3;
        newStack->hash ^= ZOBRIST_CASTLING[newStack->castling & 0xF];
    }
    else if (piece == PIECE_ROOK || newStack->capturedPiece == PIECE_ROOK) {
        newStack->hash ^= ZOBRIST_CASTLING[newStack->castling & 0xF];
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
        newStack->hash ^= ZOBRIST_CASTLING[newStack->castling & 0xF];
    }

    // Update king checking stuff
    assert((board->byColor[1 - board->stm] & board->byPiece[PIECE_KING]) > 0);

    Square enemyKing = lsb(board->byColor[1 - board->stm] & board->byPiece[PIECE_KING]);
    newStack->checkers = attackersTo(board, enemyKing, board->byColor[COLOR_WHITE] | board->byColor[COLOR_BLACK]) & board->byColor[board->stm];
    newStack->checkerCount = newStack->checkers ? __builtin_popcountll(newStack->checkers) : 0; // TODO: givesCheck(move) implementation
    updateSliderPins(board, COLOR_WHITE);
    updateSliderPins(board, COLOR_BLACK);

    // Calculate repetition information
    newStack->repetition = 0;
    int end = std::min(newStack->rule50_ply - 1, newStack->nullmove_ply - 1);
    if (end >= 4) {
        BoardStack* st = newStack;
        for (int i = 2; i <= end; i += 2) {
            st = st->previous->previous;
            if (newStack->hash == st->hash) {
                newStack->repetition = st->repetition ? -i : i;
                break;
            }
        }
    }

    board->stm = 1 - board->stm;
    newStack->move = move;
}

void undoMove(Board* board, Move move) {
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
        castlingRookSquares(board, origin, target, &rookOrigin, &rookTarget);

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

    board->stack = board->stack->previous;
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

// Check for any repetition since the last capture / pawn move
bool hasRepeated(Board* board) {
    BoardStack* stack = board->stack;
    int end = std::min(board->stack->rule50_ply, board->stack->nullmove_ply);
    for (; end >= 4; end--) {
        if (stack->repetition)
            return true;
        stack = stack->previous;
    }
    return false;
}

// Check for any repetition since the last capture / pawn move
bool isDraw(Board* board, int ply) {
    if (board->stack->rule50_ply > 99)
        return true;

    return board->stack->repetition && board->stack->repetition < ply;
}

Bitboard attackersTo(Board* board, Square s, Bitboard occupied) {
    Bitboard sBB = C64(1) << s;

    Bitboard pawnAtks = ((pawnAttacks(sBB, COLOR_WHITE) & board->byColor[COLOR_BLACK]) | (pawnAttacks(sBB, COLOR_BLACK) & board->byColor[COLOR_WHITE])) & board->byPiece[PIECE_PAWN];
    Bitboard knightAtks = knightAttacks(sBB) & board->byPiece[PIECE_KNIGHT];
    Bitboard bishopAtks = getBishopMoves(s, occupied) & (board->byPiece[PIECE_BISHOP] | board->byPiece[PIECE_QUEEN]);
    Bitboard rookAtks = getRookMoves(s, occupied) & (board->byPiece[PIECE_ROOK] | board->byPiece[PIECE_QUEEN]);
    Bitboard kingAtks = kingAttacks(s) & board->byPiece[PIECE_KING];
    return pawnAtks | knightAtks | bishopAtks | rookAtks | kingAtks;
}

Bitboard pawnAttacksLeft(Bitboard pawns, Color side) {
    return side == COLOR_WHITE ?
        ((pawns & (~FILE_A)) << 7) :
        ((pawns & (~FILE_A)) >> 9);
}

Bitboard pawnAttacksRight(Bitboard pawns, Color side) {
    return side == COLOR_WHITE ?
        ((pawns & (~FILE_H)) << 9) :
        ((pawns & (~FILE_H)) >> 7);
}

Bitboard pawnAttacks(Bitboard pawns, Color side) {
    return pawnAttacksLeft(pawns, side) | pawnAttacksRight(pawns, side);
}

Bitboard pawnAttacks(Board* board, Color side) {
    Bitboard pawns = board->byPiece[PIECE_PAWN] & board->byColor[side];
    return pawnAttacks(pawns, side);
}

Bitboard knightAttacksAll(Board* board, Color side) {
    Bitboard knights = board->byPiece[PIECE_KNIGHT] & board->byColor[side];
    return knightAttacks(knights);
}

Bitboard kingAttacks(Board* board, Color color) {
    assert((board->byColor[color] & board->byPiece[PIECE_KING]) > 0);

    Square origin = lsb(board->byPiece[PIECE_KING] & board->byColor[color]);
    return kingAttacks(origin);
}

Bitboard kingAttacks(Square origin) {
    Bitboard attacksBB = C64(0);

    int8_t direction;
    Square lastSquare, toSquare;
    Bitboard toSquareBB;

    for (direction = DIRECTIONS[PIECE_KING][0]; direction <= DIRECTIONS[PIECE_KING][1]; direction++) {
        lastSquare = LASTSQ_TABLE[origin][direction];
        toSquare = origin + DIRECTION_DELTAS[direction];
        if (toSquare >= 64) continue;

        toSquareBB = C64(1) << toSquare;
        if (origin != lastSquare && toSquareBB)
            attacksBB |= toSquareBB;
    }
    return attacksBB;
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

// This function is only intended for pieces that can be promoted to (not kings & pawns)
Bitboard attackedSquaresByPiece(Piece pieceType, Square square, Bitboard occupied) {
    switch (pieceType) {
    case PIECE_PAWN:
        return C64(0);
    case PIECE_KNIGHT:
        return knightAttacks(C64(1) << square);
    case PIECE_KING:
        return C64(0);
    case PIECE_BISHOP:
        return getBishopMoves(square, occupied);
    case PIECE_ROOK:
        return getRookMoves(square, occupied);
    default:
        return getBishopMoves(square, occupied) | getRookMoves(square, occupied);
    }
}

bool isInCheck(Board* board, Color side) {
    assert((board->byPiece[PIECE_KING] & board->byColor[side]) > 0);

    Square kingSquare = lsb(board->byPiece[PIECE_KING] & board->byColor[side]);
    return attackersTo(board, kingSquare, board->byColor[COLOR_WHITE] | board->byColor[COLOR_BLACK]) > 0;
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
            Bitboard mask = 1L << idx;
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