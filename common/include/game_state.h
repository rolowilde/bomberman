#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "config.h"

typedef struct {
    uint16_t rows;
    uint16_t cols;
    uint8_t *cells;
} map_t;

typedef struct {
    game_status_t status;
    uint64_t tick;
    map_t map;
    player_t players[MAX_PLAYERS];
    bomb_t bombs[MAX_BOMBS];
} game_state_t;

void gs_init(game_state_t *state);
void gs_free(game_state_t *state);

int gs_resize_map(game_state_t *state, uint16_t rows, uint16_t cols);
bool gs_in_bounds(const game_state_t *state, uint16_t row, uint16_t col);

uint8_t gs_cell_get(const game_state_t *state, uint16_t row, uint16_t col);
int gs_cell_set(game_state_t *state, uint16_t row, uint16_t col, uint8_t value);
bool gs_cell_is_walkable(const game_state_t *state, uint16_t row, uint16_t col);

int gs_find_player_slot_by_id(const game_state_t *state, uint8_t player_id);
size_t gs_count_alive_players(const game_state_t *state);
bool gs_has_alive_player_at(const game_state_t *state, uint16_t row, uint16_t col);

int gs_add_bomb(game_state_t *state, const bomb_t *bomb);
void gs_clear_bomb(game_state_t *state, size_t bomb_index);

#endif
