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
#include "spsa.h"

TUNE_INT(mpPromotionScoreFactor, 157, 10, 10000);
TUNE_INT(mpMvvLvaScoreFactor, 218, 10, 10000);
TUNE_INT(mpSeeDivisor, 79, 10, 150);

bool isPseudoLegal(Board* board, Move move) {
    Square origin = moveOrigin(move);
    Square target = moveTarget(move);
    Bitboard originBB = C64(1) << origin;
    Bitboard targetBB = C64(1) << target;
    Piece piece = board->pieces[origin];
    Move specialMove = move & 0x3000;

    // A valid piece needs to be on the origin square
    if (board->pieces[origin] == NO_PIECE) return false;
    // We can't capture our own piece, except in castling
    if (specialMove != MOVE_CASTLING && (board->byColor[board->stm] & targetBB)) return false;
    // We can't move the enemies piece
    if (board->byColor[1 - board->stm] & originBB) return false;

    // Non-standard movetypes
    switch (specialMove) {
    case MOVE_CASTLING: {
        if (piece != PIECE_KING || board->stack->checkers) return false;
        // Check for pieces between king and rook
        Square kingSquare = lsb(board->byColor[board->stm] & board->byPiece[PIECE_KING]);
        Square rookSquare = board->stm == COLOR_WHITE ? (target > origin ? board->castlingSquares[0] : board->castlingSquares[1]) : (target > origin ? board->castlingSquares[2] : board->castlingSquares[3]);
        Square kingTarget = board->stm == COLOR_WHITE ? (target > origin ? 6 : 2) : (target > origin ? 62 : 58);
        Square rookTarget = board->stm == COLOR_WHITE ? (target > origin ? 5 : 3) : (target > origin ? 61 : 59);
        Bitboard importantSquares = BETWEEN[rookSquare][rookTarget] | BETWEEN[kingSquare][kingTarget];
        importantSquares &= ~(C64(1) << rookSquare) & ~(C64(1) << kingSquare);
        if ((board->byColor[COLOR_WHITE] | board->byColor[COLOR_BLACK]) & importantSquares) return false;
        // Check for castling flags (Attackers are done in isLegal())
        return board->stack->castling & (board->stm == COLOR_WHITE ? (target > origin ? 0b0001 : 0b0010) : (target > origin ? 0b0100 : 0b1000));
    }
    case MOVE_ENPASSANT:
        if (piece != PIECE_PAWN) return false;
        if (board->stack->checkerCount > 1) return false;
        if (board->stack->checkers && (!(lsb(board->stack->checkers) == target - UP[board->stm]))) return false;
        // Check for EP flag on the right file
        return ((C64(1) << target) & board->stack->enpassantTarget) && board->pieces[target - UP[board->stm]] == PIECE_PAWN;
    case MOVE_PROMOTION:
        if (piece != PIECE_PAWN) return false;
        if (board->stack->checkerCount > 1) return false;
        if (board->stack->checkers) {
            bool capturesChecker = target == lsb(board->stack->checkers);
            bool blocksCheck = BETWEEN[lsb(board->byColor[board->stm] & board->byPiece[PIECE_KING])][lsb(board->stack->checkers)] & (C64(1) << target);
            if (!capturesChecker && !blocksCheck)
                return false;
        }
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
        if (!(KNIGHT_ATTACKS[origin] & targetBB)) return false;
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
        if (!(KING_ATTACKS[origin] & targetBB)) return false;
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
        Bitboard bishopAttacks = getBishopMoves(king, occupied) & (board->byPiece[PIECE_BISHOP] | board->byPiece[PIECE_QUEEN]) & board->byColor[1 - board->stm];
        return !rookAttacks && !bishopAttacks;
    }

    Bitboard occupied = board->byColor[COLOR_WHITE] | board->byColor[COLOR_BLACK];

    if (specialMove == MOVE_CASTLING) {
        // Check that none of the important squares (including the current king position!) are being attacked
        Square adjustedTarget = target < origin ? (board->stm == COLOR_WHITE ? 2 : 58) : (board->stm == COLOR_WHITE ? 6 : 62);
        if (adjustedTarget < origin) {
            for (Square s = adjustedTarget; s <= origin; s++) {
                if (board->byColor[1 - board->stm] & attackersTo(board, s, occupied))
                    return false;
            }
        }
        else {
            for (Square s = adjustedTarget; s >= origin; s--) {
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
        Bitboard targets = pieceType == PIECE_KNIGHT ? KNIGHT_ATTACKS[piece] : pieceType == PIECE_BISHOP ? getBishopMoves(piece, occupied) : pieceType == PIECE_ROOK ? getRookMoves(piece, occupied) : pieceType == PIECE_QUEEN ? (getRookMoves(piece, occupied) | getBishopMoves(piece, occupied)) : pieceType == PIECE_KING ? kingAttacks(board, board->stm) : C64(0);
        targets &= mask;

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
        if ((board->stack->castling & 0x1)) {
            Square kingsideRook = board->castlingSquares[0];
            Bitboard between = BETWEEN[kingsideRook][5] | BETWEEN[king][6];
            between &= ~(C64(1) << kingsideRook) & ~(C64(1) << king);
            if (!(occupied & between)) {
                *(*moves)++ = createMove(king, kingsideRook) | MOVE_CASTLING;
                (*counter)++;
            }
        }
        // Queenside
        if ((board->stack->castling & 0x2)) {
            Square queensideRook = board->castlingSquares[1];
            Bitboard between = BETWEEN[queensideRook][3] | BETWEEN[king][2];
            between &= ~(C64(1) << queensideRook) & ~(C64(1) << king);
            if (!(occupied & between)) {
                *(*moves)++ = createMove(king, queensideRook) | MOVE_CASTLING;
                (*counter)++;
            }
        }
        break;
    case COLOR_BLACK:
        // Kingside
        if ((board->stack->castling & 0x4)) {
            Square kingsideRook = board->castlingSquares[2];
            Bitboard between = BETWEEN[kingsideRook][61] | BETWEEN[king][62];
            between &= ~(C64(1) << kingsideRook) & ~(C64(1) << king);
            if (!(occupied & between)) {
                *(*moves)++ = createMove(king, kingsideRook) | MOVE_CASTLING;
                (*counter)++;
            }
        }
        // Queenside
        if ((board->stack->castling & 0x8)) {
            Square queensideRook = board->castlingSquares[3];
            Bitboard between = BETWEEN[queensideRook][59] | BETWEEN[king][58];
            between &= ~(C64(1) << queensideRook) & ~(C64(1) << king);
            if (!(occupied & between)) {
                *(*moves)++ = createMove(king, queensideRook) | MOVE_CASTLING;
                (*counter)++;
            }
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
    assert((board->byColor[board->stm] & board->byPiece[PIECE_KING]) > 0);

    Move* moves;
    int beginIndex, endIndex;

    // Generate moves for the current stage
    switch (generationStage) {
    case GEN_STAGE_TTMOVE:

        generationStage++;
        if (ttMove != MOVE_NONE && ttMove != MOVE_NULL && isPseudoLegal(board, ttMove)) {
            return ttMove;
        }

        [[fallthrough]];

    case GEN_STATE_GEN_CAPTURES:
        beginIndex = generatedMoves;
        moves = moveList + generatedMoves;
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
        endIndex = generatedMoves;

        endIndex = scoreCaptures(beginIndex, endIndex);
        sortMoves(moveList, moveListScores, beginIndex, endIndex);

        generationStage++;
        [[fallthrough]];

    case GEN_STAGE_CAPTURES:

        while (returnedMoves < generatedMoves) {
            Move move = moveList[returnedMoves];
            int score = moveListScores[returnedMoves++];

            bool goodCapture = probCut ? SEE(board, move, probCutThreshold) : (onlyCaptures || SEE(board, move, -score / mpSeeDivisor));
            if (!goodCapture) {
                badCaptureList[generatedBadCaptures++] = move;
                continue;
            }
            return move;
        }

        if (probCut || onlyCaptures) {
            generationStage = GEN_STAGE_DONE;
            return MOVE_NONE;
        }
        generationStage++;
        [[fallthrough]];

    case GEN_STAGE_KILLERS:

        while (killerCount < 2) {
            Move killer = killers[killerCount++];

            if (killer != MOVE_NONE && killer != ttMove && isPseudoLegal(board, killer))
                return killer;
        }

        generationStage++;
        [[fallthrough]];

    case GEN_STAGE_COUNTERMOVES:

        generationStage++;
        if (counterMove != MOVE_NONE && counterMove != ttMove && counterMove != killers[0] && counterMove != killers[1] && !isCapture(board, counterMove) && isPseudoLegal(board, counterMove))
            return counterMove;

        [[fallthrough]];

    case GEN_STAGE_GEN_REMAINING:
        beginIndex = generatedMoves;
        moves = moveList + generatedMoves;
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
        endIndex = generatedMoves;

        endIndex = scoreQuiets(beginIndex, endIndex);
        sortMoves(moveList, moveListScores, beginIndex, endIndex);

        generationStage++;
        [[fallthrough]];

    case GEN_STAGE_REMAINING:

        if (returnedMoves < generatedMoves)
            return moveList[returnedMoves++];

        generationStage++;
        [[fallthrough]];

    case GEN_STAGE_BAD_CAPTURES:

        if (returnedBadCaptures < generatedBadCaptures)
            return badCaptureList[returnedBadCaptures++];

        generationStage = GEN_STAGE_DONE;
    }

    return MOVE_NONE;
}

int MoveGen::scoreCaptures(int beginIndex, int endIndex) {
    for (int i = beginIndex; i < endIndex; i++) {
        Move move = moveList[i];

        // Skip all previously searched moves
        if (move == ttMove) {
            moveList[i] = moveList[endIndex - 1];
            moveList[endIndex - 1] = MOVE_NONE;
            endIndex--;
            generatedMoves--;
            i--;
            continue;
        }

        int score = *history->getCaptureHistory(board, move);
        if ((move & 0x3000) == MOVE_ENPASSANT)
            score += 0;
        else if ((move & 0x3000) == MOVE_PROMOTION)
            score += PIECE_VALUES[PROMOTION_PIECE[move >> 14]] * mpPromotionScoreFactor / 100;
        else
            score += (PIECE_VALUES[board->pieces[moveTarget(move)]] - PIECE_VALUES[board->pieces[moveOrigin(move)]]) * mpMvvLvaScoreFactor / 100;

        moveListScores[i] = score;
    }
    return endIndex;
}

int MoveGen::scoreQuiets(int beginIndex, int endIndex) {
    Bitboard pawnThreats = attackedSquaresByPiece(board, 1 - board->stm, PIECE_PAWN);
    Bitboard knightThreats = attackedSquaresByPiece(board, 1 - board->stm, PIECE_KNIGHT);
    Bitboard bishopThreats = attackedSquaresByPiece(board, 1 - board->stm, PIECE_BISHOP);
    Bitboard rookThreats = attackedSquaresByPiece(board, 1 - board->stm, PIECE_ROOK);

    for (int i = beginIndex; i < endIndex; i++) {
        Move move = moveList[i];

        // Skip all previously searched moves
        if (move == ttMove || move == killers[0] || move == killers[1] || move == counterMove) {
            moveList[i] = moveList[endIndex - 1];
            moveList[endIndex - 1] = MOVE_NONE;
            endIndex--;
            generatedMoves--;
            i--;
            continue;
        }

        int threatScore = 0;
        Piece piece = board->pieces[moveOrigin(move)];
        Bitboard fromBB = C64(1) << moveOrigin(move);
        Bitboard toBB = C64(1) << moveTarget(move);
        if (piece == PIECE_QUEEN) {
            if (fromBB & (pawnThreats | knightThreats | bishopThreats | rookThreats))
                threatScore += 30000;
            if (toBB & (pawnThreats | knightThreats | bishopThreats | rookThreats))
                threatScore -= 30000;
        } else if (piece == PIECE_ROOK) {
            if (fromBB & (pawnThreats | knightThreats | bishopThreats))
                threatScore += 15000;
            if (toBB & (pawnThreats | knightThreats | bishopThreats))
                threatScore -= 15000;
        } else if (piece == PIECE_KNIGHT || piece == PIECE_BISHOP) {
            if (fromBB & pawnThreats)
                threatScore += 15000;
            if (toBB & pawnThreats)
                threatScore -= 15000;
        }

        moveListScores[i] = history->getHistory(board, searchStack, move, false) + threatScore;
    }
    return endIndex;
}

void MoveGen::sortMoves(Move* moves, int* scores, int beginIndex, int endIndex) {
    int limit = -3500 * depth;
    for (int i = beginIndex + 1; i < endIndex; i++) {
        if (scores[i] < limit)
            continue;
        Move move = moves[i];
        int score = scores[i];
        int j = i - 1;

        while (j >= beginIndex && scores[j] < score) {
            moves[j + 1] = moves[j];
            scores[j + 1] = scores[j];
            j--;
        }

        moves[j + 1] = move;
        scores[j + 1] = score;
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
        int rank = origin / 8;
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
        if (board->chess960 && board->pieces[origin] == PIECE_KING && board->pieces[target] == PIECE_ROOK && !((C64(1) << target) & board->byColor[1 - board->stm]))
            move |= MOVE_CASTLING;
        if (!board->chess960 && board->pieces[origin] == PIECE_KING && std::abs(target - origin) == 2)
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