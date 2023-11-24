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

    #define PROMOTION_QUEEN (Move) (0 << 14)
    #define PROMOTION_ROOK (Move) (1 << 14)
    #define PROMOTION_BISHOP (Move) (2 << 14)
    #define PROMOTION_KNIGHT (Move) (3 << 14)

    extern const uint8_t DIRECTIONS[PIECE_TYPES][2];
    extern const int8_t DIRECTION_DELTAS[8];
    extern const Square LASTSQ_TABLE[64][8];

    extern const int8_t UP[2];
    extern const int8_t UP_DOUBLE[2];
    extern const int8_t UP_LEFT[2];
    extern const int8_t UP_RIGHT[2];

    extern const Piece PROMOTION_PIECE[4];

    Square moveOrigin(Move move);
    Square moveTarget(Move move);

    void generateLastSqTable();

    void generateMoves(struct Board* board, Move* moves, int* counter);

    void moveToString(char* string, Move move);

    Square stringToSquare(char* string);
    Move stringToMove(char* string);

#endif