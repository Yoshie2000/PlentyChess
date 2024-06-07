#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <cassert>

#include "move.h"
#include "types.h"
#include "board.h"
#include "bitboard.h"
#include "zobrist.h"
#include "magic.h"
#include "evaluation.h"

void Board::startpos() {
    parseFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", false);
}

size_t Board::parseFen(std::string fen, bool isChess960) {
    Square currentSquare = 56;
    uint8_t currentRank = 7;
    size_t i = 0;

    // Reset everything (there might be garbage data)
    for (Square s = 0; s < 64; s++) {
        pieces[s] = NO_PIECE;
    }
    for (Color c = 0; c <= 1; c++) {
        byColor[c] = bitboard(0);
        for (Piece p = 0; p < PIECE_TYPES; p++) {
            stack->pieceCount[c][p] = 0;
            byPiece[p] = bitboard(0);
        }
    }
    stack->checkers = bitboard(0);
    stack->capturedPiece = NO_PIECE;
    stack->hash = 0;
    stack->pawnHash = ZOBRIST_NO_PAWNS;
    stack->nullmove_ply = 0;

    // Board position and everything
    Bitboard currentSquareBB = bitboard(currentSquare);
    for (i = 0; i < fen.length(); i++) {
        char c = fen.at(i);
        if (c == ' ') {
            i++;
            break;
        }

        if (currentSquare < 64)
            currentSquareBB = bitboard(currentSquare);
        switch (c) {
        case 'p':
            byColor[COLOR_BLACK] |= currentSquareBB;
            byPiece[PIECE_PAWN] |= currentSquareBB;
            stack->pieceCount[COLOR_BLACK][PIECE_PAWN]++;
            pieces[currentSquare] = PIECE_PAWN;
            stack->hash ^= ZOBRIST_PIECE_SQUARES[COLOR_BLACK][PIECE_PAWN][currentSquare];
            stack->pawnHash ^= ZOBRIST_PIECE_SQUARES[COLOR_BLACK][PIECE_PAWN][currentSquare];
            currentSquare++;
            break;
        case 'P':
            byColor[COLOR_WHITE] |= currentSquareBB;
            byPiece[PIECE_PAWN] |= currentSquareBB;
            stack->pieceCount[COLOR_WHITE][PIECE_PAWN]++;
            pieces[currentSquare] = PIECE_PAWN;
            stack->hash ^= ZOBRIST_PIECE_SQUARES[COLOR_WHITE][PIECE_PAWN][currentSquare];
            stack->pawnHash ^= ZOBRIST_PIECE_SQUARES[COLOR_WHITE][PIECE_PAWN][currentSquare];
            currentSquare++;
            break;
        case 'n':
            byColor[COLOR_BLACK] |= currentSquareBB;
            byPiece[PIECE_KNIGHT] |= currentSquareBB;
            stack->pieceCount[COLOR_BLACK][PIECE_KNIGHT]++;
            pieces[currentSquare] = PIECE_KNIGHT;
            stack->hash ^= ZOBRIST_PIECE_SQUARES[COLOR_BLACK][PIECE_KNIGHT][currentSquare];
            currentSquare++;
            break;
        case 'N':
            byColor[COLOR_WHITE] |= currentSquareBB;
            byPiece[PIECE_KNIGHT] |= currentSquareBB;
            stack->pieceCount[COLOR_WHITE][PIECE_KNIGHT]++;
            pieces[currentSquare] = PIECE_KNIGHT;
            stack->hash ^= ZOBRIST_PIECE_SQUARES[COLOR_WHITE][PIECE_KNIGHT][currentSquare];
            currentSquare++;
            break;
        case 'b':
            byColor[COLOR_BLACK] |= currentSquareBB;
            byPiece[PIECE_BISHOP] |= currentSquareBB;
            stack->pieceCount[COLOR_BLACK][PIECE_BISHOP]++;
            pieces[currentSquare] = PIECE_BISHOP;
            stack->hash ^= ZOBRIST_PIECE_SQUARES[COLOR_BLACK][PIECE_BISHOP][currentSquare];
            currentSquare++;
            break;
        case 'B':
            byColor[COLOR_WHITE] |= currentSquareBB;
            byPiece[PIECE_BISHOP] |= currentSquareBB;
            stack->pieceCount[COLOR_WHITE][PIECE_BISHOP]++;
            pieces[currentSquare] = PIECE_BISHOP;
            stack->hash ^= ZOBRIST_PIECE_SQUARES[COLOR_WHITE][PIECE_BISHOP][currentSquare];
            currentSquare++;
            break;
        case 'r':
            byColor[COLOR_BLACK] |= currentSquareBB;
            byPiece[PIECE_ROOK] |= currentSquareBB;
            stack->pieceCount[COLOR_BLACK][PIECE_ROOK]++;
            pieces[currentSquare] = PIECE_ROOK;
            stack->hash ^= ZOBRIST_PIECE_SQUARES[COLOR_BLACK][PIECE_ROOK][currentSquare];
            currentSquare++;
            break;
        case 'R':
            byColor[COLOR_WHITE] |= currentSquareBB;
            byPiece[PIECE_ROOK] |= currentSquareBB;
            stack->pieceCount[COLOR_WHITE][PIECE_ROOK]++;
            pieces[currentSquare] = PIECE_ROOK;
            stack->hash ^= ZOBRIST_PIECE_SQUARES[COLOR_WHITE][PIECE_ROOK][currentSquare];
            currentSquare++;
            break;
        case 'q':
            byColor[COLOR_BLACK] |= currentSquareBB;
            byPiece[PIECE_QUEEN] |= currentSquareBB;
            stack->pieceCount[COLOR_BLACK][PIECE_QUEEN]++;
            pieces[currentSquare] = PIECE_QUEEN;
            stack->hash ^= ZOBRIST_PIECE_SQUARES[COLOR_BLACK][PIECE_QUEEN][currentSquare];
            currentSquare++;
            break;
        case 'Q':
            byColor[COLOR_WHITE] |= currentSquareBB;
            byPiece[PIECE_QUEEN] |= currentSquareBB;
            stack->pieceCount[COLOR_WHITE][PIECE_QUEEN]++;
            pieces[currentSquare] = PIECE_QUEEN;
            stack->hash ^= ZOBRIST_PIECE_SQUARES[COLOR_WHITE][PIECE_QUEEN][currentSquare];
            currentSquare++;
            break;
        case 'k':
            byColor[COLOR_BLACK] |= currentSquareBB;
            byPiece[PIECE_KING] |= currentSquareBB;
            stack->pieceCount[COLOR_BLACK][PIECE_KING]++;
            pieces[currentSquare] = PIECE_KING;
            stack->hash ^= ZOBRIST_PIECE_SQUARES[COLOR_BLACK][PIECE_KING][currentSquare];
            currentSquare++;
            break;
        case 'K':
            byColor[COLOR_WHITE] |= currentSquareBB;
            byPiece[PIECE_KING] |= currentSquareBB;
            stack->pieceCount[COLOR_WHITE][PIECE_KING]++;
            pieces[currentSquare] = PIECE_KING;
            stack->hash ^= ZOBRIST_PIECE_SQUARES[COLOR_WHITE][PIECE_KING][currentSquare];
            currentSquare++;
            break;
        case '/':
            currentRank--;
            currentSquare = 8 * currentRank;
            break;
        default: // Number
            currentSquare += int(c) - 48;
            break;
        }
    }

    // Side to move
    stm = fen.at(i) == 'w' ? COLOR_WHITE : COLOR_BLACK;
    if (stm == COLOR_BLACK)
        stack->hash ^= ZOBRIST_STM_BLACK;
    i += 2;

    // Castling
    stack->castling = 0;
    castlingSquares[0] = castlingSquares[1] = castlingSquares[2] = castlingSquares[3] = NO_SQUARE;
    for (; i < fen.length(); i++) {
        char c = fen.at(i);

        if (c == '-') {
            i += 2;
            break;
        }
        else if (c == ' ') {
            i++;
            break;
        }

        Bitboard rookBB;
        std::vector<Square> rooks;

        switch (c) {
        case 'k':
            stack->castling |= CASTLING_BLACK_KINGSIDE;
            // Chess960 support: Find black kingside rook
            rookBB = byColor[COLOR_BLACK] & byPiece[PIECE_ROOK] & BB::RANK_8;
            rooks.clear();
            while (rookBB) {
                rooks.push_back(popLSB(&rookBB));
            }
            castlingSquares[2] = *std::max_element(rooks.begin(), rooks.end());
            break;
        case 'K':
            stack->castling |= CASTLING_WHITE_KINGSIDE;
            // Chess960 support: Find white kingside rook
            rookBB = byColor[COLOR_WHITE] & byPiece[PIECE_ROOK] & BB::RANK_1;
            rooks.clear();
            while (rookBB) {
                rooks.push_back(popLSB(&rookBB));
            }
            castlingSquares[0] = *std::max_element(rooks.begin(), rooks.end());
            break;
        case 'q':
            stack->castling |= CASTLING_BLACK_QUEENSIDE;
            // Chess960 support: Find black queenside rook
            rookBB = byColor[COLOR_BLACK] & byPiece[PIECE_ROOK] & BB::RANK_8;
            rooks.clear();
            while (rookBB) {
                rooks.push_back(popLSB(&rookBB));
            }
            castlingSquares[3] = *std::min_element(rooks.begin(), rooks.end());
            break;
        case 'Q':
            stack->castling |= CASTLING_WHITE_QUEENSIDE;
            // Chess960 support: Find white queenside rook
            rookBB = byColor[COLOR_WHITE] & byPiece[PIECE_ROOK] & BB::RANK_1;
            rooks.clear();
            while (rookBB) {
                rooks.push_back(popLSB(&rookBB));
            }
            castlingSquares[1] = *std::min_element(rooks.begin(), rooks.end());
            break;
        case ' ':
        default:
            // Chess960 castling
            if (c >= 'A' && c <= 'H') {
                // White
                Square king = lsb(byColor[COLOR_WHITE] & byPiece[PIECE_KING]);
                int rookFile = int(c - 'A');
                if (rookFile > fileOf(king)) {
                    stack->castling |= CASTLING_WHITE_KINGSIDE;
                    castlingSquares[0] = Square(rookFile);
                }
                else {
                    stack->castling |= CASTLING_WHITE_QUEENSIDE;
                    castlingSquares[1] = Square(rookFile);
                }
                break;
            }
            else if (c >= 'a' && c <= 'h') {
                // Black
                Square king = lsb(byColor[COLOR_BLACK] & byPiece[PIECE_KING]);
                int rookFile = int(c - 'a');
                if (rookFile > fileOf(king)) {
                    stack->castling |= CASTLING_BLACK_KINGSIDE;
                    castlingSquares[2] = Square(56 + rookFile);
                }
                else {
                    stack->castling |= CASTLING_BLACK_QUEENSIDE;
                    castlingSquares[3] = Square(56 + rookFile);
                }
                break;
            }
            std::cout << "Weird char in fen castling: " << c << " (index " << i << ")" << std::endl;
            exit(-1);
            break;
        }
    }
    stack->hash ^= ZOBRIST_CASTLING[stack->castling & CASTLING_MASK];

    // en passent
    if (fen.at(i) == '-') {
        stack->enpassantTarget = bitboard(0);
        i += 2;
    }
    else {
        char epTargetString[2] = { fen.at(i), fen.at(i + 1) };
        Square epTargetSquare = stringToSquare(epTargetString);
        stack->enpassantTarget = bitboard(epTargetSquare);
        i += 3;

        // Check if there's *actually* a pawn that can do enpassent
        Bitboard enemyPawns = byColor[flip(stm)] & byPiece[PIECE_PAWN];
        Bitboard epRank = stm == COLOR_WHITE ? BB::RANK_4 : BB::RANK_5;
        Square pawnSquare1 = epTargetSquare + 1 + UP[stm];
        Square pawnSquare2 = epTargetSquare - 1 + UP[stm];
        Bitboard epPawns = (bitboard(pawnSquare1) | bitboard(pawnSquare2)) & epRank & enemyPawns;
        if (epPawns)
            stack->hash ^= ZOBRIST_ENPASSENT[fileOf(epTargetSquare)];
    }

    if (fen.length() <= i) {
        stack->rule50_ply = 0;
        ply = 0;
    }
    else {
        // 50 move rule
        std::string rule50String = "--";
        int rule50tmp = 0;
        while (fen.at(i) != ' ') {
            rule50String.replace(rule50tmp++, 1, 1, fen.at(i++));
        }
        if (rule50String.at(1) == '-') {
            stack->rule50_ply = int(rule50String.at(0)) - 48;
        }
        else {
            stack->rule50_ply = 10 * (int(rule50String.at(0)) - 48) + (int(rule50String.at(1)) - 48);
        }
        i++;

        // Move number
        std::string plyString = "---";
        int plyTmp = 0;
        while (i < fen.length() && fen.at(i) != ' ') {
            plyString.replace(plyTmp++, 1, 1, fen.at(i++));
        }
        if (plyString.at(1) == '-') {
            ply = int(plyString.at(0)) - 48;
        }
        else if (plyString.at(2) == '-') {
            ply = 10 * (int(plyString.at(0)) - 48) + (int(plyString.at(1)) - 48);
        }
        else {
            ply = 100 * (int(plyString.at(0)) - 48) + 10 * (int(plyString.at(1)) - 48) + (int(plyString.at(2)) - 48);
        }
    }

    // Update king checking stuff
    Square enemyKing = lsb(byColor[stm] & byPiece[PIECE_KING]);
    stack->checkers = attackersTo(enemyKing, byColor[COLOR_WHITE] | byColor[COLOR_BLACK]) & byColor[flip(stm)];
    stack->checkerCount = __builtin_popcountll(stack->checkers);

    updateSliderPins(COLOR_WHITE);
    updateSliderPins(COLOR_BLACK);

    chess960 = isChess960;

    return i;
}

void Board::calculateCastlingSquares(Square kingOrigin, Square* kingTarget, Square* rookOrigin, Square* rookTarget, uint8_t* castling) {
    if (stm == COLOR_WHITE) {
        int idx = *kingTarget <= kingOrigin;
        *rookOrigin = castlingSquares[idx];
        *rookTarget = CASTLING_ROOK_SQUARES[idx];
        *kingTarget = CASTLING_KING_SQUARES[idx];
        *castling &= CASTLING_BLACK_KINGSIDE | CASTLING_BLACK_QUEENSIDE;
    }
    else {
        int idx = 2 + (*kingTarget <= kingOrigin);
        *rookOrigin = castlingSquares[idx];
        *rookTarget = CASTLING_ROOK_SQUARES[idx];
        *kingTarget = CASTLING_KING_SQUARES[idx];
        *castling &= CASTLING_WHITE_KINGSIDE | CASTLING_WHITE_QUEENSIDE;
    }
}

void Board::doMove(BoardStack* newStack, Move move, uint64_t newHash, NNUE* nnue) {
    // General setup stuff
    newStack->previous = stack;
    stack = newStack;
    memcpy(newStack->pieceCount, newStack->previous->pieceCount, sizeof(int) * 12 + sizeof(uint8_t));

    newStack->hash = newHash;
    newStack->pawnHash = newStack->previous->pawnHash;
    newStack->rule50_ply = newStack->previous->rule50_ply + 1;
    newStack->nullmove_ply = newStack->previous->nullmove_ply + 1;

    nnue->incrementAccumulator();

    // Calculate general information about the move
    Square origin = moveOrigin(move);
    Square target = moveTarget(move);
    MoveType type = moveType(move);

    assert(origin < 64 && target < 64);

    newStack->capturedPiece = pieces[target];
    newStack->enpassantTarget = 0;
    Square captureTarget = target;
    Bitboard captureTargetBB = bitboard(captureTarget);

    Piece piece = pieces[origin];
    Piece promotionPiece = NO_PIECE;

    Bitboard originBB = bitboard(origin);
    Bitboard targetBB = bitboard(target);
    Bitboard fromTo = originBB | targetBB;

    switch (type) {
    case MOVE_PROMOTION:
        movePiece(piece, origin, target, fromTo);
        newStack->rule50_ply = 0;

        newStack->pawnHash ^= ZOBRIST_PIECE_SQUARES[stm][PIECE_PAWN][origin];

        if (newStack->capturedPiece != NO_PIECE) {
            byColor[flip(stm)] ^= captureTargetBB; // take away the captured piece
            byPiece[newStack->capturedPiece] ^= captureTargetBB;

            nnue->removePiece(captureTarget, newStack->capturedPiece, flip(stm));

            newStack->pieceCount[flip(stm)][newStack->capturedPiece]--;

            if (newStack->capturedPiece == PIECE_ROOK) {
                Square rookSquare = piece == PIECE_ROOK ? origin : captureTarget;
                if (rookSquare == castlingSquares[0]) {
                    newStack->castling &= ~CASTLING_WHITE_KINGSIDE;
                }
                else if (rookSquare == castlingSquares[1]) {
                    newStack->castling &= ~CASTLING_WHITE_QUEENSIDE;
                }
                else if (rookSquare == castlingSquares[2]) {
                    newStack->castling &= ~CASTLING_BLACK_KINGSIDE;
                }
                else if (rookSquare == castlingSquares[3]) {
                    newStack->castling &= ~CASTLING_BLACK_QUEENSIDE;
                }
            }
        }

        promotionPiece = PROMOTION_PIECE[promotionType(move)];
        byPiece[piece] ^= targetBB;
        byPiece[promotionPiece] ^= targetBB;

        pieces[target] = promotionPiece;

        // Promotion, we don't move the current piece, instead we remove it from the origin square
        // and place the promotionPiece on the target square. This saves one accumulator update
        nnue->removePiece(origin, piece, stm);
        nnue->addPiece(target, promotionPiece, stm);

        newStack->pieceCount[stm][PIECE_PAWN]--;
        newStack->pieceCount[stm][promotionPiece]++;

        break;

    case MOVE_CASTLING: {
        Square rookOrigin, rookTarget;
        calculateCastlingSquares(origin, &target, &rookOrigin, &rookTarget, &newStack->castling);
        assert(rookOrigin < 64 && rookTarget < 64 && target < 64);

        Bitboard rookFromToBB = bitboard(rookOrigin) ^ bitboard(rookTarget);
        targetBB = bitboard(target);
        fromTo = originBB ^ targetBB;

        pieces[rookOrigin] = NO_PIECE;
        pieces[origin] = NO_PIECE;
        pieces[rookTarget] = PIECE_ROOK;
        pieces[target] = PIECE_KING;
        byColor[stm] ^= rookFromToBB ^ fromTo;
        byPiece[PIECE_ROOK] ^= rookFromToBB;
        byPiece[PIECE_KING] ^= fromTo;

        nnue->movePiece(origin, target, PIECE_KING, stm);
        nnue->movePiece(rookOrigin, rookTarget, PIECE_ROOK, stm);

        newStack->capturedPiece = NO_PIECE;
    }
                      break;

    case MOVE_ENPASSANT:
        movePiece(piece, origin, target, fromTo);
        nnue->movePiece(origin, target, piece, stm);
        newStack->rule50_ply = 0;

        captureTarget = target - UP[stm];
        newStack->capturedPiece = PIECE_PAWN;

        assert(captureTarget < 64);

        captureTargetBB = bitboard(captureTarget);
        pieces[captureTarget] = NO_PIECE; // remove the captured pawn

        newStack->pawnHash ^= ZOBRIST_PIECE_SQUARES[stm][PIECE_PAWN][origin] ^ ZOBRIST_PIECE_SQUARES[stm][PIECE_PAWN][target] ^ ZOBRIST_PIECE_SQUARES[flip(stm)][PIECE_PAWN][captureTarget];

        byColor[flip(stm)] ^= captureTargetBB; // take away the captured piece
        byPiece[PIECE_PAWN] ^= captureTargetBB;

        nnue->removePiece(captureTarget, PIECE_PAWN, flip(stm));

        newStack->pieceCount[flip(stm)][PIECE_PAWN]--;

        break;

    default: // Normal moves
        movePiece(piece, origin, target, fromTo);
        nnue->movePiece(origin, target, piece, stm);

        if (piece == PIECE_PAWN) {
            // Double push
            if ((origin ^ target) == 16) {
                assert(target - UP[stm] < 64);

                Bitboard enemyPawns = byColor[flip(stm)] & byPiece[PIECE_PAWN];
                Bitboard epRank = stm == COLOR_WHITE ? BB::RANK_4 : BB::RANK_5;
                Square pawnSquare1 = target + 1;
                Square pawnSquare2 = target - 1;
                Bitboard epPawns = (bitboard(pawnSquare1) | bitboard(pawnSquare2)) & epRank & enemyPawns;
                if (epPawns) {
                    Square epTarget = target - UP[stm];
                    newStack->enpassantTarget = bitboard(epTarget);
                }

            }

            newStack->rule50_ply = 0;
            newStack->pawnHash ^= ZOBRIST_PIECE_SQUARES[stm][PIECE_PAWN][origin] ^ ZOBRIST_PIECE_SQUARES[stm][PIECE_PAWN][target];
        }

        if (newStack->capturedPiece != NO_PIECE) {
            byColor[flip(stm)] ^= captureTargetBB; // take away the captured piece
            byPiece[newStack->capturedPiece] ^= captureTargetBB;

            if (newStack->capturedPiece == PIECE_PAWN)
                newStack->pawnHash ^= ZOBRIST_PIECE_SQUARES[flip(stm)][PIECE_PAWN][captureTarget];

            nnue->removePiece(captureTarget, newStack->capturedPiece, flip(stm));

            newStack->pieceCount[flip(stm)][newStack->capturedPiece]--;
            newStack->rule50_ply = 0;
        }

        // Unset castling flags if necessary
        if (piece == PIECE_KING) {
            if (stm == COLOR_WHITE) {
                newStack->castling &= CASTLING_BLACK_KINGSIDE | CASTLING_BLACK_QUEENSIDE;
            }
            else {
                newStack->castling &= CASTLING_WHITE_KINGSIDE | CASTLING_WHITE_QUEENSIDE;
            }
        }
        if (piece == PIECE_ROOK || newStack->capturedPiece == PIECE_ROOK) {
            Square rookSquare = piece == PIECE_ROOK ? origin : captureTarget;
            if (rookSquare == castlingSquares[0]) {
                newStack->castling &= ~CASTLING_WHITE_KINGSIDE;
            }
            else if (rookSquare == castlingSquares[1]) {
                newStack->castling &= ~CASTLING_WHITE_QUEENSIDE;
            }
            else if (rookSquare == castlingSquares[2]) {
                newStack->castling &= ~CASTLING_BLACK_KINGSIDE;
            }
            else if (rookSquare == castlingSquares[3]) {
                newStack->castling &= ~CASTLING_BLACK_QUEENSIDE;
            }
        }
        break;
    }

    // Update king checking stuff
    assert((byColor[flip(stm)] & byPiece[PIECE_KING]) > 0);
    assert((byColor[stm] & byPiece[PIECE_KING]) > 0);

    Square enemyKing = lsb(byColor[flip(stm)] & byPiece[PIECE_KING]);
    newStack->checkers = attackersTo(enemyKing, byColor[COLOR_WHITE] | byColor[COLOR_BLACK]) & byColor[stm];
    newStack->checkerCount = newStack->checkers ? __builtin_popcountll(newStack->checkers) : 0;
    updateSliderPins(COLOR_WHITE);
    updateSliderPins(COLOR_BLACK);

    stm = flip(stm);
    newStack->move = move;
}

void Board::undoMove(Move move, NNUE* nnue) {
    stm = flip(stm);

    Square origin = moveOrigin(move);
    Square target = moveTarget(move);
    Square captureTarget = target;
    Bitboard captureTargetBB = bitboard(captureTarget);

    assert(origin < 64 && target < 64);

    Bitboard originBB = bitboard(origin);
    Bitboard targetBB = bitboard(target);
    Bitboard fromTo = originBB | targetBB;

    Piece piece = pieces[target];
    MoveType type = moveType(move);

    // Castling
    if (type == MOVE_CASTLING) {
        Square rookOrigin, rookTarget;
        calculateCastlingSquares(origin, &target, &rookOrigin, &rookTarget, &stack->castling);

        assert(rookOrigin < 64 && rookTarget < 64 && target < 64);

        Bitboard rookFromToBB = (bitboard(rookOrigin)) ^ (bitboard(rookTarget));
        targetBB = bitboard(target);
        fromTo = originBB ^ targetBB;

        pieces[rookTarget] = NO_PIECE;
        pieces[target] = NO_PIECE;
        pieces[rookOrigin] = PIECE_ROOK;
        pieces[origin] = PIECE_KING;
        byColor[stm] ^= rookFromToBB ^ fromTo;
        byPiece[PIECE_ROOK] ^= rookFromToBB;
        byPiece[PIECE_KING] ^= fromTo;
    }
    else {
        byColor[stm] ^= fromTo;
        byPiece[piece] ^= fromTo;

        pieces[target] = NO_PIECE;
        pieces[origin] = piece;
    }

    // This move is en passent
    if (type == MOVE_ENPASSANT) {
        captureTarget = target - UP[stm];

        assert(captureTarget < 64);

        captureTargetBB = bitboard(captureTarget);
    }

    // Handle capture
    if (stack->capturedPiece != NO_PIECE && type != MOVE_CASTLING) {
        pieces[captureTarget] = stack->capturedPiece;
        byColor[flip(stm)] ^= captureTargetBB;
        byPiece[stack->capturedPiece] ^= captureTargetBB;
    }

    // This move is promotion
    if (type == MOVE_PROMOTION) {
        byPiece[piece] ^= originBB;
        byPiece[PIECE_PAWN] ^= originBB;

        pieces[origin] = PIECE_PAWN;
    }

    nnue->decrementAccumulator();

    stack = stack->previous;
}

void Board::doNullMove(BoardStack* newStack) {
    assert(!stack->checkers);

    newStack->previous = stack;
    stack = newStack;
    memcpy(newStack->pieceCount, newStack->previous->pieceCount, sizeof(int) * 12 + sizeof(uint8_t));

    newStack->hash = newStack->previous->hash ^ ZOBRIST_STM_BLACK;
    newStack->pawnHash = newStack->previous->pawnHash;
    newStack->rule50_ply = newStack->previous->rule50_ply + 1;
    newStack->nullmove_ply = 0;

    // En passent square
    if (newStack->previous->enpassantTarget != 0) {
        newStack->hash ^= ZOBRIST_ENPASSENT[fileOf(lsb(newStack->previous->enpassantTarget))];
    }
    newStack->enpassantTarget = 0;

    // Update king checking stuff
    assert((byColor[flip(stm)] & byPiece[PIECE_KING]) > 0);

    newStack->checkers = bitboard(0);
    newStack->checkerCount = 0;
    updateSliderPins(COLOR_WHITE);
    updateSliderPins(COLOR_BLACK);

    stm = flip(stm);
    newStack->move = MOVE_NULL;
}

void Board::undoNullMove() {
    stm = flip(stm);
    stack = stack->previous;
}

bool Board::isCapture(Move move) {
    MoveType type = moveType(move);
    if (type == MOVE_CASTLING) return false;
    if (type == MOVE_ENPASSANT || (type == MOVE_PROMOTION && promotionType(move) == PROMOTION_QUEEN)) return true;
    return pieces[moveTarget(move)] != NO_PIECE;
}

bool Board::isPseudoLegal(Move move) {
    Square origin = moveOrigin(move);
    Square target = moveTarget(move);
    Bitboard originBB = bitboard(origin);
    Bitboard targetBB = bitboard(target);
    Piece piece = pieces[origin];
    MoveType type = moveType(move);

    // A valid piece needs to be on the origin square
    if (pieces[origin] == NO_PIECE) return false;
    // We can't capture our own piece, except in castling
    if (type != MOVE_CASTLING && (byColor[stm] & targetBB)) return false;
    // We can't move the enemies piece
    if (byColor[flip(stm)] & originBB) return false;

    // Non-standard movetypes
    switch (type) {
    case MOVE_CASTLING: {
        if (piece != PIECE_KING || stack->checkers) return false;

        // Check for pieces between king and rook
        int castlingIdx = castlingIndex(stm, origin, target);
        Square kingSquare = lsb(byColor[stm] & byPiece[PIECE_KING]);
        Square rookSquare = castlingSquares[castlingIdx];
        Square kingTarget = CASTLING_KING_SQUARES[castlingIdx];
        Square rookTarget = CASTLING_ROOK_SQUARES[castlingIdx];

        Bitboard importantSquares = BB::BETWEEN[rookSquare][rookTarget] | BB::BETWEEN[kingSquare][kingTarget];
        importantSquares &= ~bitboard(rookSquare) & ~bitboard(kingSquare);
        if ((byColor[COLOR_WHITE] | byColor[COLOR_BLACK]) & importantSquares) return false;
        // Check for castling flags (Attackers are done in isLegal())
        return stack->castling & CASTLING_FLAGS[castlingIdx];
    }
    case MOVE_ENPASSANT:
        if (piece != PIECE_PAWN) return false;
        if (stack->checkerCount > 1) return false;
        if (stack->checkers && !(lsb(stack->checkers) == target - UP[stm])) return false;
        // Check for EP flag on the right file
        return (bitboard(target) & stack->enpassantTarget) && pieces[target - UP[stm]] == PIECE_PAWN;
    case MOVE_PROMOTION:
        if (piece != PIECE_PAWN) return false;
        if (stack->checkerCount > 1) return false;
        if (stack->checkers) {
            bool capturesChecker = target == lsb(stack->checkers);
            bool blocksCheck = BB::BETWEEN[lsb(byColor[stm] & byPiece[PIECE_KING])][lsb(stack->checkers)] & bitboard(target);
            if (!capturesChecker && !blocksCheck)
                return false;
        }
        if (
            !(BB::pawnAttacks(originBB, stm) & targetBB & byColor[flip(stm)]) && // Capture promotion?
            !(origin + UP[stm] == target && pieces[target] == NO_PIECE)) // Push promotion?
            return false;
        return true;
    default:
        break;
    }

    Bitboard occupied = byColor[COLOR_WHITE] | byColor[COLOR_BLACK];
    switch (piece) {
    case PIECE_PAWN:
        if (targetBB & (BB::RANK_8 | BB::RANK_1)) return false;

        if (
            !(BB::pawnAttacks(originBB, stm) & targetBB & byColor[flip(stm)]) && // Capture?
            !(origin + UP[stm] == target && pieces[target] == NO_PIECE) && // Single push?
            !(origin + 2 * UP[stm] == target && pieces[target] == NO_PIECE && pieces[target - UP[stm]] == NO_PIECE && (originBB & (BB::RANK_2 | BB::RANK_7))) // Double push?
            )
            return false;
        break;
    case PIECE_KNIGHT:
        if (!(BB::KNIGHT_ATTACKS[origin] & targetBB)) return false;
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
        if (!(BB::KING_ATTACKS[origin] & targetBB)) return false;
        break;
    default:
        return false;
    }

    if (stack->checkers) {
        if (piece != PIECE_KING) {
            // Double check requires king moves
            if (stack->checkerCount > 1)
                return false;

            bool capturesChecker = target == lsb(stack->checkers);
            bool blocksCheck = BB::BETWEEN[lsb(byColor[stm] & byPiece[PIECE_KING])][lsb(stack->checkers)] & bitboard(target);
            if (!capturesChecker && !blocksCheck)
                return false;
        }
        // King moves *should* be handled in isLegal()
    }

    return true;
}

bool Board::isLegal(Move move) {
    assert(isPseudoLegal(move));

    Square origin = moveOrigin(move);
    Square target = moveTarget(move);
    Bitboard originBB = bitboard(origin);

    Square king = lsb(byPiece[PIECE_KING] & byColor[stm]);

    MoveType type = moveType(move);
    if (type == MOVE_ENPASSANT) {
        // Check if king would be in check after this move
        Square epSquare = target - UP[stm];

        Bitboard epSquareBB = bitboard(epSquare);
        Bitboard targetBB = bitboard(target);
        Bitboard occupied = ((byColor[COLOR_WHITE] | byColor[COLOR_BLACK]) ^ originBB ^ epSquareBB) | targetBB;

        // Check if any rooks/queens/bishops attack the king square, given the occupied pieces after this EP move
        Bitboard rookAttacks = getRookMoves(king, occupied) & (byPiece[PIECE_ROOK] | byPiece[PIECE_QUEEN]) & byColor[flip(stm)];
        Bitboard bishopAttacks = getBishopMoves(king, occupied) & (byPiece[PIECE_BISHOP] | byPiece[PIECE_QUEEN]) & byColor[flip(stm)];
        return !rookAttacks && !bishopAttacks;
    }

    Bitboard occupied = byColor[COLOR_WHITE] | byColor[COLOR_BLACK];

    if (type == MOVE_CASTLING) {
        int castlingIdx = castlingIndex(stm, origin, target);
        Square adjustedTarget = CASTLING_KING_SQUARES[castlingIdx];

        // Check that none of the important squares (including the current king position!) are being attacked
        if (adjustedTarget < origin) {
            for (Square s = adjustedTarget; s <= origin; s++) {
                if (byColor[flip(stm)] & attackersTo(s, occupied))
                    return false;
            }
        }
        else {
            for (Square s = adjustedTarget; s >= origin; s--) {
                if (byColor[flip(stm)] & attackersTo(s, occupied))
                    return false;
            }
        }
        // Check for castling flags
        return stack->castling & CASTLING_FLAGS[castlingIdx];
    }

    if (pieces[origin] == PIECE_KING) {
        // Check that we're not moving into an attack
        return !(byColor[flip(stm)] & attackersTo(target, occupied ^ bitboard(origin)));
    }

    // Check if we're not pinned to the king, or are moving along the pin
    bool pinned = stack->blockers[stm] & originBB;
    return !pinned || (BB::LINE[origin][target] & bitboard(king));
}

bool Board::givesCheck(Move move) {
    Square origin = moveOrigin(move);
    Square target = moveTarget(move);
    Bitboard occ = byColor[COLOR_WHITE] | byColor[COLOR_BLACK];
    Bitboard enemyKing = byColor[flip(stm)] & byPiece[PIECE_KING];

    // Direct check
    Bitboard attacks = BB::attackedSquares(pieces[origin], target, occ ^ bitboard(origin) ^ bitboard(target), stm);
    if (attacks & enemyKing)
        return true;

    // Discovered check: Are we blocking a check to the enemy king?
    MoveType type = moveType(move);
    if (stack->blockers[flip(stm)] & bitboard(origin))
        return !(BB::LINE[origin][target] & enemyKing) || type == MOVE_CASTLING;

    switch (type) {
    case MOVE_PROMOTION: {
        Piece promotionPiece = PROMOTION_PIECE[promotionType(move)];
        return BB::attackedSquares(promotionPiece, target, occ ^ bitboard(origin), stm) & enemyKing;
    }
    case MOVE_CASTLING: {
        return BB::attackedSquares(PIECE_ROOK, CASTLING_ROOK_SQUARES[castlingIndex(stm, origin, target)], occ, stm) & enemyKing;
    }
    case MOVE_ENPASSANT: {
        Square epSquare = target - UP[stm];
        Bitboard occupied = (occ ^ bitboard(origin) ^ bitboard(epSquare)) | bitboard(target);
        Square ek = lsb(enemyKing);
        return (BB::attackedSquares(PIECE_ROOK, ek, occupied, stm) & byColor[stm] & (byPiece[PIECE_ROOK] | byPiece[PIECE_QUEEN]))
            | (BB::attackedSquares(PIECE_BISHOP, ek, occupied, stm) & byColor[stm] & (byPiece[PIECE_BISHOP] | byPiece[PIECE_QUEEN]));
    }
    default:
        return false;
    }
}

uint64_t Board::hashAfter(Move move) {
    uint64_t hash = stack->hash ^ ZOBRIST_STM_BLACK;

    Square origin = moveOrigin(move);
    Square target = moveTarget(move);
    Square captureTarget = target;

    Piece piece = pieces[origin];
    Piece capturedPiece = pieces[target];

    MoveType type = moveType(move);;

    uint8_t newCastling = stack->castling;

    if (stack->enpassantTarget != 0)
        hash ^= ZOBRIST_ENPASSENT[fileOf(lsb(stack->enpassantTarget))];

    if (piece == PIECE_PAWN) {
        if (type == MOVE_ENPASSANT) {
            capturedPiece = PIECE_PAWN;
            captureTarget = target - UP[stm];
        }

        // Double push
        if ((origin ^ target) == 16) {
            Bitboard enemyPawns = byColor[flip(stm)] & byPiece[PIECE_PAWN];
            Bitboard epRank = stm == COLOR_WHITE ? BB::RANK_4 : BB::RANK_5;
            Square pawnSquare1 = target + 1;
            Square pawnSquare2 = target - 1;
            Bitboard epPawns = (bitboard(pawnSquare1) | bitboard(pawnSquare2)) & epRank & enemyPawns;
            if (epPawns)
                hash ^= ZOBRIST_ENPASSENT[fileOf(origin)];
        }
    }

    if (type == MOVE_CASTLING) {
        Square rookOrigin, rookTarget;

        hash ^= ZOBRIST_CASTLING[newCastling];
        calculateCastlingSquares(origin, &target, &rookOrigin, &rookTarget, &newCastling);
        hash ^= ZOBRIST_CASTLING[newCastling];

        hash ^= ZOBRIST_PIECE_SQUARES[stm][PIECE_KING][origin] ^ ZOBRIST_PIECE_SQUARES[stm][PIECE_KING][target];
        hash ^= ZOBRIST_PIECE_SQUARES[stm][PIECE_ROOK][rookOrigin] ^ ZOBRIST_PIECE_SQUARES[stm][PIECE_ROOK][rookTarget];
    }
    else {
        hash ^= ZOBRIST_PIECE_SQUARES[stm][piece][origin] ^ ZOBRIST_PIECE_SQUARES[stm][piece][target];

        if (capturedPiece != NO_PIECE)
            hash ^= ZOBRIST_PIECE_SQUARES[flip(stm)][capturedPiece][captureTarget];
    }

    if (type == MOVE_PROMOTION) {
        Piece promotionPiece = PROMOTION_PIECE[promotionType(move)];
        hash ^= ZOBRIST_PIECE_SQUARES[stm][piece][target] ^ ZOBRIST_PIECE_SQUARES[stm][promotionPiece][target];
    }

    if (piece == PIECE_KING) {
        hash ^= ZOBRIST_CASTLING[newCastling & CASTLING_MASK];
        if (stm == COLOR_WHITE)
            newCastling &= CASTLING_BLACK_KINGSIDE | CASTLING_BLACK_QUEENSIDE;
        else
            newCastling &= CASTLING_WHITE_KINGSIDE | CASTLING_WHITE_QUEENSIDE;
        hash ^= ZOBRIST_CASTLING[newCastling & CASTLING_MASK];
    }
    if (piece == PIECE_ROOK || capturedPiece == PIECE_ROOK) {
        hash ^= ZOBRIST_CASTLING[newCastling & CASTLING_MASK];
        Square rookSquare = piece == PIECE_ROOK ? origin : captureTarget;
        if (rookSquare == castlingSquares[0]) {
            newCastling &= ~CASTLING_WHITE_KINGSIDE;
        }
        else if (rookSquare == castlingSquares[1]) {
            newCastling &= ~CASTLING_WHITE_QUEENSIDE;
        }
        else if (rookSquare == castlingSquares[2]) {
            newCastling &= ~CASTLING_BLACK_KINGSIDE;
        }
        else if (rookSquare == castlingSquares[3]) {
            newCastling &= ~CASTLING_BLACK_QUEENSIDE;
        }
        hash ^= ZOBRIST_CASTLING[newCastling & CASTLING_MASK];
    }

    return hash;
}

void Board::updateSliderPins(Color side) {
    assert((byColor[side] & byPiece[PIECE_KING]) > 0);

    Square king = lsb(byColor[side] & byPiece[PIECE_KING]);

    stack->blockers[side] = 0;

    Bitboard possiblePinnersRook = getRookMoves(king, bitboard(0)) & (byPiece[PIECE_ROOK] | byPiece[PIECE_QUEEN]);
    Bitboard possiblePinnersBishop = getBishopMoves(king, bitboard(0)) & (byPiece[PIECE_BISHOP] | byPiece[PIECE_QUEEN]);
    Bitboard possiblePinners = (possiblePinnersBishop | possiblePinnersRook) & byColor[flip(side)];
    Bitboard occupied = (byColor[side] | byColor[flip(side)]) ^ possiblePinners;

    // Go through all pieces that could potentially pin the king
    while (possiblePinners) {
        Square pinnerSquare = popLSB(&possiblePinners);
        Bitboard blockerBB = BB::BETWEEN[king][pinnerSquare] & occupied;

        if (__builtin_popcountll(blockerBB) == 1) {
            // We have exactly one blocker for this pinner
            stack->blockers[side] |= blockerBB;
        }
    }
}

// Using cuckoo tables, check if the side to move has any move that would lead to a repetition
bool Board::hasUpcomingRepetition(int ply) {

    int maxPlyOffset = std::min(stack->rule50_ply, stack->nullmove_ply);
    if (maxPlyOffset < 3)
        return false;

    uint64_t hash = stack->hash;
    BoardStack* compareStack = stack->previous;

    int j = 0;
    for (int i = 3; i <= maxPlyOffset; i += 2) {
        compareStack = compareStack->previous->previous;

        uint64_t moveHash = hash ^ compareStack->hash;
        if ((j = H1(moveHash), CUCKOO_HASHES[j] == moveHash) || (j = H2(moveHash), CUCKOO_HASHES[j] == moveHash)) {
            Move move = CUCKOO_MOVES[j];
            Square origin = moveOrigin(move);
            Square target = moveTarget(move);

            if (BB::BETWEEN[origin][target] & (byColor[COLOR_WHITE] | byColor[COLOR_BLACK]))
                continue;

            if (ply > i)
                return true;

            Square pieceSquare = pieces[origin] == NO_PIECE ? target : origin;
            Color pieceColor = (byColor[COLOR_WHITE] & bitboard(pieceSquare)) ? COLOR_WHITE : COLOR_BLACK;
            if (pieceColor != stm)
                continue;

            // Check for 2-fold repetition
            BoardStack* compareStack2 = compareStack;
            for (int k = i + 4; k <= maxPlyOffset; k += 2) {
                if (k == i + 4)
                    compareStack2 = compareStack2->previous->previous;
                compareStack2 = compareStack->previous->previous;
                if (compareStack2->hash == stack->hash)
                    return true;
            }
        }
    }

    return false;
}

// Checks for 2-fold repetition and rule50 draw
bool Board::isDraw() {

    // The stack needs to go back far enough
    if (!stack->previous || !stack->previous->previous)
        return false;

    // 2-fold repetition
    int maxPlyOffset = std::min(stack->rule50_ply, stack->nullmove_ply);
    BoardStack* compareStack = stack->previous->previous;

    for (int i = 4; i <= maxPlyOffset; i += 2) {
        compareStack = compareStack->previous->previous;
        if (stack->hash == compareStack->hash) {
            return true;
        }
    }

    // 50 move rule draw
    if (stack->rule50_ply > 99) {
        if (!stack->checkers)
            return true;

        // If in check, it might be checkmate
        Move moves[MAX_MOVES] = { MOVE_NONE };
        int moveCount = 0;
        int legalMoveCount = 0;
        generateMoves(this, moves, &moveCount);
        for (int i = 0; i < moveCount; i++) {
            Move move = moves[i];
            if (!isLegal(move))
                continue;
            legalMoveCount++;
        }

        return legalMoveCount > 0;
    }

    // Otherwise, no draw
    return false;
}

Bitboard Board::attackersTo(Square s, Bitboard occupied) {
    Bitboard sBB = bitboard(s);

    Bitboard pawnAtks = byPiece[PIECE_PAWN] & ((byColor[COLOR_BLACK] & BB::pawnAttacks(sBB, COLOR_WHITE)) | (byColor[COLOR_WHITE] & BB::pawnAttacks(sBB, COLOR_BLACK)));
    Bitboard knightAtks = byPiece[PIECE_KNIGHT] & BB::KNIGHT_ATTACKS[s];
    Bitboard bishopAtks = (byPiece[PIECE_BISHOP] | byPiece[PIECE_QUEEN]) & getBishopMoves(s, occupied);
    Bitboard rookAtks = (byPiece[PIECE_ROOK] | byPiece[PIECE_QUEEN]) & getRookMoves(s, occupied);
    Bitboard kingAtks = byPiece[PIECE_KING] & BB::KING_ATTACKS[s];
    return pawnAtks | knightAtks | bishopAtks | rookAtks | kingAtks;
}

void Board::debugBoard() {
    for (int rank = 7; rank >= 0; rank--) {

        printf("-");
        for (int file = 0; file <= 15; file++) {
            printf(" -");
        }
        printf("\n");

        for (int file = 0; file <= 7; file++) {

            // Get piece at index
            int idx = file + 8 * rank;
            Bitboard mask = bitboard(idx);
            if ((stack->enpassantTarget & mask) != 0)
                printf("| E ");
            else if (((byColor[COLOR_WHITE] | byColor[COLOR_BLACK]) & mask) == 0)
                printf("|   ");
            else if ((byColor[COLOR_WHITE] & byPiece[PIECE_PAWN] & mask) != 0)
                printf("| ♙ ");
            else if ((byColor[COLOR_BLACK] & byPiece[PIECE_PAWN] & mask) != 0)
                printf("| ♟︎ ");
            else if ((byColor[COLOR_WHITE] & byPiece[PIECE_KNIGHT] & mask) != 0)
                printf("| ♘ ");
            else if ((byColor[COLOR_BLACK] & byPiece[PIECE_KNIGHT] & mask) != 0)
                printf("| ♞ ");
            else if ((byColor[COLOR_WHITE] & byPiece[PIECE_BISHOP] & mask) != 0)
                printf("| ♗ ");
            else if ((byColor[COLOR_BLACK] & byPiece[PIECE_BISHOP] & mask) != 0)
                printf("| ♝ ");
            else if ((byColor[COLOR_WHITE] & byPiece[PIECE_ROOK] & mask) != 0)
                printf("| ♖ ");
            else if ((byColor[COLOR_BLACK] & byPiece[PIECE_ROOK] & mask) != 0)
                printf("| ♜ ");
            else if ((byColor[COLOR_WHITE] & byPiece[PIECE_QUEEN] & mask) != 0)
                printf("| ♕ ");
            else if ((byColor[COLOR_BLACK] & byPiece[PIECE_QUEEN] & mask) != 0)
                printf("| ♛ ");
            else if ((byColor[COLOR_WHITE] & byPiece[PIECE_KING] & mask) != 0)
                printf("| ♔ ");
            else if ((byColor[COLOR_BLACK] & byPiece[PIECE_KING] & mask) != 0)
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
    printf("\n%" PRIu64 "\n", stack->hash);
    printf("%" PRIu64 "\n", stack->pawnHash);
}

int Board::validateBoard() {
    for (int rank = 7; rank >= 0; rank--) {
        for (int file = 0; file <= 7; file++) {

            // Get piece at index
            int idx = file + 8 * rank;
            Bitboard mask = bitboard(idx);
            int first = 0;
            int second = 0;
            if ((stack->enpassantTarget & mask) != 0)
                first = 1;
            else if (pieces[idx] == NO_PIECE)
                first = 2;
            else if (pieces[idx] == PIECE_PAWN && (byColor[COLOR_WHITE] & mask) != 0)
                first = 3;
            else if (pieces[idx] == PIECE_PAWN && (byColor[COLOR_BLACK] & mask) != 0)
                first = 4;
            else if (pieces[idx] == PIECE_KNIGHT && (byColor[COLOR_WHITE] & mask) != 0)
                first = 5;
            else if (pieces[idx] == PIECE_KNIGHT && (byColor[COLOR_BLACK] & mask) != 0)
                first = 6;
            else if (pieces[idx] == PIECE_BISHOP && (byColor[COLOR_WHITE] & mask) != 0)
                first = 7;
            else if (pieces[idx] == PIECE_BISHOP && (byColor[COLOR_BLACK] & mask) != 0)
                first = 8;
            else if (pieces[idx] == PIECE_ROOK && (byColor[COLOR_WHITE] & mask) != 0)
                first = 9;
            else if (pieces[idx] == PIECE_ROOK && (byColor[COLOR_BLACK] & mask) != 0)
                first = 10;
            else if (pieces[idx] == PIECE_QUEEN && (byColor[COLOR_WHITE] & mask) != 0)
                first = 11;
            else if (pieces[idx] == PIECE_QUEEN && (byColor[COLOR_BLACK] & mask) != 0)
                first = 12;
            else if (pieces[idx] == PIECE_KING && (byColor[COLOR_WHITE] & mask) != 0)
                first = 13;
            else if (pieces[idx] == PIECE_KING && (byColor[COLOR_BLACK] & mask) != 0)
                first = 14;

            if ((stack->enpassantTarget & mask) != 0)
                second = 1;
            else if (((byColor[COLOR_WHITE] | byColor[COLOR_BLACK]) & mask) == 0)
                second = 2;
            else if ((byColor[COLOR_WHITE] & byPiece[PIECE_PAWN] & mask) != 0)
                second = 3;
            else if ((byColor[COLOR_BLACK] & byPiece[PIECE_PAWN] & mask) != 0)
                second = 4;
            else if ((byColor[COLOR_WHITE] & byPiece[PIECE_KNIGHT] & mask) != 0)
                second = 5;
            else if ((byColor[COLOR_BLACK] & byPiece[PIECE_KNIGHT] & mask) != 0)
                second = 6;
            else if ((byColor[COLOR_WHITE] & byPiece[PIECE_BISHOP] & mask) != 0)
                second = 7;
            else if ((byColor[COLOR_BLACK] & byPiece[PIECE_BISHOP] & mask) != 0)
                second = 8;
            else if ((byColor[COLOR_WHITE] & byPiece[PIECE_ROOK] & mask) != 0)
                second = 9;
            else if ((byColor[COLOR_BLACK] & byPiece[PIECE_ROOK] & mask) != 0)
                second = 10;
            else if ((byColor[COLOR_WHITE] & byPiece[PIECE_QUEEN] & mask) != 0)
                second = 11;
            else if ((byColor[COLOR_BLACK] & byPiece[PIECE_QUEEN] & mask) != 0)
                second = 12;
            else if ((byColor[COLOR_WHITE] & byPiece[PIECE_KING] & mask) != 0)
                second = 13;
            else if ((byColor[COLOR_BLACK] & byPiece[PIECE_KING] & mask) != 0)
                second = 14;

            if (first != second) {
                std::cout << first << " " << second << std::endl;
                return idx;
            }
        }
    }
    return -1;
}

void debugbitboard(Bitboard bb) {
    for (int rank = 7; rank >= 0; rank--) {

        printf("-");
        for (int file = 0; file <= 15; file++) {
            printf(" -");
        }
        printf("\n");

        for (int file = 0; file <= 7; file++) {

            // Get piece at index
            int idx = file + 8 * rank;
            Bitboard mask = bitboard(idx);
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