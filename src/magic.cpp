#include <vector>
#include <random>

#include "types.h"
#include "magic.h"
#include "move.h"

const MagicEntry ROOK_MAGICS[64] = {};
const MagicEntry BISHOP_MAGICS[64] = {};

const Bitboard ROOK_MOVES[64][10] = { C64(0) };
const Bitboard BISHOP_MOVES[64][10] = { C64(0) };

Bitboard sliderMoves(Piece pieceType, Square origin, Bitboard blockers) {
    Bitboard attacksBB = C64(0);

    int8_t delta, direction;
    Square lastSquare, toSquare;
    Bitboard toSquareBB;

    for (direction = DIRECTIONS[pieceType][0]; direction <= DIRECTIONS[pieceType][1]; direction++) {
        delta = DIRECTION_DELTAS[direction];
        lastSquare = LASTSQ_TABLE[origin][direction];

        if (origin != lastSquare)
            for (toSquare = origin + delta; toSquare < 64; toSquare += delta) {
                toSquareBB = C64(1) << toSquare;
                attacksBB |= toSquareBB;
                if ((blockers & toSquareBB) || (toSquare == lastSquare))
                    break;
            }
    }
    return attacksBB;
}

Bitboard relevantBlockers(Piece pieceType, Square origin) {
    Bitboard attacksBB = C64(0);

    int8_t delta, direction;
    Square lastSquare, toSquare;
    Bitboard toSquareBB;

    for (direction = DIRECTIONS[pieceType][0]; direction <= DIRECTIONS[pieceType][1]; direction++) {
        delta = DIRECTION_DELTAS[direction];
        lastSquare = LASTSQ_TABLE[origin][direction];

        if (origin != lastSquare)
            for (toSquare = origin + delta; toSquare < 64 && toSquare != lastSquare; toSquare += delta) {
                toSquareBB = C64(1) << toSquare;
                attacksBB |= toSquareBB;
            }
    }
    return attacksBB;
}

// Attempt to fill in a hash table using a magic number.
// Fails if there are any non-constructive collisions.
bool tryMakeTable(Piece slider, Square square, MagicEntry magicEntry, std::vector<Bitboard>& outTable) {
    outTable.clear();

    Bitboard blockers = C64(0);

    // Iterate all configurations of blockers
    for (int index = 0; blockers != C64(0) || index == 0; index++) {
        Bitboard moves = sliderMoves(slider, square, blockers);
        Bitboard* tableEntry = outTable.data() + magicIndex(magicEntry, blockers);
        if (tableEntry == 0) {
            *tableEntry = moves;
        }
        else if (*tableEntry != moves) {
            // Having two different move sets in the same slot is a hash collision
            return false;
        }

        blockers = (blockers - magicEntry.mask) & magicEntry.mask;
    }
    return true;
}

// Finds magics for a given piece/square combination
void findMagic(Piece slider, Square square, uint8_t indexBits, MagicEntry* outMagicEntry, std::vector<Bitboard> table) {
    std::mt19937 rng;
    rng.seed(12357856);
    std::uniform_int_distribution<uint64_t> dist;

    table.clear();
    table.resize(C64(1) << indexBits);

    Bitboard mask = relevantBlockers(slider, square);
    for (int i = 0; i < 100000000; i++) {
        if (i % 1000000 == 0) printf("\t%d\n", i);
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
    printf("NO MAGICS FOUND %d %d\n", slider, square);
    exit(-1);
}

void generateMagics() {
    printf("Generating magics\n");
    for (Square square = 0; square < 64; square++) {
        MagicEntry rookEntry, bishopEntry;
        std::vector<Bitboard> rookTable, bishopTable;
        // debugBitboard(relevantBlockers(PIECE_ROOK, square));
        // debugBitboard(relevantBlockers(PIECE_BISHOP, square));
        findMagic(PIECE_ROOK, square, 20, &rookEntry, rookTable);
        findMagic(PIECE_BISHOP, square, 11, &bishopEntry, bishopTable);
        printf("Square %d\n", square);
    }
    printf("Generated magics\n");
}