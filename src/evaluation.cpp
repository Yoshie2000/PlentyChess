#include <iostream>
#include <cassert>

#include "types.h"
#include "board.h"
#include "evaluation.h"
#include "magic.h"

const Eval PIECE_VALUES[PIECE_TYPES] = {
    100, 290, 310, 500, 900, 0
};

const Eval SEE_VALUES[PIECE_TYPES + 1] = {
    100, 300, 300, 500, 900, 0, 0
};

const PhaseEval PSQ[PIECE_TYPES][64] = {
    {
        // pawn
        PhaseEval(0, 0), PhaseEval(0, 0), PhaseEval(0, 0), PhaseEval(0, 0), PhaseEval(0, 0), PhaseEval(0, 0), PhaseEval(0, 0), PhaseEval(0, 0),
        PhaseEval(98,178),  PhaseEval(134,173), PhaseEval(61,158),  PhaseEval(95,134),  PhaseEval(68,147),  PhaseEval(126,132), PhaseEval(34,165),  PhaseEval(-11,187),
        PhaseEval(-6,94),  PhaseEval(7,100),   PhaseEval(26,85),  PhaseEval(31,67),  PhaseEval(65,56),  PhaseEval(56,53),  PhaseEval(25,82),  PhaseEval(-20,84),
        PhaseEval(-14,32), PhaseEval(13,23),  PhaseEval(6,13),   PhaseEval(21,5),  PhaseEval(23,-2),  PhaseEval(12,4),  PhaseEval(17,17),  PhaseEval(-23,17),
        PhaseEval(-27,13), PhaseEval(-2,9),  PhaseEval(-5,-3),  PhaseEval(12,-7),  PhaseEval(17,-7),  PhaseEval(6,-8),   PhaseEval(10,3),  PhaseEval(-25,-1),
        PhaseEval(-26,4), PhaseEval(-4,7),  PhaseEval(-4,-6),  PhaseEval(-10,1), PhaseEval(3,0),   PhaseEval(3,-5),   PhaseEval(33,-1),  PhaseEval(-12,-8),
        PhaseEval(-35,13), PhaseEval(-1,8),  PhaseEval(-20,8), PhaseEval(-23,10), PhaseEval(-15,13), PhaseEval(24,0),  PhaseEval(38,2),  PhaseEval(-22,-7),
        PhaseEval(0, 0), PhaseEval(0, 0), PhaseEval(0, 0), PhaseEval(0, 0), PhaseEval(0, 0), PhaseEval(0, 0), PhaseEval(0, 0), PhaseEval(0, 0),
    },
    {
        // knight
        PhaseEval(-167,-58), PhaseEval(-89,-38), PhaseEval(-34,-13), PhaseEval(-49,-28), PhaseEval(61,-31),  PhaseEval(-97,-27), PhaseEval(-15,-63), PhaseEval(-107,-99),
        PhaseEval(-73,-25),  PhaseEval(-41,-8), PhaseEval(72,-25),  PhaseEval(36,-2),  PhaseEval(23,-9),  PhaseEval(62,-25),  PhaseEval(7,-24),   PhaseEval(-17,-52),
        PhaseEval(-47,-24),  PhaseEval(60,-20),  PhaseEval(37,10),  PhaseEval(65,9),  PhaseEval(84,-1),  PhaseEval(129,-9), PhaseEval(73,-19),  PhaseEval(44,-41),
        PhaseEval(-9,-17),   PhaseEval(17,3),  PhaseEval(19,22),  PhaseEval(53,22),  PhaseEval(37,22),  PhaseEval(69,11),  PhaseEval(18,8),  PhaseEval(22,-18),
        PhaseEval(-13,-18),  PhaseEval(4,-6),   PhaseEval(16,16),  PhaseEval(13,25),  PhaseEval(28,16),  PhaseEval(19,17),  PhaseEval(21,4),  PhaseEval(-8,-18),
        PhaseEval(-23,-23),  PhaseEval(-9,-3),  PhaseEval(12,-1),  PhaseEval(10,15),  PhaseEval(19,10),  PhaseEval(17,-3),  PhaseEval(25,-20),  PhaseEval(-16,-22),
        PhaseEval(-29,-42),  PhaseEval(-53,-20), PhaseEval(-12,-10), PhaseEval(-3,-5),  PhaseEval(-1,-2),  PhaseEval(18,-20),  PhaseEval(-14,-23), PhaseEval(-19,-44),
        PhaseEval(-105,-29), PhaseEval(-21,-51), PhaseEval(-58,-23), PhaseEval(-33,-15), PhaseEval(-17,-22), PhaseEval(-28,-18), PhaseEval(-19,-50), PhaseEval(-23,-64),
    },
    {
        // bishop
        PhaseEval(-29, -14),  PhaseEval(4, -21), PhaseEval(-82, -11), PhaseEval(-37, -8), PhaseEval(-25, -7), PhaseEval(-42, -9), PhaseEval(7, -17), PhaseEval(-8, -24),
        PhaseEval(-26, -8),  PhaseEval(16, -4), PhaseEval(-18, 7), PhaseEval(-13, -12), PhaseEval(30, -3), PhaseEval(59, -13), PhaseEval(18, -4), PhaseEval(-47, -14),
        PhaseEval(-16, 2),  PhaseEval(37, -8), PhaseEval(43, 0), PhaseEval(40, -1), PhaseEval(35, -2), PhaseEval(50, 6), PhaseEval(37, 0), PhaseEval(-2, 4),
        PhaseEval(-4, -3),  PhaseEval(5, 9), PhaseEval(19, 12), PhaseEval(50, 9), PhaseEval(37, 14), PhaseEval(37, 10), PhaseEval(7, 3), PhaseEval(-2, 2),
        PhaseEval(-6, -6),  PhaseEval(13, 3), PhaseEval(13, 13), PhaseEval(26, 19), PhaseEval(34, 7), PhaseEval(12, 10), PhaseEval(10, -3), PhaseEval(4, -9),
        PhaseEval(0, -12),  PhaseEval(15, -3), PhaseEval(15, 8), PhaseEval(15, 10), PhaseEval(14, 13), PhaseEval(27, 3), PhaseEval(18, -7), PhaseEval(10, -15),
        PhaseEval(4, -14),  PhaseEval(15, -18), PhaseEval(16, -7), PhaseEval(0, -1), PhaseEval(7, 4), PhaseEval(21, -9), PhaseEval(33, -15), PhaseEval(1, -27),
        PhaseEval(-33, -23),  PhaseEval(-3, -9), PhaseEval(-14, -23), PhaseEval(-21, -5), PhaseEval(-13, -9), PhaseEval(-12, -16), PhaseEval(-39, -5), PhaseEval(-21, -17),
    },
    {
        // rook
        PhaseEval(32, 13), PhaseEval(42, 1), PhaseEval(32, 18), PhaseEval(51, 15), PhaseEval(63, 12), PhaseEval(9, 12), PhaseEval(31, 8), PhaseEval(43, 5),
        PhaseEval(27, 11), PhaseEval(32, 13), PhaseEval(58, 13), PhaseEval(62, 11), PhaseEval(80, -3), PhaseEval(67, 3), PhaseEval(26, 8), PhaseEval(44, 3),
        PhaseEval(-5, 7), PhaseEval(19, 7), PhaseEval(26, 7), PhaseEval(36, 5), PhaseEval(17, 4), PhaseEval(45, -3), PhaseEval(61, -5), PhaseEval(16, -3),
        PhaseEval(-24,4), PhaseEval(-11, 3), PhaseEval(7, 13), PhaseEval(26, 1), PhaseEval(24, 2), PhaseEval(35, 1), PhaseEval(-8, -1), PhaseEval(-20, 2),
        PhaseEval(-36, 3), PhaseEval(-26, 5), PhaseEval(-12, 8), PhaseEval(-1, 4), PhaseEval(9, -5), PhaseEval(-7, -6), PhaseEval(6, -8), PhaseEval(-23, -11),
        PhaseEval(-45, -4), PhaseEval(-25, 0), PhaseEval(-16, -5), PhaseEval(-17, -1), PhaseEval(3, -7), PhaseEval(0, -12), PhaseEval(-5, -8), PhaseEval(-33, -16),
        PhaseEval(-44, -6), PhaseEval(-16, -6), PhaseEval(-20, 0), PhaseEval(-9, 2), PhaseEval(-1, -9), PhaseEval(11, -9), PhaseEval(-6, -11), PhaseEval(-71, -3),
        PhaseEval(-19, -9), PhaseEval(-13, 2), PhaseEval(1, 3), PhaseEval(17, -1), PhaseEval(16, -5), PhaseEval(7, -13), PhaseEval(-37, 4), PhaseEval(-26, -20),
    },
    {
        // queen
        PhaseEval(-28, -9), PhaseEval(0, 22), PhaseEval(29, 22), PhaseEval(12, 27), PhaseEval(59, 27), PhaseEval(44, 19), PhaseEval(43, 10), PhaseEval(45, 20),
        PhaseEval(-24, -17), PhaseEval(-39, 20), PhaseEval(-5, 32), PhaseEval(1, 41), PhaseEval(-16, 58), PhaseEval(57, 25), PhaseEval(28, 30), PhaseEval(54, 0),
        PhaseEval(-13, -20), PhaseEval(-17, 6), PhaseEval(7, 9), PhaseEval(8, 49), PhaseEval(29, 47), PhaseEval(56, 35), PhaseEval(47, 19), PhaseEval(57, 9),
        PhaseEval(-27, 3), PhaseEval(-27, 22), PhaseEval(-16, 24), PhaseEval(-16, 45), PhaseEval(-1, 57), PhaseEval(17, 40), PhaseEval(-2, 57), PhaseEval(1, 36),
        PhaseEval(-9, -18), PhaseEval(-26, 28), PhaseEval(-9, 19), PhaseEval(-10, 47), PhaseEval(-2, 31), PhaseEval(-4, 34), PhaseEval(3, 39), PhaseEval(-3, 23),
        PhaseEval(-14, -16), PhaseEval(2, -27), PhaseEval(-11, 15), PhaseEval(-2, 6), PhaseEval(-5, 9), PhaseEval(2, 17), PhaseEval(14, 10), PhaseEval(5, 5),
        PhaseEval(-35, -22), PhaseEval(-8, -23), PhaseEval(11, -30), PhaseEval(2, -16), PhaseEval(8, -16), PhaseEval(15, -23), PhaseEval(-3, -36), PhaseEval(1, -32),
        PhaseEval(-1, -33), PhaseEval(-18, -28), PhaseEval(-9, -22), PhaseEval(10, -43), PhaseEval(-15, -5), PhaseEval(-25, -32), PhaseEval(-31, -20), PhaseEval(-50, -41),
    },
    {
        // king
        PhaseEval(-65, -74), PhaseEval(23, -35), PhaseEval(16, -18), PhaseEval(-15, -18), PhaseEval(-56, -11), PhaseEval(-34, 15), PhaseEval(2, 4), PhaseEval(13, -17),
        PhaseEval(29, -12),  PhaseEval(-1, 17), PhaseEval(-20, 14), PhaseEval(-7, 17), PhaseEval(-8, 17), PhaseEval(-4, 38), PhaseEval(-38, 23), PhaseEval(-29, 11),
        PhaseEval(-9, 10),  PhaseEval(24, 17), PhaseEval(2, 23), PhaseEval(-16, 15), PhaseEval(-20, 20), PhaseEval(6, 45), PhaseEval(22, 44), PhaseEval(-22, 13),
        PhaseEval(-17, -8), PhaseEval(-20, 22), PhaseEval(-12, 24), PhaseEval(-27, 27), PhaseEval(-30, 26), PhaseEval(-25, 33), PhaseEval(-14, 26), PhaseEval(-36, 3),
        PhaseEval(-49, -18), PhaseEval(-1, -4), PhaseEval(-27, 21), PhaseEval(-39, 24), PhaseEval(-46, 27), PhaseEval(-44, 23), PhaseEval(-33, 9), PhaseEval(-51, -11),
        PhaseEval(-14, -19), PhaseEval(-14, -3), PhaseEval(-22, 11), PhaseEval(-46, 21), PhaseEval(-44, 23), PhaseEval(-30, 16), PhaseEval(-15, 7), PhaseEval(-27, -9),
        PhaseEval(1, -27),  PhaseEval(7, -11), PhaseEval(-8, 4), PhaseEval(-64, 13), PhaseEval(-43, 14), PhaseEval(-16, 4), PhaseEval(9, -5), PhaseEval(8, -17),
        PhaseEval(-15, -53), PhaseEval(36, -34), PhaseEval(12, -21), PhaseEval(-54, -11), PhaseEval(8, -28), PhaseEval(-28, -14), PhaseEval(24, -24), PhaseEval(14, -43),
    }
};

int GAMEPHASE_VALUES[PIECE_TYPES] = { 0, 1, 1, 2, 4, 0 };

// std::vector<PawnEval> pawnTable = std::vector<PawnEval>(131072);
constexpr size_t pawnTableSize = 131072;
PawnEval pawnTable[pawnTableSize];

size_t pawnTableIndex(uint64_t pawnHash) {
    __extension__ using uint128 = unsigned __int128;
    return ((uint128)pawnHash * (uint128)pawnTableSize) >> 64;
}

template<Color side>
Eval evaluatePawns(Board* board) {
    Eval result = 0;

    // Doubled pawns
    Bitboard ourPawns = board->byPiece[PIECE_PAWN] & board->byColor[side];
    Bitboard theirPawns = board->byPiece[PIECE_PAWN] & board->byColor[1 - side];
    result -= 30 * __builtin_popcount((ourPawns & (ourPawns >> 8)) | (ourPawns & (ourPawns >> 16)));

    // Passed pawns
    while (ourPawns) {
        Square pawn = popLSB(&ourPawns);
        int file = pawn % 8;
        Bitboard passedPawnMask = FILES[file];
        Bitboard widePassedPawnMask = passedPawnMask;
        if (file != 0)
            widePassedPawnMask |= FILES[file - 1];
        if (file != 7)
            widePassedPawnMask |= FILES[file + 1];
        
        bool passedPawn = !(theirPawns & passedPawnMask);
        bool widePassedPawn = !(theirPawns & widePassedPawnMask);
        result += (passedPawn + 3 * widePassedPawn) * PIECE_VALUES[PIECE_PAWN] / 4;
    }

    return result;
}

PawnEval* probePawnTable(Board* board) {
    uint64_t pawnHash = board->stack->pawnHash;

    // std::cout << pawnHash << " " << ((uint32_t) pawnHash & (131072 - 1)) << std::endl;
    PawnEval* entry = &pawnTable[pawnTableIndex(pawnHash)];
    if (pawnHash == entry->pawnHash) {
        // std::cout << "returning " << pawnHash << std::endl;
        return entry;
    }
    
    entry->pawnHash = pawnHash;
    entry->pawnEval[COLOR_WHITE] = evaluatePawns<COLOR_WHITE>(board);
    entry->pawnEval[COLOR_BLACK] = evaluatePawns<COLOR_BLACK>(board);

    // std::cout << "calculating " << pawnHash << " " << entry->pawnEval[COLOR_WHITE] << " " << entry->pawnEval[COLOR_BLACK] << std::endl;
    // if (pawnHash == 936407206996055834ULL)
    //     debugBoard(board);

    return entry;
}

template<Color side>
Eval evaluate(Board* board) {
    Eval result = 0;

    int gamePhase = 0;
    // Basic material evaluation
    for (Piece piece = 0; piece < PIECE_TYPES - 1; piece++) {
        result += PIECE_VALUES[piece] * board->stack->pieceCount[side][piece];
        gamePhase += GAMEPHASE_VALUES[piece] * (board->stack->pieceCount[COLOR_WHITE][piece] + board->stack->pieceCount[COLOR_BLACK][piece]);
    }

    // PSQ
    int mgPhase = gamePhase;
    if (mgPhase > 24) mgPhase = 24;
    int egPhase = 24 - mgPhase;
    result += (board->stack->psq[side][PHASE_MG] * mgPhase + board->stack->psq[side][PHASE_EG] * egPhase) / 24;

    // Pawn eval
    PawnEval* pawnEval = probePawnTable(board);
    result += pawnEval->pawnEval[side];

    // if (std::abs(evaluatePawns<side == COLOR_WHITE ? COLOR_WHITE : COLOR_BLACK>(board) - pawnEval->pawnEval[side]) >= 1) {
    //     // debugBoard(board);
    //     std::cout << (int) evaluatePawns<side == COLOR_WHITE ? COLOR_WHITE : COLOR_BLACK>(board) << " " << (int) pawnEval->pawnEval[side] << std::endl;
    //     std::cout << (int) evaluatePawns<side == COLOR_BLACK ? COLOR_WHITE : COLOR_BLACK>(board) << " " << (int) pawnEval->pawnEval[1 - side] << std::endl;
    //     // exit(-1);
    // }

    return result;
}

Eval evaluate(Board* board) {
    Eval result;
    if (board->stm == COLOR_WHITE)
        result = evaluate<COLOR_WHITE>(board) - evaluate<COLOR_BLACK>(board);
    else
        result = evaluate<COLOR_BLACK>(board) - evaluate<COLOR_WHITE>(board);
    return result;
}

void debugEval(Board* board) {
    if (board->stm == COLOR_WHITE)
        std::cout << evaluate(board) << " = " << evaluate<COLOR_WHITE>(board) << " - " << evaluate<COLOR_BLACK>(board) << std::endl;
    else
        std::cout << evaluate(board) << " = " << evaluate<COLOR_BLACK>(board) << " - " << evaluate<COLOR_WHITE>(board) << std::endl;
}

std::string formatEval(Eval value) {
    std::string evalString;
    if (value >= EVAL_MATE_IN_MAX_PLY) {
        evalString = "mate " + std::to_string(EVAL_MATE - value);
    }
    else if (value <= -EVAL_MATE_IN_MAX_PLY) {
        evalString = "mate " + std::to_string(-EVAL_MATE - value);
    }
    else {
        evalString = "cp " + std::to_string(value);
    }
    return evalString;
}

bool SEE(Board* board, Move move, Eval threshold) {
    assert(isPseudoLegal(board, move));

    // "Special" moves pass SEE
    if (move >> 12)
        return true;
    
    Square origin = moveOrigin(move);
    Square target = moveTarget(move);

    // If winning the piece on the target square (if there is one) for free doesn't pass the threshold, fail
    int value = SEE_VALUES[board->pieces[target]] - threshold;
    if (value < 0) return false;

    // If we beat the threshold after losing our piece, pass
    value -= SEE_VALUES[board->pieces[origin]];
    if (value >= 0) return true;

    Bitboard occupied = (board->byColor[COLOR_WHITE] | board->byColor[COLOR_BLACK]) ^ (C64(1) << origin);
    Bitboard attackersToTarget = attackersTo(board, target, occupied);

    Bitboard bishops = board->byPiece[PIECE_BISHOP] | board->byPiece[PIECE_QUEEN];
    Bitboard rooks = board->byPiece[PIECE_ROOK] | board->byPiece[PIECE_QUEEN];

    Color side = 1 - board->stm;

    // Make captures until one side has none left / fails to beat the threshold
    while (true) {

        // Remove attackers that have been captured
        attackersToTarget &= occupied;

        // No attackers left for the current side
        Bitboard stmAttackers = board->byColor[side] & attackersToTarget;
        if (!stmAttackers) break;

        // Find least valuable piece
        Piece piece;
        for (piece = PIECE_PAWN; piece < PIECE_KING; piece++) {
            // If the piece attacks the target square, stop
            if (stmAttackers & board->byPiece[piece])
                break;
        }

        side = 1 - side;
        value = -value - 1 - SEE_VALUES[piece];
        
        // Value beats (or can't beat) threshold (negamax)
        if (value >= 0) {
            if (piece == PIECE_KING && (attackersToTarget & board->byColor[side]))
                side = 1 - side;
            break;
        }

        // Remove the used piece
        occupied ^= C64(1) << lsb(stmAttackers & board->byPiece[piece]);

        // Add discovered attacks
        if (piece == PIECE_PAWN || piece == PIECE_BISHOP || piece == PIECE_QUEEN)
            attackersToTarget |= getBishopMoves(target, occupied) & bishops;
        if (piece == PIECE_ROOK || piece == PIECE_QUEEN)
            attackersToTarget |= getRookMoves(target, occupied) & rooks;
    }

    return side != board->stm;
}

void debugSEE(Board* board) {
    SearchStack* stack = {};

    Move move;
    Move killers[2] = { MOVE_NONE, MOVE_NONE };
    MoveGen movegen(board, stack, MOVE_NONE, MOVE_NONE, killers);
    int i = 0;
    while ((move = movegen.nextMove()) != MOVE_NONE) {
        if (!isCapture(board, move)) {
            std::cout << i << ": " << moveToString(move) << " is not a capture!" << std::endl;
            i++;
            continue;
        }

        bool goodCapture = SEE(board, move, -107);
        if (goodCapture)
            std::cout << i << ": " << moveToString(move) << " is a good capture!" << std::endl;
        else
            std::cout << i << ": " << moveToString(move) << " is a bad capture!" << std::endl;
        i++;
    }
}
