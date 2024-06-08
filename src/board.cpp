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
        pieces[s] = Piece::NONE;
    }
    for (Color c = Color::WHITE; c <= Color::BLACK; ++c) {
        byColor[c] = bitboard(0);
        for (Piece p = Piece::PAWN; p < Piece::TOTAL; ++p) {
            stack->pieceCount[c][p] = 0;
            byPiece[p] = bitboard(0);
        }
    }
    stack->checkers = bitboard(0);
    stack->capturedPiece = Piece::NONE;
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
            byColor[Color::BLACK] |= currentSquareBB;
            byPiece[Piece::PAWN] |= currentSquareBB;
            stack->pieceCount[Color::BLACK][Piece::PAWN]++;
            pieces[currentSquare] = Piece::PAWN;
            stack->hash ^= ZOBRIST_PIECE_SQUARES[Color::BLACK][Piece::PAWN][currentSquare];
            stack->pawnHash ^= ZOBRIST_PIECE_SQUARES[Color::BLACK][Piece::PAWN][currentSquare];
            currentSquare++;
            break;
        case 'P':
            byColor[Color::WHITE] |= currentSquareBB;
            byPiece[Piece::PAWN] |= currentSquareBB;
            stack->pieceCount[Color::WHITE][Piece::PAWN]++;
            pieces[currentSquare] = Piece::PAWN;
            stack->hash ^= ZOBRIST_PIECE_SQUARES[Color::WHITE][Piece::PAWN][currentSquare];
            stack->pawnHash ^= ZOBRIST_PIECE_SQUARES[Color::WHITE][Piece::PAWN][currentSquare];
            currentSquare++;
            break;
        case 'n':
            byColor[Color::BLACK] |= currentSquareBB;
            byPiece[Piece::KNIGHT] |= currentSquareBB;
            stack->pieceCount[Color::BLACK][Piece::KNIGHT]++;
            pieces[currentSquare] = Piece::KNIGHT;
            stack->hash ^= ZOBRIST_PIECE_SQUARES[Color::BLACK][Piece::KNIGHT][currentSquare];
            currentSquare++;
            break;
        case 'N':
            byColor[Color::WHITE] |= currentSquareBB;
            byPiece[Piece::KNIGHT] |= currentSquareBB;
            stack->pieceCount[Color::WHITE][Piece::KNIGHT]++;
            pieces[currentSquare] = Piece::KNIGHT;
            stack->hash ^= ZOBRIST_PIECE_SQUARES[Color::WHITE][Piece::KNIGHT][currentSquare];
            currentSquare++;
            break;
        case 'b':
            byColor[Color::BLACK] |= currentSquareBB;
            byPiece[Piece::BISHOP] |= currentSquareBB;
            stack->pieceCount[Color::BLACK][Piece::BISHOP]++;
            pieces[currentSquare] = Piece::BISHOP;
            stack->hash ^= ZOBRIST_PIECE_SQUARES[Color::BLACK][Piece::BISHOP][currentSquare];
            currentSquare++;
            break;
        case 'B':
            byColor[Color::WHITE] |= currentSquareBB;
            byPiece[Piece::BISHOP] |= currentSquareBB;
            stack->pieceCount[Color::WHITE][Piece::BISHOP]++;
            pieces[currentSquare] = Piece::BISHOP;
            stack->hash ^= ZOBRIST_PIECE_SQUARES[Color::WHITE][Piece::BISHOP][currentSquare];
            currentSquare++;
            break;
        case 'r':
            byColor[Color::BLACK] |= currentSquareBB;
            byPiece[Piece::ROOK] |= currentSquareBB;
            stack->pieceCount[Color::BLACK][Piece::ROOK]++;
            pieces[currentSquare] = Piece::ROOK;
            stack->hash ^= ZOBRIST_PIECE_SQUARES[Color::BLACK][Piece::ROOK][currentSquare];
            currentSquare++;
            break;
        case 'R':
            byColor[Color::WHITE] |= currentSquareBB;
            byPiece[Piece::ROOK] |= currentSquareBB;
            stack->pieceCount[Color::WHITE][Piece::ROOK]++;
            pieces[currentSquare] = Piece::ROOK;
            stack->hash ^= ZOBRIST_PIECE_SQUARES[Color::WHITE][Piece::ROOK][currentSquare];
            currentSquare++;
            break;
        case 'q':
            byColor[Color::BLACK] |= currentSquareBB;
            byPiece[Piece::QUEEN] |= currentSquareBB;
            stack->pieceCount[Color::BLACK][Piece::QUEEN]++;
            pieces[currentSquare] = Piece::QUEEN;
            stack->hash ^= ZOBRIST_PIECE_SQUARES[Color::BLACK][Piece::QUEEN][currentSquare];
            currentSquare++;
            break;
        case 'Q':
            byColor[Color::WHITE] |= currentSquareBB;
            byPiece[Piece::QUEEN] |= currentSquareBB;
            stack->pieceCount[Color::WHITE][Piece::QUEEN]++;
            pieces[currentSquare] = Piece::QUEEN;
            stack->hash ^= ZOBRIST_PIECE_SQUARES[Color::WHITE][Piece::QUEEN][currentSquare];
            currentSquare++;
            break;
        case 'k':
            byColor[Color::BLACK] |= currentSquareBB;
            byPiece[Piece::KING] |= currentSquareBB;
            stack->pieceCount[Color::BLACK][Piece::KING]++;
            pieces[currentSquare] = Piece::KING;
            stack->hash ^= ZOBRIST_PIECE_SQUARES[Color::BLACK][Piece::KING][currentSquare];
            currentSquare++;
            break;
        case 'K':
            byColor[Color::WHITE] |= currentSquareBB;
            byPiece[Piece::KING] |= currentSquareBB;
            stack->pieceCount[Color::WHITE][Piece::KING]++;
            pieces[currentSquare] = Piece::KING;
            stack->hash ^= ZOBRIST_PIECE_SQUARES[Color::WHITE][Piece::KING][currentSquare];
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
    stm = fen.at(i) == 'w' ? Color::WHITE : Color::BLACK;
    if (stm == Color::BLACK)
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
            rookBB = byColor[Color::BLACK] & byPiece[Piece::ROOK] & BB::RANK_8;
            rooks.clear();
            while (rookBB) {
                rooks.push_back(popLSB(&rookBB));
            }
            castlingSquares[2] = *std::max_element(rooks.begin(), rooks.end());
            break;
        case 'K':
            stack->castling |= CASTLING_WHITE_KINGSIDE;
            // Chess960 support: Find white kingside rook
            rookBB = byColor[Color::WHITE] & byPiece[Piece::ROOK] & BB::RANK_1;
            rooks.clear();
            while (rookBB) {
                rooks.push_back(popLSB(&rookBB));
            }
            castlingSquares[0] = *std::max_element(rooks.begin(), rooks.end());
            break;
        case 'q':
            stack->castling |= CASTLING_BLACK_QUEENSIDE;
            // Chess960 support: Find black queenside rook
            rookBB = byColor[Color::BLACK] & byPiece[Piece::ROOK] & BB::RANK_8;
            rooks.clear();
            while (rookBB) {
                rooks.push_back(popLSB(&rookBB));
            }
            castlingSquares[3] = *std::min_element(rooks.begin(), rooks.end());
            break;
        case 'Q':
            stack->castling |= CASTLING_WHITE_QUEENSIDE;
            // Chess960 support: Find white queenside rook
            rookBB = byColor[Color::WHITE] & byPiece[Piece::ROOK] & BB::RANK_1;
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
                Square king = lsb(byColor[Color::WHITE] & byPiece[Piece::KING]);
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
                Square king = lsb(byColor[Color::BLACK] & byPiece[Piece::KING]);
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
        Bitboard enemyPawns = byColor[flip(stm)] & byPiece[Piece::PAWN];
        Bitboard epRank = stm == Color::WHITE ? BB::RANK_4 : BB::RANK_5;
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
    Square enemyKing = lsb(byColor[stm] & byPiece[Piece::KING]);
    stack->checkers = attackersTo(enemyKing, byColor[Color::WHITE] | byColor[Color::BLACK]) & byColor[flip(stm)];
    stack->checkerCount = BB::popcount(stack->checkers);

    updateSliderPins(Color::WHITE);
    updateSliderPins(Color::BLACK);

    chess960 = isChess960;

    return i;
}

void Board::calculateCastlingSquares(Square kingOrigin, Square* kingTarget, Square* rookOrigin, Square* rookTarget, uint8_t* castling) {
    if (stm == Color::WHITE) {
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
    Piece promotionPiece = Piece::NONE;

    Bitboard originBB = bitboard(origin);
    Bitboard targetBB = bitboard(target);
    Bitboard fromTo = originBB | targetBB;

    switch (type) {
    case MOVE_PROMOTION:
        movePiece(piece, origin, target, fromTo);
        newStack->rule50_ply = 0;

        newStack->pawnHash ^= ZOBRIST_PIECE_SQUARES[stm][Piece::PAWN][origin];

        if (newStack->capturedPiece != Piece::NONE) {
            byColor[flip(stm)] ^= captureTargetBB; // take away the captured piece
            byPiece[newStack->capturedPiece] ^= captureTargetBB;

            nnue->removePiece(captureTarget, newStack->capturedPiece, flip(stm));

            newStack->pieceCount[flip(stm)][newStack->capturedPiece]--;

            if (newStack->capturedPiece == Piece::ROOK) {
                Square rookSquare = piece == Piece::ROOK ? origin : captureTarget;
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

        newStack->pieceCount[stm][Piece::PAWN]--;
        newStack->pieceCount[stm][promotionPiece]++;

        break;

    case MOVE_CASTLING: {
        Square rookOrigin, rookTarget;
        calculateCastlingSquares(origin, &target, &rookOrigin, &rookTarget, &newStack->castling);
        assert(rookOrigin < 64 && rookTarget < 64 && target < 64);

        Bitboard rookFromToBB = bitboard(rookOrigin) ^ bitboard(rookTarget);
        targetBB = bitboard(target);
        fromTo = originBB ^ targetBB;

        pieces[rookOrigin] = Piece::NONE;
        pieces[origin] = Piece::NONE;
        pieces[rookTarget] = Piece::ROOK;
        pieces[target] = Piece::KING;
        byColor[stm] ^= rookFromToBB ^ fromTo;
        byPiece[Piece::ROOK] ^= rookFromToBB;
        byPiece[Piece::KING] ^= fromTo;

        nnue->movePiece(origin, target, Piece::KING, stm);
        nnue->movePiece(rookOrigin, rookTarget, Piece::ROOK, stm);

        newStack->capturedPiece = Piece::NONE;
    }
                      break;

    case MOVE_ENPASSANT:
        movePiece(piece, origin, target, fromTo);
        nnue->movePiece(origin, target, piece, stm);
        newStack->rule50_ply = 0;

        captureTarget = target - UP[stm];
        newStack->capturedPiece = Piece::PAWN;

        assert(captureTarget < 64);

        captureTargetBB = bitboard(captureTarget);
        pieces[captureTarget] = Piece::NONE; // remove the captured pawn

        newStack->pawnHash ^= ZOBRIST_PIECE_SQUARES[stm][Piece::PAWN][origin] ^ ZOBRIST_PIECE_SQUARES[stm][Piece::PAWN][target] ^ ZOBRIST_PIECE_SQUARES[flip(stm)][Piece::PAWN][captureTarget];

        byColor[flip(stm)] ^= captureTargetBB; // take away the captured piece
        byPiece[Piece::PAWN] ^= captureTargetBB;

        nnue->removePiece(captureTarget, Piece::PAWN, flip(stm));

        newStack->pieceCount[flip(stm)][Piece::PAWN]--;

        break;

    default: // Normal moves
        movePiece(piece, origin, target, fromTo);
        nnue->movePiece(origin, target, piece, stm);

        if (piece == Piece::PAWN) {
            // Double push
            if ((origin ^ target) == 16) {
                assert(target - UP[stm] < 64);

                Bitboard enemyPawns = byColor[flip(stm)] & byPiece[Piece::PAWN];
                Bitboard epRank = stm == Color::WHITE ? BB::RANK_4 : BB::RANK_5;
                Square pawnSquare1 = target + 1;
                Square pawnSquare2 = target - 1;
                Bitboard epPawns = (bitboard(pawnSquare1) | bitboard(pawnSquare2)) & epRank & enemyPawns;
                if (epPawns) {
                    Square epTarget = target - UP[stm];
                    newStack->enpassantTarget = bitboard(epTarget);
                }

            }

            newStack->rule50_ply = 0;
            newStack->pawnHash ^= ZOBRIST_PIECE_SQUARES[stm][Piece::PAWN][origin] ^ ZOBRIST_PIECE_SQUARES[stm][Piece::PAWN][target];
        }

        if (newStack->capturedPiece != Piece::NONE) {
            byColor[flip(stm)] ^= captureTargetBB; // take away the captured piece
            byPiece[newStack->capturedPiece] ^= captureTargetBB;

            if (newStack->capturedPiece == Piece::PAWN)
                newStack->pawnHash ^= ZOBRIST_PIECE_SQUARES[flip(stm)][Piece::PAWN][captureTarget];

            nnue->removePiece(captureTarget, newStack->capturedPiece, flip(stm));

            newStack->pieceCount[flip(stm)][newStack->capturedPiece]--;
            newStack->rule50_ply = 0;
        }

        // Unset castling flags if necessary
        if (piece == Piece::KING) {
            if (stm == Color::WHITE) {
                newStack->castling &= CASTLING_BLACK_KINGSIDE | CASTLING_BLACK_QUEENSIDE;
            }
            else {
                newStack->castling &= CASTLING_WHITE_KINGSIDE | CASTLING_WHITE_QUEENSIDE;
            }
        }
        if (piece == Piece::ROOK || newStack->capturedPiece == Piece::ROOK) {
            Square rookSquare = piece == Piece::ROOK ? origin : captureTarget;
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
    assert((byColor[flip(stm)] & byPiece[Piece::KING]) > 0);
    assert((byColor[stm] & byPiece[Piece::KING]) > 0);

    Square enemyKing = lsb(byColor[flip(stm)] & byPiece[Piece::KING]);
    newStack->checkers = attackersTo(enemyKing, byColor[Color::WHITE] | byColor[Color::BLACK]) & byColor[stm];
    newStack->checkerCount = newStack->checkers ? BB::popcount(newStack->checkers) : 0;
    updateSliderPins(Color::WHITE);
    updateSliderPins(Color::BLACK);

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

        pieces[rookTarget] = Piece::NONE;
        pieces[target] = Piece::NONE;
        pieces[rookOrigin] = Piece::ROOK;
        pieces[origin] = Piece::KING;
        byColor[stm] ^= rookFromToBB ^ fromTo;
        byPiece[Piece::ROOK] ^= rookFromToBB;
        byPiece[Piece::KING] ^= fromTo;
    }
    else {
        byColor[stm] ^= fromTo;
        byPiece[piece] ^= fromTo;

        pieces[target] = Piece::NONE;
        pieces[origin] = piece;
    }

    // This move is en passent
    if (type == MOVE_ENPASSANT) {
        captureTarget = target - UP[stm];

        assert(captureTarget < 64);

        captureTargetBB = bitboard(captureTarget);
    }

    // Handle capture
    if (stack->capturedPiece != Piece::NONE && type != MOVE_CASTLING) {
        pieces[captureTarget] = stack->capturedPiece;
        byColor[flip(stm)] ^= captureTargetBB;
        byPiece[stack->capturedPiece] ^= captureTargetBB;
    }

    // This move is promotion
    if (type == MOVE_PROMOTION) {
        byPiece[piece] ^= originBB;
        byPiece[Piece::PAWN] ^= originBB;

        pieces[origin] = Piece::PAWN;
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
    assert((byColor[flip(stm)] & byPiece[Piece::KING]) > 0);

    newStack->checkers = bitboard(0);
    newStack->checkerCount = 0;
    updateSliderPins(Color::WHITE);
    updateSliderPins(Color::BLACK);

    stm = flip(stm);
    newStack->move = MOVE_NULL;
}

void Board::undoNullMove() {
    stm = flip(stm);
    stack = stack->previous;
}

bool Board::isPseudoLegal(Move move) {
    Square origin = moveOrigin(move);
    Square target = moveTarget(move);
    Bitboard originBB = bitboard(origin);
    Bitboard targetBB = bitboard(target);
    Piece piece = pieces[origin];
    MoveType type = moveType(move);

    // A valid piece needs to be on the origin square
    if (pieces[origin] == Piece::NONE) return false;
    // We can't capture our own piece, except in castling
    if (type != MOVE_CASTLING && (byColor[stm] & targetBB)) return false;
    // We can't move the enemies piece
    if (byColor[flip(stm)] & originBB) return false;

    // Non-standard movetypes
    switch (type) {
    case MOVE_CASTLING: {
        if (piece != Piece::KING || stack->checkers) return false;

        // Check for pieces between king and rook
        int castlingIdx = castlingIndex(stm, origin, target);
        Square kingSquare = lsb(byColor[stm] & byPiece[Piece::KING]);
        Square rookSquare = castlingSquares[castlingIdx];
        Square kingTarget = CASTLING_KING_SQUARES[castlingIdx];
        Square rookTarget = CASTLING_ROOK_SQUARES[castlingIdx];

        Bitboard importantSquares = BB::BETWEEN[rookSquare][rookTarget] | BB::BETWEEN[kingSquare][kingTarget];
        importantSquares &= ~bitboard(rookSquare) & ~bitboard(kingSquare);
        if ((byColor[Color::WHITE] | byColor[Color::BLACK]) & importantSquares) return false;
        // Check for castling flags (Attackers are done in isLegal())
        return stack->castling & CASTLING_FLAGS[castlingIdx];
    }
    case MOVE_ENPASSANT:
        if (piece != Piece::PAWN) return false;
        if (stack->checkerCount > 1) return false;
        if (stack->checkers && !(lsb(stack->checkers) == target - UP[stm])) return false;
        // Check for EP flag on the right file
        return (bitboard(target) & stack->enpassantTarget) && pieces[target - UP[stm]] == Piece::PAWN;
    case MOVE_PROMOTION:
        if (piece != Piece::PAWN) return false;
        if (stack->checkerCount > 1) return false;
        if (stack->checkers) {
            bool capturesChecker = target == lsb(stack->checkers);
            bool blocksCheck = BB::BETWEEN[lsb(byColor[stm] & byPiece[Piece::KING])][lsb(stack->checkers)] & bitboard(target);
            if (!capturesChecker && !blocksCheck)
                return false;
        }
        if (
            !(BB::pawnAttacks(originBB, stm) & targetBB & byColor[flip(stm)]) && // Capture promotion?
            !(origin + UP[stm] == target && pieces[target] == Piece::NONE)) // Push promotion?
            return false;
        return true;
    default:
        break;
    }

    Bitboard occupied = byColor[Color::WHITE] | byColor[Color::BLACK];
    switch (piece) {
    case Piece::PAWN:
        if (targetBB & (BB::RANK_8 | BB::RANK_1)) return false;

        if (
            !(BB::pawnAttacks(originBB, stm) & targetBB & byColor[flip(stm)]) && // Capture?
            !(origin + UP[stm] == target && pieces[target] == Piece::NONE) && // Single push?
            !(origin + 2 * UP[stm] == target && pieces[target] == Piece::NONE && pieces[target - UP[stm]] == Piece::NONE && (originBB & (BB::RANK_2 | BB::RANK_7))) // Double push?
            )
            return false;
        break;
    case Piece::KNIGHT:
        if (!(BB::KNIGHT_ATTACKS[origin] & targetBB)) return false;
        break;
    case Piece::BISHOP:
        if (!(getBishopMoves(origin, occupied) & targetBB)) return false;
        break;
    case Piece::ROOK:
        if (!(getRookMoves(origin, occupied) & targetBB)) return false;
        break;
    case Piece::QUEEN:
        if (!((getBishopMoves(origin, occupied) | getRookMoves(origin, occupied)) & targetBB)) return false;
        break;
    case Piece::KING:
        if (!(BB::KING_ATTACKS[origin] & targetBB)) return false;
        break;
    default:
        return false;
    }

    if (stack->checkers) {
        if (piece != Piece::KING) {
            // Double check requires king moves
            if (stack->checkerCount > 1)
                return false;

            bool capturesChecker = target == lsb(stack->checkers);
            bool blocksCheck = BB::BETWEEN[lsb(byColor[stm] & byPiece[Piece::KING])][lsb(stack->checkers)] & bitboard(target);
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

    Square king = lsb(byPiece[Piece::KING] & byColor[stm]);

    MoveType type = moveType(move);
    if (type == MOVE_ENPASSANT) {
        // Check if king would be in check after this move
        Square epSquare = target - UP[stm];

        Bitboard epSquareBB = bitboard(epSquare);
        Bitboard targetBB = bitboard(target);
        Bitboard occupied = ((byColor[Color::WHITE] | byColor[Color::BLACK]) ^ originBB ^ epSquareBB) | targetBB;

        // Check if any rooks/queens/bishops attack the king square, given the occupied pieces after this EP move
        Bitboard rookAttacks = getRookMoves(king, occupied) & (byPiece[Piece::ROOK] | byPiece[Piece::QUEEN]) & byColor[flip(stm)];
        Bitboard bishopAttacks = getBishopMoves(king, occupied) & (byPiece[Piece::BISHOP] | byPiece[Piece::QUEEN]) & byColor[flip(stm)];
        return !rookAttacks && !bishopAttacks;
    }

    Bitboard occupied = byColor[Color::WHITE] | byColor[Color::BLACK];

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

    if (pieces[origin] == Piece::KING) {
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
    Bitboard occ = byColor[Color::WHITE] | byColor[Color::BLACK];
    Bitboard enemyKing = byColor[flip(stm)] & byPiece[Piece::KING];

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
        return BB::attackedSquares(Piece::ROOK, CASTLING_ROOK_SQUARES[castlingIndex(stm, origin, target)], occ, stm) & enemyKing;
    }
    case MOVE_ENPASSANT: {
        Square epSquare = target - UP[stm];
        Bitboard occupied = (occ ^ bitboard(origin) ^ bitboard(epSquare)) | bitboard(target);
        Square ek = lsb(enemyKing);
        return (BB::attackedSquares(Piece::ROOK, ek, occupied, stm) & byColor[stm] & (byPiece[Piece::ROOK] | byPiece[Piece::QUEEN]))
            | (BB::attackedSquares(Piece::BISHOP, ek, occupied, stm) & byColor[stm] & (byPiece[Piece::BISHOP] | byPiece[Piece::QUEEN]));
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

    if (piece == Piece::PAWN) {
        if (type == MOVE_ENPASSANT) {
            capturedPiece = Piece::PAWN;
            captureTarget = target - UP[stm];
        }

        // Double push
        if ((origin ^ target) == 16) {
            Bitboard enemyPawns = byColor[flip(stm)] & byPiece[Piece::PAWN];
            Bitboard epRank = stm == Color::WHITE ? BB::RANK_4 : BB::RANK_5;
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

        hash ^= ZOBRIST_PIECE_SQUARES[stm][Piece::KING][origin] ^ ZOBRIST_PIECE_SQUARES[stm][Piece::KING][target];
        hash ^= ZOBRIST_PIECE_SQUARES[stm][Piece::ROOK][rookOrigin] ^ ZOBRIST_PIECE_SQUARES[stm][Piece::ROOK][rookTarget];
    }
    else {
        hash ^= ZOBRIST_PIECE_SQUARES[stm][piece][origin] ^ ZOBRIST_PIECE_SQUARES[stm][piece][target];

        if (capturedPiece != Piece::NONE)
            hash ^= ZOBRIST_PIECE_SQUARES[flip(stm)][capturedPiece][captureTarget];
    }

    if (type == MOVE_PROMOTION) {
        Piece promotionPiece = PROMOTION_PIECE[promotionType(move)];
        hash ^= ZOBRIST_PIECE_SQUARES[stm][piece][target] ^ ZOBRIST_PIECE_SQUARES[stm][promotionPiece][target];
    }

    if (piece == Piece::KING) {
        hash ^= ZOBRIST_CASTLING[newCastling & CASTLING_MASK];
        if (stm == Color::WHITE)
            newCastling &= CASTLING_BLACK_KINGSIDE | CASTLING_BLACK_QUEENSIDE;
        else
            newCastling &= CASTLING_WHITE_KINGSIDE | CASTLING_WHITE_QUEENSIDE;
        hash ^= ZOBRIST_CASTLING[newCastling & CASTLING_MASK];
    }
    if (piece == Piece::ROOK || capturedPiece == Piece::ROOK) {
        hash ^= ZOBRIST_CASTLING[newCastling & CASTLING_MASK];
        Square rookSquare = piece == Piece::ROOK ? origin : captureTarget;
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
    assert((byColor[side] & byPiece[Piece::KING]) > 0);

    Square king = lsb(byColor[side] & byPiece[Piece::KING]);

    stack->blockers[side] = 0;

    Bitboard possiblePinnersRook = getRookMoves(king, bitboard(0)) & (byPiece[Piece::ROOK] | byPiece[Piece::QUEEN]);
    Bitboard possiblePinnersBishop = getBishopMoves(king, bitboard(0)) & (byPiece[Piece::BISHOP] | byPiece[Piece::QUEEN]);
    Bitboard possiblePinners = (possiblePinnersBishop | possiblePinnersRook) & byColor[flip(side)];
    Bitboard occupied = (byColor[side] | byColor[flip(side)]) ^ possiblePinners;

    // Go through all pieces that could potentially pin the king
    while (possiblePinners) {
        Square pinnerSquare = popLSB(&possiblePinners);
        Bitboard blockerBB = BB::BETWEEN[king][pinnerSquare] & occupied;

        if (BB::popcount(blockerBB) == 1) {
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

            if (BB::BETWEEN[origin][target] & (byColor[Color::WHITE] | byColor[Color::BLACK]))
                continue;

            if (ply > i)
                return true;

            Square pieceSquare = pieces[origin] == Piece::NONE ? target : origin;
            Color pieceColor = (byColor[Color::WHITE] & bitboard(pieceSquare)) ? Color::WHITE : Color::BLACK;
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

    Bitboard pawnAtks = byPiece[Piece::PAWN] & ((byColor[Color::BLACK] & BB::pawnAttacks(sBB, Color::WHITE)) | (byColor[Color::WHITE] & BB::pawnAttacks(sBB, Color::BLACK)));
    Bitboard knightAtks = byPiece[Piece::KNIGHT] & BB::KNIGHT_ATTACKS[s];
    Bitboard bishopAtks = (byPiece[Piece::BISHOP] | byPiece[Piece::QUEEN]) & getBishopMoves(s, occupied);
    Bitboard rookAtks = (byPiece[Piece::ROOK] | byPiece[Piece::QUEEN]) & getRookMoves(s, occupied);
    Bitboard kingAtks = byPiece[Piece::KING] & BB::KING_ATTACKS[s];
    return pawnAtks | knightAtks | bishopAtks | rookAtks | kingAtks;
}

void Board::debugBoard() {
    for (int rank = 7; rank >= 0; rank--) {

        std::cout << "-";
        for (int file = 0; file <= 15; file++) {
            std::cout << " -";
        }
        std::cout << std::endl;

        for (int file = 0; file <= 7; file++) {

            // Get piece at index
            Square idx = file + 8 * rank;
            Bitboard mask = bitboard(idx);
            if ((stack->enpassantTarget & mask) != 0)
                std::cout << "| E ";
            else if (((byColor[Color::WHITE] | byColor[Color::BLACK]) & mask) == 0)
                std::cout << "|   ";
            else if ((byColor[Color::WHITE] & byPiece[Piece::PAWN] & mask) != 0)
                std::cout << "| ♙ ";
            else if ((byColor[Color::BLACK] & byPiece[Piece::PAWN] & mask) != 0)
                std::cout << "| ♟︎ ";
            else if ((byColor[Color::WHITE] & byPiece[Piece::KNIGHT] & mask) != 0)
                std::cout << "| ♘ ";
            else if ((byColor[Color::BLACK] & byPiece[Piece::KNIGHT] & mask) != 0)
                std::cout << "| ♞ ";
            else if ((byColor[Color::WHITE] & byPiece[Piece::BISHOP] & mask) != 0)
                std::cout << "| ♗ ";
            else if ((byColor[Color::BLACK] & byPiece[Piece::BISHOP] & mask) != 0)
                std::cout << "| ♝ ";
            else if ((byColor[Color::WHITE] & byPiece[Piece::ROOK] & mask) != 0)
                std::cout << "| ♖ ";
            else if ((byColor[Color::BLACK] & byPiece[Piece::ROOK] & mask) != 0)
                std::cout << "| ♜ ";
            else if ((byColor[Color::WHITE] & byPiece[Piece::QUEEN] & mask) != 0)
                std::cout << "| ♕ ";
            else if ((byColor[Color::BLACK] & byPiece[Piece::QUEEN] & mask) != 0)
                std::cout << "| ♛ ";
            else if ((byColor[Color::WHITE] & byPiece[Piece::KING] & mask) != 0)
                std::cout << "| ♔ ";
            else if ((byColor[Color::BLACK] & byPiece[Piece::KING] & mask) != 0)
                std::cout << "| ♚ ";
            else
                std::cout << "| ? ";
        }
        std::cout << "|" << std::endl;
    }
    std::cout << "-";
    for (int file = 0; file <= 15; file++) {
        std::cout << " -";
    }
    std::cout << std::endl << stack->hash;
    std::cout << std::endl << stack->pawnHash << std::endl;
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
            else if (pieces[idx] == Piece::NONE)
                first = 2;
            else if (pieces[idx] == Piece::PAWN && (byColor[Color::WHITE] & mask) != 0)
                first = 3;
            else if (pieces[idx] == Piece::PAWN && (byColor[Color::BLACK] & mask) != 0)
                first = 4;
            else if (pieces[idx] == Piece::KNIGHT && (byColor[Color::WHITE] & mask) != 0)
                first = 5;
            else if (pieces[idx] == Piece::KNIGHT && (byColor[Color::BLACK] & mask) != 0)
                first = 6;
            else if (pieces[idx] == Piece::BISHOP && (byColor[Color::WHITE] & mask) != 0)
                first = 7;
            else if (pieces[idx] == Piece::BISHOP && (byColor[Color::BLACK] & mask) != 0)
                first = 8;
            else if (pieces[idx] == Piece::ROOK && (byColor[Color::WHITE] & mask) != 0)
                first = 9;
            else if (pieces[idx] == Piece::ROOK && (byColor[Color::BLACK] & mask) != 0)
                first = 10;
            else if (pieces[idx] == Piece::QUEEN && (byColor[Color::WHITE] & mask) != 0)
                first = 11;
            else if (pieces[idx] == Piece::QUEEN && (byColor[Color::BLACK] & mask) != 0)
                first = 12;
            else if (pieces[idx] == Piece::KING && (byColor[Color::WHITE] & mask) != 0)
                first = 13;
            else if (pieces[idx] == Piece::KING && (byColor[Color::BLACK] & mask) != 0)
                first = 14;

            if ((stack->enpassantTarget & mask) != 0)
                second = 1;
            else if (((byColor[Color::WHITE] | byColor[Color::BLACK]) & mask) == 0)
                second = 2;
            else if ((byColor[Color::WHITE] & byPiece[Piece::PAWN] & mask) != 0)
                second = 3;
            else if ((byColor[Color::BLACK] & byPiece[Piece::PAWN] & mask) != 0)
                second = 4;
            else if ((byColor[Color::WHITE] & byPiece[Piece::KNIGHT] & mask) != 0)
                second = 5;
            else if ((byColor[Color::BLACK] & byPiece[Piece::KNIGHT] & mask) != 0)
                second = 6;
            else if ((byColor[Color::WHITE] & byPiece[Piece::BISHOP] & mask) != 0)
                second = 7;
            else if ((byColor[Color::BLACK] & byPiece[Piece::BISHOP] & mask) != 0)
                second = 8;
            else if ((byColor[Color::WHITE] & byPiece[Piece::ROOK] & mask) != 0)
                second = 9;
            else if ((byColor[Color::BLACK] & byPiece[Piece::ROOK] & mask) != 0)
                second = 10;
            else if ((byColor[Color::WHITE] & byPiece[Piece::QUEEN] & mask) != 0)
                second = 11;
            else if ((byColor[Color::BLACK] & byPiece[Piece::QUEEN] & mask) != 0)
                second = 12;
            else if ((byColor[Color::WHITE] & byPiece[Piece::KING] & mask) != 0)
                second = 13;
            else if ((byColor[Color::BLACK] & byPiece[Piece::KING] & mask) != 0)
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

        std::cout << "-";
        for (int file = 0; file <= 15; file++) {
            std::cout << " -";
        }
        std::cout << std::endl;

        for (int file = 0; file <= 7; file++) {

            // Get piece at index
            int idx = file + 8 * rank;
            Bitboard mask = bitboard(idx);
            if ((bb & mask) == 0)
                std::cout << "|   ";
            else
                std::cout << "| X ";
        }
        std::cout << "|" << std::endl;
    }
    std::cout << "-";
    for (int file = 0; file <= 15; file++) {
        std::cout << " -";
    }
    std::cout << std::endl;
}