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

void Board::parseFen(std::istringstream& iss, bool isChess960) {
    std::string boardPart, stmPart, castlingPart, epPart;
    iss >> boardPart >> stmPart >> castlingPart >> epPart;

    int rule50_int, ply_int;
    rule50_ply = (iss >> rule50_int) ? rule50_int : 0;
    ply = (iss >> ply_int) ? ply_int : 1;

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
    hashes = {};
    nullmove_ply = 0;

    // Set up pieces
    Square currentSquare = 56;
    for (char c : boardPart) {
        if (c == '/') {
            currentSquare -= 16;
            continue;
        }
        if (isdigit(c)) {
            currentSquare += c - '0';
            continue;
        }

        auto addPiece = [this](Piece piece, Color color, Square sq) {
            Bitboard sqBB = bitboard(sq);
            byColor[color] |= sqBB;
            byPiece[piece] |= sqBB;
            pieces[sq] = piece;

            Hash hash = Zobrist::PIECE_SQUARES[color][piece][sq];
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
            };

        std::string pieceChars = "PNBRQKpnbrqk";
        size_t idx = pieceChars.find(c);
        if (idx != std::string::npos) {
            Piece piece = static_cast<Piece>(idx % 6);
            Color color = idx < 6 ? Color::WHITE : Color::BLACK;
            addPiece(piece, color, currentSquare++);
        }
    }

    // Side to move
    stm = (stmPart == "w") ? Color::WHITE : Color::BLACK;
    if (stm == Color::BLACK)
        hashes.hash ^= Zobrist::STM_BLACK;

    // Castling
    castling = 0;
    castlingSquares[0] = castlingSquares[1] = castlingSquares[2] = castlingSquares[3] = NO_SQUARE;
    for (char c : castlingPart) {
        if (c == '-') break;

        auto castlingChars = { 'K', 'Q', 'k', 'q' };
        auto it = std::find(castlingChars.begin(), castlingChars.end(), c);
        if (it != castlingChars.end()) {
            int index = it - castlingChars.begin();
            Color side = static_cast<Color>(index >= 2);
            Bitboard backrank = side == Color::WHITE ? BB::RANK_1 : BB::RANK_8;
            bool kingside = index % 2 == 0;

            // Find position of corresponding rook
            Bitboard rookBB = byColor[side] & byPiece[Piece::ROOK] & backrank;
            std::vector<Square> rooks;
            while (rookBB) {
                rooks.push_back(popLSB(&rookBB));
            }
            castlingSquares[index] = kingside ? *std::max_element(rooks.begin(), rooks.end()) : *std::min_element(rooks.begin(), rooks.end());
            castling |= 1 << index;
        }
        else {
            // Rook file is already given
            Color side = c >= 'A' && c <= 'H' ? Color::WHITE : Color::BLACK;
            int rookFile = side == Color::WHITE ? int(c - 'A') : int(c - 'a');
            Square king = lsb(byColor[side] & byPiece[Piece::KING]);
            int index = 2 * side + (rookFile < fileOf(king));

            castling |= 1 << index;
            castlingSquares[index] = Square(rookFile + 56 * (side == Color::BLACK));
        }
    }
    hashes.hash ^= Zobrist::CASTLING[castling];

    // en passent
    if (epPart == "-") {
        enpassantTarget = bitboard(0);
    }
    else {
        Square epTargetSquare = stringToSquare(epPart.c_str());
        enpassantTarget = bitboard(epTargetSquare);

        // Check if there's *actually* a pawn that can do enpassent
        Bitboard enemyPawns = byColor[flip(stm)] & byPiece[Piece::PAWN];
        Bitboard epRank = stm == Color::WHITE ? BB::RANK_4 : BB::RANK_5;
        Square pawnSquare1 = epTargetSquare + 1 + Direction::UP[stm];
        Square pawnSquare2 = epTargetSquare - 1 + Direction::UP[stm];
        Bitboard epPawns = (bitboard(pawnSquare1) | bitboard(pawnSquare2)) & epRank & enemyPawns;
        if (epPawns)
            hashes.hash ^= Zobrist::ENPASSENT[fileOf(epTargetSquare)];
    }

    threats = calculateAllThreats();

    // Update king checking stuff
    Square enemyKing = lsb(byColor[stm] & byPiece[Piece::KING]);
    checkers = attackersTo(enemyKing) & byColor[flip(stm)];
    checkerCount = BB::popcount(checkers);

    updateSliderPins(Color::WHITE);
    updateSliderPins(Color::BLACK);

    chess960 = isChess960;
}

std::string Board::fen() {
    std::ostringstream os;

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
                    os << std::to_string(freeSquares);
                    freeSquares = 0;
                }
                os << ((byColor[Color::WHITE] & bitboard(s)) ? toupper(pieceChars[p]) : pieceChars[p]);
            }
        }
        if (freeSquares != 0)
            os << std::to_string(freeSquares);
        if (rank != 0)
            os << "/";
    }

    if (stm == Color::WHITE)
        os << " w ";
    else
        os << " b ";

    if (castling & Castling::getMask(Color::WHITE, Castling::KINGSIDE)) os << "K";
    if (castling & Castling::getMask(Color::WHITE, Castling::QUEENSIDE)) os << "Q";
    if (castling & Castling::getMask(Color::BLACK, Castling::KINGSIDE)) os << "k";
    if (castling & Castling::getMask(Color::BLACK, Castling::QUEENSIDE)) os << "q";
    if (castling)
        os << " ";
    else
        os << "- ";

    if (enpassantTarget) {
        Square epSquare = lsb(enpassantTarget);
        os << epSquare << " ";
    }
    else
        os << "- ";

    os << std::to_string(rule50_ply) + " " + std::to_string(ply);

    return os.str();
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

void Board::updatePieceHash(Piece piece, Color pieceColor, Hash hashDelta) {
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

void Board::updatePieceCastling(Piece piece, Color pieceColor, Square origin) {
    if (piece == Piece::ROOK) {
        for (auto direction : { Castling::KINGSIDE, Castling::QUEENSIDE }) {
            if (origin == getCastlingRookSquare(pieceColor, direction)) {
                castling &= ~Castling::getMask(pieceColor, direction);
            }
        }
    }

    if (piece == Piece::KING) {
        castling &= ~Castling::getMask(pieceColor);
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

    updatePieceHash(piece, pieceColor, Zobrist::PIECE_SQUARES[pieceColor][piece][square]);
    updatePieceCastling(piece, pieceColor, square);
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

    updatePieceHash(piece, pieceColor, Zobrist::PIECE_SQUARES[pieceColor][piece][square]);
    updatePieceCastling(piece, pieceColor, square);
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

    updatePieceHash(piece, pieceColor, Zobrist::PIECE_SQUARES[pieceColor][piece][origin] ^ Zobrist::PIECE_SQUARES[pieceColor][piece][target]);
    updatePieceCastling(piece, pieceColor, origin);
};

void Board::doMove(Move move, Hash newHash, NNUE* nnue) {
    // Increment ply counters
    rule50_ply++;
    nullmove_ply++;
    if (stm == Color::BLACK)
        ply++;
    hashes.hash = newHash;

    nnue->incrementAccumulator();

    // Calculate general information about the move
    Square origin = move.origin();
    Square target = move.target();
    MoveType type = move.type();

    assert(origin < 64 && target < 64);

    enpassantTarget = 0;
    Piece capturedPiece = pieces[target];
    Square captureTarget = target;

    Piece piece = pieces[origin];
    Piece promotionPiece = Piece::NONE;

    switch (type) {
    case MoveType::PROMOTION:

        if (capturedPiece != Piece::NONE) {
            removePiece(capturedPiece, flip(stm), captureTarget, nnue);
        }

        removePiece(piece, stm, origin, nnue);
        rule50_ply = 0;
        promotionPiece = move.promotionPiece();
        addPiece(promotionPiece, stm, target, nnue);

        break;

    case MoveType::CASTLING: {

        auto direction = Castling::getDirection(origin, target);
        Square rookOrigin = getCastlingRookSquare(stm, direction);
        Square rookTarget = Castling::getRookSquare(stm, direction);
        Square kingTarget = Castling::getKingSquare(stm, direction);

        removePiece(Piece::KING, stm, origin, nnue);
        removePiece(Piece::ROOK, stm, rookOrigin, nnue);
        addPiece(Piece::KING, stm, kingTarget, nnue);
        addPiece(Piece::ROOK, stm, rookTarget, nnue);

        capturedPiece = Piece::NONE;
    }
                      break;

    case MoveType::ENPASSANT:
        movePiece(piece, stm, origin, target, nnue);
        rule50_ply = 0;

        captureTarget = target - Direction::UP[stm];
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
                assert(target - Direction::UP[stm] < 64);

                Bitboard enemyPawns = byColor[flip(stm)] & byPiece[Piece::PAWN];
                Bitboard epRank = stm == Color::WHITE ? BB::RANK_4 : BB::RANK_5;
                Square pawnSquare1 = target + 1;
                Square pawnSquare2 = target - 1;
                Bitboard epPawns = (bitboard(pawnSquare1) | bitboard(pawnSquare2)) & epRank & enemyPawns;
                if (epPawns) {
                    Square epTarget = target - Direction::UP[stm];
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
    hashes.hash ^= Zobrist::STM_BLACK;

    // En passent square
    if (enpassantTarget != 0) {
        hashes.hash ^= Zobrist::ENPASSENT[fileOf(lsb(enpassantTarget))];
    }
    enpassantTarget = 0;

    // Update king checking stuff
    assert((byColor[flip(stm)] & byPiece[Piece::KING]) > 0);

    checkers = bitboard(0);
    checkerCount = 0;
    updateSliderPins(Color::WHITE);
    updateSliderPins(Color::BLACK);

    stm = flip(stm);
    lastMove = Move::none();
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
    Square origin = move.origin();
    Square target = move.target();
    Bitboard originBB = bitboard(origin);
    Bitboard targetBB = bitboard(target);
    Piece piece = pieces[origin];
    MoveType type = move.type();

    // A valid piece needs to be on the origin square
    if (pieces[origin] == Piece::NONE) return false;
    // We can't capture our own piece, except in castling
    if (type != MoveType::CASTLING && (byColor[stm] & targetBB)) return false;
    // We can't move the enemies piece
    if (byColor[flip(stm)] & originBB) return false;

    // Non-standard movetypes
    switch (type) {
    case MoveType::CASTLING: {
        if (piece != Piece::KING || checkers) return false;

        // Check for pieces between king and rook
        auto direction = Castling::getDirection(origin, target);
        Square kingOrigin = lsb(byColor[stm] & byPiece[Piece::KING]);
        Square kingTarget = Castling::getKingSquare(stm, direction);
        Square rookOrigin = getCastlingRookSquare(stm, direction);
        Square rookTarget = Castling::getRookSquare(stm, direction);

        if (kingOrigin == NO_SQUARE || rookOrigin == NO_SQUARE || kingTarget == NO_SQUARE || rookTarget == NO_SQUARE) return false;

        Bitboard importantSquares = BB::BETWEEN[rookOrigin][rookTarget] | BB::BETWEEN[kingOrigin][kingTarget];
        importantSquares &= ~bitboard(rookOrigin) & ~bitboard(kingOrigin);
        if ((byColor[Color::WHITE] | byColor[Color::BLACK]) & importantSquares) return false;
        // Check for castling flags (Attackers are done in isLegal())
        return castling & Castling::getMask(stm, Castling::getDirection(origin, target));
    }
    case MoveType::ENPASSANT:
        if (piece != Piece::PAWN) return false;
        if (checkerCount > 1) return false;
        if (checkers && !(lsb(checkers) == target - Direction::UP[stm])) return false;
        // Check for EP flag on the right file
        return (bitboard(target) & enpassantTarget) && pieces[target - Direction::UP[stm]] == Piece::PAWN;
    case MoveType::PROMOTION:
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
            !(origin + Direction::UP[stm] == target && pieces[target] == Piece::NONE)) // Push promotion?
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
            !(origin + Direction::UP[stm] == target && pieces[target] == Piece::NONE) && // Single push?
            !(origin + 2 * Direction::UP[stm] == target && pieces[target] == Piece::NONE && pieces[target - Direction::UP[stm]] == Piece::NONE && (originBB & (BB::RANK_2 | BB::RANK_7))) // Double push?
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

    Square origin = move.origin();
    Square target = move.target();
    Bitboard originBB = bitboard(origin);

    Square king = lsb(byPiece[Piece::KING] & byColor[stm]);

    MoveType type = move.type();
    if (type == MoveType::ENPASSANT) {
        // Check if king would be in check after this move
        Square epSquare = target - Direction::UP[stm];

        Bitboard epSquareBB = bitboard(epSquare);
        Bitboard targetBB = bitboard(target);
        Bitboard occupied = ((byColor[Color::WHITE] | byColor[Color::BLACK]) ^ originBB ^ epSquareBB) | targetBB;

        // Check if any rooks/queens/bishops attack the king square, given the occupied pieces after this EP move
        Bitboard rookAttacks = getRookMoves(king, occupied) & (byPiece[Piece::ROOK] | byPiece[Piece::QUEEN]) & byColor[flip(stm)];
        Bitboard bishopAttacks = getBishopMoves(king, occupied) & (byPiece[Piece::BISHOP] | byPiece[Piece::QUEEN]) & byColor[flip(stm)];
        return !rookAttacks && !bishopAttacks;
    }

    if (type == MoveType::CASTLING) {
        auto direction = Castling::getDirection(origin, target);
        Square kingTarget = Castling::getKingSquare(stm, direction);

        // Check that none of the important squares (including the current king position!) are being attacked
        if (kingTarget < origin) {
            for (Square s = kingTarget; s <= origin; s++) {
                if (byColor[flip(stm)] & attackersTo(s))
                    return false;
            }
        }
        else {
            for (Square s = kingTarget; s >= origin; s--) {
                if (byColor[flip(stm)] & attackersTo(s))
                    return false;
            }
        }
        // Check for castling flags
        return (castling & Castling::getMask(stm, direction)) && !(blockers[stm] & bitboard(target));
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
    Square origin = move.origin();
    Square target = move.target();
    Bitboard occ = byColor[Color::WHITE] | byColor[Color::BLACK];
    Bitboard enemyKing = byColor[flip(stm)] & byPiece[Piece::KING];

    // Direct check
    Bitboard attacks = BB::attackedSquares(pieces[origin], target, occ ^ bitboard(origin) ^ bitboard(target), stm);
    if (attacks & enemyKing)
        return true;

    // Discovered check: Are we blocking a check to the enemy king?
    MoveType type = move.type();
    if (blockers[flip(stm)] & bitboard(origin))
        return !(BB::LINE[origin][target] & enemyKing) || type == MoveType::CASTLING;

    switch (type) {
    case MoveType::PROMOTION: {
        Piece promotionPiece = move.promotionPiece();
        return BB::attackedSquares(promotionPiece, target, occ ^ bitboard(origin), stm) & enemyKing;
    }
    case MoveType::CASTLING: {
        return BB::attackedSquares(Piece::ROOK, Castling::getRookSquare(stm, Castling::getDirection(origin, target)), occ, stm) & enemyKing;
    }
    case MoveType::ENPASSANT: {
        Square epSquare = target - Direction::UP[stm];
        Bitboard occupied = (occ ^ bitboard(origin) ^ bitboard(epSquare)) | bitboard(target);
        Square ek = lsb(enemyKing);
        return (BB::attackedSquares(Piece::ROOK, ek, occupied, stm) & byColor[stm] & (byPiece[Piece::ROOK] | byPiece[Piece::QUEEN]))
            | (BB::attackedSquares(Piece::BISHOP, ek, occupied, stm) & byColor[stm] & (byPiece[Piece::BISHOP] | byPiece[Piece::QUEEN]));
    }
    default:
        return false;
    }
}

Hash Board::hashAfter(Move move) {
    Hash hash = hashes.hash ^ Zobrist::STM_BLACK;

    Square origin = move.origin();
    Square target = move.target();
    Square captureTarget = target;

    Piece piece = pieces[origin];
    Piece capturedPiece = pieces[target];

    MoveType type = move.type();

    if (enpassantTarget != 0)
        hash ^= Zobrist::ENPASSENT[fileOf(lsb(enpassantTarget))];

    if (piece == Piece::PAWN) {
        if (type == MoveType::ENPASSANT) {
            capturedPiece = Piece::PAWN;
            captureTarget = target - Direction::UP[stm];
        }

        // Double push
        if ((origin ^ target) == 16) {
            Bitboard enemyPawns = byColor[flip(stm)] & byPiece[Piece::PAWN];
            Bitboard epRank = stm == Color::WHITE ? BB::RANK_4 : BB::RANK_5;
            Square pawnSquare1 = target + 1;
            Square pawnSquare2 = target - 1;
            Bitboard epPawns = (bitboard(pawnSquare1) | bitboard(pawnSquare2)) & epRank & enemyPawns;
            if (epPawns)
                hash ^= Zobrist::ENPASSENT[fileOf(origin)];
        }
    }

    if (type == MoveType::CASTLING) {
        capturedPiece = Piece::NONE;

        auto direction = Castling::getDirection(origin, target);
        Square rookOrigin = getCastlingRookSquare(stm, direction);
        Square rookTarget = Castling::getRookSquare(stm, direction);
        Square kingTarget = Castling::getKingSquare(stm, direction);

        hash ^= Zobrist::CASTLING[castling];
        hash ^= Zobrist::CASTLING[castling & ~Castling::getMask(stm)];

        hash ^= Zobrist::PIECE_SQUARES[stm][Piece::KING][origin] ^ Zobrist::PIECE_SQUARES[stm][Piece::KING][kingTarget];
        hash ^= Zobrist::PIECE_SQUARES[stm][Piece::ROOK][rookOrigin] ^ Zobrist::PIECE_SQUARES[stm][Piece::ROOK][rookTarget];
    }
    else {
        hash ^= Zobrist::PIECE_SQUARES[stm][piece][origin] ^ Zobrist::PIECE_SQUARES[stm][piece][target];

        if (capturedPiece != Piece::NONE)
            hash ^= Zobrist::PIECE_SQUARES[flip(stm)][capturedPiece][captureTarget];

        if (piece == Piece::KING) {
            hash ^= Zobrist::CASTLING[castling];
            hash ^= Zobrist::CASTLING[castling & ~Castling::getMask(stm)];
        }

        uint8_t tempCastling = castling;
        if (piece == Piece::ROOK) {
            hash ^= Zobrist::CASTLING[tempCastling];
            for (auto direction : { Castling::KINGSIDE, Castling::QUEENSIDE }) {
                if (origin == getCastlingRookSquare(stm, direction)) {
                    tempCastling &= ~Castling::getMask(stm, direction);
                }
            }
            hash ^= Zobrist::CASTLING[tempCastling];
        }
        if (capturedPiece == Piece::ROOK) {
            hash ^= Zobrist::CASTLING[tempCastling];
            for (auto direction : { Castling::KINGSIDE, Castling::QUEENSIDE }) {
                if (captureTarget == getCastlingRookSquare(flip(stm), direction)) {
                    tempCastling &= ~Castling::getMask(flip(stm), direction);
                }
            }
            hash ^= Zobrist::CASTLING[tempCastling];
        }
    }

    if (type == MoveType::PROMOTION) {
        Piece promotionPiece = move.promotionPiece();
        hash ^= Zobrist::PIECE_SQUARES[stm][piece][target] ^ Zobrist::PIECE_SQUARES[stm][promotionPiece][target];
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