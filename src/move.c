#include <inttypes.h>
#include <stdio.h>

#include "move.h"
#include "board.h"
#include "types.h"

const Move MOVE_NULL = 0;

const Move MOVE_PROMOTION = 1 << 12;
const Move MOVE_ENPASSANT = 2 << 12;
const Move MOVE_CASTLING = 3 << 12;

// Sliding piece stuff
const uint8_t DIRECTION_UP = 0;
const uint8_t DIRECTION_RIGHT = 1;
const uint8_t DIRECTION_DOWN = 2;
const uint8_t DIRECTION_LEFT = 3;
const uint8_t DIRECTION_UP_RIGHT = 4;
const uint8_t DIRECTION_DOWN_RIGHT = 5;
const uint8_t DIRECTION_DOWN_LEFT = 6;
const uint8_t DIRECTION_UP_LEFT = 7;
const uint8_t DIRECTION_NONE = 8;

const uint8_t SLIDING_PIECES[4] = { PIECE_QUEEN, PIECE_ROOK, PIECE_BISHOP };

// Start & End indices in the direction indices for each piece type
const uint8_t DIRECTIONS[PIECE_TYPES][2] = {
    { DIRECTION_NONE, DIRECTION_NONE }, // pawn
    { DIRECTION_NONE, DIRECTION_NONE }, // knight
    { DIRECTION_UP_RIGHT, DIRECTION_UP_LEFT }, // bishop
    { DIRECTION_UP, DIRECTION_LEFT }, // rook
    { DIRECTION_UP, DIRECTION_UP_LEFT }, // queen
    { DIRECTION_UP, DIRECTION_UP_LEFT }, // king
};

const int8_t DIRECTION_DELTAS[8] = { 8, 1, -8, -1, 9, -7, -9, 7 };

const int8_t UP[2] = { 8, -8 };
const int8_t UP_DOUBLE[2] = { 16, -16 };
const int8_t UP_LEFT[2] = { 7, -9 };
const int8_t UP_RIGHT[2] = { 9, -7 };

const Piece PROMOTION_PIECE[4] = { PIECE_QUEEN, PIECE_ROOK, PIECE_BISHOP, PIECE_KNIGHT };

const Square LASTSQ_TABLE[64][8] = {
    { 56, 7, 0, 0, 63, 0, 0, 0 },
    { 57, 7, 1, 0, 55, 1, 1, 8 },
    { 58, 7, 2, 0, 47, 2, 2, 16 },
    { 59, 7, 3, 0, 39, 3, 3, 24 },
    { 60, 7, 4, 0, 31, 4, 4, 32 },
    { 61, 7, 5, 0, 23, 5, 5, 40 },
    { 62, 7, 6, 0, 15, 6, 6, 48 },
    { 63, 7, 7, 0, 7, 7, 7, 56 },
    { 56, 15, 0, 8, 62, 1, 8, 8 },
    { 57, 15, 1, 8, 63, 2, 0, 16 },
    { 58, 15, 2, 8, 55, 3, 1, 24 },
    { 59, 15, 3, 8, 47, 4, 2, 32 },
    { 60, 15, 4, 8, 39, 5, 3, 40 },
    { 61, 15, 5, 8, 31, 6, 4, 48 },
    { 62, 15, 6, 8, 23, 7, 5, 56 },
    { 63, 15, 7, 8, 15, 15, 6, 57 },
    { 56, 23, 0, 16, 61, 2, 16, 16 },
    { 57, 23, 1, 16, 62, 3, 8, 24 },
    { 58, 23, 2, 16, 63, 4, 0, 32 },
    { 59, 23, 3, 16, 55, 5, 1, 40 },
    { 60, 23, 4, 16, 47, 6, 2, 48 },
    { 61, 23, 5, 16, 39, 7, 3, 56 },
    { 62, 23, 6, 16, 31, 15, 4, 57 },
    { 63, 23, 7, 16, 23, 23, 5, 58 },
    { 56, 31, 0, 24, 60, 3, 24, 24 },
    { 57, 31, 1, 24, 61, 4, 16, 32 },
    { 58, 31, 2, 24, 62, 5, 8, 40 },
    { 59, 31, 3, 24, 63, 6, 0, 48 },
    { 60, 31, 4, 24, 55, 7, 1, 56 },
    { 61, 31, 5, 24, 47, 15, 2, 57 },
    { 62, 31, 6, 24, 39, 23, 3, 58 },
    { 63, 31, 7, 24, 31, 31, 4, 59 },
    { 56, 39, 0, 32, 59, 4, 32, 32 },
    { 57, 39, 1, 32, 60, 5, 24, 40 },
    { 58, 39, 2, 32, 61, 6, 16, 48 },
    { 59, 39, 3, 32, 62, 7, 8, 56 },
    { 60, 39, 4, 32, 63, 15, 0, 57 },
    { 61, 39, 5, 32, 55, 23, 1, 58 },
    { 62, 39, 6, 32, 47, 31, 2, 59 },
    { 63, 39, 7, 32, 39, 39, 3, 60 },
    { 56, 47, 0, 40, 58, 5, 40, 40 },
    { 57, 47, 1, 40, 59, 6, 32, 48 },
    { 58, 47, 2, 40, 60, 7, 24, 56 },
    { 59, 47, 3, 40, 61, 15, 16, 57 },
    { 60, 47, 4, 40, 62, 23, 8, 58 },
    { 61, 47, 5, 40, 63, 31, 0, 59 },
    { 62, 47, 6, 40, 55, 39, 1, 60 },
    { 63, 47, 7, 40, 47, 47, 2, 61 },
    { 56, 55, 0, 48, 57, 6, 48, 48 },
    { 57, 55, 1, 48, 58, 7, 40, 56 },
    { 58, 55, 2, 48, 59, 15, 32, 57 },
    { 59, 55, 3, 48, 60, 23, 24, 58 },
    { 60, 55, 4, 48, 61, 31, 16, 59 },
    { 61, 55, 5, 48, 62, 39, 8, 60 },
    { 62, 55, 6, 48, 63, 47, 0, 61 },
    { 63, 55, 7, 48, 55, 55, 1, 62 },
    { 56, 63, 0, 56, 56, 7, 56, 56 },
    { 57, 63, 1, 56, 57, 15, 48, 57 },
    { 58, 63, 2, 56, 58, 23, 40, 58 },
    { 59, 63, 3, 56, 59, 31, 32, 59 },
    { 60, 63, 4, 56, 60, 39, 24, 60 },
    { 61, 63, 5, 56, 61, 47, 16, 61 },
    { 62, 63, 6, 56, 62, 55, 8, 62 },
    { 63, 63, 7, 56, 63, 63, 0, 63 }
};

inline Move createMove(Square origin, Square target) {
    return (Move)((origin & 0x3F) | ((target & 0x3F) << 6));
}

inline Square moveOrigin(Move move) {
    return (Square)(move & 0x3F);
}

inline Square moveTarget(Move move) {
    return (Square)((move >> 6) & 0x3F);
}

void generateMoves(struct Board* board, Move* moves, int* counter) {

    Bitboard blocked = board->board;
    Bitboard blockedUs = board->byColor[board->stm];
    Bitboard blockedEnemy = board->byColor[1 - board->stm];
    Bitboard free = ~blocked;

    Bitboard attackedEnemy = board->stack->attackedByColor[1 - board->stm];

    // ------------------- PAWN MOVES ----------------------
    {
        Bitboard pawns = board->byPiece[board->stm][PIECE_PAWN];

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

            if (__builtin_expect((C64(1) << target) & enemyBackrank, 0)) {
                // Promotion
                for (uint8_t promotion = 0; promotion <= 3; promotion++) {
                    *moves++ = move | (promotion << 14) | MOVE_PROMOTION;
                    (*counter)++;
                }
                continue;
            }

            *moves++ = move;
            (*counter)++;
        }

        // Double pushes
        while (doublePushedPawns) {
            Square target = popLSB(&doublePushedPawns);
            *moves++ = createMove(target - UP_DOUBLE[board->stm], target);
            (*counter)++;
        }

        // Captures
        Bitboard pAttacksLeft = pawnAttacksLeft(board, board->stm);
        Bitboard pAttacksRight = pawnAttacksRight(board, board->stm);

        Bitboard leftCaptures = pAttacksLeft & blockedEnemy;
        while (leftCaptures) {
            Square target = popLSB(&leftCaptures);
            Move move = createMove(target - UP_LEFT[board->stm], target);

            if (__builtin_expect((C64(1) << target) & enemyBackrank, 0)) {
                // Promotion
                for (uint8_t promotion = 0; promotion <= 3; promotion++) {
                    *moves++ = move | (promotion << 14) | MOVE_PROMOTION;
                    (*counter)++;
                }
                continue;
            }

            *moves++ = move;
            (*counter)++;
        }
        Bitboard rightCaptures = pAttacksRight & blockedEnemy;
        while (rightCaptures) {
            Square target = popLSB(&rightCaptures);
            Move move = createMove(target - UP_RIGHT[board->stm], target);

            if (__builtin_expect((C64(1) << target) & enemyBackrank, 0)) {
                // Promotion
                for (uint8_t promotion = 0; promotion <= 3; promotion++) {
                    *moves++ = move | (promotion << 14) | MOVE_PROMOTION;
                    (*counter)++;
                }
                continue;
            }

            *moves++ = move;
            (*counter)++;
        }

        // En passent
        Bitboard epBB = board->stack->enpassantTarget;
        Bitboard leftEp = pAttacksLeft & epBB;
        if (leftEp) {
            Square target = popLSB(&leftEp);
            *moves++ = createMove(target - UP_LEFT[board->stm], target) | MOVE_ENPASSANT;
            (*counter)++;
        }
        Bitboard rightEp = pAttacksRight & epBB;
        if (rightEp) {
            Square target = popLSB(&rightEp);
            *moves++ = createMove(target - UP_RIGHT[board->stm], target) | MOVE_ENPASSANT;
            (*counter)++;
        }
    }

    // ------------------- KNIGHT MOVES ----------------------
    {
        Bitboard knights = board->byPiece[board->stm][PIECE_KNIGHT];
        while (knights) {
            Square knight = popLSB(&knights);
            Bitboard knightTargets = knightAttacks(C64(1) << knight) & ~blockedUs;

            while (knightTargets) {
                Square target = popLSB(&knightTargets);
                *moves++ = createMove(knight, target);
                (*counter)++;
            }
        }
    }

    // ------------------- SLIDING MOVES ----------------------
    for (int i = 0; i < 3; i++) {
        Piece pieceType = SLIDING_PIECES[i];
        Bitboard pieces = board->byPiece[board->stm][pieceType];
        while (pieces) {
            Square piece = popLSB(&pieces);
            Bitboard targets = slidingPieceAttacks(board, C64(1) << piece) & ~blockedUs;

            while (targets) {
                Square target = popLSB(&targets);
                *moves++ = createMove(piece, target);
                (*counter)++;
            }
        }
    }

    // ------------------- KING MOVES ----------------------
    {
        Square king = lsb(board->byPiece[board->stm][PIECE_KING]);
        Bitboard kingTargets = kingAttacks(board, board->stm) & ~blockedUs & ~attackedEnemy;

        while (kingTargets) {
            Square target = popLSB(&kingTargets);
            *moves++ = createMove(king, target);
            (*counter)++;
        }

        if (!isInCheck(board, board->stm)) {
            // Castling: Nothing on the squares between king and rook, also nothing from the enemy attacking it
            switch (board->stm) {
            case COLOR_WHITE:
                // Kingside
                if ((board->stack->castling & 0x1) && !(board->board & 0x60) && !(board->stack->attackedByColor[COLOR_BLACK] & 0x60)) {
                    *moves++ = createMove(king, king + 2) | MOVE_CASTLING;
                    (*counter)++;
                }
                // Queenside
                if ((board->stack->castling & 0x2) && !(board->board & 0x0E) && !(board->stack->attackedByColor[COLOR_BLACK] & 0x0C)) {
                    *moves++ = createMove(king, king - 2) | MOVE_CASTLING;
                    (*counter)++;
                }
                break;
            case COLOR_BLACK:
                // Kingside
                if ((board->stack->castling & 0x4) && !(board->board & C64(0x6000000000000000)) && !(board->stack->attackedByColor[COLOR_WHITE] & C64(0x6000000000000000))) {
                    *moves++ = createMove(king, king + 2) | MOVE_CASTLING;
                    (*counter)++;
                }
                // Queenside
                if ((board->stack->castling & 0x8) && !(board->board & C64(0x0E00000000000000)) && !(board->stack->attackedByColor[COLOR_WHITE] & C64(0x0C00000000000000))) {
                    *moves++ = createMove(king, king - 2) | MOVE_CASTLING;
                    (*counter)++;
                }
                break;
            }
        }
    }
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

void squareToString(char* string, Square square) {
    string[0] = fileFromSquare(square);
    string[1] = rankFromSquare(square);
}

void moveToString(char* string, Move move) {
    Square origin = moveOrigin(move);
    Square target = moveTarget(move);

    squareToString(&string[0], origin);
    squareToString(&string[2], target);

    if ((move & (0x3 << 12)) == MOVE_PROMOTION) {
        Move promotionType = move & (0x3) << 14;
        if (promotionType == PROMOTION_QUEEN)
            string[4] = 'q';
        if (promotionType == PROMOTION_ROOK)
            string[4] = 'r';
        if (promotionType == PROMOTION_BISHOP)
            string[4] = 'b';
        if (promotionType == PROMOTION_KNIGHT)
            string[4] = 'n';
    }
    else {
        string[4] = '\0';
    }
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

Move stringToMove(char* string) {
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