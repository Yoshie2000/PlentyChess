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
        pieces[s] = Piece::NO_PIECE;
    }
    for (Color c = Color::WHITE; c <= Color::BLACK; ++c) {
        byColor[c] = bitboard(0);
        for (PieceType p = PieceType::PAWN; p < PieceType::TOTAL; ++p) {
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

        auto addPiece = [this](PieceType piece, Color color, Square sq) {
            Bitboard sqBB = bitboard(sq);
            byColor[color] |= sqBB;
            byPiece[piece] |= sqBB;
            pieces[sq] = makePiece(piece, color);

            uint64_t hash = Zobrist::PIECE_SQUARES[pieces[sq]][sq];
            hashes.hash ^= hash;
            if (piece == PieceType::PAWN)
                hashes.pawnHash ^= hash;
            else {
                hashes.nonPawnHash[color] ^= hash;
                if (piece == PieceType::KNIGHT || piece == PieceType::BISHOP || piece == PieceType::KING)
                    hashes.minorHash ^= hash;
                if (piece == PieceType::ROOK || piece == PieceType::QUEEN || piece == PieceType::KING)
                    hashes.majorHash ^= hash;
            }
            };

        std::string pieceChars = "PNBRQKpnbrqk";
        size_t idx = pieceChars.find(c);
        if (idx != std::string::npos) {
            PieceType piece = static_cast<PieceType>(idx % 6);
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
            Bitboard rookBB = byColor[side] & byPiece[PieceType::ROOK] & backrank;
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
            Square king = lsb(byColor[side] & byPiece[PieceType::KING]);
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
        Bitboard enemyPawns = byColor[flip(stm)] & byPiece[PieceType::PAWN];
        Bitboard epRank = stm == Color::WHITE ? BB::RANK_4 : BB::RANK_5;
        Square pawnSquare1 = epTargetSquare + 1 + UP[stm];
        Square pawnSquare2 = epTargetSquare - 1 + UP[stm];
        Bitboard epPawns = (bitboard(pawnSquare1) | bitboard(pawnSquare2)) & epRank & enemyPawns;
        if (epPawns)
            hashes.hash ^= Zobrist::ENPASSENT[fileOf(epTargetSquare)];
    }

    // Update king checking stuff
    Square enemyKing = lsb(byColor[stm] & byPiece[PieceType::KING]);
    checkers = attackersTo(enemyKing) & byColor[flip(stm)];
    checkerCount = BB::popcount(checkers);

    updateSliderPins(Color::WHITE);
    updateSliderPins(Color::BLACK);

    chess960 = isChess960;

    calculateThreats();
}

std::string Board::fen() {
    std::string result;

    std::string pieceChars = "pnbrqk ";

    for (int rank = 7; rank >= 0; rank--) {
        int freeSquares = 0;
        for (int file = 0; file < 8; file++) {
            Square s = Square(8 * rank + file);
            Piece p = pieces[s];
            if (p == Piece::NO_PIECE)
                freeSquares++;
            else {
                if (freeSquares != 0) {
                    result += std::to_string(freeSquares);
                    freeSquares = 0;
                }
                result += (byColor[Color::WHITE] & bitboard(s)) ? toupper(pieceChars[typeOf(p)]) : pieceChars[typeOf(p)];
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

    if (castling & Castling::getMask(Color::WHITE, Castling::KINGSIDE)) result += "K";
    if (castling & Castling::getMask(Color::WHITE, Castling::QUEENSIDE)) result += "Q";
    if (castling & Castling::getMask(Color::BLACK, Castling::KINGSIDE)) result += "k";
    if (castling & Castling::getMask(Color::BLACK, Castling::QUEENSIDE)) result += "q";
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
void Board::updatePieceThreats(Piece piece, Square square, NNUE* nnue) {
    // Process attacks of the current piece to other pieces
    Bitboard occupancy = byColor[Color::WHITE] | byColor[Color::BLACK];
    Bitboard attacked = BB::attackedSquares(typeOf(piece), square, occupancy, colorOf(piece)) & occupancy;
    while (attacked) {
        Square attackedSquare = popLSB(&attacked);
        Piece attackedPiece = pieces[attackedSquare];

        assert(attackedPiece != Piece::NO_PIECE);

        nnue->updateThreat(piece, attackedPiece, square, attackedSquare, add);
    }

    Bitboard rookAttacks = getRookMoves(square, occupancy);
    Bitboard bishopAttacks = getBishopMoves(square, occupancy);
    Bitboard queenAttacks = rookAttacks | bishopAttacks;

    Bitboard slidingPieces = (byPiece[PieceType::BISHOP] | byPiece[PieceType::QUEEN]) & bishopAttacks;
    slidingPieces |= (byPiece[PieceType::ROOK] | byPiece[PieceType::QUEEN]) & rookAttacks;

    // Process attacks of sliding pieces that are now blocked by this piece
    while (slidingPieces) {
        Square slidingPieceSquare = popLSB(&slidingPieces);
        Piece slidingPiece = pieces[slidingPieceSquare];

        Bitboard ray = BB::RAY_PASS[slidingPieceSquare][square];
        Bitboard threatened = ray & occupancy & queenAttacks;

        assert(BB::popcount(threatened) < 2);

        if (threatened) {
            Square attackedSquare = lsb(threatened);
            Piece attackedPiece = pieces[attackedSquare];

            ray &= BB::BETWEEN[square][attackedSquare];
            nnue->updateThreat(slidingPiece, attackedPiece, slidingPieceSquare, attackedSquare, !add);
        }

        nnue->updateThreat(slidingPiece, piece, slidingPieceSquare, square, add);
    }

    // Process attacks of non-slider pieces that were already attacking this square
    Bitboard attackingPawns = byPiece[PieceType::PAWN] & ((byColor[Color::BLACK] & BB::pawnAttacks(bitboard(square), Color::WHITE)) | (byColor[Color::WHITE] & BB::pawnAttacks(bitboard(square), Color::BLACK)));
    Bitboard attackingKnights = byPiece[PieceType::KNIGHT] & BB::KNIGHT_ATTACKS[square];
    Bitboard attackingKings = byPiece[PieceType::KING] & BB::KING_ATTACKS[square];
    Bitboard attackingSquares = attackingPawns | attackingKnights | attackingKings;
    while (attackingSquares) {
        Square attackingSquare = popLSB(&attackingSquares);
        Piece attackingPiece = pieces[attackingSquare];

        assert(attackingPiece != Piece::NO_PIECE);

        nnue->updateThreat(attackingPiece, piece, attackingSquare, square, add);
    }
}

void Board::updatePieceHash(Piece piece, uint64_t hashDelta) {
    PieceType type = typeOf(piece);
    if (type == PieceType::PAWN) {
        hashes.pawnHash ^= hashDelta;
    }
    else {
        hashes.nonPawnHash[colorOf(piece)] ^= hashDelta;
        if (type == PieceType::KNIGHT || type == PieceType::BISHOP || type == PieceType::KING)
            hashes.minorHash ^= hashDelta;
        if (type == PieceType::ROOK || type == PieceType::QUEEN || type == PieceType::KING)
            hashes.majorHash ^= hashDelta;
    }
}

void Board::updatePieceCastling(Piece piece, Square origin) {
    PieceType type = typeOf(piece);
    Color pieceColor = colorOf(piece);
    if (type == PieceType::ROOK) {
        for (auto direction : { Castling::KINGSIDE, Castling::QUEENSIDE }) {
            if (origin == getCastlingRookSquare(pieceColor, direction)) {
                castling &= ~Castling::getMask(pieceColor, direction);
            }
        }
    }

    if (type == PieceType::KING) {
        castling &= ~Castling::getMask(pieceColor);
    }
}

void Board::addPiece(Piece piece, Square square, NNUE* nnue) {
    assert(pieces[square] == Piece::NO_PIECE);

    Bitboard squareBB = bitboard(square);
    byColor[colorOf(piece)] ^= squareBB;
    byPiece[typeOf(piece)] ^= squareBB;

    pieces[square] = piece;

    nnue->addPiece(square, piece);

    updatePieceThreats<true>(piece, square, nnue);
    updatePieceHash(piece, Zobrist::PIECE_SQUARES[piece][square]);
    updatePieceCastling(piece, square);
};

void Board::removePiece(Piece piece, Square square, NNUE* nnue) {
    assert(pieces[square] != Piece::NO_PIECE);

    Bitboard squareBB = bitboard(square);
    byColor[colorOf(piece)] ^= squareBB;
    byPiece[typeOf(piece)] ^= squareBB;

    pieces[square] = Piece::NO_PIECE;

    nnue->removePiece(square, piece);

    updatePieceThreats<false>(piece, square, nnue);
    updatePieceHash(piece, Zobrist::PIECE_SQUARES[piece][square]);
    updatePieceCastling(piece, square);
};

void Board::movePiece(Piece piece, Square origin, Square target, NNUE* nnue) {
    assert(pieces[origin] != Piece::NO_PIECE);
    assert(pieces[target] == Piece::NO_PIECE);

    nnue->movePiece(origin, target, piece);

    updatePieceThreats<false>(piece, origin, nnue);

    Bitboard fromTo = bitboard(origin) ^ bitboard(target);
    byColor[colorOf(piece)] ^= fromTo;
    byPiece[typeOf(piece)] ^= fromTo;

    pieces[origin] = Piece::NO_PIECE;
    pieces[target] = piece;

    updatePieceThreats<true>(piece, target, nnue);
    updatePieceHash(piece, Zobrist::PIECE_SQUARES[piece][origin] ^ Zobrist::PIECE_SQUARES[piece][target]);
    updatePieceCastling(piece, origin);
};

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
    Piece promotionPiece = Piece::NO_PIECE;

    switch (type) {
    case MOVE_PROMOTION:

        if (capturedPiece != Piece::NO_PIECE) {
            removePiece(capturedPiece, captureTarget, nnue);
        }

        removePiece(piece, origin, nnue);
        rule50_ply = 0;

        promotionPiece = makePiece(PROMOTION_PIECE[promotionType(move)], stm);
        addPiece(promotionPiece, target, nnue);

        break;

    case MOVE_CASTLING: {

        auto direction = Castling::getDirection(origin, target);
        Square rookOrigin = getCastlingRookSquare(stm, direction);
        Square rookTarget = Castling::getRookSquare(stm, direction);
        Square kingTarget = Castling::getKingSquare(stm, direction);

        Piece kingPiece = makePiece(PieceType::KING, stm);
        Piece rookPiece = makePiece(PieceType::ROOK, stm);
        removePiece(kingPiece, origin, nnue);
        removePiece(rookPiece, rookOrigin, nnue);
        addPiece(kingPiece, kingTarget, nnue);
        addPiece(rookPiece, rookTarget, nnue);

        capturedPiece = Piece::NO_PIECE;
    }
                      break;

    case MOVE_ENPASSANT:
        movePiece(piece, origin, target, nnue);
        rule50_ply = 0;

        captureTarget = target - UP[stm];
        capturedPiece = makePiece(PieceType::PAWN, flip(stm));

        assert(captureTarget < 64);

        removePiece(capturedPiece, captureTarget, nnue);

        break;

    default: // Normal moves

        if (capturedPiece != Piece::NO_PIECE) {
            removePiece(capturedPiece, captureTarget, nnue);
            rule50_ply = 0;
        }

        movePiece(piece, origin, target, nnue);

        if (typeOf(piece) == PieceType::PAWN) {
            rule50_ply = 0;

            // Double push
            if ((origin ^ target) == 16) {
                assert(target - UP[stm] < 64);

                Bitboard enemyPawns = byColor[flip(stm)] & byPiece[PieceType::PAWN];
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
    assert((byColor[flip(stm)] & byPiece[PieceType::KING]) > 0);
    assert((byColor[stm] & byPiece[PieceType::KING]) > 0);

    Square enemyKing = lsb(byColor[flip(stm)] & byPiece[PieceType::KING]);
    checkers = attackersTo(enemyKing) & byColor[stm];
    checkerCount = checkers ? BB::popcount(checkers) : 0;
    updateSliderPins(Color::WHITE);
    updateSliderPins(Color::BLACK);

    stm = flip(stm);
    lastMove = move;

    nnue->finalizeMove(this);

    calculateThreats();
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
    assert((byColor[flip(stm)] & byPiece[PieceType::KING]) > 0);

    checkers = bitboard(0);
    checkerCount = 0;
    updateSliderPins(Color::WHITE);
    updateSliderPins(Color::BLACK);

    stm = flip(stm);
    lastMove = MOVE_NULL;

    calculateThreats();
}

void Board::calculateThreats() {
    Bitboard occupied = byColor[Color::WHITE] | byColor[Color::BLACK];
    Color them = flip(stm);

    threats.pawnThreats = BB::pawnAttacks(byPiece[PieceType::PAWN] & byColor[them], them);
    threats.knightThreats = BB::knightAttacks(byPiece[PieceType::KNIGHT] & byColor[them]);

    threats.bishopThreats = 0;
    Bitboard bishops = byPiece[PieceType::BISHOP] & byColor[them];
    while (bishops) {
        threats.bishopThreats |= getBishopMoves(popLSB(&bishops), occupied);
    }

    threats.rookThreats = 0;
    Bitboard rooks = byPiece[PieceType::ROOK] & byColor[them];
    while (rooks) {
        threats.rookThreats |= getRookMoves(popLSB(&rooks), occupied);
    }

    threats.queenThreats = 0;
    Bitboard queens = byPiece[PieceType::QUEEN] & byColor[them];
    while (queens) {
        Square square = popLSB(&queens);
        threats.queenThreats |= getRookMoves(square, occupied);
        threats.queenThreats |= getBishopMoves(square, occupied);
    }

    threats.kingThreats = 0;
    Bitboard kings = byPiece[PieceType::KING] & byColor[them];
    while (kings) {
        threats.kingThreats |= BB::KING_ATTACKS[popLSB(&kings)];
    };
}

bool Board::isSquareThreatened(Square square) {
    Bitboard squareBB = bitboard(square);
    return squareBB & (threats.pawnThreats | threats.knightThreats | threats.bishopThreats | threats.rookThreats | threats.queenThreats | threats.kingThreats);
}

bool Board::opponentHasGoodCapture() {
    Bitboard queens = byColor[stm] & byPiece[PieceType::QUEEN];
    Bitboard rooks = byColor[stm] & byPiece[PieceType::ROOK];
    rooks |= queens;
    Bitboard minors = byColor[stm] & (byPiece[PieceType::KNIGHT] | byPiece[PieceType::BISHOP]);
    minors |= rooks;

    Bitboard minorThreats = threats.knightThreats | threats.bishopThreats | threats.pawnThreats;
    Bitboard rookThreats = minorThreats | threats.rookThreats;

    return (queens & rookThreats) | (rooks & minorThreats) | (minors & threats.pawnThreats);
}

bool Board::isPseudoLegal(Move move) {
    Square origin = moveOrigin(move);
    Square target = moveTarget(move);
    Bitboard originBB = bitboard(origin);
    Bitboard targetBB = bitboard(target);
    Piece piece = pieces[origin];
    MoveType type = moveType(move);

    // A valid piece needs to be on the origin square
    if (pieces[origin] == Piece::NO_PIECE) return false;
    // We can't capture our own piece, except in castling
    if (type != MOVE_CASTLING && (byColor[stm] & targetBB)) return false;
    // We can't move the enemies piece
    if (byColor[flip(stm)] & originBB) return false;

    // Non-standard movetypes
    switch (type) {
    case MOVE_CASTLING: {
        if (typeOf(piece) != PieceType::KING || checkers) return false;

        // Check for pieces between king and rook
        auto direction = Castling::getDirection(origin, target);
        Square kingOrigin = lsb(byColor[stm] & byPiece[PieceType::KING]);
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
    case MOVE_ENPASSANT:
        if (typeOf(piece) != PieceType::PAWN) return false;
        if (checkerCount > 1) return false;
        if (checkers && !(lsb(checkers) == target - UP[stm])) return false;
        // Check for EP flag on the right file
        return (bitboard(target) & enpassantTarget) && typeOf(pieces[target - UP[stm]]) == PieceType::PAWN;
    case MOVE_PROMOTION:
        if (typeOf(piece) != PieceType::PAWN) return false;
        if (checkerCount > 1) return false;
        if (checkers) {
            bool capturesChecker = target == lsb(checkers);
            bool blocksCheck = BB::BETWEEN[lsb(byColor[stm] & byPiece[PieceType::KING])][lsb(checkers)] & bitboard(target);
            if (!capturesChecker && !blocksCheck)
                return false;
        }
        if (
            !(BB::pawnAttacks(originBB, stm) & targetBB & byColor[flip(stm)]) && // Capture promotion?
            !(origin + UP[stm] == target && pieces[target] == Piece::NO_PIECE)) // Push promotion?
            return false;
        return true;
    default:
        break;
    }

    Bitboard occupied = byColor[Color::WHITE] | byColor[Color::BLACK];
    switch (typeOf(piece)) {
    case PieceType::PAWN:
        if (targetBB & (BB::RANK_8 | BB::RANK_1)) return false;

        if (
            !(BB::pawnAttacks(originBB, stm) & targetBB & byColor[flip(stm)]) && // Capture?
            !(origin + UP[stm] == target && pieces[target] == Piece::NO_PIECE) && // Single push?
            !(origin + 2 * UP[stm] == target && pieces[target] == Piece::NO_PIECE && pieces[target - UP[stm]] == Piece::NO_PIECE && (originBB & (BB::RANK_2 | BB::RANK_7))) // Double push?
            )
            return false;
        break;
    case PieceType::KNIGHT:
        if (!(BB::KNIGHT_ATTACKS[origin] & targetBB)) return false;
        break;
    case PieceType::BISHOP:
        if (!(getBishopMoves(origin, occupied) & targetBB)) return false;
        break;
    case PieceType::ROOK:
        if (!(getRookMoves(origin, occupied) & targetBB)) return false;
        break;
    case PieceType::QUEEN:
        if (!((getBishopMoves(origin, occupied) | getRookMoves(origin, occupied)) & targetBB)) return false;
        break;
    case PieceType::KING:
        if (!(BB::KING_ATTACKS[origin] & targetBB)) return false;
        break;
    default:
        return false;
    }

    if (checkers) {
        if (typeOf(piece) != PieceType::KING) {
            // Double check requires king moves
            if (checkerCount > 1)
                return false;

            bool capturesChecker = target == lsb(checkers);
            bool blocksCheck = BB::BETWEEN[lsb(byColor[stm] & byPiece[PieceType::KING])][lsb(checkers)] & bitboard(target);
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

    Square king = lsb(byPiece[PieceType::KING] & byColor[stm]);

    MoveType type = moveType(move);
    if (type == MOVE_ENPASSANT) {
        // Check if king would be in check after this move
        Square epSquare = target - UP[stm];

        Bitboard epSquareBB = bitboard(epSquare);
        Bitboard targetBB = bitboard(target);
        Bitboard occupied = ((byColor[Color::WHITE] | byColor[Color::BLACK]) ^ originBB ^ epSquareBB) | targetBB;

        // Check if any rooks/queens/bishops attack the king square, given the occupied pieces after this EP move
        Bitboard rookAttacks = getRookMoves(king, occupied) & (byPiece[PieceType::ROOK] | byPiece[PieceType::QUEEN]) & byColor[flip(stm)];
        Bitboard bishopAttacks = getBishopMoves(king, occupied) & (byPiece[PieceType::BISHOP] | byPiece[PieceType::QUEEN]) & byColor[flip(stm)];
        return !rookAttacks && !bishopAttacks;
    }

    if (type == MOVE_CASTLING) {
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

    if (typeOf(pieces[origin]) == PieceType::KING) {
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
    Bitboard enemyKing = byColor[flip(stm)] & byPiece[PieceType::KING];

    // Direct check
    Bitboard attacks = BB::attackedSquares(typeOf(pieces[origin]), target, occ ^ bitboard(origin) ^ bitboard(target), stm);
    if (attacks & enemyKing)
        return true;

    // Discovered check: Are we blocking a check to the enemy king?
    MoveType type = moveType(move);
    if (blockers[flip(stm)] & bitboard(origin))
        return !(BB::LINE[origin][target] & enemyKing) || type == MOVE_CASTLING;

    switch (type) {
    case MOVE_PROMOTION: {
        PieceType promotionPiece = PROMOTION_PIECE[promotionType(move)];
        return BB::attackedSquares(promotionPiece, target, occ ^ bitboard(origin), stm) & enemyKing;
    }
    case MOVE_CASTLING: {
        return BB::attackedSquares(PieceType::ROOK, Castling::getRookSquare(stm, Castling::getDirection(origin, target)), occ, stm) & enemyKing;
    }
    case MOVE_ENPASSANT: {
        Square epSquare = target - UP[stm];
        Bitboard occupied = (occ ^ bitboard(origin) ^ bitboard(epSquare)) | bitboard(target);
        Square ek = lsb(enemyKing);
        return (BB::attackedSquares(PieceType::ROOK, ek, occupied, stm) & byColor[stm] & (byPiece[PieceType::ROOK] | byPiece[PieceType::QUEEN]))
            | (BB::attackedSquares(PieceType::BISHOP, ek, occupied, stm) & byColor[stm] & (byPiece[PieceType::BISHOP] | byPiece[PieceType::QUEEN]));
    }
    default:
        return false;
    }
}

uint64_t Board::hashAfter(Move move) {
    uint64_t hash = hashes.hash ^ Zobrist::STM_BLACK;

    Square origin = moveOrigin(move);
    Square target = moveTarget(move);
    Square captureTarget = target;

    Piece piece = pieces[origin];
    Piece capturedPiece = pieces[target];

    MoveType type = moveType(move);

    if (enpassantTarget != 0)
        hash ^= Zobrist::ENPASSENT[fileOf(lsb(enpassantTarget))];

    if (typeOf(piece) == PieceType::PAWN) {
        if (type == MOVE_ENPASSANT) {
            capturedPiece = makePiece(PieceType::PAWN, flip(stm));
            captureTarget = target - UP[stm];
        }

        // Double push
        if ((origin ^ target) == 16) {
            Bitboard enemyPawns = byColor[flip(stm)] & byPiece[PieceType::PAWN];
            Bitboard epRank = stm == Color::WHITE ? BB::RANK_4 : BB::RANK_5;
            Square pawnSquare1 = target + 1;
            Square pawnSquare2 = target - 1;
            Bitboard epPawns = (bitboard(pawnSquare1) | bitboard(pawnSquare2)) & epRank & enemyPawns;
            if (epPawns)
                hash ^= Zobrist::ENPASSENT[fileOf(origin)];
        }
    }

    if (type == MOVE_CASTLING) {
        capturedPiece = Piece::NO_PIECE;

        auto direction = Castling::getDirection(origin, target);
        Square rookOrigin = getCastlingRookSquare(stm, direction);
        Square rookTarget = Castling::getRookSquare(stm, direction);
        Square kingTarget = Castling::getKingSquare(stm, direction);

        hash ^= Zobrist::CASTLING[castling];
        hash ^= Zobrist::CASTLING[castling & ~Castling::getMask(stm)];

        Piece kingPiece = makePiece(PieceType::KING, stm);
        Piece rookPiece = makePiece(PieceType::ROOK, stm);
        hash ^= Zobrist::PIECE_SQUARES[kingPiece][origin] ^ Zobrist::PIECE_SQUARES[kingPiece][kingTarget];
        hash ^= Zobrist::PIECE_SQUARES[rookPiece][rookOrigin] ^ Zobrist::PIECE_SQUARES[rookPiece][rookTarget];
    }
    else {
        hash ^= Zobrist::PIECE_SQUARES[piece][origin] ^ Zobrist::PIECE_SQUARES[piece][target];

        if (capturedPiece != Piece::NO_PIECE)
            hash ^= Zobrist::PIECE_SQUARES[capturedPiece][captureTarget];

        if (typeOf(piece) == PieceType::KING) {
            hash ^= Zobrist::CASTLING[castling];
            hash ^= Zobrist::CASTLING[castling & ~Castling::getMask(stm)];
        }

        uint8_t tempCastling = castling;
        if (typeOf(piece) == PieceType::ROOK) {
            hash ^= Zobrist::CASTLING[tempCastling];
            for (auto direction : { Castling::KINGSIDE, Castling::QUEENSIDE }) {
                if (origin == getCastlingRookSquare(stm, direction)) {
                    tempCastling &= ~Castling::getMask(stm, direction);
                }
            }
            hash ^= Zobrist::CASTLING[tempCastling];
        }
        if (capturedPiece != Piece::NO_PIECE && typeOf(capturedPiece) == PieceType::ROOK) {
            hash ^= Zobrist::CASTLING[tempCastling];
            for (auto direction : { Castling::KINGSIDE, Castling::QUEENSIDE }) {
                if (captureTarget == getCastlingRookSquare(flip(stm), direction)) {
                    tempCastling &= ~Castling::getMask(flip(stm), direction);
                }
            }
            hash ^= Zobrist::CASTLING[tempCastling];
        }
    }

    if (type == MOVE_PROMOTION) {
        PieceType promotionPiece = PROMOTION_PIECE[promotionType(move)];
        hash ^= Zobrist::PIECE_SQUARES[piece][target] ^ Zobrist::PIECE_SQUARES[promotionPiece][target];
    }

    return hash;
}

void Board::updateSliderPins(Color side) {
    assert((byColor[side] & byPiece[PieceType::KING]) > 0);

    Square king = lsb(byColor[side] & byPiece[PieceType::KING]);

    blockers[side] = 0;

    Bitboard possiblePinnersRook = getRookMoves(king, bitboard(0)) & (byPiece[PieceType::ROOK] | byPiece[PieceType::QUEEN]);
    Bitboard possiblePinnersBishop = getBishopMoves(king, bitboard(0)) & (byPiece[PieceType::BISHOP] | byPiece[PieceType::QUEEN]);
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
    return attackersTo(s, byColor[Color::WHITE] | byColor[Color::BLACK]);
}

Bitboard Board::attackersTo(Square s, Bitboard occupied) {
    Bitboard sBB = bitboard(s);

    Bitboard pawnAtks = byPiece[PieceType::PAWN] & ((byColor[Color::BLACK] & BB::pawnAttacks(sBB, Color::WHITE)) | (byColor[Color::WHITE] & BB::pawnAttacks(sBB, Color::BLACK)));
    Bitboard knightAtks = byPiece[PieceType::KNIGHT] & BB::KNIGHT_ATTACKS[s];
    Bitboard bishopAtks = (byPiece[PieceType::BISHOP] | byPiece[PieceType::QUEEN]) & getBishopMoves(s, occupied);
    Bitboard rookAtks = (byPiece[PieceType::ROOK] | byPiece[PieceType::QUEEN]) & getRookMoves(s, occupied);
    Bitboard kingAtks = byPiece[PieceType::KING] & BB::KING_ATTACKS[s];
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
            else if ((byColor[Color::WHITE] & byPiece[PieceType::PAWN] & mask) != 0)
                std::cout << "| ♙ ";
            else if ((byColor[Color::BLACK] & byPiece[PieceType::PAWN] & mask) != 0)
                std::cout << "| ♟︎ ";
            else if ((byColor[Color::WHITE] & byPiece[PieceType::KNIGHT] & mask) != 0)
                std::cout << "| ♘ ";
            else if ((byColor[Color::BLACK] & byPiece[PieceType::KNIGHT] & mask) != 0)
                std::cout << "| ♞ ";
            else if ((byColor[Color::WHITE] & byPiece[PieceType::BISHOP] & mask) != 0)
                std::cout << "| ♗ ";
            else if ((byColor[Color::BLACK] & byPiece[PieceType::BISHOP] & mask) != 0)
                std::cout << "| ♝ ";
            else if ((byColor[Color::WHITE] & byPiece[PieceType::ROOK] & mask) != 0)
                std::cout << "| ♖ ";
            else if ((byColor[Color::BLACK] & byPiece[PieceType::ROOK] & mask) != 0)
                std::cout << "| ♜ ";
            else if ((byColor[Color::WHITE] & byPiece[PieceType::QUEEN] & mask) != 0)
                std::cout << "| ♕ ";
            else if ((byColor[Color::BLACK] & byPiece[PieceType::QUEEN] & mask) != 0)
                std::cout << "| ♛ ";
            else if ((byColor[Color::WHITE] & byPiece[PieceType::KING] & mask) != 0)
                std::cout << "| ♔ ";
            else if ((byColor[Color::BLACK] & byPiece[PieceType::KING] & mask) != 0)
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
            else if (typeOf(pieces[idx]) == PieceType::NONE)
                first = 2;
            else if (typeOf(pieces[idx]) == PieceType::PAWN && (byColor[Color::WHITE] & mask) != 0)
                first = 3;
            else if (typeOf(pieces[idx]) == PieceType::PAWN && (byColor[Color::BLACK] & mask) != 0)
                first = 4;
            else if (typeOf(pieces[idx]) == PieceType::KNIGHT && (byColor[Color::WHITE] & mask) != 0)
                first = 5;
            else if (typeOf(pieces[idx]) == PieceType::KNIGHT && (byColor[Color::BLACK] & mask) != 0)
                first = 6;
            else if (typeOf(pieces[idx]) == PieceType::BISHOP && (byColor[Color::WHITE] & mask) != 0)
                first = 7;
            else if (typeOf(pieces[idx]) == PieceType::BISHOP && (byColor[Color::BLACK] & mask) != 0)
                first = 8;
            else if (typeOf(pieces[idx]) == PieceType::ROOK && (byColor[Color::WHITE] & mask) != 0)
                first = 9;
            else if (typeOf(pieces[idx]) == PieceType::ROOK && (byColor[Color::BLACK] & mask) != 0)
                first = 10;
            else if (typeOf(pieces[idx]) == PieceType::QUEEN && (byColor[Color::WHITE] & mask) != 0)
                first = 11;
            else if (typeOf(pieces[idx]) == PieceType::QUEEN && (byColor[Color::BLACK] & mask) != 0)
                first = 12;
            else if (typeOf(pieces[idx]) == PieceType::KING && (byColor[Color::WHITE] & mask) != 0)
                first = 13;
            else if (typeOf(pieces[idx]) == PieceType::KING && (byColor[Color::BLACK] & mask) != 0)
                first = 14;

            if ((enpassantTarget & mask) != 0)
                second = 1;
            else if (((byColor[Color::WHITE] | byColor[Color::BLACK]) & mask) == 0)
                second = 2;
            else if ((byColor[Color::WHITE] & byPiece[PieceType::PAWN] & mask) != 0)
                second = 3;
            else if ((byColor[Color::BLACK] & byPiece[PieceType::PAWN] & mask) != 0)
                second = 4;
            else if ((byColor[Color::WHITE] & byPiece[PieceType::KNIGHT] & mask) != 0)
                second = 5;
            else if ((byColor[Color::BLACK] & byPiece[PieceType::KNIGHT] & mask) != 0)
                second = 6;
            else if ((byColor[Color::WHITE] & byPiece[PieceType::BISHOP] & mask) != 0)
                second = 7;
            else if ((byColor[Color::BLACK] & byPiece[PieceType::BISHOP] & mask) != 0)
                second = 8;
            else if ((byColor[Color::WHITE] & byPiece[PieceType::ROOK] & mask) != 0)
                second = 9;
            else if ((byColor[Color::BLACK] & byPiece[PieceType::ROOK] & mask) != 0)
                second = 10;
            else if ((byColor[Color::WHITE] & byPiece[PieceType::QUEEN] & mask) != 0)
                second = 11;
            else if ((byColor[Color::BLACK] & byPiece[PieceType::QUEEN] & mask) != 0)
                second = 12;
            else if ((byColor[Color::WHITE] & byPiece[PieceType::KING] & mask) != 0)
                second = 13;
            else if ((byColor[Color::BLACK] & byPiece[PieceType::KING] & mask) != 0)
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