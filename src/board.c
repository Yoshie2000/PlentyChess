#include <stdio.h>
#include <inttypes.h>
#include <string.h>

#include "move.h"
#include "types.h"
#include "board.h"

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

void startpos(struct Board* result) {
    struct Board board = {
        .byPiece = {
            {
                0x000000000000FF00, // 0b0000000000000000000000000000000000000000000000001111111100000000,
                0x0000000000000042, // 0b0000000000000000000000000000000000000000000000000000000001000010,
                0x0000000000000024, // 0b0000000000000000000000000000000000000000000000000000000000100100,
                0x0000000000000081, // 0b0000000000000000000000000000000000000000000000000000000010000001,
                0x0000000000000008, // 0b0000000000000000000000000000000000000000000000000000000000001000,
                0x0000000000000010  // 0b0000000000000000000000000000000000000000000000000000000000010000,
            },
            {
                0x00FF000000000000, // 0b0000000011111111000000000000000000000000000000000000000000000000,
                0x4200000000000000, // 0b0100001000000000000000000000000000000000000000000000000000000000,
                0x2400000000000000, //, 0b0010010000000000000000000000000000000000000000000000000000000000,
                0x8100000000000000, //, 0b1000000100000000000000000000000000000000000000000000000000000000,
                0x0800000000000000, //, 0b0001000000000000000000000000000000000000000000000000000000000000,
                0x1000000000000000  //, 0b0000100000000000000000000000000000000000000000000000000000000000,
            },
        },
        .byColor = {
            0x000000000000FFFF,
            0xFFFF000000000000
        },
        .board = 0xFFFF00000000FFFF, // 0b1111111111111111000000000000000000000000000000001111111111111111,
        .pieces = {
            PIECE_ROOK, PIECE_KNIGHT, PIECE_BISHOP, PIECE_QUEEN, PIECE_KING, PIECE_BISHOP, PIECE_KNIGHT, PIECE_ROOK,
            PIECE_PAWN, PIECE_PAWN, PIECE_PAWN, PIECE_PAWN, PIECE_PAWN, PIECE_PAWN, PIECE_PAWN, PIECE_PAWN,
            NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE,
            NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE,
            NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE,
            NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE, NO_PIECE,
            PIECE_PAWN, PIECE_PAWN, PIECE_PAWN, PIECE_PAWN, PIECE_PAWN, PIECE_PAWN, PIECE_PAWN, PIECE_PAWN,
            PIECE_ROOK, PIECE_KNIGHT, PIECE_BISHOP, PIECE_QUEEN, PIECE_KING, PIECE_BISHOP, PIECE_KNIGHT, PIECE_ROOK,
        },
        .stm = COLOR_WHITE,
        .ply = 1,
        .rule50_ply = 0,
        .castling = 0xF, // 0b1111
        .stack = result->stack
    };
    *result = board;
    result->stack->capturedPiece = NO_PIECE;
    result->stack->enpassantTarget = 0;

    for (Color side = 0; side <= 1; side++) {
        Bitboard attackers = C64(0);
        for (Piece piece = 0; piece < PIECE_TYPES; piece++) {
            result->stack->attackedByPiece[side][piece] = attackedSquaresByPiece(result, side, piece);
            attackers |= result->stack->attackedByPiece[side][piece];
        }
        result->stack->attackedByColor[side] = attackers;
    }
}

void doMove(struct Board* board, struct BoardStack* newStack, Move move) {
    newStack->previous = board->stack;
    board->stack = newStack;

    Square origin = moveOrigin(move);
    Square target = moveTarget(move);

    newStack->capturedPiece = board->pieces[target];
    Square captureTarget = target;
    Bitboard captureTargetBB = C64(1) << captureTarget;

    Piece piece = board->pieces[origin];

    Bitboard originBB = C64(1) << origin;
    Bitboard targetBB = C64(1) << target;
    Bitboard fromTo = originBB | targetBB;

    board->board ^= fromTo;
    board->byColor[board->stm] ^= fromTo;
    board->byPiece[board->stm][piece] ^= fromTo;

    board->pieces[origin] = NO_PIECE;
    board->pieces[target] = piece;

    // This move is en passent
    if (__builtin_expect((move & MOVE_ENPASSANT) == MOVE_ENPASSANT, 0)) {
        newStack->capturedPiece = PIECE_PAWN;
        captureTarget = target - UP[board->stm];
        captureTargetBB = C64(1) << captureTarget;
        board->pieces[captureTarget] = NO_PIECE; // remove the captured pawn
        board->board ^= captureTargetBB;
    }

    // Handle capture
    if (newStack->capturedPiece != NO_PIECE) {
        board->board |= targetBB; // Put piece on target square (in case xor didn't work)
        board->byColor[1 - board->stm] ^= captureTargetBB; // take away the captured piece
        board->byPiece[1 - board->stm][newStack->capturedPiece] ^= captureTargetBB;
    }

    // En passent square
    newStack->enpassantTarget = 0;
    if (__builtin_expect(piece == PIECE_PAWN && (origin ^ target) == 16, 0)) {
        newStack->enpassantTarget = C64(1) << (target - UP[board->stm]);
    }

    memcpy(board->stack->attackedByPiece, board->stack->previous->attackedByPiece, sizeof(Bitboard) * 14);
    for (Color side = 0; side <= 1; side++) {
        Bitboard attackers = C64(0);
        for (Piece _piece = 0; _piece < PIECE_TYPES; _piece++) {
            // If from or to are attacked, or this piece type was moved or captured, regenerate
            if (
                (board->stack->attackedByPiece[side][_piece] & (fromTo | captureTargetBB)) ||
                (side == board->stm && piece == _piece) ||
                (side != board->stm && _piece == newStack->capturedPiece)
            ) {
                board->stack->attackedByPiece[side][_piece] = attackedSquaresByPiece(board, side, _piece);
            }
            attackers |= board->stack->attackedByPiece[side][_piece];
        }
        board->stack->attackedByColor[side] = attackers;
    }

    board->stm = 1 - board->stm;
}

void undoMove(struct Board* board, Move move) {
    board->stm = 1 - board->stm;

    Square origin = moveOrigin(move);
    Square target = moveTarget(move);
    Square captureTarget = target;

    Piece piece = board->pieces[target];

    Bitboard fromTo = (C64(1) << origin) | (C64(1) << target);

    board->board ^= fromTo;
    board->byColor[board->stm] ^= fromTo;
    board->byPiece[board->stm][piece] ^= fromTo;

    board->pieces[target] = NO_PIECE;
    board->pieces[origin] = piece;

    // This move is en passent
    if ((move & MOVE_ENPASSANT) == MOVE_ENPASSANT) {
        captureTarget = target - UP[board->stm];
        board->board ^= C64(1) << captureTarget;
    }

    // Handle capture
    if (board->stack->capturedPiece != NO_PIECE) {
        board->pieces[captureTarget] = board->stack->capturedPiece;
        board->board |= C64(1) << captureTarget;
        board->byColor[1 - board->stm] ^= C64(1) << captureTarget;
        board->byPiece[1 - board->stm][board->stack->capturedPiece] ^= C64(1) << captureTarget;
    }

    board->stack = board->stack->previous;
}

inline Bitboard pawnAttacksLeft(struct Board* board, Color side) {
    Bitboard pawns = board->byPiece[side][PIECE_PAWN];
    return side == COLOR_WHITE ?
        ((pawns & (~FILE_A)) << 7) :
        ((pawns & (~FILE_A)) >> 9);
}

inline Bitboard pawnAttacksRight(struct Board* board, Color side) {
    Bitboard pawns = board->byPiece[side][PIECE_PAWN];
    return side == COLOR_WHITE ?
        ((pawns & (~FILE_H)) << 9) :
        ((pawns & (~FILE_H)) >> 7);
}

inline Bitboard pawnAttacks(struct Board* board, Color side) {
    return pawnAttacksLeft(board, side) | pawnAttacksRight(board, side);
}

inline Bitboard knightAttacks(Bitboard knightBB) {
    Bitboard l1 = (knightBB >> 1) & C64(0x7f7f7f7f7f7f7f7f);
    Bitboard l2 = (knightBB >> 2) & C64(0x3f3f3f3f3f3f3f3f);
    Bitboard r1 = (knightBB << 1) & C64(0xfefefefefefefefe);
    Bitboard r2 = (knightBB << 2) & C64(0xfcfcfcfcfcfcfcfc);
    Bitboard h1 = l1 | r1;
    Bitboard h2 = l2 | r2;
    return (h1 << 16) | (h1 >> 16) | (h2 << 8) | (h2 >> 8);
}

inline Bitboard knightAttacksAll(struct Board* board, Color side) {
    Bitboard knights = board->byPiece[side][PIECE_KNIGHT];
    return knightAttacks(knights);
}

inline Bitboard slidingPieceAttacks(struct Board* board, Bitboard pieceBB) {
    Bitboard attacksBB = C64(0);
    Square origin = lsb(pieceBB);
    Piece pieceType = board->pieces[origin];

    int8_t delta, direction;
    Square lastSquare, toSquare;
    Bitboard toSquareBB;

    for (direction = DIRECTIONS[pieceType][0]; direction <= DIRECTIONS[pieceType][1]; direction++) {
        delta = DIRECTION_DELTAS[direction];
        lastSquare = LASTSQ_TABLE[origin][direction];

        if (origin != lastSquare)
            for (toSquare = origin + delta; (toSquareBB = C64(1) << toSquare); toSquare += delta) {
                attacksBB |= toSquareBB;
                if ((board->board & toSquareBB) || (toSquare == lastSquare))
                    break;
            }
    }
    return attacksBB;
}

inline Bitboard kingAttacks(struct Board* board, Color color) {
    Bitboard attacksBB = C64(0);
    Square origin = lsb(board->byPiece[color][PIECE_KING]);

    int8_t direction;
    Square lastSquare;
    Bitboard toSquareBB;

    for (direction = DIRECTIONS[PIECE_KING][0]; direction <= DIRECTIONS[PIECE_KING][1]; direction++) {
        lastSquare = LASTSQ_TABLE[origin][direction];

        toSquareBB = C64(1) << (origin + DIRECTION_DELTAS[direction]);
        if (origin != lastSquare && toSquareBB)
            attacksBB |= toSquareBB;
    }
    return attacksBB;
}

inline Bitboard slidingPieceAttacksAll(struct Board* board, Color side, Piece pieceType) {
    Bitboard attacksBB = C64(0);

    Bitboard pieces = board->byPiece[side][pieceType];
    while (pieces) {
        Bitboard pieceBB = C64(1) << popLSB(&pieces);
        attacksBB |= slidingPieceAttacks(board, pieceBB);
    }
    return attacksBB;
}

inline bool isSquareAttacked(struct Board* board, Square square, Color side) {
    Bitboard squareBB = C64(1) << square;

    if (pawnAttacks(board, side) & squareBB)
        return true;
    if (knightAttacksAll(board, side) & squareBB)
        return true;
    if (kingAttacks(board, side) & squareBB)
        return true;
    if (slidingPieceAttacksAll(board, side, PIECE_BISHOP) & squareBB)
        return true;
    if (slidingPieceAttacksAll(board, side, PIECE_ROOK) & squareBB)
        return true;
    if (slidingPieceAttacksAll(board, side, PIECE_QUEEN) & squareBB)
        return true;
    return false;
}

inline Bitboard attackedSquares(struct Board* board, Color side) {
    return
        pawnAttacks(board, side) |
        knightAttacksAll(board, side) |
        kingAttacks(board, side) |
        slidingPieceAttacksAll(board, side, PIECE_BISHOP) |
        slidingPieceAttacksAll(board, side, PIECE_ROOK) |
        slidingPieceAttacksAll(board, side, PIECE_QUEEN);
}

inline Bitboard attackedSquaresByPiece(struct Board* board, Color side, Piece pieceType) {
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

inline bool isInCheck(struct Board* board, Color side) {
    Square kingSquare = lsb(board->byPiece[side][PIECE_KING]);
    Bitboard attackedEnemy = board->stack->attackedByColor[1 - side];
    return (C64(1) << kingSquare) & attackedEnemy;
}

void debugBoard(struct Board* board) {
    for (int rank = 7; rank >= 0; rank--) {

        printf("-");
        for (int file = 0; file <= 15; file++) {
            printf(" -");
        }
        printf("\n");

        for (int file = 0; file <= 7; file++) {

            // Get piece at index
            int idx = file + 8 * rank;
            // printf("| ");
            // if (board->pieces[idx] == 6)
            //     printf(" ");
            // else
            //     printf("%d", board->pieces[idx]);
            // printf(" ");
            Bitboard mask = 1L << idx;
            if ((board->stack->enpassantTarget & mask) != 0)
                printf("| E ");
            else if ((board->board & mask) == 0)
                printf("|   ");
            else if ((board->byPiece[COLOR_WHITE][PIECE_PAWN] & mask) != 0)
                printf("| ♙ ");
            else if ((board->byPiece[COLOR_BLACK][PIECE_PAWN] & mask) != 0)
                printf("| ♟︎ ");
            else if ((board->byPiece[COLOR_WHITE][PIECE_KNIGHT] & mask) != 0)
                printf("| ♘ ");
            else if ((board->byPiece[COLOR_BLACK][PIECE_KNIGHT] & mask) != 0)
                printf("| ♞ ");
            else if ((board->byPiece[COLOR_WHITE][PIECE_BISHOP] & mask) != 0)
                printf("| ♗ ");
            else if ((board->byPiece[COLOR_BLACK][PIECE_BISHOP] & mask) != 0)
                printf("| ♝ ");
            else if ((board->byPiece[COLOR_WHITE][PIECE_ROOK] & mask) != 0)
                printf("| ♖ ");
            else if ((board->byPiece[COLOR_BLACK][PIECE_ROOK] & mask) != 0)
                printf("| ♜ ");
            else if ((board->byPiece[COLOR_WHITE][PIECE_QUEEN] & mask) != 0)
                printf("| ♕ ");
            else if ((board->byPiece[COLOR_BLACK][PIECE_QUEEN] & mask) != 0)
                printf("| ♛ ");
            else if ((board->byPiece[COLOR_WHITE][PIECE_KING] & mask) != 0)
                printf("| ♔ ");
            else if ((board->byPiece[COLOR_BLACK][PIECE_KING] & mask) != 0)
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
    printf("\n");
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