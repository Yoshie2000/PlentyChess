#ifndef MOVE_H_INCLUDED
    #define MOVE_H_INCLUDED

    #include <stdint.h>
    
    #include "types.h"
    #include "board.h"

    #define MAX_MOVES 218

    extern const Move MOVE_NULL;

    extern const Move MOVE_PROMOTION;
    extern const Move MOVE_ENPASSANT;
    extern const Move MOVE_CASTLING;

    extern const Move PROMOTION_KNIGHT;
    extern const Move PROMOTION_BISHOP;
    extern const Move PROMOTION_ROOK;
    extern const Move PROMOTION_QUEEN;

    extern const uint8_t DIRECTIONS[PIECE_TYPES][2];
    extern const int8_t DIRECTION_DELTAS[8];
    extern const Square LASTSQ_TABLE[64][8];

    extern const int8_t UP[2];
    extern const int8_t UP_DOUBLE[2];
    extern const int8_t UP_LEFT[2];
    extern const int8_t UP_RIGHT[2];

    Square moveOrigin(Move move);
    Square moveTarget(Move move);

    void generateLastSqTable();

    void generateMoves(struct Board* board, Move* moves, int* counter);

    void moveToString(char* string, Move move);

    Move stringToMove(char* string);

#endif