#include "../include/game_state.h"

#include <stdlib.h>
#include <string.h>

void gs_init(game_state_t *state) {
    size_t i;

    memset(state, 0, sizeof(*state));
    state->status = GAME_LOBBY;

    for (i = 0; i < MAX_PLAYERS; ++i) {
        state->players[i].id = (uint8_t)(i + 1);
        state->players[i].lives = 0;
        state->players[i].alive = false;
        state->players[i].ready = false;
        state->players[i].bomb_count = 1;
        state->players[i].bomb_radius = 1;
        state->players[i].bomb_timer_ticks = (uint16_t)(3 * TICKS_PER_SECOND);
        state->players[i].speed = 3;
    }
}

void gs_free(game_state_t *state) {
    free(state->map.cells);
    state->map.cells = NULL;
    state->map.rows = 0;
    state->map.cols = 0;
}

int gs_resize_map(game_state_t *state, uint16_t rows, uint16_t cols) {
    size_t count;
    uint8_t *cells;

    if (rows == 0 || cols == 0) {
        return -1;
    }

    count = (size_t)rows * (size_t)cols;
    if (count > MAX_MAP_CELLS) {
        return -1;
    }

    cells = (uint8_t *)calloc(count, sizeof(uint8_t));
    if (cells == NULL) {
        return -1;
    }

    memset(cells, CELL_EMPTY, count);

    free(state->map.cells);
    state->map.cells = cells;
    state->map.rows = rows;
    state->map.cols = cols;

    return 0;
}

bool gs_in_bounds(const game_state_t *state, uint16_t row, uint16_t col) {
    return state != NULL && state->map.cells != NULL && row < state->map.rows && col < state->map.cols;
}

uint8_t gs_cell_get(const game_state_t *state, uint16_t row, uint16_t col) {
    if (!gs_in_bounds(state, row, col)) {
        return CELL_HARD;
    }
    return state->map.cells[make_cell_index(row, col, state->map.cols)];
}

int gs_cell_set(game_state_t *state, uint16_t row, uint16_t col, uint8_t value) {
    if (!gs_in_bounds(state, row, col)) {
        return -1;
    }

    state->map.cells[make_cell_index(row, col, state->map.cols)] = value;
    return 0;
}

bool gs_cell_is_walkable(const game_state_t *state, uint16_t row, uint16_t col) {
    uint8_t cell = gs_cell_get(state, row, col);

    return cell == CELL_EMPTY ||
           cell == CELL_BONUS_SPEED ||
           cell == CELL_BONUS_RADIUS ||
           cell == CELL_BONUS_TIMER;
}

int gs_find_player_slot_by_id(const game_state_t *state, uint8_t player_id) {
    size_t i;

    for (i = 0; i < MAX_PLAYERS; ++i) {
        if (state->players[i].id == player_id) {
            return (int)i;
        }
    }

    return -1;
}

size_t gs_count_alive_players(const game_state_t *state) {
    size_t i;
    size_t alive = 0;

    for (i = 0; i < MAX_PLAYERS; ++i) {
        if (state->players[i].alive) {
            alive++;
        }
    }

    return alive;
}

bool gs_has_alive_player_at(const game_state_t *state, uint16_t row, uint16_t col) {
    size_t i;

    for (i = 0; i < MAX_PLAYERS; ++i) {
        if (state->players[i].alive && state->players[i].row == row && state->players[i].col == col) {
            return true;
        }
    }

    return false;
}

int gs_add_bomb(game_state_t *state, const bomb_t *bomb) {
    size_t i;

    for (i = 0; i < MAX_BOMBS; ++i) {
        if (state->bombs[i].owner_id == 0) {
            state->bombs[i] = *bomb;
            return (int)i;
        }
    }

    return -1;
}

void gs_clear_bomb(game_state_t *state, size_t bomb_index) {
    if (bomb_index >= MAX_BOMBS) {
        return;
    }
    memset(&state->bombs[bomb_index], 0, sizeof(state->bombs[bomb_index]));
}
