#include <cstdint>
#include <algorithm>
#include <inttypes.h>
#include <cassert>
#include <chrono>
#include <cmath>
#include <thread>
#include <map>

#include <chrono>
#include <thread>

#include "search.h"
#include "board.h"
#include "move.h"
#include "evaluation.h"
#include "thread.h"
#include "tt.h"
#include "time.h"
#include "spsa.h"
#include "nnue.h"
#include "uci.h"

// Time management
TUNE_FLOAT(tmInitialAdjustment, 1.062091379202246f, 0.5f, 1.5f);
TUNE_INT(tmBestMoveStabilityMax, 18, 10, 30);
TUNE_FLOAT(tmBestMoveStabilityBase, 1.4781560567180774f, 0.75f, 2.5f);
TUNE_FLOAT(tmBestMoveStabilityFactor, 0.04883548163780087f, 0.001f, 0.1f);
TUNE_FLOAT(tmEvalDiffBase, 0.9436184253414133f, 0.5f, 1.5f);
TUNE_FLOAT(tmEvalDiffFactor, 0.006456311895665374f, 0.001f, 0.1f);
TUNE_INT(tmEvalDiffMin, -14, -250, 50);
TUNE_INT(tmEvalDiffMax, 49, -50, 250);
TUNE_FLOAT(tmNodesBase, 1.769829375370982f, 0.5f, 5.0f);
TUNE_FLOAT(tmNodesFactor, 0.8491490118233571f, 0.1f, 2.5f);

// Aspiration windows
TUNE_INT(aspirationWindowMinDepth, 4, 2, 6);
TUNE_INT(aspirationWindowDelta, 15, 1, 30);
TUNE_INT(aspirationWindowMaxFailHighs, 3, 1, 10);
TUNE_FLOAT(aspirationWindowDeltaFactor, 1.6372607104437336f, 1.0f, 3.0f);

// Reduction / Margin tables
TUNE_FLOAT(lmrReductionNoisyBase, -0.46179633731699155f, -2.0f, -0.1f);
TUNE_FLOAT(lmrReductionNoisyFactor, 3.124395856375975f, 2.0f, 4.0f);
TUNE_FLOAT(lmrReductionQuietBase, 0.8551863747477736f, 0.50f, 1.5f);
TUNE_FLOAT(lmrReductionQuietFactor, 2.957870775454128f, 2.0f, 4.0f);

TUNE_FLOAT(seeMarginNoisy, -21.777710812647356f, -50.0f, -10.0f);
TUNE_FLOAT(seeMarginQuiet, -72.3449107458693f, -100.0f, -50.0f);
TUNE_FLOAT(lmpMarginWorseningBase, 1.7147651934589725f, -1.0f, 2.5f);
TUNE_FLOAT(lmpMarginWorseningFactor, 0.3416547004680056f, 0.1f, 1.5f);
TUNE_FLOAT(lmpMarginWorseningPower, 1.7195307690695079f, 1.0f, 3.0f);
TUNE_FLOAT(lmpMarginImprovingBase, 2.9109050117999864f, 2.0f, 5.0f);
TUNE_FLOAT(lmpMarginImprovingFactor, 1.029509208258066f, 0.5f, 2.0f);
TUNE_FLOAT(lmpMarginImprovingPower, 1.8719699078773355f, 1.0f, 3.0f);

// Search values
TUNE_INT(qsFutilityOffset, 61, 1, 125);
TUNE_INT(qsSeeMargin, -101, -200, 50);

// Pre-search pruning
TUNE_INT_DISABLED(iirMinDepth, 4, 1, 10);

TUNE_INT(worseningOffset, 15, 1, 50);

TUNE_INT(staticHistoryFactor, -47, -500, -1);
TUNE_INT(staticHistoryMin, -69, -1000, -1);
TUNE_INT(staticHistoryMax, 75, 1, 1000);

TUNE_INT_DISABLED(rfpDepth, 8, 2, 20);
TUNE_INT(rfpFactor, 75, 1, 250);

TUNE_INT_DISABLED(razoringDepth, 5, 2, 20);
TUNE_INT(razoringFactor, 218, 1, 1000);

TUNE_INT_DISABLED(nmpRedBase, 3, 1, 5);
TUNE_INT_DISABLED(nmpDepthDiv, 3, 1, 6);
TUNE_INT_DISABLED(nmpMin, 3, 1, 10);
TUNE_INT(nmpDivisor, 85, 10, 1000);

TUNE_INT(probCutBetaOffset, 181, 1, 500);
TUNE_INT_DISABLED(probCutDepth, 5, 1, 15);

// In-search pruning
TUNE_INT_DISABLED(fpDepth, 11, 1, 20);
TUNE_INT(fpBase, 257, 1, 1000);
TUNE_INT(fpFactor, 106, 1, 500);

TUNE_INT_DISABLED(fpCaptDepth, 9, 1, 20);
TUNE_INT(fpCaptBase, 455, 150, 750);
TUNE_INT(fpCaptFactor, 334, 100, 600);

TUNE_INT_DISABLED(historyPruningDepth, 4, 1, 15);
TUNE_INT(historyPruningFactor, -2551, -8192, -128);

TUNE_INT(doubleExtensionMargin, 15, 1, 30);
TUNE_INT(doubleExtensionDepthIncrease, 11, 2, 20);

TUNE_INT_DISABLED(seeDepth, 9, 2, 15);

TUNE_INT_DISABLED(lmrMcBase, 2, 1, 10);
TUNE_INT_DISABLED(lmrMcPv, 2, 1, 10);
TUNE_INT_DISABLED(lmrMinDepth, 3, 1, 10);

TUNE_INT(lmrHistoryFactorQuiet, 16870, 128, 32768);
TUNE_INT(lmrHistoryFactorCapture, 13507, 128, 32768);
TUNE_INT(lmrDeeperBase, 38, 1, 100);
TUNE_INT_DISABLED(lmrDeeperFactor, 2, 0, 10);

TUNE_INT(lmrPassBonusBase, -182, -500, 500);
TUNE_INT(lmrPassBonusFactor, 176, 1, 500);
TUNE_INT(lmrPassBonusMax, 1031, 32, 4096);

TUNE_INT(historyBonusBase, 9, -500, 500);
TUNE_INT(historyBonusFactor, 197, 1, 500);
TUNE_INT(historyBonusMax, 1809, 32, 4096);
TUNE_INT(historyBonusBetaOffset, 276, 1, 500);

TUNE_INT(correctionHistoryFactor, 173, 32, 512);

int REDUCTIONS[2][MAX_PLY][MAX_MOVES];
int SEE_MARGIN[MAX_PLY][2];
int LMP_MARGIN[MAX_PLY][2];

void initReductions() {
    REDUCTIONS[0][0][0] = 0;
    REDUCTIONS[1][0][0] = 0;

    for (int i = 1; i < MAX_PLY; i++) {
        for (int j = 1; j < MAX_MOVES; j++) {
            REDUCTIONS[0][i][j] = lmrReductionNoisyBase + log(i) * log(j) / lmrReductionNoisyFactor; // non-quiet
            REDUCTIONS[1][i][j] = lmrReductionQuietBase + log(i) * log(j) / lmrReductionQuietFactor; // quiet
        }
    }

    for (int depth = 0; depth < MAX_PLY; depth++) {
        SEE_MARGIN[depth][0] = seeMarginNoisy * depth * depth; // non-quiet
        SEE_MARGIN[depth][1] = seeMarginQuiet * depth; // quiet

        LMP_MARGIN[depth][0] = lmpMarginWorseningBase + lmpMarginWorseningFactor * std::pow(depth, lmpMarginWorseningPower); // non-improving
        LMP_MARGIN[depth][1] = lmpMarginImprovingBase + lmpMarginImprovingFactor * std::pow(depth, lmpMarginImprovingPower); // improving
    }
}

uint64_t perftInternal(Board* board, NNUE* nnue, int depth) {
    if (depth == 0) return 1;

    BoardStack stack;

    Move moves[MAX_MOVES] = { MOVE_NONE };
    int moveCount = 0;
    generateMoves(board, moves, &moveCount);

    uint64_t nodes = 0;
    for (int i = 0; i < moveCount; i++) {
        Move move = moves[i];

        if (!board->isLegal(move))
            continue;

        board->doMove(&stack, move, board->hashAfter(move), nnue);
        uint64_t subNodes = perftInternal(board, nnue, depth - 1);
        board->undoMove(move, nnue);

        nodes += subNodes;
    }
    return nodes;
}

uint64_t perft(Board* board, int depth) {
    clock_t begin = clock();
    BoardStack stack;
    UCI::nnue.reset(board);

    Move moves[MAX_MOVES] = { MOVE_NONE };
    int moveCount = 0;
    generateMoves(board, moves, &moveCount);

    uint64_t nodes = 0;
    for (int i = 0; i < moveCount; i++) {
        Move move = moves[i];

        if (!board->isLegal(move))
            continue;

        board->doMove(&stack, move, board->hashAfter(move), &UCI::nnue);
        uint64_t subNodes = perftInternal(board, &UCI::nnue, depth - 1);
        board->undoMove(move, &UCI::nnue);

        std::cout << moveToString(move, UCI::Options.chess960.value) << ": " << subNodes << std::endl;

        nodes += subNodes;
    }

    clock_t end = clock();
    double time = (double)(end - begin) / CLOCKS_PER_SEC;
    uint64_t nps = nodes / time;
    std::cout << "Perft: " << nodes << " nodes in " << time << "s => " << nps << "nps" << std::endl;

    return nodes;
}

void updatePv(SearchStack* stack, Move move) {
    stack->pvLength = (stack + 1)->pvLength;
    stack->pv[stack->ply] = move;

    for (int i = stack->ply + 1; i < (stack + 1)->pvLength; i++)
        stack->pv[i] = (stack + 1)->pv[i];
}

int valueToTT(int value, int ply) {
    if (value == EVAL_NONE) return EVAL_NONE;
    if (value > EVAL_MATE_IN_MAX_PLY) value += ply;
    else if (value < -EVAL_MATE_IN_MAX_PLY) value -= ply;
    return value;
}

int valueFromTt(int value, int ply) {
    if (value == EVAL_NONE) return EVAL_NONE;
    if (value > EVAL_MATE_IN_MAX_PLY) value -= ply;
    else if (value < -EVAL_MATE_IN_MAX_PLY) value += ply;
    return value;
}

Eval drawEval(Thread* thread) {
    return 4 - (thread->searchData.nodesSearched & 3);  // Small overhead to avoid 3-fold blindness
}

template <NodeType nodeType>
Eval Thread::qsearch(Board* board, SearchStack* stack, Eval alpha, Eval beta) {
    constexpr bool pvNode = nodeType == PV_NODE;

    if (pvNode)
        stack->pvLength = stack->ply;
    searchData.selDepth = std::max(stack->ply, searchData.selDepth);

    assert(alpha >= -EVAL_INFINITE && alpha < beta && beta <= EVAL_INFINITE);

    if (mainThread && timeOver(searchParameters, &searchData))
        threadPool->stopSearching();

    // Check for stop
    if (stopped || exiting || stack->ply >= MAX_PLY || board->isDraw())
        return (stack->ply >= MAX_PLY && !board->stack->checkers) ? evaluate(board, &nnue) : drawEval(this);

    stack->inCheck = board->stack->checkerCount > 0;

    // TT Lookup
    bool ttHit = false;
    TTEntry* ttEntry = nullptr;
    Move ttMove = MOVE_NONE;
    Eval ttValue = EVAL_NONE;
    Eval ttEval = EVAL_NONE;
    uint8_t ttFlag = TT_NOBOUND;
    bool ttPv = pvNode;

    ttEntry = TT.probe(board->stack->hash, &ttHit);
    if (ttHit) {
        ttMove = ttEntry->getMove();
        ttValue = valueFromTt(ttEntry->getValue(), stack->ply);
        ttEval = ttEntry->getEval();
        ttFlag = ttEntry->getFlag();
        ttPv = ttPv || ttEntry->getTtPv();
    }

    // TT cutoff
    if (!pvNode && ttValue != EVAL_NONE && ((ttFlag == TT_UPPERBOUND && ttValue <= alpha) || (ttFlag == TT_LOWERBOUND && ttValue >= beta) || (ttFlag == TT_EXACTBOUND)))
        return ttValue;

    Move bestMove = MOVE_NONE;
    Eval bestValue, futilityValue, unadjustedEval;

    if (board->stack->checkers) {
        stack->staticEval = bestValue = unadjustedEval = futilityValue = -EVAL_INFINITE;
        goto movesLoopQsearch;
    }
    else if (ttHit && ttEval != EVAL_NONE) {
        unadjustedEval = ttEval;
        stack->staticEval = bestValue = history.correctStaticEval(unadjustedEval, board);

        if (ttValue != EVAL_NONE && ((ttFlag == TT_UPPERBOUND && ttValue < bestValue) || (ttFlag == TT_LOWERBOUND && ttValue > bestValue) || (ttFlag == TT_EXACTBOUND)))
            bestValue = ttValue;
    }
    else {
        unadjustedEval = evaluate(board, &nnue);
        stack->staticEval = bestValue = history.correctStaticEval(unadjustedEval, board);
        ttEntry->update(board->stack->hash, MOVE_NONE, 0, unadjustedEval, EVAL_NONE, ttPv, TT_NOBOUND);
    }
    futilityValue = stack->staticEval + qsFutilityOffset;

    // Stand pat
    if (bestValue >= beta)
        return beta;
    if (alpha < bestValue)
        alpha = bestValue;

movesLoopQsearch:
    // Mate distance pruning
    alpha = std::max((int)alpha, (int)matedIn(stack->ply));
    beta = std::min((int)beta, (int)mateIn(stack->ply + 1));
    if (alpha >= beta)
        return alpha;

    BoardStack boardStack;

    // Moves loop
    MoveGen movegen(board, &history, stack, ttMove, !board->stack->checkers, 1);
    Move move;
    int moveCount = 0;
    bool playedQuiet = false;
    while ((move = movegen.nextMove()) != MOVE_NONE) {

        bool capture = board->isCapture(move);
        if (!capture && playedQuiet)
            continue;

        if (futilityValue > -EVAL_INFINITE) { // Only prune when not in check
            if (bestValue >= -EVAL_MATE_IN_MAX_PLY
                && futilityValue <= alpha
                && !SEE(board, move, 1)
                ) {
                bestValue = std::max(bestValue, futilityValue);
                continue;
            }

            if (!SEE(board, move, qsSeeMargin))
                break;

            if (moveTarget(move) != moveTarget((stack - 1)->move) && (moveType(move) != MOVE_PROMOTION) && !board->givesCheck(move) && moveCount > 2)
                continue;
        }

        if (!board->isLegal(move))
            continue;

        uint64_t newHash = board->hashAfter(move);
        TT.prefetch(newHash);
        moveCount++;
        searchData.nodesSearched++;

        Square origin = moveOrigin(move);
        Square target = moveTarget(move);
        stack->capture = capture;
        stack->move = move;
        stack->movedPiece = board->pieces[origin];
        stack->contHist = history.continuationHistory[board->stm][stack->movedPiece][target];

        playedQuiet |= move != ttMove && !capture;

        board->doMove(&boardStack, move, newHash, &nnue);

        Eval value = -qsearch<nodeType>(board, stack + 1, -beta, -alpha);
        board->undoMove(move, &nnue);
        assert(value > -EVAL_INFINITE && value < EVAL_INFINITE);

        if (stopped || exiting)
            return 0;

        if (value > bestValue) {
            bestValue = value;

            if (value > alpha) {
                bestMove = move;
                alpha = value;

                if (pvNode)
                    updatePv(stack, move);

                if (bestValue >= beta)
                    break;
            }
        }
    }

    if (bestValue == -EVAL_INFINITE) {
        assert(board->stack->checkers && moveCount == 0);
        bestValue = matedIn(stack->ply); // Checkmate
    }

    // Insert into TT
    int flags = bestValue >= beta ? TT_LOWERBOUND : TT_UPPERBOUND;
    ttEntry->update(board->stack->hash, bestMove, 0, unadjustedEval, valueToTT(bestValue, stack->ply), ttPv, flags);

    return bestValue;
}

template <NodeType nt>
Eval Thread::search(Board* board, SearchStack* stack, int depth, Eval alpha, Eval beta, bool cutNode) {
    constexpr bool rootNode = nt == ROOT_NODE;
    constexpr bool pvNode = nt == PV_NODE || nt == ROOT_NODE;
    constexpr NodeType nodeType = nt == ROOT_NODE ? PV_NODE : NON_PV_NODE;

    assert(-EVAL_INFINITE <= alpha && alpha < beta && beta <= EVAL_INFINITE);
    assert(!(pvNode && cutNode));
    assert(pvNode || alpha == beta - 1);

    // Set up PV length and selDepth
    if (pvNode)
        stack->pvLength = stack->ply;
    searchData.selDepth = std::max(stack->ply, searchData.selDepth);

    // Check for upcoming repetition
    if (!rootNode && alpha < 0 && board->hasUpcomingRepetition(stack->ply)) {
        alpha = drawEval(this);
        if (alpha >= beta)
            return alpha;
    }

    if (depth <= 0) return qsearch<nodeType>(board, stack, alpha, beta);

    if (!rootNode) {

        // Check for time / node limits on main thread
        if (mainThread && timeOver(searchParameters, &searchData))
            threadPool->stopSearching();

        // Check for stop or max depth
        if (stopped || exiting || stack->ply >= MAX_PLY || board->isDraw())
            return (stack->ply >= MAX_PLY && !board->stack->checkers) ? evaluate(board, &nnue) : drawEval(this);

        // Mate distance pruning
        alpha = std::max((int)alpha, (int)matedIn(stack->ply));
        beta = std::min((int)beta, (int)mateIn(stack->ply + 1));
        if (alpha >= beta)
            return alpha;
    }

    // Initialize some stuff
    Move bestMove = MOVE_NONE;
    Move excludedMove = stack->excludedMove;
    Eval bestValue = -EVAL_INFINITE;
    Eval oldAlpha = alpha;
    bool improving = false, worsening = false, skipQuiets = false, excluded = excludedMove != MOVE_NONE;

    (stack + 1)->killer = MOVE_NONE;
    (stack + 1)->excludedMove = MOVE_NONE;
    stack->inCheck = board->stack->checkerCount > 0;

    // TT Lookup
    bool ttHit = false;
    TTEntry* ttEntry = nullptr;
    Move ttMove = MOVE_NONE;
    Eval ttValue = EVAL_NONE, ttEval = EVAL_NONE;
    int ttDepth = 0;
    uint8_t ttFlag = TT_NOBOUND;
    bool ttPv = pvNode;

    if (!excluded) {
        ttEntry = TT.probe(board->stack->hash, &ttHit);
        if (ttHit) {
            ttMove = rootNode && rootMoves[0].value > -EVAL_INFINITE ? rootMoves[0].move : ttEntry->getMove();
            ttValue = valueFromTt(ttEntry->getValue(), stack->ply);
            ttEval = ttEntry->getEval();
            ttDepth = ttEntry->getDepth();
            ttFlag = ttEntry->getFlag();
            ttPv = ttPv || ttEntry->getTtPv();
        }
    }

    // TT cutoff
    if (!pvNode && ttDepth >= depth && ttValue != EVAL_NONE && ((ttFlag == TT_UPPERBOUND && ttValue <= alpha) || (ttFlag == TT_LOWERBOUND && ttValue >= beta) || (ttFlag == TT_EXACTBOUND)))
        return ttValue;

    // Static evaluation
    Eval eval = EVAL_NONE, unadjustedEval = EVAL_NONE, probCutBeta = EVAL_NONE;
    if (board->stack->checkers) {
        stack->staticEval = EVAL_NONE;
        goto movesLoop;
    }
    else if (excluded) {
        unadjustedEval = eval = stack->staticEval;
    }
    else if (ttHit) {
        unadjustedEval = ttEval != EVAL_NONE ? ttEval : evaluate(board, &nnue);
        eval = stack->staticEval = history.correctStaticEval(unadjustedEval, board);

        if (ttValue != EVAL_NONE && ((ttFlag == TT_UPPERBOUND && ttValue < eval) || (ttFlag == TT_LOWERBOUND && ttValue > eval) || (ttFlag == TT_EXACTBOUND)))
            eval = ttValue;
    }
    else {
        unadjustedEval = evaluate(board, &nnue);
        eval = stack->staticEval = history.correctStaticEval(unadjustedEval, board);

        ttEntry->update(board->stack->hash, MOVE_NONE, 0, unadjustedEval, EVAL_NONE, ttPv, TT_NOBOUND);
    }

    // IIR
    if ((!ttHit || ttDepth + 4 < depth) && depth >= iirMinDepth)
        depth--;

    // Improving
    if ((stack - 2)->staticEval != EVAL_NONE) {
        improving = stack->staticEval > (stack - 2)->staticEval;
        worsening = stack->staticEval + worseningOffset < (stack - 2)->staticEval;
    }
    else if ((stack - 4)->staticEval != EVAL_NONE) {
        improving = stack->staticEval > (stack - 4)->staticEval;
        worsening = stack->staticEval + worseningOffset < (stack - 4)->staticEval;
    }

    // Adjust quiet history based on how much the previous move changed static eval
    if ((stack - 1)->movedPiece != Piece::NONE && !(stack - 1)->capture && !(stack - 1)->inCheck && stack->ply > 1) {
        int bonus = std::clamp(staticHistoryFactor * int(stack->staticEval + (stack - 1)->staticEval) / 10, staticHistoryMin, staticHistoryMax);
        history.updateQuietHistory((stack - 1)->move, flip(board->stm), board, board->stack->previous, bonus);
    }

    // Reverse futility pruning
    if (!rootNode && depth < rfpDepth && std::abs(eval) < EVAL_MATE_IN_MAX_PLY && eval - rfpFactor * (depth - improving) >= beta)
        return (eval + beta) / 2;

    // Razoring
    if (!rootNode && depth < razoringDepth && eval + (razoringFactor * depth) < alpha) {
        Eval razorValue = qsearch<NON_PV_NODE>(board, stack, alpha, beta);
        if (razorValue <= alpha)
            return razorValue;
    }

    BoardStack boardStack;

    // Null move pruning
    if (!pvNode
        && eval >= stack->staticEval
        && eval >= beta
        && beta > -EVAL_MATE_IN_MAX_PLY
        && !excluded
        && (stack - 1)->movedPiece != Piece::NONE
        && depth >= 3
        && stack->ply >= searchData.nmpPlies
        && board->hasNonPawns()
        ) {
        stack->capture = false;
        stack->move = MOVE_NULL;
        stack->movedPiece = Piece::NONE;
        stack->contHist = history.continuationHistory[board->stm][0][0];
        int R = nmpRedBase + depth / nmpDepthDiv + std::min((eval - beta) / nmpDivisor, nmpMin);

        board->doNullMove(&boardStack);
        Eval nullValue = -search<NON_PV_NODE>(board, stack + 1, depth - R, -beta, -beta + 1, !cutNode);
        board->undoNullMove();

        if (stopped || exiting)
            return 0;

        if (nullValue >= beta) {
            if (nullValue > EVAL_MATE_IN_MAX_PLY)
                nullValue = beta;

            if (searchData.nmpPlies || depth < 15)
                return nullValue;

            searchData.nmpPlies = stack->ply + (depth - R) * 2 / 3;
            Eval verificationValue = search<NON_PV_NODE>(board, stack, depth - R, beta - 1, beta, false);
            searchData.nmpPlies = 0;

            if (verificationValue >= beta)
                return nullValue;
        }
    }

    // ProbCut
    probCutBeta = std::min(beta + probCutBetaOffset, EVAL_MATE_IN_MAX_PLY - 1);
    if (!pvNode
        && !excluded
        && depth > probCutDepth
        && std::abs(beta) < EVAL_MATE_IN_MAX_PLY - 1
        && !(ttDepth >= depth - 3 && ttValue != EVAL_NONE && ttValue < probCutBeta)) {

        assert(probCutBeta > beta);
        assert(probCutBeta < EVAL_MATE_IN_MAX_PLY);

        Move probcutTtMove = ttMove != MOVE_NONE && board->isPseudoLegal(ttMove) && SEE(board, ttMove, probCutBeta - stack->staticEval) ? ttMove : MOVE_NONE;
        MoveGen movegen(board, &history, stack, probcutTtMove, probCutBeta - stack->staticEval, depth);
        Move move;
        while ((move = movegen.nextMove()) != MOVE_NONE) {
            if (move == excludedMove || !board->isLegal(move))
                continue;

            uint64_t newHash = board->hashAfter(move);
            TT.prefetch(newHash);

            Square origin = moveOrigin(move);
            Square target = moveTarget(move);
            stack->capture = true;
            stack->move = move;
            stack->movedPiece = board->pieces[origin];
            stack->contHist = history.continuationHistory[board->stm][stack->movedPiece][target];
            board->doMove(&boardStack, move, newHash, &nnue);

            Eval value = -qsearch<NON_PV_NODE>(board, stack + 1, -probCutBeta, -probCutBeta + 1);

            if (value >= probCutBeta)
                value = -search<NON_PV_NODE>(board, stack + 1, depth - 4, -probCutBeta, -probCutBeta + 1, !cutNode);

            board->undoMove(move, &nnue);

            if (value >= probCutBeta) {
                ttEntry->update(board->stack->hash, move, depth - 3, unadjustedEval, valueToTT(value, stack->ply), ttPv, TT_LOWERBOUND);
                return value;
            }
        }

    }

    assert(board->stack);

    // IIR 2: Electric boolagoo
    if (!ttHit && depth >= iirMinDepth && pvNode)
        depth--;

movesLoop:

    Move quietMoves[32];
    Move captureMoves[32];
    int quietMoveCount = 0;
    int captureMoveCount = 0;

    // Moves loop
    MoveGen movegen(board, &history, stack, ttMove, depth);
    Move move;
    int moveCount = 0;
    while ((move = movegen.nextMove()) != MOVE_NONE) {

        if (move == excludedMove)
            continue;
        if (rootNode && std::find(excludedRootMoves.begin(), excludedRootMoves.end(), move) != excludedRootMoves.end())
            continue;

        bool capture = board->isCapture(move);
        if (!capture && skipQuiets)
            continue;

        if (!board->isLegal(move))
            continue;

        uint64_t nodesBeforeMove = searchData.nodesSearched;
        int moveHistory = history.getHistory(board, board->stack, stack, move, capture);

        if (!pvNode
            && bestValue > -EVAL_MATE_IN_MAX_PLY
            && board->hasNonPawns()
            ) {

            int lmrDepth = std::max(0, depth - REDUCTIONS[!capture][depth][moveCount] - !improving + moveHistory / (capture ? lmrHistoryFactorCapture : lmrHistoryFactorQuiet));

            if (!skipQuiets) {

                // Movecount pruning (LMP)
                if (moveCount >= LMP_MARGIN[depth][improving]) {
                    skipQuiets = true;
                }

                // Futility pruning
                if (!capture && lmrDepth < fpDepth && eval + fpBase + fpFactor * lmrDepth <= alpha)
                    skipQuiets = true;
            }

            // Futility pruning for captures
            if (capture && moveType(move) != MOVE_PROMOTION) {
                Piece capturedPiece = moveType(move) == MOVE_ENPASSANT ? Piece::PAWN : board->pieces[moveTarget(move)];
                if (lmrDepth < fpCaptDepth && eval + fpCaptBase + PIECE_VALUES[capturedPiece] + fpCaptFactor * lmrDepth <= alpha)
                    continue;
            }

            // History pruning
            int hpFactor = capture ? historyPruningFactor / 2 : historyPruningFactor * 2;
            if (lmrDepth < historyPruningDepth && moveHistory < hpFactor * depth)
                continue;

            // SEE Pruning
            if (!SEE(board, move, SEE_MARGIN[!capture ? lmrDepth : depth][!capture]))
                continue;

        }

        // Extensions
        bool doExtensions = !rootNode && stack->ply < searchData.rootDepth * 2;
        bool singular = false;
        int extension = 0;
        if (doExtensions
            && depth >= 7
            && move == ttMove
            && !excluded
            && (ttFlag & TT_LOWERBOUND)
            && std::abs(ttValue) < EVAL_MATE_IN_MAX_PLY
            && ttDepth >= depth - 3
            ) {
            Eval singularBeta = ttValue - depth;
            int singularDepth = (depth - 1) / 2;

            stack->excludedMove = move;
            Eval singularValue = search<NON_PV_NODE>(board, stack, singularDepth, singularBeta - 1, singularBeta, cutNode);
            stack->excludedMove = MOVE_NONE;

            if (singularValue < singularBeta) {
                singular = true;
                // This move is singular and we should investigate it further
                extension = 1;
                if (!pvNode && singularValue + doubleExtensionMargin < singularBeta) {
                    extension = 2;
                    depth += depth < doubleExtensionDepthIncrease;
                }
            }
            // Multicut: If we beat beta, that means there's likely more moves that beat beta and we can skip this node
            else if (singularBeta >= beta)
                return singularBeta;
            // We didn't prove singularity and an excluded search couldn't beat beta, but if the ttValue can we still reduce the depth
            else if (ttValue >= beta)
                extension = -2;
            // We didn't prove singularity and an excluded search couldn't beat beta, but we are expected to fail low, so reduce
            else if (cutNode)
                extension = -2;
            else if (ttValue <= alpha)
                extension = -1;
        }

        uint64_t newHash = board->hashAfter(move);
        TT.prefetch(newHash);

        if (!capture) {
            if (quietMoveCount < 32)
                quietMoves[quietMoveCount++] = move;
        }
        else {
            if (captureMoveCount < 32)
                captureMoves[captureMoveCount++] = move;
        }

        // Some setup stuff
        Square origin = moveOrigin(move);
        Square target = moveTarget(move);
        stack->capture = capture;
        stack->move = move;
        stack->movedPiece = board->pieces[origin];
        stack->contHist = history.continuationHistory[board->stm][stack->movedPiece][target];

        moveCount++;
        searchData.nodesSearched++;
        board->doMove(&boardStack, move, newHash, &nnue);

        if (doExtensions && extension == 0 && board->stack->checkers)
            extension = 1;

        Eval value = 0;
        int newDepth = depth - 1 + extension;

        // Very basic LMR: Late moves are being searched with less depth
        // Check if the move can exceed alpha
        if (moveCount > lmrMcBase + lmrMcPv * pvNode && depth >= lmrMinDepth && (!capture || !ttPv)) {
            int reducedDepth = newDepth - REDUCTIONS[!capture][depth][moveCount];

            if (!ttPv)
                reducedDepth--;

            if (cutNode)
                reducedDepth -= 2;

            if (capture)
                reducedDepth += moveHistory * std::abs(moveHistory) / (lmrHistoryFactorCapture * lmrHistoryFactorCapture * 2);
            else
                reducedDepth += moveHistory / lmrHistoryFactorQuiet;

            if (worsening)
                reducedDepth--;

            reducedDepth = std::clamp(reducedDepth, 1, newDepth);
            value = -search<NON_PV_NODE>(board, stack + 1, reducedDepth, -(alpha + 1), -alpha, true);

            bool doShallowerSearch = !rootNode && value < bestValue + newDepth;
            bool doDeeperSearch = value > (bestValue + lmrDeeperBase + lmrDeeperFactor * newDepth);
            newDepth += doDeeperSearch - doShallowerSearch;

            if (value > alpha && reducedDepth < newDepth) {
                value = -search<NON_PV_NODE>(board, stack + 1, newDepth, -(alpha + 1), -alpha, !cutNode);

                if (!capture) {
                    int bonus = std::min(lmrPassBonusBase + lmrPassBonusFactor * depth, lmrPassBonusMax);
                    history.updateContinuationHistory(stack, flip(board->stm), stack->movedPiece, move, bonus);
                }
            }
        }
        else if (!pvNode || moveCount > 1) {
            value = -search<NON_PV_NODE>(board, stack + 1, newDepth, -(alpha + 1), -alpha, !cutNode);
        }

        // PV moves will be researched at full depth if good enough
        if (pvNode && (moveCount == 1 || value > alpha)) {
            value = -search<PV_NODE>(board, stack + 1, newDepth, -beta, -alpha, false);
        }

        board->undoMove(move, &nnue);
        assert(value > -EVAL_INFINITE && value < EVAL_INFINITE);

        if (stopped || exiting)
            return 0;

        if (rootNode) {
            if (rootMoveNodes.count(move) == 0)
                rootMoveNodes[move] = searchData.nodesSearched - nodesBeforeMove;
            else
                rootMoveNodes[move] = searchData.nodesSearched - nodesBeforeMove + rootMoveNodes[move];

            RootMove* rootMove = &rootMoves[0];
            for (RootMove& rm : rootMoves) {
                if (rm.move == move) {
                    rootMove = &rm;
                    break;
                }
            }

            if (moveCount == 1 || value > alpha) {
                rootMove->value = value;
                rootMove->depth = searchData.rootDepth;
                rootMove->selDepth = searchData.selDepth;

                rootMove->pv.resize(0);
                rootMove->pv.push_back(move);
                for (int i = 1; i < (stack + 1)->pvLength; i++)
                    rootMove->pv.push_back((stack + 1)->pv[i]);
            }
        }

        if (value > bestValue) {
            bestValue = value;

            if (value > alpha) {
                bestMove = move;
                alpha = value;

                if (pvNode)
                    updatePv(stack, move);

                if (bestValue >= beta) {

                    int bonus = std::min(historyBonusBase + historyBonusFactor * (depth + (eval <= alpha) + (value - historyBonusBetaOffset > beta)), historyBonusMax);
                    if (!capture) {
                        // Update quiet killer
                        stack->killer = move;

                        // Update counter move
                        if (stack->ply > 0)
                            history.setCounterMove((stack - 1)->move, move);

                        history.updateQuietHistories(board, board->stack, stack, move, bonus, quietMoves, quietMoveCount);

                        if (singular)
                            bonus = 3 * bonus / 2;
                    }
                    history.updateCaptureHistory(board, move, bonus, captureMoves, captureMoveCount);
                    break;
                }

                if (depth > 4 && depth < 10 && beta < EVAL_MATE_IN_MAX_PLY && value > -EVAL_MATE_IN_MAX_PLY)
                    depth--;
            }
        }

    }

    if (moveCount == 0) {
        if (board->stack->checkers) {
            return excluded ? -EVAL_INFINITE : matedIn(stack->ply); // Checkmate
        }
        return 0; // Stalemate
    }

    // Insert into TT
    bool failLow = alpha == oldAlpha;
    bool failHigh = bestValue >= beta;
    int flags = failHigh ? TT_LOWERBOUND : !failLow ? TT_EXACTBOUND : TT_UPPERBOUND;
    if (!excluded)
        ttEntry->update(board->stack->hash, bestMove, depth, unadjustedEval, valueToTT(bestValue, stack->ply), ttPv, flags);

    // Adjust correction history
    if (!board->stack->checkers && (bestMove == MOVE_NONE || !board->isCapture(bestMove)) && (!failHigh || bestValue > stack->staticEval) && (!failLow || bestValue <= stack->staticEval)) {
        int bonus = std::clamp((int)(bestValue - stack->staticEval) * depth * correctionHistoryFactor / 1024, -CORRECTION_HISTORY_LIMIT / 4, CORRECTION_HISTORY_LIMIT / 4);
        history.updateCorrectionHistory(board, bonus);
    }

    assert(bestValue > -EVAL_INFINITE && bestValue < EVAL_INFINITE);

    return bestValue;
}

void Thread::tsearch() {
    if (TUNE_ENABLED)
        initReductions();

    nnue.reset(&rootBoard);

    searchData.nodesSearched = 0;
    if (mainThread)
        initTimeManagement(&rootBoard, searchParameters, &searchData);

    iterativeDeepening();
    sortRootMoves();

    if (mainThread) {
        threadPool->stopSearching();
        threadPool->waitForHelpersFinished();

        Thread* bestThread = chooseBestThread();

        if (bestThread != this) {
            printUCI(bestThread);
        }

        if (!UCI::Options.ponder.value || bestThread->rootMoves[0].pv.size() < 2) {
            std::cout << "bestmove " << moveToString(bestThread->rootMoves[0].move, UCI::Options.chess960.value) << std::endl;
        }
        else {
            std::cout << "bestmove " << moveToString(bestThread->rootMoves[0].move, UCI::Options.chess960.value) << " ponder " << moveToString(bestThread->rootMoves[0].pv[1], UCI::Options.chess960.value) << std::endl;
        }
    }
}

void Thread::iterativeDeepening() {
    int multiPvCount = 0;
    {
        Move moves[MAX_MOVES] = { MOVE_NONE };
        int m = 0;
        generateMoves(&rootBoard, moves, &m);
        for (int i = 0; i < m; i++) {
            if (rootBoard.isLegal(moves[i])) {
                multiPvCount++;

                RootMove rootMove = {};
                rootMove.move = moves[i];
                rootMoves.push_back(rootMove);
            }
        }
    }
    multiPvCount = std::min(multiPvCount, UCI::Options.multiPV.value);

    int maxDepth = searchParameters->depth == 0 ? MAX_PLY - 1 : searchParameters->depth;

    Eval previousValue = EVAL_NONE;
    Move previousMove = MOVE_NONE;

    int bestMoveStability = 0;

    constexpr int STACK_OVERHEAD = 4;
    SearchStack stackList[MAX_PLY + STACK_OVERHEAD];
    SearchStack* stack = &stackList[STACK_OVERHEAD];

    rootMoveNodes.clear();

    for (int depth = 1; depth <= maxDepth; depth++) {
        excludedRootMoves.clear();
        for (int rootMoveIdx = 0; rootMoveIdx < multiPvCount; rootMoveIdx++) {

            for (int i = 0; i < MAX_PLY + STACK_OVERHEAD; i++) {
                stackList[i].pvLength = 0;
                stackList[i].ply = i - STACK_OVERHEAD;
                stackList[i].staticEval = EVAL_NONE;
                stackList[i].excludedMove = MOVE_NONE;
                stackList[i].killer = MOVE_NONE;
                stackList[i].movedPiece = Piece::NONE;
                stackList[i].move = MOVE_NONE;
                stackList[i].capture = false;
                stackList[i].inCheck = false;
            }

            searchData.rootDepth = depth;
            searchData.selDepth = 0;

            // Aspiration Windows
            Eval delta = 2 * EVAL_INFINITE;
            Eval alpha = -EVAL_INFINITE;
            Eval beta = EVAL_INFINITE;
            Eval value;

            if (depth >= aspirationWindowMinDepth) {
                // Set up interval for the start of this aspiration window
                delta = aspirationWindowDelta;
                alpha = std::max(previousValue - delta, -EVAL_INFINITE);
                beta = std::min(previousValue + delta, (int)EVAL_INFINITE);
            }

            int failHighs = 0;
            while (true) {
                int searchDepth = std::max(1, depth - failHighs);
                value = search<ROOT_NODE>(&rootBoard, stack, searchDepth, alpha, beta, false);

                sortRootMoves();

                // Stop if we need to
                if (stopped || exiting)
                    break;

                // Our window was too high, lower alpha for next iteration
                if (value <= alpha) {
                    beta = (alpha + beta) / 2;
                    alpha = std::max(value - delta, -EVAL_INFINITE);
                    failHighs = 0;
                }
                // Our window was too low, increase beta for next iteration
                else if (value >= beta) {
                    beta = std::min(value + delta, (int)EVAL_INFINITE);
                    failHighs = std::min(failHighs + 1, aspirationWindowMaxFailHighs);
                }
                // Our window was good, increase depth for next iteration
                else
                    break;

                if (value >= EVAL_MATE_IN_MAX_PLY) {
                    beta = EVAL_INFINITE;
                    failHighs = 0;
                }

                delta *= aspirationWindowDeltaFactor;
            }

            if (stopped || exiting)
                return;

            excludedRootMoves.push_back(stack->pv[0]);
        }

        if (mainThread) {
            printUCI(this, multiPvCount);

            // Adjust time management
            double tmAdjustment = tmInitialAdjustment;

            // Based on best move stability
            if (rootMoves[0].move == previousMove)
                bestMoveStability = std::min(bestMoveStability + 1, tmBestMoveStabilityMax);
            else
                bestMoveStability = 0;
            tmAdjustment *= tmBestMoveStabilityBase - bestMoveStability * tmBestMoveStabilityFactor;

            // Based on score difference to last iteration
            tmAdjustment *= tmEvalDiffBase + std::clamp(previousValue - rootMoves[0].value, tmEvalDiffMin, tmEvalDiffMax) * tmEvalDiffFactor;

            // Based on fraction of nodes that went into the best move
            tmAdjustment *= tmNodesBase - tmNodesFactor * ((double)rootMoveNodes[rootMoves[0].move] / (double)searchData.nodesSearched);

            if (timeOverDepthCleared(searchParameters, &searchData, tmAdjustment)) {
                threadPool->stopSearching();
                return;
            }
        }

        previousMove = rootMoves[0].move;
        previousValue = rootMoves[0].value;
    }
}

void Thread::printUCI(Thread* thread, int multiPvCount) {
    int64_t ms = getTime() - searchData.startTime;
    int64_t nodes = threadPool->nodesSearched();
    int64_t nps = ms == 0 ? 0 : nodes / ((double)ms / 1000);

    for (int rootMoveIdx = 0; rootMoveIdx < multiPvCount; rootMoveIdx++) {
        RootMove rootMove = thread->rootMoves[rootMoveIdx];
        std::cout << "info depth " << rootMove.depth << " seldepth " << rootMove.selDepth << " score " << formatEval(rootMove.value) << " multipv " << (rootMoveIdx + 1) << " nodes " << nodes << " time " << ms << " nps " << nps << " hashfull " << TT.hashfull() << " pv ";

        // Send PV
        for (Move move : rootMove.pv)
            std::cout << moveToString(move, UCI::Options.chess960.value) << " ";
        std::cout << std::endl;
    }
}

void Thread::sortRootMoves() {
    std::stable_sort(rootMoves.begin(), rootMoves.end(), [](RootMove rm1, RootMove rm2) {
        if (rm1.depth > rm2.depth) return true;
        if (rm1.depth < rm2.depth) return false;
        return rm1.value > rm2.value;
        });
}

Thread* Thread::chooseBestThread() {
    Thread* bestThread = this;
    size_t bestMoveIdx = 0;

    if (threadPool->threads.size() > 1 && UCI::Options.multiPV.value == 1) {
        std::map<Move, int64_t> votes;
        Eval minValue = EVAL_INFINITE;

        for (auto& th : threadPool->threads) {
            for (auto& rm : th.get()->rootMoves) {
                if (rm.value == -EVAL_INFINITE)
                    break;
                minValue = std::min(minValue, rm.value);
            }
        }
        minValue++;

        for (auto& th : threadPool->threads) {
            for (auto& rm : th->rootMoves) {
                if (rm.value == -EVAL_INFINITE)
                    break;
                // Votes weighted by depth and difference to the minimum value
                votes[rm.move] += (rm.value - minValue + 10) * std::max(1, rm.depth - (th->rootMoves[0].depth - rm.depth));
            }
        }

        for (auto& th : threadPool->threads) {
            Thread* thread = th.get();
            for (size_t rmi = 0; rmi < thread->rootMoves.size(); rmi++) {
                RootMove& rm = thread->rootMoves[rmi];
                if (rm.value == -EVAL_INFINITE)
                    break;

                Eval thValue = rm.value;
                Eval bestValue = bestThread->rootMoves[bestMoveIdx].value;
                Move thMove = rm.move;
                Move bestMove = bestThread->rootMoves[bestMoveIdx].move;

                // In case of checkmate, take the shorter mate / longer getting mated
                if (std::abs(bestValue) >= EVAL_MATE_IN_MAX_PLY) {
                    if (thValue > bestValue) {
                        bestThread = thread;
                        bestMoveIdx = rmi;
                    }
                }
                // We have found a mate, take it without voting
                else if (thValue >= EVAL_MATE_IN_MAX_PLY) {
                    bestThread = thread;
                    bestMoveIdx = rmi;
                }
                // No mate found by any thread so far, take the thread with more votes
                else if (votes[thMove] > votes[bestMove]) {
                    bestThread = thread;
                    bestMoveIdx = rmi;
                }
                // In case of same move, choose the thread with the highest score
                else if (thMove == bestMove && thValue > bestValue && rm.pv.size() > 2) {
                    bestThread = thread;
                    bestMoveIdx = rmi;
                }
            }
        }
    }

    if (bestMoveIdx)
        std::swap(bestThread->rootMoves[0], bestThread->rootMoves[bestMoveIdx]);

    return bestThread;
}

void Thread::tdatagen() {
    nnue.reset(&rootBoard);

    searchData.nodesSearched = 0;
    initTimeManagement(&rootBoard, searchParameters, &searchData);
    {
        Move moves[MAX_MOVES] = { MOVE_NONE };
        int m = 0;
        generateMoves(&rootBoard, moves, &m);
        for (int i = 0; i < m; i++) {
            if (rootBoard.isLegal(moves[i])) {
                RootMove rootMove = {};
                rootMove.move = moves[i];
                rootMoves.push_back(rootMove);
            }
        }
    }

    Eval previousValue = EVAL_NONE;

    constexpr int STACK_OVERHEAD = 4;
    SearchStack stackList[MAX_PLY + STACK_OVERHEAD];
    SearchStack* stack = &stackList[STACK_OVERHEAD];

    rootMoveNodes.clear();

    int maxDepth = searchParameters->depth ? searchParameters->depth : MAX_PLY - 1;

    for (int depth = 1; depth <= maxDepth; depth++) {
        for (int i = 0; i < MAX_PLY + STACK_OVERHEAD; i++) {
            stackList[i].pvLength = 0;
            stackList[i].ply = i - STACK_OVERHEAD;
            stackList[i].staticEval = EVAL_NONE;
            stackList[i].excludedMove = MOVE_NONE;
            stackList[i].killer = MOVE_NONE;
            stackList[i].movedPiece = Piece::NONE;
            stackList[i].move = MOVE_NONE;
            stackList[i].capture = false;
            stackList[i].inCheck = false;
        }

        searchData.rootDepth = depth;
        searchData.selDepth = 0;

        // Aspiration Windows
        Eval delta = 2 * EVAL_INFINITE;
        Eval alpha = -EVAL_INFINITE;
        Eval beta = EVAL_INFINITE;
        Eval value;

        if (depth >= aspirationWindowMinDepth) {
            // Set up interval for the start of this aspiration window
            delta = aspirationWindowDelta;
            alpha = std::max(previousValue - delta, -EVAL_INFINITE);
            beta = std::min(previousValue + delta, (int)EVAL_INFINITE);
        }

        int failHighs = 0;
        while (true) {
            int searchDepth = std::max(1, depth - failHighs);
            value = search<ROOT_NODE>(&rootBoard, stack, searchDepth, alpha, beta, false);

            sortRootMoves();

            // Stop if we need to
            if (stopped || exiting || searchData.nodesSearched >= searchParameters->nodes)
                break;

            // Our window was too high, lower alpha for next iteration
            if (value <= alpha) {
                beta = (alpha + beta) / 2;
                alpha = std::max(value - delta, -EVAL_INFINITE);
                failHighs = 0;
            }
            // Our window was too low, increase beta for next iteration
            else if (value >= beta) {
                beta = std::min(value + delta, (int)EVAL_INFINITE);
                failHighs = std::min(failHighs + 1, aspirationWindowMaxFailHighs);
            }
            // Our window was good, increase depth for next iteration
            else
                break;

            if (value >= EVAL_MATE_IN_MAX_PLY) {
                beta = EVAL_INFINITE;
                failHighs = 0;
            }

            delta *= aspirationWindowDeltaFactor;
        }

        if (stopped || exiting || searchData.nodesSearched >= searchParameters->nodes)
            break;

        previousValue = rootMoves[0].value;
    }

    sortRootMoves();
    printUCI(this);

    std::cout << "bestmove " << moveToString(rootMoves[0].move, UCI::Options.chess960.value) << std::endl;
}