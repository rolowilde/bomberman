#ifndef GAME_PROTOCOL_H
#define GAME_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "config.h"

typedef struct {
    uint8_t msg_type;
    uint8_t sender_id;
    uint8_t target_id;
} msg_header_t;

typedef struct {
    char client_id[MAX_ID_LEN + 1];
    char player_name[MAX_NAME_LEN + 1];
} msg_hello_t;

typedef struct {
    uint8_t id;
    bool ready;
    char name[MAX_NAME_LEN + 1];
} msg_player_summary_t;

typedef struct {
    char server_id[MAX_ID_LEN + 1];
    uint8_t game_status;
    uint8_t player_count;
    msg_player_summary_t players[MAX_PLAYERS];
} msg_welcome_t;

typedef struct {
    uint8_t status;
} msg_set_status_t;

typedef struct {
    uint8_t direction;
} msg_move_attempt_t;

typedef struct {
    uint8_t player_id;
    uint16_t cell_index;
} msg_moved_t;

typedef struct {
    uint16_t cell_index;
} msg_bomb_attempt_t;

typedef struct {
    uint8_t player_id;
    uint16_t cell_index;
} msg_bomb_t;

typedef struct {
    uint8_t radius;
    uint16_t cell_index;
} msg_explosion_start_t;

typedef struct {
    uint16_t cell_index;
} msg_explosion_end_t;

typedef struct {
    uint8_t player_id;
} msg_death_t;

typedef struct {
    uint8_t bonus_type;
    uint16_t cell_index;
} msg_bonus_available_t;

typedef struct {
    uint8_t player_id;
    uint16_t cell_index;
} msg_bonus_retrieved_t;

typedef struct {
    uint16_t cell_index;
} msg_block_destroyed_t;

typedef struct {
    uint8_t winner_id;
} msg_winner_t;

typedef struct {
    uint16_t len;
    char text[MAX_ERROR_LEN + 1];
} msg_error_t;

typedef struct {
    uint8_t rows;
    uint8_t cols;
    uint16_t cell_count;
    uint8_t cells[MAX_MAP_CELLS];
} msg_map_t;

typedef struct {
    uint8_t id;
    uint16_t cell_index;
    bool alive;
    bool ready;
} msg_sync_player_t;

typedef struct {
    uint8_t status;
    uint8_t player_count;
    msg_sync_player_t players[MAX_PLAYERS];
} msg_sync_board_t;

const char *msg_type_name(uint8_t msg_type);
bool msg_type_variable_payload(uint8_t msg_type);
size_t msg_type_fixed_payload_size(uint8_t msg_type);

#endif
