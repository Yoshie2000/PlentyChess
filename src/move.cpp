#include <inttypes.h>
#include <stdio.h>
#include <cassert>
#include <iostream>

#include "move.h"
#include "board.h"
#include "types.h"
#include "magic.h"
#include "bitboard.h"
#include "evaluation.h"
#include "spsa.h"

void generatePawn_quiet(Board* board, Move** moves, int* counter, Bitboard targetMask) {
    Bitboard pawns = board->byPiece[Piece::PAWN] & board->byColor[board->stm];
    Bitboard free = ~(board->byColor[Color::WHITE] | board->byColor[Color::BLACK]);

    Bitboard pushedPawns, secondRankPawns, doublePushedPawns, enemyBackrank;
    if (board->stm == Color::WHITE) {
        pushedPawns = (pawns << 8) & free & targetMask;
        secondRankPawns = (pawns & BB::RANK_2) & pawns;
        doublePushedPawns = (((secondRankPawns << 8) & free) << 8) & free & targetMask;
        enemyBackrank = BB::RANK_8;
    }
    else {
        pushedPawns = (pawns >> 8) & free & targetMask;
        secondRankPawns = (pawns & BB::RANK_7) & pawns;
        doublePushedPawns = (((secondRankPawns >> 8) & free) >> 8) & free & targetMask;
        enemyBackrank = BB::RANK_1;
    }

    while (pushedPawns) {
        Square target = popLSB(&pushedPawns);
        Move move = createMove(target - UP[board->stm], target);

        if (bitboard(target) & enemyBackrank) {
            // Promotion: Queen promotions are considered captures
            for (uint8_t promotion = 1; promotion <= 3; promotion++) {
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

void generatePawn_capture(Board* board, Move** moves, int* counter, Bitboard targetMask) {
    // Captures
    Bitboard pawns = board->byPiece[Piece::PAWN] & board->byColor[board->stm];
    Bitboard pAttacksLeft = BB::pawnAttacksLeft(pawns, board->stm);
    Bitboard pAttacksRight = BB::pawnAttacksRight(pawns, board->stm);
    Bitboard blockedEnemy = board->byColor[1 - board->stm];
    Bitboard enemyBackrank = board->stm == Color::WHITE ? BB::RANK_8 : BB::RANK_1;
    Bitboard free = ~(board->byColor[Color::WHITE] | board->byColor[Color::BLACK]);

    // Queen promotions (without capture)
    Bitboard pushedPawns;
    if (board->stm == Color::WHITE) {
        pushedPawns = (pawns << 8) & free & targetMask & BB::RANK_8;
    }
    else {
        pushedPawns = (pawns >> 8) & free & targetMask & BB::RANK_1;
    }
    while (pushedPawns) {
        Square target = popLSB(&pushedPawns);
        *(*moves)++ = createMove(target - UP[board->stm], target) | PROMOTION_QUEEN | MOVE_PROMOTION;
        (*counter)++;
    }

    // Capture promotions
    Bitboard leftCaptures = pAttacksLeft & blockedEnemy & targetMask;
    while (leftCaptures) {
        Square target = popLSB(&leftCaptures);
        Move move = createMove(target - UP_LEFT[board->stm], target);

        if (bitboard(target) & enemyBackrank) {
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
    Bitboard rightCaptures = pAttacksRight & blockedEnemy & targetMask;
    while (rightCaptures) {
        Square target = popLSB(&rightCaptures);
        Move move = createMove(target - UP_RIGHT[board->stm], target);

        if (bitboard(target) & enemyBackrank) {
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
    Bitboard epBB = board->enpassantTarget;
    if (epBB) {
        // Check handling: If we are using a checkers mask, and it doesn't match the captured square of this EP move, don't allow the EP
        Bitboard epCapt = board->stm == Color::WHITE ? (epBB >> 8) : (epBB << 8);
        if (~targetMask != bitboard(0) && !(epCapt & targetMask))
            return;

        Bitboard leftEp = pAttacksLeft & epBB;
        if (leftEp) {
            Square target = popLSB(&leftEp);
            Square origin = target - UP_LEFT[board->stm];
            *(*moves)++ = createMove(origin, target) | MOVE_ENPASSANT;
            (*counter)++;
        }
        Bitboard rightEp = pAttacksRight & epBB;
        if (rightEp) {
            Square target = popLSB(&rightEp);
            Square origin = target - UP_RIGHT[board->stm];
            *(*moves)++ = createMove(origin, target) | MOVE_ENPASSANT;
            (*counter)++;
        }
    }
}

// For all pieces other than pawns
template <Piece pieceType>
void generatePiece(Board* board, Move** moves, int* counter, bool captures, Bitboard targetMask) {
    Bitboard blockedUs = board->byColor[board->stm];
    Bitboard blockedEnemy = board->byColor[1 - board->stm];
    Bitboard occupied = blockedUs | blockedEnemy;

    // Decide whether only captures or only non-captures
    Bitboard mask;
    if (captures)
        mask = blockedEnemy & ~blockedUs & targetMask;
    else
        mask = ~blockedEnemy & ~blockedUs & targetMask;

    Bitboard pieces = board->byPiece[pieceType] & blockedUs;
    while (pieces) {
        Square piece = popLSB(&pieces);
        Bitboard targets = pieceType == Piece::KNIGHT ? BB::KNIGHT_ATTACKS[piece] : pieceType == Piece::BISHOP ? getBishopMoves(piece, occupied) : pieceType == Piece::ROOK ? getRookMoves(piece, occupied) : pieceType == Piece::QUEEN ? (getRookMoves(piece, occupied) | getBishopMoves(piece, occupied)) : pieceType == Piece::KING ? BB::KING_ATTACKS[lsb(board->byPiece[Piece::KING] & board->byColor[board->stm])] : bitboard(0);
        targets &= mask;

        while (targets) {
            Square target = popLSB(&targets);
            *(*moves)++ = createMove(piece, target);
            (*counter)++;
        }
    }
}

void generateCastling(Board* board, Move** moves, int* counter) {
    assert((board->byPiece[Piece::KING] & board->byColor[board->stm]) > 0);

    Square king = lsb(board->byPiece[Piece::KING] & board->byColor[board->stm]);
    Bitboard occupied = board->byColor[Color::WHITE] | board->byColor[Color::BLACK];

    // Castling: Nothing on the squares between king and rook
    // Checking if we are in check, or there are attacks in the way is done later (see isLegal())
    switch (board->stm) {
    case Color::WHITE:
        // Kingside
        if ((board->castling & CASTLING_WHITE_KINGSIDE)) {
            Square kingsideRook = board->castlingSquares[0];
            Bitboard between = BB::BETWEEN[kingsideRook][CASTLING_ROOK_SQUARES[0]] | BB::BETWEEN[king][CASTLING_KING_SQUARES[0]];
            between &= ~bitboard(kingsideRook) & ~bitboard(king);
            if (!(occupied & between)) {
                *(*moves)++ = createMove(king, kingsideRook) | MOVE_CASTLING;
                (*counter)++;
            }
        }
        // Queenside
        if ((board->castling & CASTLING_WHITE_QUEENSIDE)) {
            Square queensideRook = board->castlingSquares[1];
            Bitboard between = BB::BETWEEN[queensideRook][CASTLING_ROOK_SQUARES[1]] | BB::BETWEEN[king][CASTLING_KING_SQUARES[1]];
            between &= ~bitboard(queensideRook) & ~bitboard(king);
            if (!(occupied & between)) {
                *(*moves)++ = createMove(king, queensideRook) | MOVE_CASTLING;
                (*counter)++;
            }
        }
        break;
    case Color::BLACK:
        // Kingside
        if ((board->castling & CASTLING_BLACK_KINGSIDE)) {
            Square kingsideRook = board->castlingSquares[2];
            Bitboard between = BB::BETWEEN[kingsideRook][CASTLING_ROOK_SQUARES[2]] | BB::BETWEEN[king][CASTLING_KING_SQUARES[2]];
            between &= ~bitboard(kingsideRook) & ~bitboard(king);
            if (!(occupied & between)) {
                *(*moves)++ = createMove(king, kingsideRook) | MOVE_CASTLING;
                (*counter)++;
            }
        }
        // Queenside
        if ((board->castling & CASTLING_BLACK_QUEENSIDE)) {
            Square queensideRook = board->castlingSquares[3];
            Bitboard between = BB::BETWEEN[queensideRook][CASTLING_ROOK_SQUARES[3]] | BB::BETWEEN[king][CASTLING_KING_SQUARES[3]];
            between &= ~bitboard(queensideRook) & ~bitboard(king);
            if (!(occupied & between)) {
                *(*moves)++ = createMove(king, queensideRook) | MOVE_CASTLING;
                (*counter)++;
            }
        }
        break;
    }
}

void generateMoves(Board* board, Move* moves, int* counter, bool onlyCaptures) {
    assert((board->byColor[board->stm] & board->byPiece[Piece::KING]) > 0);

    // If in double check, only generate king moves
    if (board->checkerCount > 1) {
        generatePiece<Piece::KING>(board, &moves, counter, true, ~bitboard(0));
        if (!onlyCaptures)
            generatePiece<Piece::KING>(board, &moves, counter, false, ~bitboard(0));
        return;
    }

    // If in check, only generate targets that take care of the check
    Bitboard checkMask = board->checkers ? BB::BETWEEN[lsb(board->byColor[board->stm] & board->byPiece[Piece::KING])][lsb(board->checkers)] : ~bitboard(0);

    // ------------------- CAPTURES ----------------------
    generatePawn_capture(board, &moves, counter, checkMask);
    generatePiece<Piece::KNIGHT>(board, &moves, counter, true, checkMask);
    generatePiece<Piece::BISHOP>(board, &moves, counter, true, checkMask);
    generatePiece<Piece::ROOK>(board, &moves, counter, true, checkMask);
    generatePiece<Piece::QUEEN>(board, &moves, counter, true, checkMask);
    generatePiece<Piece::KING>(board, &moves, counter, true, ~bitboard(0));

    // ------------------- NON CAPTURES ----------------------
    if (!onlyCaptures) {
        generatePawn_quiet(board, &moves, counter, checkMask);
        generatePiece<Piece::KNIGHT>(board, &moves, counter, false, checkMask);
        generatePiece<Piece::BISHOP>(board, &moves, counter, false, checkMask);
        generatePiece<Piece::ROOK>(board, &moves, counter, false, checkMask);
        generatePiece<Piece::QUEEN>(board, &moves, counter, false, checkMask);
        generatePiece<Piece::KING>(board, &moves, counter, false, ~bitboard(0));
    }

    // ------------------- CASTLING ----------------------
    if (!onlyCaptures && !board->checkers) {
        generateCastling(board, &moves, counter);
    }
}

inline char fileFromSquare(Square square) {
    int file = fileOf(square);
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
    int rank = rankOf(square);
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

std::string moveToString(Move move, bool chess960) {
    if (move == MOVE_NONE) return "move_none";
    if (move == MOVE_NULL) return "move_null";
    std::string result = "";

    Square origin = moveOrigin(move);
    Square target = moveTarget(move);
    if ((move & (0x3 << 12)) == MOVE_CASTLING && !chess960) {
        int rank = rankOf(origin);
        int file = origin > target ? 2 : 6;
        target = rank * 8 + file;
    }

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

Square stringToSquare(const char* string) {
    int file = fileFromString(string[0]);
    int rank = rankFromString(string[1]);
    return (Square)(8 * rank + file);
}

Move stringToMove(const char* string, Board* board) {
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
    if (board->chess960 && board->pieces[origin] == Piece::KING && board->pieces[target] == Piece::ROOK && !(bitboard(target) & board->byColor[1 - board->stm]))
        move |= MOVE_CASTLING;
    if (!board->chess960 && board->pieces[origin] == Piece::KING && std::abs(target - origin) == 2)
        move |= MOVE_CASTLING;
    if (board->pieces[origin] == Piece::PAWN && board->pieces[target] == Piece::NONE && (std::abs(target - origin) == 7 || std::abs(target - origin) == 9))
        move |= MOVE_ENPASSANT;

    return move;
}