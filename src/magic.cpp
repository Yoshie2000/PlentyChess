#include <vector>
#include <random>
#include <inttypes.h>
#include <chrono>
#include <iostream>

#include "types.h"
#include "magic.h"
#include "move.h"
#include "bitboard.h"

#if defined(USE_BMI2)

MagicEntry ROOK_MAGICS[64] = {};
MagicEntry BISHOP_MAGICS[64] = {};

Bitboard ROOK_MOVES[102400] = { bitboard(0) };
Bitboard BISHOP_MOVES[5248] = { bitboard(0) };

#else

MagicEntry ROOK_MAGICS[64] = {};
MagicEntry BISHOP_MAGICS[64] = {};

Bitboard ROOK_MOVES[64][4096] = { {bitboard(0)} };
Bitboard BISHOP_MOVES[64][4096] = { {bitboard(0)} };

#endif

Bitboard sliderMoves(Piece pieceType, Square origin, Bitboard blockers) {
    Bitboard attacksBB = bitboard(0);

    int8_t delta, direction;
    Square lastSquare, toSquare;
    Bitboard toSquareBB;

    for (direction = DIRECTIONS[pieceType][0]; direction <= DIRECTIONS[pieceType][1]; direction++) {
        delta = DIRECTION_DELTAS[direction];
        lastSquare = LASTSQ_TABLE[origin][direction];

        if (origin != lastSquare)
            for (toSquare = origin + delta; toSquare < 64; toSquare += delta) {
                toSquareBB = bitboard(toSquare);
                attacksBB |= toSquareBB;
                if ((blockers & toSquareBB) || (toSquare == lastSquare))
                    break;
            }
    }
    return attacksBB;
}

Bitboard relevantBlockers(Piece pieceType, Square origin) {
    Bitboard attacksBB = bitboard(0);

    int8_t delta, direction;
    Square lastSquare, toSquare;
    Bitboard toSquareBB;

    for (direction = DIRECTIONS[pieceType][0]; direction <= DIRECTIONS[pieceType][1]; direction++) {
        delta = DIRECTION_DELTAS[direction];
        lastSquare = LASTSQ_TABLE[origin][direction];

        if (origin != lastSquare)
            for (toSquare = origin + delta; toSquare < 64 && toSquare != lastSquare; toSquare += delta) {
                toSquareBB = bitboard(toSquare);
                attacksBB |= toSquareBB;
            }
    }
    return attacksBB;
}

#if defined(USE_BMI2)

// Finds magics for a given piece/square combination
void findMagics(Piece slider, MagicEntry* magicTable, Bitboard* table) {
    for (Square square = 0; square < 64; square++) {
        MagicEntry* outMagicEntry = &magicTable[square];

        Bitboard blockers = bitboard(0);
        Bitboard mask = relevantBlockers(slider, square);

        outMagicEntry->tableIndex = table;
        outMagicEntry->mask = mask;

        for (int index = 0; blockers != bitboard(0) || index == 0; index++) {
            Bitboard moves = sliderMoves(slider, square, blockers);
            if (slider == Piece::ROOK)
                outMagicEntry->tableIndex[magicIndexRook(square, blockers)] = moves;
            else
                outMagicEntry->tableIndex[magicIndexBishop(square, blockers)] = moves;
            blockers = (blockers - mask) & mask;
            table++;
        }
    }
}

#else

// Attempt to fill in a hash table using a magic number.
// Fails if there are any non-constructive collisions.
bool tryMakeTable(Piece slider, Square square, MagicEntry magicEntry, Bitboard* table) {
    for (int i = 0; i < 4096; i++)
        table[i] = bitboard(0);

    Bitboard blockers = bitboard(0);

    // Iterate all configurations of blockers
    for (int index = 0; blockers != bitboard(0) || index == 0; index++) {
        Bitboard moves = sliderMoves(slider, square, blockers);
        Bitboard* tableEntry = &table[magicIndex(magicEntry, blockers)];
        if (*tableEntry == 0) {
            *tableEntry = moves;
        }
        else if (*tableEntry != moves) {
            return false;
        }

        blockers = (blockers - magicEntry.mask) & magicEntry.mask;
    }
    return true;
}

// Finds magics for a given piece/square combination
void findMagic(uint32_t seed, Piece slider, Square square, uint8_t indexBits, MagicEntry* outMagicEntry, Bitboard* table) {
    std::mt19937 rng;
    rng.seed(seed);
    std::uniform_int_distribution<uint64_t> dist;

    Bitboard mask = relevantBlockers(slider, square);
    for (int i = 0; i < 100000000; i++) {
        Bitboard magic = dist(rng) & dist(rng) & dist(rng);
        MagicEntry magicEntry;
        magicEntry.mask = mask;
        magicEntry.magic = magic;
        magicEntry.shift = 64 - indexBits;
        if (tryMakeTable(slider, square, magicEntry, table)) {
            *outMagicEntry = magicEntry;
            return;
        }
    }
    std::cout << "NO MAGICS FOUND FOR SLIDER " << slider << " / SQUARE " << square << std::endl;
    exit(-1);
}

#endif

void generateMagics() {
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

#if defined(USE_BMI2)
    findMagics(Piece::ROOK, ROOK_MAGICS, ROOK_MOVES);
    findMagics(Piece::BISHOP, BISHOP_MAGICS, BISHOP_MOVES);
#else
    uint32_t seed = 2816384844;

    for (Square square = 0; square < 64; square++) {
        findMagic(seed, Piece::ROOK, square, 12, &ROOK_MAGICS[square], ROOK_MOVES[square]);
        findMagic(seed, Piece::BISHOP, square, 9, &BISHOP_MAGICS[square], BISHOP_MOVES[square]);
    }
#endif
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    std::cout << "Generated magics in " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << "ms" << std::endl;
}