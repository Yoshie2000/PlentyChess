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
#include "history.h"

TUNE_INT_DISABLED(mpPromotionScoreFactor, 101, 10, 10000);
TUNE_INT_DISABLED(mpMvvLvaScoreFactor, 147, 10, 10000);
TUNE_INT_DISABLED(mpSeeDivisor, 83, 10, 150);

void generatePawn_quiet(Board* board, MoveList& moves, Bitboard targetMask) {
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
        Square origin = target - Direction::UP[board->stm];

        if (bitboard(target) & enemyBackrank) {
            // Promotion: Queen promotions are considered captures
            for (Piece promotionPiece : { Piece::ROOK, Piece::BISHOP, Piece::KNIGHT }) {
                moves.add(Move::makePromotion(origin, target, promotionPiece));
            }
            continue;
        }

        moves.add(Move::makeNormal(origin, target));
    }

    // Double pushes
    while (doublePushedPawns) {
        Square target = popLSB(&doublePushedPawns);
        moves.add(Move::makeNormal(target - Direction::UP_DOUBLE[board->stm], target));
    }
}

void generatePawn_capture(Board* board, MoveList& moves, Bitboard targetMask) {
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
        moves.add(Move::makePromotion(target - Direction::UP[board->stm], target, Piece::QUEEN));
    }

    // Capture promotions
    Bitboard leftCaptures = pAttacksLeft & blockedEnemy & targetMask;
    while (leftCaptures) {
        Square target = popLSB(&leftCaptures);
        Square origin = target - Direction::UP_LEFT[board->stm];

        if (bitboard(target) & enemyBackrank) {
            // Promotion
            for (Piece promotionPiece : { Piece::QUEEN, Piece::ROOK, Piece::BISHOP, Piece::KNIGHT }) {
                moves.add(Move::makePromotion(origin, target, promotionPiece));
            }
            continue;
        }

        moves.add(Move::makeNormal(origin, target));
    }
    Bitboard rightCaptures = pAttacksRight & blockedEnemy & targetMask;
    while (rightCaptures) {
        Square target = popLSB(&rightCaptures);
        Square origin = target - Direction::UP_RIGHT[board->stm];

        if (bitboard(target) & enemyBackrank) {
            // Promotion
            for (Piece promotionPiece : { Piece::QUEEN, Piece::ROOK, Piece::BISHOP, Piece::KNIGHT }) {
                moves.add(Move::makePromotion(origin, target, promotionPiece));
            }
            continue;
        }

        moves.add(Move::makeNormal(origin, target));
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
            Square origin = target - Direction::UP_LEFT[board->stm];
            moves.add(Move::makeEnpassant(origin, target));
        }
        Bitboard rightEp = pAttacksRight & epBB;
        if (rightEp) {
            Square target = popLSB(&rightEp);
            Square origin = target - Direction::UP_RIGHT[board->stm];
            moves.add(Move::makeEnpassant(origin, target));
        }
    }
}

// For all pieces other than pawns
template <Piece pieceType>
void generatePiece(Board* board, MoveList& moves, bool captures, Bitboard targetMask) {
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
            moves.add(Move::makeNormal(piece, target));
        }
    }
}

void generateCastling(Board* board, MoveList& moves) {
    assert((board->byPiece[Piece::KING] & board->byColor[board->stm]) > 0);

    Square king = lsb(board->byPiece[Piece::KING] & board->byColor[board->stm]);
    Bitboard occupied = board->byColor[Color::WHITE] | board->byColor[Color::BLACK];

    // Castling: Nothing on the squares between king and rook
    // Checking if we are in check, or there are attacks in the way is done later (see isLegal())
    for (auto direction : { Castling::KINGSIDE, Castling::QUEENSIDE }) {
        if (board->castling & Castling::getMask(board->stm, direction)) {
            Square rookOrigin = board->getCastlingRookSquare(board->stm, direction);
            Square rookTarget = Castling::getRookSquare(board->stm, direction);
            Square kingTarget = Castling::getKingSquare(board->stm, direction);

            Bitboard between = BB::BETWEEN[rookOrigin][rookTarget] | BB::BETWEEN[king][kingTarget];
            between &= ~bitboard(rookOrigin) & ~bitboard(king);
            if (!(occupied & between)) {
                moves.add(Move::makeCastling(king, rookOrigin));
            }
        }
    }
}

void generateMoves(Board* board, MoveList& moves, bool onlyCaptures) {
    assert((board->byColor[board->stm] & board->byPiece[Piece::KING]) > 0);

    // If in double check, only generate king moves
    if (board->checkerCount > 1) {
        generatePiece<Piece::KING>(board, moves, true, ~bitboard(0));
        if (!onlyCaptures)
            generatePiece<Piece::KING>(board, moves, false, ~bitboard(0));
        return;
    }

    // If in check, only generate targets that take care of the check
    Bitboard checkMask = board->checkers ? BB::BETWEEN[lsb(board->byColor[board->stm] & board->byPiece[Piece::KING])][lsb(board->checkers)] : ~bitboard(0);

    // ------------------- CAPTURES ----------------------
    generatePawn_capture(board, moves, checkMask);
    generatePiece<Piece::KNIGHT>(board, moves, true, checkMask);
    generatePiece<Piece::BISHOP>(board, moves, true, checkMask);
    generatePiece<Piece::ROOK>(board, moves, true, checkMask);
    generatePiece<Piece::QUEEN>(board, moves, true, checkMask);
    generatePiece<Piece::KING>(board, moves, true, ~bitboard(0));

    // ------------------- NON CAPTURES ----------------------
    if (!onlyCaptures) {
        generatePawn_quiet(board, moves, checkMask);
        generatePiece<Piece::KNIGHT>(board, moves, false, checkMask);
        generatePiece<Piece::BISHOP>(board, moves, false, checkMask);
        generatePiece<Piece::ROOK>(board, moves, false, checkMask);
        generatePiece<Piece::QUEEN>(board, moves, false, checkMask);
        generatePiece<Piece::KING>(board, moves, false, ~bitboard(0));
    }

    // ------------------- CASTLING ----------------------
    if (!onlyCaptures && !board->checkers) {
        generateCastling(board, moves);
    }
}

// Main search
MoveGen::MoveGen(Board* board, History* history, SearchStack* searchStack, Move ttMove, Depth depth) : board(board), history(history), searchStack(searchStack), ttMove(ttMove), onlyCaptures(false), killer(searchStack->killer), returnedMoves(0), returnedBadCaptures(0), stage(STAGE_TTMOVE), depth(depth), probCut(false), probCutThreshold(0), skipQuiets(false) {
    counterMove = searchStack->ply > 0 ? history->getCounterMove((searchStack - 1)->move) : Move::none();
}

// qSearch
MoveGen::MoveGen(Board* board, History* history, SearchStack* searchStack, Move ttMove, bool onlyCaptures, Depth depth) : board(board), history(history), searchStack(searchStack), ttMove(ttMove), onlyCaptures(onlyCaptures), killer(Move::none()), returnedMoves(0), returnedBadCaptures(0), stage(STAGE_TTMOVE), depth(depth), probCut(false), probCutThreshold(0), skipQuiets(false) {
    counterMove = onlyCaptures || searchStack->ply == 0 ? Move::none() : history->getCounterMove((searchStack - 1)->move);
}

// ProbCut
MoveGen::MoveGen(Board* board, History* history, SearchStack* searchStack, Move ttMove, int probCutThreshold, Depth depth) : board(board), history(history), searchStack(searchStack), ttMove(ttMove), onlyCaptures(true), killer(Move::none()), returnedMoves(0), returnedBadCaptures(0), stage(STAGE_TTMOVE), depth(depth), probCut(true), probCutThreshold(probCutThreshold), skipQuiets(false) {
    counterMove = Move::none();
}

Move MoveGen::nextMove() {
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
        // If in double check, only generate king moves
        if (board->checkerCount > 1) {
            generatePiece<Piece::KING>(board, moveList, true, ~bitboard(0));
        }
        else {
            // If in check, only generate targets that take care of the check
            Bitboard checkMask = board->checkers ? BB::BETWEEN[lsb(board->byColor[board->stm] & board->byPiece[Piece::KING])][lsb(board->checkers)] : ~bitboard(0);

            generatePawn_capture(board, moveList, checkMask);
            generatePiece<Piece::KNIGHT>(board, moveList, true, checkMask);
            generatePiece<Piece::BISHOP>(board, moveList, true, checkMask);
            generatePiece<Piece::ROOK>(board, moveList, true, checkMask);
            generatePiece<Piece::QUEEN>(board, moveList, true, checkMask);
            generatePiece<Piece::KING>(board, moveList, true, ~bitboard(0));
        }

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
        // If in double check, only generate king moves
        if (board->checkerCount > 1) {
            generatePiece<Piece::KING>(board, moveList, false, ~bitboard(0));
        }
        else {
            // If in check, only generate targets that take care of the check
            Bitboard checkMask = board->checkers ? BB::BETWEEN[lsb(board->byColor[board->stm] & board->byPiece[Piece::KING])][lsb(board->checkers)] : ~bitboard(0);

            generatePawn_quiet(board, moveList, checkMask);
            generatePiece<Piece::KNIGHT>(board, moveList, false, checkMask);
            generatePiece<Piece::BISHOP>(board, moveList, false, checkMask);
            generatePiece<Piece::ROOK>(board, moveList, false, checkMask);
            generatePiece<Piece::QUEEN>(board, moveList, false, checkMask);
            generatePiece<Piece::KING>(board, moveList, false, ~bitboard(0));
            if (!board->checkers)
                generateCastling(board, moveList);
        }

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

void MoveGen::scoreCaptures() {
    for (int i = returnedMoves; i < moveList.size(); i++) {
        Move move = moveList[i];

        // Skip all previously searched moves
        if (move == ttMove) {
            moveList.remove(i--);
            continue;
        }

        int score = *history->getCaptureHistory(board, move);
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

void MoveGen::scoreQuiets() {
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

        moveListScores.add(history->getWeightedQuietHistory(board, searchStack, move, std::make_tuple(100, 100, 400, 200, 0, 200, 100)) + threatScore);
    }
}

void MoveGen::sortMoves() {
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

Square stringToSquare(const char* string) {
    int file = string[0] - 'a';
    int rank = string[1] - '1';
    return (Square)(8 * rank + file);
}

Move stringToMove(const char* string, Board* board) {
    Square origin = stringToSquare(&string[0]);
    Square target = stringToSquare(&string[2]);
    Move move;

    switch (string[4]) {
    case 'q':
        move = Move::makePromotion(origin, target, Piece::QUEEN);
        break;
    case 'r':
        move = Move::makePromotion(origin, target, Piece::ROOK);
        break;
    case 'b':
        move = Move::makePromotion(origin, target, Piece::BISHOP);
        break;
    case 'n':
        move = Move::makePromotion(origin, target, Piece::KNIGHT);
        break;
    default:
        // Figure out whether this is en passent or castling and set the flags accordingly
        if (board->chess960 && board->pieces[origin] == Piece::KING && board->pieces[target] == Piece::ROOK && !(bitboard(target) & board->byColor[1 - board->stm]))
            move = Move::makeCastling(origin, target);
        else if (!board->chess960 && board->pieces[origin] == Piece::KING && std::abs(target - origin) == 2)
            move = Move::makeCastling(origin, target);
        else if (board->pieces[origin] == Piece::PAWN && board->pieces[target] == Piece::NONE && (std::abs(target - origin) == 7 || std::abs(target - origin) == 9))
            move = Move::makeEnpassant(origin, target);
        else
            move = Move::makeNormal(origin, target);
    }

    assert(move);

    return move;
}