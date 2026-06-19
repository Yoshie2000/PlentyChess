#pragma once

#include <array>
#include <cstdint>

#include "../types.h"
#include "../bitboard.h"

namespace ThreatGeometry {

    using Bitrays = uint64_t;

    constexpr uint8_t BIT_BLACK_PAWN = 0x01;
    constexpr uint8_t BIT_WHITE_PAWN = 0x02;
    constexpr uint8_t BIT_KNIGHT = 0x04;
    constexpr uint8_t BIT_BISHOP = 0x08;
    constexpr uint8_t BIT_ROOK = 0x10;
    constexpr uint8_t BIT_QUEEN = 0x20;
    constexpr uint8_t BIT_KING = 0x40;

    inline constexpr std::array<std::array<uint8_t, 64>, 64> PERMUTATION_TABLE = [] {
        constexpr int rayRank[8] = {1, 1, 0, -1, -1, -1, 0, 1};
        constexpr int rayFile[8] = {0, 1, 1, 1, 0, -1, -1, -1};
        constexpr int knightRank[8] = {2, 2, 1, -1, -2, -2, -1, 1};
        constexpr int knightFile[8] = {-1, 1, 2, 2, 1, -1, -2, -2};

        std::array<std::array<uint8_t, 64>, 64> table{};
        for (int focus = 0; focus < 64; focus++) {
            int rank = focus / 8, file = focus % 8;
            auto square = [](int r, int f) -> uint8_t {
                return (r >= 0 && r < 8 && f >= 0 && f < 8) ? uint8_t(r * 8 + f) : 0x80;
            };
            for (int dir = 0; dir < 8; dir++) {
                table[focus][dir * 8] = square(rank + knightRank[dir], file + knightFile[dir]);
                for (int dist = 1; dist < 8; dist++)
                    table[focus][dir * 8 + dist] = square(rank + rayRank[dir] * dist, file + rayFile[dir] * dist);
            }
        }
        return table;
    }();

    inline constexpr std::array<uint8_t, 16> PIECE_TO_BIT = {
        BIT_WHITE_PAWN, BIT_KNIGHT, BIT_BISHOP, BIT_ROOK, BIT_QUEEN, BIT_KING, 0, 0,
        BIT_BLACK_PAWN, BIT_KNIGHT, BIT_BISHOP, BIT_ROOK, BIT_QUEEN, BIT_KING, 0, 0,
    };

    inline constexpr std::array<Bitrays, 16> OUTGOING_THREATS = {
        0x0200000000000200ULL, 0x0101010101010101ULL, 0xFE00FE00FE00FE00ULL, 0x00FE00FE00FE00FEULL, 0xFEFEFEFEFEFEFEFEULL, 0, 0, 0,
        0x0000020002000000ULL, 0x0101010101010101ULL, 0xFE00FE00FE00FE00ULL, 0x00FE00FE00FE00FEULL, 0xFEFEFEFEFEFEFEFEULL, 0, 0, 0,
    };

    inline constexpr std::array<uint8_t, 64> INCOMING_THREATS_MASK = [] {
        constexpr uint8_t horse = BIT_KNIGHT;
        constexpr uint8_t orth = BIT_QUEEN | BIT_ROOK;
        constexpr uint8_t diag = BIT_QUEEN | BIT_BISHOP;
        constexpr uint8_t wPawnNear = BIT_WHITE_PAWN | diag;
        constexpr uint8_t bPawnNear = BIT_BLACK_PAWN | diag;
        return std::array<uint8_t, 64>{
            horse, orth, orth, orth, orth, orth, orth, orth,       // N
            horse, bPawnNear, diag, diag, diag, diag, diag, diag,  // NE
            horse, orth, orth, orth, orth, orth, orth, orth,       // E
            horse, wPawnNear, diag, diag, diag, diag, diag, diag,  // SE
            horse, orth, orth, orth, orth, orth, orth, orth,       // S
            horse, wPawnNear, diag, diag, diag, diag, diag, diag,  // SW
            horse, orth, orth, orth, orth, orth, orth, orth,       // W
            horse, bPawnNear, diag, diag, diag, diag, diag, diag,  // NW
        };
    }();

    inline constexpr std::array<uint8_t, 64> INCOMING_SLIDERS_MASK = [] {
        constexpr uint8_t orth = BIT_QUEEN | BIT_ROOK;
        constexpr uint8_t diag = BIT_QUEEN | BIT_BISHOP;
        return std::array<uint8_t, 64>{
            0x80, orth, orth, orth, orth, orth, orth, orth, // N
            0x80, diag, diag, diag, diag, diag, diag, diag, // NE
            0x80, orth, orth, orth, orth, orth, orth, orth, // E
            0x80, diag, diag, diag, diag, diag, diag, diag, // SE
            0x80, orth, orth, orth, orth, orth, orth, orth, // S
            0x80, diag, diag, diag, diag, diag, diag, diag, // SW
            0x80, orth, orth, orth, orth, orth, orth, orth, // W
            0x80, diag, diag, diag, diag, diag, diag, diag, // NW
        };
    }();

}
