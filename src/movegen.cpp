#include "types.h"
#include "board.h"
#include "movegen.h"
#include "magic.h"

void generatePawn_quiet(Board* board, MoveList& moves, Bitboard targetMask = ~bitboard(0)) {
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

void generatePawn_capture(Board* board, MoveList& moves, Bitboard targetMask = ~bitboard(0)) {
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
void generatePiece(Board* board, MoveList& moves, bool captures, Bitboard targetMask = ~bitboard(0)) {
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

template<MoveGenMode MODE>
void MoveGen::generateMoves(Board* board, MoveList& moves) {
    assert((board->byColor[board->stm] & board->byPiece[Piece::KING]) > 0);

    // If in double check, only generate king moves
    if (board->checkerCount > 1) {
        if (MODE & MoveGenMode::CAPTURES)
            generatePiece<Piece::KING>(board, moves, true);
        if (MODE & MoveGenMode::QUIETS)
            generatePiece<Piece::KING>(board, moves, false);
        return;
    }

    // If in check, only generate targets that take care of the check
    Bitboard checkMask = board->checkers ? BB::BETWEEN[lsb(board->byColor[board->stm] & board->byPiece[Piece::KING])][lsb(board->checkers)] : ~bitboard(0);

    if (MODE & MoveGenMode::CAPTURES) {
        generatePawn_capture(board, moves, checkMask);
        generatePiece<Piece::KNIGHT>(board, moves, true, checkMask);
        generatePiece<Piece::BISHOP>(board, moves, true, checkMask);
        generatePiece<Piece::ROOK>(board, moves, true, checkMask);
        generatePiece<Piece::QUEEN>(board, moves, true, checkMask);
        generatePiece<Piece::KING>(board, moves, true);
    }

    if (MODE & MoveGenMode::QUIETS) {
        generatePawn_quiet(board, moves, checkMask);
        generatePiece<Piece::KNIGHT>(board, moves, false, checkMask);
        generatePiece<Piece::BISHOP>(board, moves, false, checkMask);
        generatePiece<Piece::ROOK>(board, moves, false, checkMask);
        generatePiece<Piece::QUEEN>(board, moves, false, checkMask);
        generatePiece<Piece::KING>(board, moves, false);

        if (!board->checkers) {
            generateCastling(board, moves);
        }
    }
}

template void MoveGen::generateMoves<MoveGenMode::ALL>(Board* board, MoveList& moves);
template void MoveGen::generateMoves<MoveGenMode::QUIETS>(Board* board, MoveList& moves);
template void MoveGen::generateMoves<MoveGenMode::CAPTURES>(Board* board, MoveList& moves);

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