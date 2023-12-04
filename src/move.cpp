#include <inttypes.h>
#include <stdio.h>

#include "move.h"
#include "board.h"
#include "types.h"
#include "magic.h"
#include <cassert>
#include <iostream>
#include "evaluation.h"

constexpr Move createMove(Square origin, Square target) {
    return (Move)((origin & 0x3F) | ((target & 0x3F) << 6));
}

bool isValid(Board* board, Move move) {
    Square origin = moveOrigin(move);
    Square target = moveTarget(move);
    if (board->pieces[origin] == NO_PIECE) return false;
    if (board->byColor[board->stm] & (C64(1) << target)) return false;
    if (board->byColor[1 - board->stm] & (C64(1) << origin)) return false;
    return true;
}

void generatePawn_quiet(Board* board, Move** moves, int* counter) {
    Bitboard pawns = board->byPiece[board->stm][PIECE_PAWN];
    Bitboard free = ~board->board;

    Bitboard pushedPawns, secondRankPawns, doublePushedPawns, enemyBackrank;
    if (board->stm == COLOR_WHITE) {
        pushedPawns = (pawns << 8) & free;
        secondRankPawns = (pawns & RANK_2) & pawns;
        doublePushedPawns = (((secondRankPawns << 8) & free) << 8) & free;
        enemyBackrank = RANK_8;
    }
    else {
        pushedPawns = (pawns >> 8) & free;
        secondRankPawns = (pawns & RANK_7) & pawns;
        doublePushedPawns = (((secondRankPawns >> 8) & free) >> 8) & free;
        enemyBackrank = RANK_1;
    }

    while (pushedPawns) {
        Square target = popLSB(&pushedPawns);
        Move move = createMove(target - UP[board->stm], target);

        if ((C64(1) << target) & enemyBackrank) {
            // Promotion
            for (uint8_t promotion = 0; promotion <= 3; promotion++) {
                *(*moves)++ = move | (promotion << 14) | MOVE_PROMOTION;
                (*counter)++;
            }
            continue;
        }

        *(*moves)++ = move;
        (*counter)++;
    }

    // Double pushes
    while (doublePushedPawns) {
        Square target = popLSB(&doublePushedPawns);
        *(*moves)++ = createMove(target - UP_DOUBLE[board->stm], target);
        (*counter)++;
    }
}

void generatePawn_capture(Board* board, Move** moves, int* counter) {
    // Captures
    Bitboard pAttacksLeft = pawnAttacksLeft(board, board->stm);
    Bitboard pAttacksRight = pawnAttacksRight(board, board->stm);
    Bitboard blockedEnemy = board->byColor[1 - board->stm];
    Bitboard enemyBackrank = board->stm == COLOR_WHITE ? RANK_8 : RANK_1;

    Bitboard leftCaptures = pAttacksLeft & blockedEnemy;
    while (leftCaptures) {
        Square target = popLSB(&leftCaptures);
        Move move = createMove(target - UP_LEFT[board->stm], target);

        if (__builtin_expect((C64(1) << target) & enemyBackrank, 0)) {
            // Promotion
            for (uint8_t promotion = 0; promotion <= 3; promotion++) {
                *(*moves)++ = move | (promotion << 14) | MOVE_PROMOTION;
                (*counter)++;
            }
            continue;
        }

        *(*moves)++ = move;
        (*counter)++;
    }
    Bitboard rightCaptures = pAttacksRight & blockedEnemy;
    while (rightCaptures) {
        Square target = popLSB(&rightCaptures);
        Move move = createMove(target - UP_RIGHT[board->stm], target);

        if ((C64(1) << target) & enemyBackrank) {
            // Promotion
            for (uint8_t promotion = 0; promotion <= 3; promotion++) {
                *(*moves)++ = move | (promotion << 14) | MOVE_PROMOTION;
                (*counter)++;
            }
            continue;
        }

        *(*moves)++ = move;
        (*counter)++;
    }

    // En passent
    Bitboard epBB = board->stack->enpassantTarget;
    Bitboard leftEp = pAttacksLeft & epBB;
    if (leftEp) {
        Square target = popLSB(&leftEp);
        *(*moves)++ = createMove(target - UP_LEFT[board->stm], target) | MOVE_ENPASSANT;
        (*counter)++;
    }
    Bitboard rightEp = pAttacksRight & epBB;
    if (rightEp) {
        Square target = popLSB(&rightEp);
        *(*moves)++ = createMove(target - UP_RIGHT[board->stm], target) | MOVE_ENPASSANT;
        (*counter)++;
    }
}

// For all pieces other than pawns
template <Piece pieceType>
void generatePiece(Board* board, Move** moves, int* counter, bool captures) {
    Bitboard blockedUs = board->byColor[board->stm];
    Bitboard blockedEnemy = board->byColor[1 - board->stm];
    Bitboard attackedEnemy = board->stack->attackedByColor[1 - board->stm];

    Bitboard pieces = board->byPiece[board->stm][pieceType];
    while (pieces) {
        Square piece = popLSB(&pieces);
        Bitboard targets = pieceType == PIECE_KNIGHT ? knightAttacks(C64(1) << piece) : pieceType == PIECE_BISHOP ? getBishopMoves(piece, board->board) : pieceType == PIECE_ROOK ? getRookMoves(piece, board->board) : pieceType == PIECE_QUEEN ? (getRookMoves(piece, board->board) | getBishopMoves(piece, board->board)) : pieceType == PIECE_KING ? kingAttacks(board, board->stm) & ~attackedEnemy : C64(0);
        // Decide whether only captures or only non-captures
        if (captures)
            targets &= blockedEnemy & ~blockedUs;
        else
            targets &= ~blockedEnemy & ~blockedUs;

        while (targets) {
            Square target = popLSB(&targets);
            *(*moves)++ = createMove(piece, target);
            (*counter)++;
        }
    }
}

void generateCastling(Board* board, Move** moves, int* counter) {
    Square king = lsb(board->byPiece[board->stm][PIECE_KING]);

    // Castling: Nothing on the squares between king and rook, also nothing from the enemy attacking it
    switch (board->stm) {
    case COLOR_WHITE:
        // Kingside
        if ((board->stack->castling & 0x1) && !(board->board & 0x60) && !(board->stack->attackedByColor[COLOR_BLACK] & 0x60)) {
            *(*moves)++ = createMove(king, king + 2) | MOVE_CASTLING;
            (*counter)++;
        }
        // Queenside
        if ((board->stack->castling & 0x2) && !(board->board & 0x0E) && !(board->stack->attackedByColor[COLOR_BLACK] & 0x0C)) {
            *(*moves)++ = createMove(king, king - 2) | MOVE_CASTLING;
            (*counter)++;
        }
        break;
    case COLOR_BLACK:
        // Kingside
        if ((board->stack->castling & 0x4) && !(board->board & C64(0x6000000000000000)) && !(board->stack->attackedByColor[COLOR_WHITE] & C64(0x6000000000000000))) {
            *(*moves)++ = createMove(king, king + 2) | MOVE_CASTLING;
            (*counter)++;
        }
        // Queenside
        if ((board->stack->castling & 0x8) && !(board->board & C64(0x0E00000000000000)) && !(board->stack->attackedByColor[COLOR_WHITE] & C64(0x0C00000000000000))) {
            *(*moves)++ = createMove(king, king - 2) | MOVE_CASTLING;
            (*counter)++;
        }
        break;
    }
}

void generateMoves(Board* board, Move* moves, int* counter, bool onlyCaptures) {
    // ------------------- CAPTURES ----------------------
    generatePawn_capture(board, &moves, counter);
    generatePiece<PIECE_KNIGHT>(board, &moves, counter, true);
    generatePiece<PIECE_BISHOP>(board, &moves, counter, true);
    generatePiece<PIECE_ROOK>(board, &moves, counter, true);
    generatePiece<PIECE_QUEEN>(board, &moves, counter, true);
    generatePiece<PIECE_KING>(board, &moves, counter, true);

    // ------------------- NON CAPTURES ----------------------
    if (!onlyCaptures) {
        generatePawn_quiet(board, &moves, counter);
        generatePiece<PIECE_KNIGHT>(board, &moves, counter, false);
        generatePiece<PIECE_BISHOP>(board, &moves, counter, false);
        generatePiece<PIECE_ROOK>(board, &moves, counter, false);
        generatePiece<PIECE_QUEEN>(board, &moves, counter, false);
        generatePiece<PIECE_KING>(board, &moves, counter, false);
    }

    // ------------------- CASTLING ----------------------
    if (!onlyCaptures && !isInCheck(board, board->stm)) {
        generateCastling(board, &moves, counter);
    }
}

Move MoveGen::nextMove() {
    // If there's still unused moves in the list, return those
    if (returnedMoves < generatedMoves) {
        assert(returnedMoves < MAX_MOVES && moveList[returnedMoves] != MOVE_NONE);

        return moveList[returnedMoves++];
    }

    Move* moves = moveList + generatedMoves;

    while (returnedMoves >= generatedMoves && generationStage <= GEN_STAGE_REMAINING) {

        // Generate moves for the current stage
        switch (generationStage) {
        case GEN_STAGE_TTMOVE:
            assert(ttMove != MOVE_NONE);

            moveList[generatedMoves++] = ttMove;
            generationStage++;
            break;

        case GEN_STAGE_CAPTURES: {
            int beginIndex = generatedMoves;
            generatePawn_capture(board, &moves, &generatedMoves);
            generatePiece<PIECE_KNIGHT>(board, &moves, &generatedMoves, true);
            generatePiece<PIECE_BISHOP>(board, &moves, &generatedMoves, true);
            generatePiece<PIECE_ROOK>(board, &moves, &generatedMoves, true);
            generatePiece<PIECE_QUEEN>(board, &moves, &generatedMoves, true);
            generatePiece<PIECE_KING>(board, &moves, &generatedMoves, true);
            int endIndex = generatedMoves;

            int scores[MAX_MOVES];
            for (int i = beginIndex; i < endIndex; i++) {
                Move move = moveList[i];
                int score = PIECE_VALUES[board->pieces[moveTarget(move)]] - PIECE_VALUES[board->pieces[moveOrigin(move)]];
                if ((move & 0x3000) == MOVE_ENPASSANT)
                    score = 0;
                scores[i] = score;
            }

            for (int i = beginIndex + 1; i < endIndex; i++) {
                int move = moveList[i];
                int score = scores[i];
                int j = i - 1;

                while (j >= 0 && scores[j] < score) {
                    moveList[j + 1] = moveList[j];
                    scores[j + 1] = scores[j];
                    j--;
                }

                moveList[j + 1] = move;
                scores[j + 1] = score;
            }

            generationStage++;
            if (onlyCaptures)
                generationStage = GEN_STAGE_REMAINING + 1;
        }
                               break;

        case GEN_STAGE_REMAINING:
            generatePawn_quiet(board, &moves, &generatedMoves);
            generatePiece<PIECE_KNIGHT>(board, &moves, &generatedMoves, false);
            generatePiece<PIECE_BISHOP>(board, &moves, &generatedMoves, false);
            generatePiece<PIECE_ROOK>(board, &moves, &generatedMoves, false);
            generatePiece<PIECE_QUEEN>(board, &moves, &generatedMoves, false);
            generatePiece<PIECE_KING>(board, &moves, &generatedMoves, false);
            if (!onlyCaptures && !isInCheck(board, board->stm)) {
                generateCastling(board, &moves, &generatedMoves);
            }
            generationStage++;
            break;
        }
    }

    return moveList[returnedMoves++];
}

inline char fileFromSquare(Square square) {
    int file = square % 8;
    if (file == 0)
        return 'a';
    else if (file == 1)
        return 'b';
    else if (file == 2)
        return 'c';
    else if (file == 3)
        return 'd';
    else if (file == 4)
        return 'e';
    else if (file == 5)
        return 'f';
    else if (file == 6)
        return 'g';
    else
        return 'h';
}

inline char rankFromSquare(Square square) {
    int rank = square / 8;
    if (rank == 0)
        return '1';
    else if (rank == 1)
        return '2';
    else if (rank == 2)
        return '3';
    else if (rank == 3)
        return '4';
    else if (rank == 4)
        return '5';
    else if (rank == 5)
        return '6';
    else if (rank == 6)
        return '7';
    else
        return '8';
}

std::string squareToString(Square square) {
    return std::string(1, fileFromSquare(square)) + std::string(1, rankFromSquare(square));
}

std::string moveToString(Move move) {
    std::string result = "";

    Square origin = moveOrigin(move);
    Square target = moveTarget(move);

    result += squareToString(origin);
    result += squareToString(target);

    if ((move & (0x3 << 12)) == MOVE_PROMOTION) {
        Move promotionType = move & (0x3) << 14;
        if (promotionType == PROMOTION_QUEEN)
            result += 'q';
        if (promotionType == PROMOTION_ROOK)
            result += 'r';
        if (promotionType == PROMOTION_BISHOP)
            result += 'b';
        if (promotionType == PROMOTION_KNIGHT)
            result += 'n';
    }
    return result;
}

inline int fileFromString(char string) {
    if (string == 'a')
        return 0;
    else if (string == 'b')
        return 1;
    else if (string == 'c')
        return 2;
    else if (string == 'd')
        return 3;
    else if (string == 'e')
        return 4;
    else if (string == 'f')
        return 5;
    else if (string == 'g')
        return 6;
    else
        return 7;
}

inline int rankFromString(char string) {
    if (string == '1')
        return 0;
    else if (string == '2')
        return 1;
    else if (string == '3')
        return 2;
    else if (string == '4')
        return 3;
    else if (string == '5')
        return 4;
    else if (string == '6')
        return 5;
    else if (string == '7')
        return 6;
    else
        return 7;
}

Square stringToSquare(char* string) {
    int file = fileFromString(string[0]);
    int rank = rankFromString(string[1]);
    return (Square)(8 * rank + file);
}

Move stringToMove(char* string, Board* board) {
    Square origin = stringToSquare(&string[0]);
    Square target = stringToSquare(&string[2]);
    Move move = createMove(origin, target);

    switch (string[4]) {
    case 'q':
        move |= MOVE_PROMOTION | PROMOTION_QUEEN;
        break;
    case 'r':
        move |= MOVE_PROMOTION | PROMOTION_ROOK;
        break;
    case 'b':
        move |= MOVE_PROMOTION | PROMOTION_BISHOP;
        break;
    case 'n':
        move |= MOVE_PROMOTION | PROMOTION_KNIGHT;
        break;
    }

    // Figure out whether this is en passent or castling and set the flags accordingly
    if (board != nullptr) {
        if (board->pieces[origin] == PIECE_KING && std::abs(target - origin) == 2)
            move |= MOVE_CASTLING;
        if (board->pieces[origin] == PIECE_PAWN && board->pieces[target] == NO_PIECE && (std::abs(target - origin) == 7 || std::abs(target - origin) == 9))
            move |= MOVE_ENPASSANT;
    }

    return move;
}

void generateLastSqTable() {
    // char string[4];

    for (Square square = 0; square < 64; square++) {

        // squareToString(string, square);
        // printf("%s\n", string);
        printf("{ ");

        for (uint8_t direction = 0; direction < 8; direction++) {
            int8_t delta = DIRECTION_DELTAS[direction];
            Square lastSquare = square;

            // printf("\t%d ", direction);

            // todo: find lastSquare

            bool leftDisallowed = (square % 8 == 0) && (direction == DIRECTION_LEFT || direction == DIRECTION_UP_LEFT || direction == DIRECTION_DOWN_LEFT);
            bool rightDisallowed = (square % 8 == 7) && (direction == DIRECTION_RIGHT || direction == DIRECTION_UP_RIGHT || direction == DIRECTION_DOWN_RIGHT);

            if (!leftDisallowed && !rightDisallowed)
                for (Square toSquare = square + delta; toSquare <= 63; toSquare += delta) {
                    lastSquare = toSquare;

                    if ((toSquare % 8 == 0) && (direction == DIRECTION_LEFT || direction == DIRECTION_UP_LEFT || direction == DIRECTION_DOWN_LEFT))
                        break;
                    if ((toSquare % 8 == 7) && (direction == DIRECTION_RIGHT || direction == DIRECTION_UP_RIGHT || direction == DIRECTION_DOWN_RIGHT))
                        break;
                }

            // squareToString(string, lastSquare);
            // printf("%s\n", string);
            if (direction < 7)
                printf("%d, ", lastSquare);
            else
                printf("%d ", lastSquare);

        }

        printf("},\n");

    }
}