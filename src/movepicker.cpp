#include <inttypes.h>
#include <stdio.h>
#include <cassert>
#include <iostream>

#include "movepicker.h"
#include "board.h"
#include "types.h"
#include "bitboard.h"
#include "evaluation.h"
#include "tt.h"
#include "spsa.h"
#include "history.h"

TUNE_INT_DISABLED(mpPromotionScoreFactor, 101, 10, 10000);
TUNE_INT_DISABLED(mpMvvLvaScoreFactor, 147, 10, 10000);
TUNE_INT_DISABLED(mpSeeDivisor, 83, 10, 150);

// Main search
MovePicker::MovePicker(Board* board, History* history, SearchStack* searchStack, Move ttMove, Depth depth) : board(board), history(history), searchStack(searchStack), ttMove(ttMove), onlyCaptures(false), killer(searchStack->killer), returnedMoves(0), returnedBadCaptures(0), stage(STAGE_TTMOVE), depth(depth), probCut(false), probCutThreshold(0), skipQuiets(false) {
    counterMove = searchStack->ply > 0 ? history->getCounterMove((searchStack - 1)->move) : Move::none();
}

// qSearch
MovePicker::MovePicker(Board* board, History* history, SearchStack* searchStack, Move ttMove, bool onlyCaptures, Depth depth) : board(board), history(history), searchStack(searchStack), ttMove(ttMove), onlyCaptures(onlyCaptures), killer(Move::none()), returnedMoves(0), returnedBadCaptures(0), stage(STAGE_TTMOVE), depth(depth), probCut(false), probCutThreshold(0), skipQuiets(false) {
    counterMove = onlyCaptures || searchStack->ply == 0 ? Move::none() : history->getCounterMove((searchStack - 1)->move);
}

// ProbCut
MovePicker::MovePicker(Board* board, History* history, SearchStack* searchStack, Move ttMove, int probCutThreshold, Depth depth) : board(board), history(history), searchStack(searchStack), ttMove(ttMove), onlyCaptures(true), killer(Move::none()), returnedMoves(0), returnedBadCaptures(0), stage(STAGE_TTMOVE), depth(depth), probCut(true), probCutThreshold(probCutThreshold), skipQuiets(false) {
    counterMove = Move::none();
}

Move MovePicker::nextMove() {
    assert((board->byColor[board->stm] & board->byPiece[Piece::KING]) > 0);

    if (skipQuiets && stage >= STAGE_KILLER && stage != STAGE_DONE)
        stage = STAGE_PLAY_BAD_CAPTURES;

    // Generate moves for the current stage
    switch (stage) {
    case STAGE_TTMOVE:

        ++stage;
        if (ttMove && board->isPseudoLegal(ttMove)) {
            return ttMove;
        }

        [[fallthrough]];

    case STAGE_GEN_CAPTURES:
        
        MoveGen::generateMoves<MoveGenMode::CAPTURES>(board, moveList);
        scoreCaptures();
        sortMoves();

        ++stage;
        [[fallthrough]];

    case STAGE_PLAY_GOOD_CAPTURES:

        while (returnedMoves < moveList.size()) {
            Move move = moveList[returnedMoves];
            int score = moveListScores[returnedMoves++];

            bool goodCapture = probCut ? SEE(board, move, probCutThreshold) : SEE(board, move, -score / mpSeeDivisor);
            if (!goodCapture) {
                badCaptureList.add(move);
                continue;
            }
            return move;
        }

        if (probCut || onlyCaptures) {
            stage = STAGE_DONE;
            return Move::none();
        }
        ++stage;
        [[fallthrough]];

    case STAGE_KILLER:

        ++stage;

        if (killer && killer != ttMove && board->isPseudoLegal(killer))
            return killer;

        [[fallthrough]];

    case STAGE_COUNTERS:

        ++stage;
        if (counterMove && counterMove != ttMove && counterMove != killer && !board->isCapture(counterMove) && board->isPseudoLegal(counterMove))
            return counterMove;

        [[fallthrough]];

    case STAGE_GEN_QUIETS:

        MoveGen::generateMoves<MoveGenMode::QUIETS>(board, moveList);
        scoreQuiets();
        sortMoves();

        ++stage;
        [[fallthrough]];

    case STAGE_PLAY_QUIETS:

        if (returnedMoves < moveList.size())
            return moveList[returnedMoves++];

        ++stage;
        [[fallthrough]];

    case STAGE_PLAY_BAD_CAPTURES:

        if (returnedBadCaptures < badCaptureList.size())
            return badCaptureList[returnedBadCaptures++];

        stage = STAGE_DONE;
        break;

    default:
        break;
    }

    return Move::none();
}

void MovePicker::scoreCaptures() {
    for (int i = returnedMoves; i < moveList.size(); i++) {
        Move move = moveList[i];

        // Skip all previously searched moves
        if (move == ttMove) {
            moveList.remove(i--);
            continue;
        }

        int score = history->getCaptureHistory(board, move);
        switch (move.type()) {
            case MoveType::ENPASSANT:
                score += 0;    
                break;
            case MoveType::PROMOTION:
                score += PIECE_VALUES[move.promotionPiece()] * mpPromotionScoreFactor / 100;
                break;
            default:
                score += (PIECE_VALUES[board->pieces[move.target()]] - PIECE_VALUES[board->pieces[move.origin()]]) * mpMvvLvaScoreFactor / 100;
                break;
        }

        moveListScores.add(score);
    }
}

void MovePicker::scoreQuiets() {
    Threats& threats = board->threats;

    for (int i = returnedMoves; i < moveList.size(); i++) {
        Move move = moveList[i];

        // Skip all previously searched moves
        if (move == ttMove || move == killer || move == counterMove) {
            moveList.remove(i--);
            continue;
        }

        int threatScore = 0;
        Piece piece = board->pieces[move.origin()];
        Bitboard fromBB = bitboard(move.origin());
        Bitboard toBB = bitboard(move.target());
        if (piece == Piece::QUEEN) {
            if (fromBB & (threats.pawnThreats | threats.knightThreats | threats.bishopThreats | threats.rookThreats))
                threatScore += 20000;
            if (toBB & (threats.pawnThreats | threats.knightThreats | threats.bishopThreats | threats.rookThreats))
                threatScore -= 20000;
        }
        else if (piece == Piece::ROOK) {
            if (fromBB & (threats.pawnThreats | threats.knightThreats | threats.bishopThreats))
                threatScore += 12500;
            if (toBB & (threats.pawnThreats | threats.knightThreats | threats.bishopThreats))
                threatScore -= 12500;
        }
        else if (piece == Piece::KNIGHT || piece == Piece::BISHOP) {
            if (fromBB & threats.pawnThreats)
                threatScore += 7500;
            if (toBB & threats.pawnThreats)
                threatScore -= 7500;
        }

        moveListScores.add(history->getHistory(board, searchStack, move, false) + threatScore);
    }
}

void MovePicker::sortMoves() {
    int limit = -3500 * depth;
    for (int i = returnedMoves + 1; i < moveList.size(); i++) {
        if (moveListScores[i] < limit)
            continue;
            
        Move move = moveList[i];
        int score = moveListScores[i];

        int j = i - 1;
        while (j >= returnedMoves && moveListScores[j] < score) {
            moveList[j + 1] = moveList[j];
            moveListScores[j + 1] = moveListScores[j];
            j--;
        }

        moveList[j + 1] = move;
        moveListScores[j + 1] = score;
    }
}