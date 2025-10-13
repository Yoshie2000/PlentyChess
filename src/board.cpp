#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <cassert>
#include <algorithm>

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

    // Clear everything
    for (Square s = 0; s < 64; s++) {
        pieces[s] = Piece::NONE;
    }
    for (Color c = Color::WHITE; c <= Color::BLACK; ++c) {
        byColor[c] = bitboard(0);
        for (Piece p = Piece::PAWN; p < Piece::TOTAL; ++p) {
            byPiece[p] = bitboard(0);
        }
    }
    checkers = bitboard(0);
    hashes.hash = 0;
    hashes.pawnHash = ZOBRIST_NO_PAWNS;
    hashes.nonPawnHash[Color::WHITE] = 0;
    hashes.nonPawnHash[Color::BLACK] = 0;
    hashes.minorHash = 0;
    hashes.majorHash = 0;
    nullmove_ply = 0;

    // Set up pieces
    for (i = 0; i < fen.length(); i++) {
        char c = fen.at(i);
        if (c == ' ') {
            i++;
            break;
        }

        auto addPiece = [this](Piece piece, Color color, Square& currentSquare) {
            Bitboard currentSquareBB = bitboard(currentSquare);
            byColor[color] |= currentSquareBB;
            byPiece[piece] |= currentSquareBB;
            pieces[currentSquare] = piece;

            uint64_t hash = ZOBRIST_PIECE_SQUARES[color][piece][currentSquare];
            hashes.hash ^= hash;
            if (piece == Piece::PAWN)
                hashes.pawnHash ^= hash;
            else {
                hashes.nonPawnHash[color] ^= hash;
                if (piece == Piece::KNIGHT || piece == Piece::BISHOP || piece == Piece::KING)
                    hashes.minorHash ^= hash;
                if (piece == Piece::ROOK || piece == Piece::QUEEN || piece == Piece::KING)
                    hashes.majorHash ^= hash;
            }

            currentSquare++;
        };

        auto pieceChars = { 'P', 'p', 'N', 'n', 'B', 'b', 'R', 'r', 'Q', 'q', 'K', 'k' };

        auto it = std::find(pieceChars.begin(), pieceChars.end(), c);
        if (it != pieceChars.end()) {
            int pieceCharIdx = it - pieceChars.begin();
            Piece piece = static_cast<Piece>(pieceCharIdx / 2);
            Color color = static_cast<Color>(pieceCharIdx % 2);
            addPiece(piece, color, currentSquare);
        }
        else if (c == '/') {
            currentRank--;
            currentSquare = 8 * currentRank;
        }
        else {
            currentSquare += int(c) - 48;

        }
    }

    // Side to move
    stm = fen.at(i) == 'w' ? Color::WHITE : Color::BLACK;
    if (stm == Color::BLACK)
        hashes.hash ^= ZOBRIST_STM_BLACK;
    i += 2;

    // Castling
    castling = 0;
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
            castling |= CASTLING_BLACK_KINGSIDE;
            // Chess960 support: Find black kingside rook
            rookBB = byColor[Color::BLACK] & byPiece[Piece::ROOK] & BB::RANK_8;
            rooks.clear();
            while (rookBB) {
                rooks.push_back(popLSB(&rookBB));
            }
            castlingSquares[2] = *std::max_element(rooks.begin(), rooks.end());
            break;
        case 'K':
            castling |= CASTLING_WHITE_KINGSIDE;
            // Chess960 support: Find white kingside rook
            rookBB = byColor[Color::WHITE] & byPiece[Piece::ROOK] & BB::RANK_1;
            rooks.clear();
            while (rookBB) {
                rooks.push_back(popLSB(&rookBB));
            }
            castlingSquares[0] = *std::max_element(rooks.begin(), rooks.end());
            break;
        case 'q':
            castling |= CASTLING_BLACK_QUEENSIDE;
            // Chess960 support: Find black queenside rook
            rookBB = byColor[Color::BLACK] & byPiece[Piece::ROOK] & BB::RANK_8;
            rooks.clear();
            while (rookBB) {
                rooks.push_back(popLSB(&rookBB));
            }
            castlingSquares[3] = *std::min_element(rooks.begin(), rooks.end());
            break;
        case 'Q':
            castling |= CASTLING_WHITE_QUEENSIDE;
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
                    castling |= CASTLING_WHITE_KINGSIDE;
                    castlingSquares[0] = Square(rookFile);
                }
                else {
                    castling |= CASTLING_WHITE_QUEENSIDE;
                    castlingSquares[1] = Square(rookFile);
                }
                break;
            }
            else if (c >= 'a' && c <= 'h') {
                // Black
                Square king = lsb(byColor[Color::BLACK] & byPiece[Piece::KING]);
                int rookFile = int(c - 'a');
                if (rookFile > fileOf(king)) {
                    castling |= CASTLING_BLACK_KINGSIDE;
                    castlingSquares[2] = Square(56 + rookFile);
                }
                else {
                    castling |= CASTLING_BLACK_QUEENSIDE;
                    castlingSquares[3] = Square(56 + rookFile);
                }
                break;
            }
            std::cout << "Weird char in fen castling: " << c << " (index " << i << ")" << std::endl;
            exit(-1);
            break;
        }
    }
    hashes.hash ^= ZOBRIST_CASTLING[castling & CASTLING_MASK];

    // en passent
    if (fen.at(i) == '-') {
        enpassantTarget = bitboard(0);
        i += 2;
    }
    else {
        char epTargetString[2] = { fen.at(i), fen.at(i + 1) };
        Square epTargetSquare = stringToSquare(epTargetString);
        enpassantTarget = bitboard(epTargetSquare);
        i += 3;

        // Check if there's *actually* a pawn that can do enpassent
        Bitboard enemyPawns = byColor[flip(stm)] & byPiece[Piece::PAWN];
        Bitboard epRank = stm == Color::WHITE ? BB::RANK_4 : BB::RANK_5;
        Square pawnSquare1 = epTargetSquare + 1 + UP[stm];
        Square pawnSquare2 = epTargetSquare - 1 + UP[stm];
        Bitboard epPawns = (bitboard(pawnSquare1) | bitboard(pawnSquare2)) & epRank & enemyPawns;
        if (epPawns)
            hashes.hash ^= ZOBRIST_ENPASSENT[fileOf(epTargetSquare)];
    }

    if (fen.length() <= i) {
        rule50_ply = 0;
        ply = 1;
    }
    else {
        // 50 move rule
        std::string rule50String = "--";
        int rule50tmp = 0;
        while (fen.at(i) != ' ') {
            rule50String.replace(rule50tmp++, 1, 1, fen.at(i++));
        }
        if (rule50String.at(1) == '-') {
            rule50_ply = int(rule50String.at(0)) - 48;
        }
        else {
            rule50_ply = 10 * (int(rule50String.at(0)) - 48) + (int(rule50String.at(1)) - 48);
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

    threats = calculateAllThreats();

    // Update king checking stuff
    Square enemyKing = lsb(byColor[stm] & byPiece[Piece::KING]);
    checkers = attackersTo(enemyKing) & byColor[flip(stm)];
    checkerCount = BB::popcount(checkers);

    updateSliderPins(Color::WHITE);
    updateSliderPins(Color::BLACK);

    chess960 = isChess960;

    return i;
}

std::string Board::fen() {
    // rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1
    // r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 10
    std::string result;

    std::string pieceChars = "pnbrqk ";

    for (int rank = 7; rank >= 0; rank--) {
        int freeSquares = 0;
        for (int file = 0; file < 8; file++) {
            Square s = Square(8 * rank + file);
            Piece p = pieces[s];
            if (p == Piece::NONE)
                freeSquares++;
            else {
                if (freeSquares != 0) {
                    result += std::to_string(freeSquares);
                    freeSquares = 0;
                }
                result += (byColor[Color::WHITE] & bitboard(s)) ? toupper(pieceChars[p]) : pieceChars[p];
            }
        }
        if (freeSquares != 0)
            result += std::to_string(freeSquares);
        if (rank != 0)
            result += "/";
    }

    if (stm == Color::WHITE)
        result += " w ";
    else
        result += " b ";

    if (castling & CASTLING_WHITE_KINGSIDE) result += "K";
    if (castling & CASTLING_WHITE_QUEENSIDE) result += "Q";
    if (castling & CASTLING_BLACK_KINGSIDE) result += "k";
    if (castling & CASTLING_BLACK_QUEENSIDE) result += "q";
    if (castling)
        result += " ";
    else
        result += "- ";

    if (enpassantTarget) {
        Square epSquare = lsb(enpassantTarget);
        result += squareToString(epSquare) + " ";
    }
    else
        result += "- ";

    result += std::to_string(rule50_ply) + " " + std::to_string(ply);

    return result;
}

template<bool add>
void Board::updatePieceThreats(Square square, Bitboard attacked, NNUE* nnue) {
    // Add this piece as an attacker to its attacked squares, regardless of that squares occupancy
    // OR: Remove it as an attacker
    Bitboard squareBB = bitboard(square);
    Bitboard occupancy = byColor[Color::WHITE] | byColor[Color::BLACK];
    while (attacked) {
        Square attackedSquare = popLSB(&attacked);

        if (add) {
            assert((threats.toSquare[attackedSquare] & squareBB) == 0);
            threats.toSquare[attackedSquare] |= squareBB;
        }
        else {
            assert((threats.toSquare[attackedSquare] & squareBB) != 0);
            threats.toSquare[attackedSquare] &= ~squareBB;
        }
    }

    // Remove attacks of sliding pieces that are now blocked by this piece
    // OR: Add attacks that are no longer blocked
    Bitboard slidingPieces = byPiece[Piece::BISHOP] | byPiece[Piece::ROOK] | byPiece[Piece::QUEEN];
    slidingPieces &= ~squareBB;
    while (slidingPieces) {
        Square slidingPieceSquare = popLSB(&slidingPieces);
        Bitboard slidingPieceBB = bitboard(slidingPieceSquare);
        Piece slidingPiece = pieces[slidingPieceSquare];
        Color slidingPieceColor = (byColor[Color::WHITE] & slidingPieceBB) ? Color::WHITE : Color::BLACK;

        // If the slider isn't currently attacking this piece, it can't attack squares behind it either
        if ((threats.toSquare[square] & slidingPieceBB) == 0)
            continue;

        int8_t direction = BB::DIRECTION_BETWEEN[slidingPieceSquare][square];
        if (direction == DIRECTION_NONE)
            continue;

        int8_t directionDelta = DIRECTION_DELTAS[direction];
        Square lastSquare = LASTSQ_TABLE[square][direction];
        if (square == lastSquare)
            continue;

        // Move from the newly placed piece along the direction, removing attacks until the next blocker or the end of the board
        for (Square attackedSquare = square + directionDelta; attackedSquare < 64; attackedSquare += directionDelta) {

            Piece attackedPiece = pieces[attackedSquare];
            Color attackedColor = (bitboard(attackedSquare) & byColor[Color::WHITE]) ? Color::WHITE : Color::BLACK;

            if (add) {
                assert((threats.toSquare[attackedSquare] & slidingPieceBB) != 0);
                threats.toSquare[attackedSquare] &= ~slidingPieceBB;

                if (attackedPiece != Piece::NONE)
                    nnue->removeThreat(slidingPiece, attackedPiece, slidingPieceSquare, attackedSquare, slidingPieceColor, attackedColor);
            }
            else {
                assert((threats.toSquare[attackedSquare] & slidingPieceBB) == 0);
                threats.toSquare[attackedSquare] |= slidingPieceBB;

                if (attackedPiece != Piece::NONE)
                    nnue->addThreat(slidingPiece, attackedPiece, slidingPieceSquare, attackedSquare, slidingPieceColor, attackedColor);
            }

            if ((occupancy & bitboard(attackedSquare)) || (attackedSquare == lastSquare))
                break;
        }
    }
}

template<bool add>
void Board::updateThreatFeaturesFromPiece(Piece piece, Color pieceColor, Square square, Bitboard attacked, NNUE* nnue) {
    // Add threats of the piece on this square
    // OR: Remove them
    Bitboard occupancy = byColor[Color::WHITE] | byColor[Color::BLACK];
    Bitboard attackedOcc = attacked & occupancy;
    while (attackedOcc) {
        Square attackedSquare = popLSB(&attackedOcc);

        Piece attackedPiece = pieces[attackedSquare];
        Color attackedColor = (bitboard(attackedSquare) & byColor[Color::WHITE]) ? Color::WHITE : Color::BLACK;

        if (add)
            nnue->addThreat(piece, attackedPiece, square, attackedSquare, pieceColor, attackedColor);
        else
            nnue->removeThreat(piece, attackedPiece, square, attackedSquare, pieceColor, attackedColor);
    }
}

template<bool add>
void Board::updateThreatFeaturesToPiece(Piece piece, Color pieceColor, Square square, NNUE* nnue) {
    // Add threat features of pieces that were already attacking this square
    // OR: Remove them
    Bitboard attackingSquares = threats.toSquare[square] & ~bitboard(square);

    while (attackingSquares) {
        Square attackingSquare = popLSB(&attackingSquares);
        Piece attackingPiece = pieces[attackingSquare];
        Color attackingColor = (bitboard(attackingSquare) & byColor[Color::WHITE]) ? Color::WHITE : Color::BLACK;

        assert(attackingPiece != Piece::NONE);

        if (add)
            nnue->addThreat(attackingPiece, piece, attackingSquare, square, attackingColor, pieceColor);
        else
            nnue->removeThreat(attackingPiece, piece, attackingSquare, square, attackingColor, pieceColor);
    }
}

void Board::updatePieceHash(Piece piece, Color pieceColor, uint64_t hashDelta) {
    if (piece == Piece::PAWN) {
        hashes.pawnHash ^= hashDelta;
    }
    else {
        hashes.nonPawnHash[pieceColor] ^= hashDelta;
        if (piece == Piece::KNIGHT || piece == Piece::BISHOP || piece == Piece::KING)
            hashes.minorHash ^= hashDelta;
        if (piece == Piece::ROOK || piece == Piece::QUEEN || piece == Piece::KING)
            hashes.majorHash ^= hashDelta;
    }
}

void Board::updatePieceCastling(Piece piece, Square origin) {
    if (piece == Piece::ROOK) {
        if (origin == castlingSquares[0]) {
            castling &= ~CASTLING_WHITE_KINGSIDE;
        }
        else if (origin == castlingSquares[1]) {
            castling &= ~CASTLING_WHITE_QUEENSIDE;
        }
        else if (origin == castlingSquares[2]) {
            castling &= ~CASTLING_BLACK_KINGSIDE;
        }
        else if (origin == castlingSquares[3]) {
            castling &= ~CASTLING_BLACK_QUEENSIDE;
        }
    }

    if (piece == Piece::KING) {
        if (stm == Color::WHITE) {
            castling &= CASTLING_BLACK_KINGSIDE | CASTLING_BLACK_QUEENSIDE;
        }
        else {
            castling &= CASTLING_WHITE_KINGSIDE | CASTLING_WHITE_QUEENSIDE;
        }
    }
}

void Board::addPiece(Piece piece, Color pieceColor, Square square, NNUE* nnue) {
    assert(pieces[square] == Piece::NONE);

    Bitboard squareBB = bitboard(square);
    byColor[pieceColor] ^= squareBB;
    byPiece[piece] ^= squareBB;

    pieces[square] = piece;

    nnue->addPiece(square, piece, pieceColor);

    Bitboard attacked = BB::attackedSquares(piece, square, byColor[Color::WHITE] | byColor[Color::BLACK], pieceColor);
    updateThreatFeaturesFromPiece<true>(piece, pieceColor, square, attacked, nnue);
    updatePieceThreats<true>(square, attacked, nnue);
    updateThreatFeaturesToPiece<true>(piece, pieceColor, square, nnue);

    updatePieceHash(piece, pieceColor, ZOBRIST_PIECE_SQUARES[pieceColor][piece][square]);
    updatePieceCastling(piece, square);
};

void Board::removePiece(Piece piece, Color pieceColor, Square square, NNUE* nnue) {
    assert(pieces[square] != Piece::NONE);

    Bitboard squareBB = bitboard(square);
    byColor[pieceColor] ^= squareBB;
    byPiece[piece] ^= squareBB;

    pieces[square] = Piece::NONE;

    nnue->removePiece(square, piece, pieceColor);

    Bitboard attacked = BB::attackedSquares(piece, square, byColor[Color::WHITE] | byColor[Color::BLACK], pieceColor);
    updateThreatFeaturesFromPiece<false>(piece, pieceColor, square, attacked, nnue);
    updatePieceThreats<false>(square, attacked, nnue);
    updateThreatFeaturesToPiece<false>(piece, pieceColor, square, nnue);

    updatePieceHash(piece, pieceColor, ZOBRIST_PIECE_SQUARES[pieceColor][piece][square]);
    updatePieceCastling(piece, square);
};

void Board::movePiece(Piece piece, Color pieceColor, Square origin, Square target, NNUE* nnue) {
    assert(pieces[origin] != Piece::NONE);
    assert(pieces[target] == Piece::NONE);

    nnue->movePiece(origin, target, piece, pieceColor);

    Bitboard attacksBefore = BB::attackedSquares(piece, origin, byColor[Color::WHITE] | byColor[Color::BLACK], pieceColor);
    updateThreatFeaturesFromPiece<false>(piece, pieceColor, origin, attacksBefore, nnue);
    updatePieceThreats<false>(origin, attacksBefore, nnue);
    updateThreatFeaturesToPiece<false>(piece, pieceColor, origin, nnue);

    Bitboard fromTo = bitboard(origin) ^ bitboard(target);
    byColor[pieceColor] ^= fromTo;
    byPiece[piece] ^= fromTo;

    pieces[origin] = Piece::NONE;
    pieces[target] = piece;

    Bitboard attacksAfter = BB::attackedSquares(piece, target, byColor[Color::WHITE] | byColor[Color::BLACK], pieceColor);
    updateThreatFeaturesFromPiece<true>(piece, pieceColor, target, attacksAfter, nnue);
    updatePieceThreats<true>(target, attacksAfter, nnue);
    updateThreatFeaturesToPiece<true>(piece, pieceColor, target, nnue);

    updatePieceHash(piece, pieceColor, ZOBRIST_PIECE_SQUARES[pieceColor][piece][origin] ^ ZOBRIST_PIECE_SQUARES[pieceColor][piece][target]);
    updatePieceCastling(piece, origin);
};

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

void Board::doMove(Move move, uint64_t newHash, NNUE* nnue) {
    // Increment ply counters
    rule50_ply++;
    nullmove_ply++;
    if (stm == Color::BLACK)
        ply++;
    hashes.hash = newHash;

    nnue->incrementAccumulator();

    // Calculate general information about the move
    Square origin = moveOrigin(move);
    Square target = moveTarget(move);
    MoveType type = moveType(move);

    assert(origin < 64 && target < 64);

    enpassantTarget = 0;
    Piece capturedPiece = pieces[target];
    Square captureTarget = target;

    Piece piece = pieces[origin];
    Piece promotionPiece = Piece::NONE;

    switch (type) {
    case MOVE_PROMOTION:

        if (capturedPiece != Piece::NONE) {
            removePiece(capturedPiece, flip(stm), captureTarget, nnue);
        }

        removePiece(piece, stm, origin, nnue);
        rule50_ply = 0;

        promotionPiece = PROMOTION_PIECE[promotionType(move)];
        addPiece(promotionPiece, stm, target, nnue);

        break;

    case MOVE_CASTLING: {
        Square rookOrigin, rookTarget;
        calculateCastlingSquares(origin, &target, &rookOrigin, &rookTarget, &castling);
        assert(rookOrigin < 64 && rookTarget < 64 && target < 64);

        removePiece(Piece::KING, stm, origin, nnue);
        removePiece(Piece::ROOK, stm, rookOrigin, nnue);
        addPiece(Piece::KING, stm, target, nnue);
        addPiece(Piece::ROOK, stm, rookTarget, nnue);

        capturedPiece = Piece::NONE;
    }
                      break;

    case MOVE_ENPASSANT:
        movePiece(piece, stm, origin, target, nnue);
        rule50_ply = 0;

        captureTarget = target - UP[stm];
        capturedPiece = Piece::PAWN;

        assert(captureTarget < 64);

        removePiece(Piece::PAWN, flip(stm), captureTarget, nnue);

        break;

    default: // Normal moves

        if (capturedPiece != Piece::NONE) {
            removePiece(capturedPiece, flip(stm), captureTarget, nnue);
            rule50_ply = 0;
        }

        movePiece(piece, stm, origin, target, nnue);

        if (piece == Piece::PAWN) {
            rule50_ply = 0;

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
                    enpassantTarget = bitboard(epTarget);
                }
            }
        }

        break;
    }

    // Update king checking stuff
    assert((byColor[flip(stm)] & byPiece[Piece::KING]) > 0);
    assert((byColor[stm] & byPiece[Piece::KING]) > 0);

    Square enemyKing = lsb(byColor[flip(stm)] & byPiece[Piece::KING]);
    checkers = attackersTo(enemyKing) & byColor[stm];
    checkerCount = checkers ? BB::popcount(checkers) : 0;
    updateSliderPins(Color::WHITE);
    updateSliderPins(Color::BLACK);

    stm = flip(stm);
    lastMove = move;

    nnue->finalizeMove(this);
}

void Board::doNullMove() {
    assert(!checkers);

    rule50_ply++;
    nullmove_ply = 0;
    hashes.hash ^= ZOBRIST_STM_BLACK;

    // En passent square
    if (enpassantTarget != 0) {
        hashes.hash ^= ZOBRIST_ENPASSENT[fileOf(lsb(enpassantTarget))];
    }
    enpassantTarget = 0;

    // Update king checking stuff
    assert((byColor[flip(stm)] & byPiece[Piece::KING]) > 0);

    checkers = bitboard(0);
    checkerCount = 0;
    updateSliderPins(Color::WHITE);
    updateSliderPins(Color::BLACK);

    stm = flip(stm);
    lastMove = MOVE_NULL;
}

Threats Board::calculateAllThreats() {
    Threats threats;
    memset(&threats, 0, sizeof(Threats));

    Bitboard occupied = byColor[Color::WHITE] | byColor[Color::BLACK];

    for (Color color = Color::WHITE; color <= Color::BLACK; ++color) {
        for (Piece piece = Piece::PAWN; piece < Piece::TOTAL; ++piece) {
            Bitboard pieceBB = byColor[color] & byPiece[piece];

            while (pieceBB) {
                Square square = popLSB(&pieceBB);
                Bitboard pieceAttacks = BB::attackedSquares(piece, square, occupied, color);

                while (pieceAttacks) {
                    Square attackedSquare = popLSB(&pieceAttacks);

                    threats.toSquare[attackedSquare] |= bitboard(square);
                }
            }
        }
    }

    return threats;
}

bool Board::isSquareThreatened(Square square) {
    return threats.toSquare[square] & byColor[flip(stm)];
}

bool Board::opponentHasGoodCapture() {
    // Calculate bitboards for our piece types
    Bitboard queens = byColor[stm] & byPiece[Piece::QUEEN];
    Bitboard rooks = byColor[stm] & byPiece[Piece::ROOK];
    rooks |= queens;
    Bitboard minors = byColor[stm] & (byPiece[Piece::KNIGHT] | byPiece[Piece::BISHOP]);
    minors |= rooks;

    // Calculate threat bitboards of opponent piece types
    Bitboard occupied = byColor[Color::WHITE] | byColor[Color::BLACK];
    Color them = flip(stm);

    Bitboard pawnThreats = BB::pawnAttacks(byPiece[Piece::PAWN] & byColor[them], them);
    Bitboard knightThreats = BB::knightAttacks(byPiece[Piece::KNIGHT] & byColor[them]);

    Bitboard bishopThreats = 0;
    Bitboard enemyBishops = byPiece[Piece::BISHOP] & byColor[them];
    while (enemyBishops) {
        bishopThreats |= getBishopMoves(popLSB(&enemyBishops), occupied);
    }
    Bitboard minorThreats = pawnThreats | knightThreats | bishopThreats;

    Bitboard rookThreats = minorThreats;
    Bitboard enemyRooks = byPiece[Piece::ROOK] & byColor[them];
    while (enemyRooks) {
        rookThreats |= getRookMoves(popLSB(&enemyRooks), occupied);
    }

    return (queens & rookThreats) | (rooks & minorThreats) | (minors & pawnThreats);
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
        if (piece != Piece::KING || checkers) return false;

        // Check for pieces between king and rook
        int castlingIdx = castlingIndex(stm, origin, target);
        Square kingSquare = lsb(byColor[stm] & byPiece[Piece::KING]);
        Square rookSquare = castlingSquares[castlingIdx];
        Square kingTarget = CASTLING_KING_SQUARES[castlingIdx];
        Square rookTarget = CASTLING_ROOK_SQUARES[castlingIdx];

        if (kingSquare == NO_SQUARE || rookSquare == NO_SQUARE || kingTarget == NO_SQUARE || rookTarget == NO_SQUARE) return false;

        Bitboard importantSquares = BB::BETWEEN[rookSquare][rookTarget] | BB::BETWEEN[kingSquare][kingTarget];
        importantSquares &= ~bitboard(rookSquare) & ~bitboard(kingSquare);
        if ((byColor[Color::WHITE] | byColor[Color::BLACK]) & importantSquares) return false;
        // Check for castling flags (Attackers are done in isLegal())
        return castling & CASTLING_FLAGS[castlingIdx];
    }
    case MOVE_ENPASSANT:
        if (piece != Piece::PAWN) return false;
        if (checkerCount > 1) return false;
        if (checkers && !(lsb(checkers) == target - UP[stm])) return false;
        // Check for EP flag on the right file
        return (bitboard(target) & enpassantTarget) && pieces[target - UP[stm]] == Piece::PAWN;
    case MOVE_PROMOTION:
        if (piece != Piece::PAWN) return false;
        if (checkerCount > 1) return false;
        if (checkers) {
            bool capturesChecker = target == lsb(checkers);
            bool blocksCheck = BB::BETWEEN[lsb(byColor[stm] & byPiece[Piece::KING])][lsb(checkers)] & bitboard(target);
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

    if (checkers) {
        if (piece != Piece::KING) {
            // Double check requires king moves
            if (checkerCount > 1)
                return false;

            bool capturesChecker = target == lsb(checkers);
            bool blocksCheck = BB::BETWEEN[lsb(byColor[stm] & byPiece[Piece::KING])][lsb(checkers)] & bitboard(target);
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

    if (type == MOVE_CASTLING) {
        int castlingIdx = castlingIndex(stm, origin, target);
        Square adjustedTarget = CASTLING_KING_SQUARES[castlingIdx];

        // Check that none of the important squares (including the current king position!) are being attacked
        if (adjustedTarget < origin) {
            for (Square s = adjustedTarget; s <= origin; s++) {
                if (byColor[flip(stm)] & attackersTo(s))
                    return false;
            }
        }
        else {
            for (Square s = adjustedTarget; s >= origin; s--) {
                if (byColor[flip(stm)] & attackersTo(s))
                    return false;
            }
        }
        // Check for castling flags
        return (castling & CASTLING_FLAGS[castlingIdx]) && !(blockers[stm] & bitboard(target));
    }

    if (pieces[origin] == Piece::KING) {
        // Check that we're not moving into an attack
        Bitboard occupied = byColor[Color::WHITE] | byColor[Color::BLACK];
        return !(byColor[flip(stm)] & attackersTo(target, occupied ^ bitboard(origin)));
    }

    // Check if we're not pinned to the king, or are moving along the pin
    bool pinned = blockers[stm] & originBB;
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
    if (blockers[flip(stm)] & bitboard(origin))
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
    uint64_t hash = hashes.hash ^ ZOBRIST_STM_BLACK;

    Square origin = moveOrigin(move);
    Square target = moveTarget(move);
    Square captureTarget = target;

    Piece piece = pieces[origin];
    Piece capturedPiece = pieces[target];

    MoveType type = moveType(move);;

    uint8_t newCastling = castling;

    if (enpassantTarget != 0)
        hash ^= ZOBRIST_ENPASSENT[fileOf(lsb(enpassantTarget))];

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

    blockers[side] = 0;

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
            blockers[side] |= blockerBB;
        }
    }
}

Bitboard Board::attackersTo(Square s) {
    return threats.toSquare[s];
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
            if ((enpassantTarget & mask) != 0)
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
    std::cout << std::endl << hashes.hash;
    std::cout << std::endl << hashes.pawnHash << std::endl;
}

int Board::validateBoard() {
    for (int rank = 7; rank >= 0; rank--) {
        for (int file = 0; file <= 7; file++) {

            // Get piece at index
            int idx = file + 8 * rank;
            Bitboard mask = bitboard((Square)idx);
            int first = 0;
            int second = 0;
            if ((enpassantTarget & mask) != 0)
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

            if ((enpassantTarget & mask) != 0)
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
                debugBitboard(mask);
                return idx;
            }
        }
    }
    return -1;
}

void debugBitboard(Bitboard bb) {
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