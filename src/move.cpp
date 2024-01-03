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
#include "tt.h"

constexpr Move createMove(Square origin, Square target) {
    return (Move)((origin & 0x3F) | ((target & 0x3F) << 6));
}

bool isPseudoLegal(Board* board, Move move) {
    Square origin = moveOrigin(move);
    Square target = moveTarget(move);
    Bitboard originBB = C64(1) << origin;
    Bitboard targetBB = C64(1) << target;
    Piece piece = board->pieces[origin];

    // A valid piece needs to be on the origin square
    if (board->pieces[origin] == NO_PIECE) return false;
    // We can't capture our own piece
    if (board->byColor[board->stm] & targetBB) return false;
    // We can't move the enemies piece
    if (board->byColor[1 - board->stm] & originBB) return false;

    // Non-standard movetypes
    Move specialMove = move & 0x3000;
    switch (specialMove) {
    case MOVE_CASTLING: {
        if (piece != PIECE_KING || board->stack->checkers) return false;
        // Check for pieces between king and rook
        Bitboard importantSquares = board->stm == COLOR_WHITE ? (target > origin ? 0x60 : 0x0E) : (target > origin ? 0x6000000000000000 : 0x0E00000000000000);
        if ((board->byColor[COLOR_WHITE] | board->byColor[COLOR_BLACK]) & importantSquares) return false;
        // Check for castling flags (Attackers are done in isLegal())
        return board->stack->castling & (board->stm == COLOR_WHITE ? (target > origin ? 0b0001 : 0b0010) : (target > origin ? 0b0100 : 0b1000));
    }
    case MOVE_ENPASSANT:
        if (piece != PIECE_PAWN) return false;
        if (board->stack->checkerCount > 1) return false;
        if (board->stack->checkers && (!(lsb(board->stack->checkers) == target - UP[board->stm]) || (board->stack->blockers[board->stm] & originBB))) return false;
        // Check for EP flag on the right file
        return ((C64(1) << target) & board->stack->enpassantTarget) && board->pieces[target - UP[board->stm]] == PIECE_PAWN;
    case MOVE_PROMOTION:
        if (piece != PIECE_PAWN) return false;
        if (board->stack->checkerCount > 1) return false;
        if (board->stack->checkers && (!(board->stack->checkers & targetBB) || (board->stack->blockers[board->stm] & originBB))) return false;
        if (
            !(pawnAttacks(originBB, board->stm) & targetBB & board->byColor[1 - board->stm]) && // Capture promotion?
            !(origin + UP[board->stm] == target && board->pieces[target] == NO_PIECE)) // Push promotion?
            return false;
        return true;
    default:
        break;
    }

    Bitboard occupied = board->byColor[COLOR_WHITE] | board->byColor[COLOR_BLACK];
    switch (piece) {
    case PIECE_PAWN:
        if (targetBB & (RANK_8 | RANK_1)) return false;

        if (
            !(pawnAttacks(originBB, board->stm) & targetBB & board->byColor[1 - board->stm]) && // Capture?
            !(origin + UP[board->stm] == target && board->pieces[target] == NO_PIECE) && // Single push?
            !(origin + 2 * UP[board->stm] == target && board->pieces[target] == NO_PIECE && board->pieces[target - UP[board->stm]] == NO_PIECE && (originBB & (RANK_2 | RANK_7))) // Double push?
            )
            return false;
        break;
    case PIECE_KNIGHT:
        if (!(knightAttacks(originBB) & targetBB)) return false;
        break;
    case PIECE_BISHOP:
        if (!(getBishopMoves(origin, occupied) & targetBB)) return false;
        break;
    case PIECE_ROOK:
        if (!(getRookMoves(origin, occupied) & targetBB)) return false;
        break;
    case PIECE_QUEEN:
        if (!((getBishopMoves(origin, occupied) | getRookMoves(origin, occupied)) & targetBB)) return false;
        break;
    case PIECE_KING:
        if (!(kingAttacks(origin) & targetBB)) return false;
        break;
    default:
        return false;
    }

    if (board->stack->checkers) {
        if (piece != PIECE_KING) {
            // Double check requires king moves
            if (board->stack->checkerCount > 1)
                return false;

            bool capturesChecker = target == lsb(board->stack->checkers);
            bool blocksCheck = BETWEEN[lsb(board->byColor[board->stm] & board->byPiece[PIECE_KING])][lsb(board->stack->checkers)] & (C64(1) << target);
            if (!capturesChecker && !blocksCheck)
                return false;
        }
        // King moves *should* be handled in isLegal()
    }

    return true;
}

bool isLegal(Board* board, Move move) {
    assert(isPseudoLegal(board, move));

    Square origin = moveOrigin(move);
    Square target = moveTarget(move);
    Bitboard originBB = C64(1) << origin;

    Square king = lsb(board->byPiece[PIECE_KING] & board->byColor[board->stm]);

    Move specialMove = move & 0x3000;
    if (specialMove == MOVE_ENPASSANT) {
        // Check if king would be in check after this move
        Square epSquare = target - UP[board->stm];

        Bitboard epSquareBB = C64(1) << epSquare;
        Bitboard targetBB = C64(1) << target;
        Bitboard occupied = ((board->byColor[COLOR_WHITE] | board->byColor[COLOR_BLACK]) ^ originBB ^ epSquareBB) | targetBB;

        // Check if any rooks/queens/bishops attack the king square, given the occupied pieces after this EP move
        Bitboard rookAttacks = getRookMoves(king, occupied) & (board->byPiece[PIECE_ROOK] | board->byPiece[PIECE_QUEEN]) & board->byColor[1 - board->stm];
        Bitboard bishopAttacks = getBishopMoves(king, occupied) & (board->byPiece[PIECE_BISHOP] | board->byPiece[PIECE_BISHOP]) & board->byColor[1 - board->stm];
        return !rookAttacks && !bishopAttacks;
    }

    Bitboard occupied = board->byColor[COLOR_WHITE] | board->byColor[COLOR_BLACK];

    if (specialMove == MOVE_CASTLING) {
        // Check that none of the important squares (including the current king position!) are being attacked
        if (target < origin) {
            for (Square s = target; s <= origin; s++) {
                if (board->byColor[1 - board->stm] & attackersTo(board, s, occupied))
                    return false;
            }
        }
        else {
            for (Square s = target; s >= origin; s--) {
                if (board->byColor[1 - board->stm] & attackersTo(board, s, occupied))
                    return false;
            }
        }
        // Check for castling flags
        return board->stack->castling & (board->stm == COLOR_WHITE ? (target > origin ? 0b0001 : 0b0010) : (target > origin ? 0b0100 : 0b1000));
    }

    if (board->pieces[origin] == PIECE_KING) {
        // Check that we're not moving into an attack
        return !(board->byColor[1 - board->stm] & attackersTo(board, target, occupied ^ (C64(1) << origin)));
    }

    // Check if we're not pinned to the king, or are moving along the pin
    bool pinned = board->stack->blockers[board->stm] & originBB;
    return !pinned || (LINE[origin][target] & (C64(1) << king));
}

bool givesCheck(Board* board, Move move) {
    Square origin = moveOrigin(move);
    Square target = moveTarget(move);
    Bitboard occ = board->byColor[COLOR_WHITE] | board->byColor[COLOR_BLACK];
    Bitboard enemyKing = board->byColor[1 - board->stm] & board->byPiece[PIECE_KING];

    // Direct check
    Bitboard attacks = attackedSquaresByPiece(board->pieces[origin], target, occ ^ (C64(1) << origin) ^ (C64(1) << target), board->stm);
    if (attacks & enemyKing)
        return true;

    // Discovered check: Are we blocking a check to the enemy king?
    Move specialMove = move & 0x3000;
    if (board->stack->blockers[1 - board->stm] & (C64(1) << origin))
        return !(LINE[origin][target] & enemyKing) || specialMove == MOVE_CASTLING;

    switch (specialMove) {
    case MOVE_PROMOTION: {
        Piece promotionPiece = PROMOTION_PIECE[move >> 14];
        return attackedSquaresByPiece(promotionPiece, target, occ ^ (C64(1) << origin), board->stm) & enemyKing;
    }
    case MOVE_CASTLING: {
        Square rookTarget = board->stm == COLOR_WHITE ? (target > origin ? 5 : 3) : (target > origin ? 61 : 59);
        return attackedSquaresByPiece(PIECE_ROOK, rookTarget, occ, board->stm) & enemyKing;
    }
    case MOVE_ENPASSANT: {
        Square epSquare = target - UP[board->stm];
        Bitboard occupied = (occ ^ (C64(1) << origin) ^ (C64(1) << epSquare)) | (C64(1) << target);
        Square ek = lsb(enemyKing);
        return (attackedSquaresByPiece(PIECE_ROOK, ek, occupied, board->stm) & board->byColor[board->stm] & (board->byPiece[PIECE_ROOK] | board->byPiece[PIECE_QUEEN]))
            | (attackedSquaresByPiece(PIECE_BISHOP, ek, occupied, board->stm) & board->byColor[board->stm] & (board->byPiece[PIECE_BISHOP] | board->byPiece[PIECE_QUEEN]));
    }
    default:
        return false;
    }
}

void generatePawn_quiet(Board* board, Move** moves, int* counter, Bitboard targetMask) {
    Bitboard pawns = board->byPiece[PIECE_PAWN] & board->byColor[board->stm];
    Bitboard free = ~(board->byColor[COLOR_WHITE] | board->byColor[COLOR_BLACK]);

    Bitboard pushedPawns, secondRankPawns, doublePushedPawns, enemyBackrank;
    if (board->stm == COLOR_WHITE) {
        pushedPawns = (pawns << 8) & free & targetMask;
        secondRankPawns = (pawns & RANK_2) & pawns;
        doublePushedPawns = (((secondRankPawns << 8) & free) << 8) & free & targetMask;
        enemyBackrank = RANK_8;
    }
    else {
        pushedPawns = (pawns >> 8) & free & targetMask;
        secondRankPawns = (pawns & RANK_7) & pawns;
        doublePushedPawns = (((secondRankPawns >> 8) & free) >> 8) & free & targetMask;
        enemyBackrank = RANK_1;
    }

    while (pushedPawns) {
        Square target = popLSB(&pushedPawns);
        Move move = createMove(target - UP[board->stm], target);

        if ((C64(1) << target) & enemyBackrank) {
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
    Bitboard pawns = board->byPiece[PIECE_PAWN] & board->byColor[board->stm];
    Bitboard pAttacksLeft = pawnAttacksLeft(pawns, board->stm);
    Bitboard pAttacksRight = pawnAttacksRight(pawns, board->stm);
    Bitboard blockedEnemy = board->byColor[1 - board->stm];
    Bitboard enemyBackrank = board->stm == COLOR_WHITE ? RANK_8 : RANK_1;
    Bitboard free = ~(board->byColor[COLOR_WHITE] | board->byColor[COLOR_BLACK]);

    // Queen promotions (without capture)
    Bitboard pushedPawns;
    if (board->stm == COLOR_WHITE) {
        pushedPawns = (pawns << 8) & free & targetMask & RANK_8;
    }
    else {
        pushedPawns = (pawns >> 8) & free & targetMask & RANK_1;
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
    Bitboard rightCaptures = pAttacksRight & blockedEnemy & targetMask;
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
    if (epBB) {
        // Check handling: If we are using a checkers mask, and it doesn't match the captured square of this EP move, don't allow the EP
        Bitboard epCapt = board->stm == COLOR_WHITE ? (epBB >> 8) : (epBB << 8);
        if (~targetMask != C64(0) && !(epCapt & targetMask))
            return;

        Bitboard leftEp = pAttacksLeft & epBB;
        if (leftEp) {
            Square target = popLSB(&leftEp);
            Square origin = target - UP_LEFT[board->stm];

            // Don't make ep move if the pawn is blocking a check
            if (!(board->stack->blockers[board->stm] & (C64(1) << origin))) {
                *(*moves)++ = createMove(origin, target) | MOVE_ENPASSANT;
                (*counter)++;
            }
        }
        Bitboard rightEp = pAttacksRight & epBB;
        if (rightEp) {
            Square target = popLSB(&rightEp);
            Square origin = target - UP_RIGHT[board->stm];

            // Don't make ep move if the pawn is blocking a check
            if (!(board->stack->blockers[board->stm] & (C64(1) << origin))) {
                *(*moves)++ = createMove(origin, target) | MOVE_ENPASSANT;
                (*counter)++;
            }
        }
    }
}

// For all pieces other than pawns
template <Piece pieceType>
void generatePiece(Board* board, Move** moves, int* counter, bool captures, Bitboard targetMask) {
    Bitboard blockedUs = board->byColor[board->stm];
    Bitboard blockedEnemy = board->byColor[1 - board->stm];
    Bitboard occupied = blockedUs | blockedEnemy;

    Bitboard pieces = board->byPiece[pieceType] & blockedUs;
    while (pieces) {
        Square piece = popLSB(&pieces);
        Bitboard targets = pieceType == PIECE_KNIGHT ? knightAttacks(C64(1) << piece) : pieceType == PIECE_BISHOP ? getBishopMoves(piece, occupied) : pieceType == PIECE_ROOK ? getRookMoves(piece, occupied) : pieceType == PIECE_QUEEN ? (getRookMoves(piece, occupied) | getBishopMoves(piece, occupied)) : pieceType == PIECE_KING ? kingAttacks(board, board->stm) : C64(0);
        // Decide whether only captures or only non-captures
        if (captures)
            targets &= blockedEnemy & ~blockedUs & targetMask;
        else
            targets &= ~blockedEnemy & ~blockedUs & targetMask;

        while (targets) {
            Square target = popLSB(&targets);
            *(*moves)++ = createMove(piece, target);
            (*counter)++;
        }
    }
}

void generateCastling(Board* board, Move** moves, int* counter) {
    assert((board->byPiece[PIECE_KING] & board->byColor[board->stm]) > 0);

    Square king = lsb(board->byPiece[PIECE_KING] & board->byColor[board->stm]);
    Bitboard occupied = board->byColor[COLOR_WHITE] | board->byColor[COLOR_BLACK];

    // Castling: Nothing on the squares between king and rook
    // Checking if we are in check, or there are attacks in the way is done later (see isLegal())
    switch (board->stm) {
    case COLOR_WHITE:
        // Kingside
        if ((board->stack->castling & 0x1) && !(occupied & 0x60)/* && !(board->stack->attackedByColor[COLOR_BLACK] & 0x60)*/) {
            *(*moves)++ = createMove(king, king + 2) | MOVE_CASTLING;
            (*counter)++;
        }
        // Queenside
        if ((board->stack->castling & 0x2) && !(occupied & 0x0E)/* && !(board->stack->attackedByColor[COLOR_BLACK] & 0x0C)*/) {
            *(*moves)++ = createMove(king, king - 2) | MOVE_CASTLING;
            (*counter)++;
        }
        break;
    case COLOR_BLACK:
        // Kingside
        if ((board->stack->castling & 0x4) && !(occupied & C64(0x6000000000000000))/* && !(board->stack->attackedByColor[COLOR_WHITE] & C64(0x6000000000000000))*/) {
            *(*moves)++ = createMove(king, king + 2) | MOVE_CASTLING;
            (*counter)++;
        }
        // Queenside
        if ((board->stack->castling & 0x8) && !(occupied & C64(0x0E00000000000000))/* && !(board->stack->attackedByColor[COLOR_WHITE] & C64(0x0C00000000000000))*/) {
            *(*moves)++ = createMove(king, king - 2) | MOVE_CASTLING;
            (*counter)++;
        }
        break;
    }
}

void generateMoves(Board* board, Move* moves, int* counter, bool onlyCaptures) {
    assert((board->byColor[board->stm] & board->byPiece[PIECE_KING]) > 0);

    // If in double check, only generate king moves
    if (board->stack->checkerCount > 1) {
        generatePiece<PIECE_KING>(board, &moves, counter, true, ~C64(0));
        if (!onlyCaptures)
            generatePiece<PIECE_KING>(board, &moves, counter, false, ~C64(0));
        return;
    }

    // If in check, only generate targets that take care of the check
    Bitboard checkMask = board->stack->checkers ? BETWEEN[lsb(board->byColor[board->stm] & board->byPiece[PIECE_KING])][lsb(board->stack->checkers)] : ~C64(0);

    // ------------------- CAPTURES ----------------------
    generatePawn_capture(board, &moves, counter, checkMask);
    generatePiece<PIECE_KNIGHT>(board, &moves, counter, true, checkMask);
    generatePiece<PIECE_BISHOP>(board, &moves, counter, true, checkMask);
    generatePiece<PIECE_ROOK>(board, &moves, counter, true, checkMask);
    generatePiece<PIECE_QUEEN>(board, &moves, counter, true, checkMask);
    generatePiece<PIECE_KING>(board, &moves, counter, true, ~C64(0));

    // ------------------- NON CAPTURES ----------------------
    if (!onlyCaptures) {
        generatePawn_quiet(board, &moves, counter, checkMask);
        generatePiece<PIECE_KNIGHT>(board, &moves, counter, false, checkMask);
        generatePiece<PIECE_BISHOP>(board, &moves, counter, false, checkMask);
        generatePiece<PIECE_ROOK>(board, &moves, counter, false, checkMask);
        generatePiece<PIECE_QUEEN>(board, &moves, counter, false, checkMask);
        generatePiece<PIECE_KING>(board, &moves, counter, false, ~C64(0));
    }

    // ------------------- CASTLING ----------------------
    if (!onlyCaptures && !board->stack->checkers) {
        generateCastling(board, &moves, counter);
    }
}

Move MoveGen::nextMove() {
    // If there's still unused moves in the list, return those
    if (returnedMoves < generatedMoves) {
        assert(returnedMoves < MAX_MOVES && moveList[returnedMoves] != MOVE_NONE);

        return moveList[returnedMoves++];
    }

    assert((board->byColor[board->stm] & board->byPiece[PIECE_KING]) > 0);

    Move* moves = moveList + generatedMoves;

    while (returnedMoves >= generatedMoves && generationStage <= GEN_STAGE_REMAINING) {

        // Generate moves for the current stage
        switch (generationStage) {
        case GEN_STAGE_TTMOVE:
            if (ttMove != MOVE_NONE && ttMove != MOVE_NULL && isPseudoLegal(board, ttMove))
                moveList[generatedMoves++] = ttMove;
            generationStage++;
            break;

        case GEN_STAGE_CAPTURES: {
            int beginIndex = generatedMoves;

            // If in double check, only generate king moves
            if (board->stack->checkerCount > 1) {
                generatePiece<PIECE_KING>(board, &moves, &generatedMoves, true, ~C64(0));
            }
            else {
                // If in check, only generate targets that take care of the check
                Bitboard checkMask = board->stack->checkers ? BETWEEN[lsb(board->byColor[board->stm] & board->byPiece[PIECE_KING])][lsb(board->stack->checkers)] : ~C64(0);

                generatePawn_capture(board, &moves, &generatedMoves, checkMask);
                generatePiece<PIECE_KNIGHT>(board, &moves, &generatedMoves, true, checkMask);
                generatePiece<PIECE_BISHOP>(board, &moves, &generatedMoves, true, checkMask);
                generatePiece<PIECE_ROOK>(board, &moves, &generatedMoves, true, checkMask);
                generatePiece<PIECE_QUEEN>(board, &moves, &generatedMoves, true, checkMask);
                generatePiece<PIECE_KING>(board, &moves, &generatedMoves, true, ~C64(0));
            }
            int endIndex = generatedMoves;

            int scores[MAX_MOVES] = { 0 };
            for (int i = beginIndex; i < endIndex; i++) {
                Move move = moveList[i];
                int score;
                if (move == ttMove)
                    score = INT32_MIN;
                else if ((move & 0x3000) == MOVE_ENPASSANT)
                    score = 0;
                else if ((move & 0x3000) == MOVE_PROMOTION)
                    score = PIECE_VALUES[PROMOTION_PIECE[move >> 14]];
                else
                    score = PIECE_VALUES[board->pieces[moveTarget(move)]] - PIECE_VALUES[board->pieces[moveOrigin(move)]];
                scores[i] = score;
            }

            for (int i = beginIndex + 1; i < endIndex; i++) {
                int move = moveList[i];
                int score = scores[i];
                int j = i - 1;

                while (j >= beginIndex && scores[j] < score) {
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
        case GEN_STAGE_KILLERS:
            if (killers[0] != MOVE_NONE && isPseudoLegal(board, killers[0]))
                moveList[generatedMoves++] = killers[0];
            if (killers[1] != MOVE_NONE && isPseudoLegal(board, killers[1]))
                moveList[generatedMoves++] = killers[1];
            generationStage++;
            break;
        case GEN_STAGE_REMAINING: {
            int beginIndex = generatedMoves;
            // If in double check, only generate king moves
            if (board->stack->checkerCount > 1) {
                generatePiece<PIECE_KING>(board, &moves, &generatedMoves, false, ~C64(0));
            }
            else {
                // If in check, only generate targets that take care of the check
                Bitboard checkMask = board->stack->checkers ? BETWEEN[lsb(board->byColor[board->stm] & board->byPiece[PIECE_KING])][lsb(board->stack->checkers)] : ~C64(0);

                generatePawn_quiet(board, &moves, &generatedMoves, checkMask);
                generatePiece<PIECE_KNIGHT>(board, &moves, &generatedMoves, false, checkMask);
                generatePiece<PIECE_BISHOP>(board, &moves, &generatedMoves, false, checkMask);
                generatePiece<PIECE_ROOK>(board, &moves, &generatedMoves, false, checkMask);
                generatePiece<PIECE_QUEEN>(board, &moves, &generatedMoves, false, checkMask);
                generatePiece<PIECE_KING>(board, &moves, &generatedMoves, false, ~C64(0));
                if (!board->stack->checkers)
                    generateCastling(board, &moves, &generatedMoves);
            }
            int endIndex = generatedMoves;

            int scores[MAX_MOVES] = { 0 };
            for (int i = beginIndex; i < endIndex; i++) {
                Move move = moveList[i];
                int score = quietHistory[board->stm][moveOrigin(move)][moveTarget(move)];
                if (move == ttMove || move == killers[0] || move == killers[1])
                    score = INT32_MIN + 1;
                scores[i] = score;
            }

            for (int i = beginIndex + 1; i < endIndex; i++) {
                int move = moveList[i];
                int score = scores[i];
                int j = i - 1;

                while (j >= beginIndex && scores[j] < score) {
                    moveList[j + 1] = moveList[j];
                    scores[j + 1] = scores[j];
                    j--;
                }

                moveList[j + 1] = move;
                scores[j + 1] = score;
            }

            generationStage++;
        }
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