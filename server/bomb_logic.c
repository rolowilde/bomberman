#include "server.h"

#include <stdlib.h>

#include "../common/include/serialization.h"

static void broadcast_death(server_ctx_t *ctx, uint8_t player_id) {
    uint8_t payload[16];
    size_t payload_len = 0;
    msg_death_t msg;

    msg.player_id = player_id;
    if (proto_encode_death_payload(&msg, payload, sizeof(payload), &payload_len) == 0) {
        server_broadcast(ctx, MSG_DEATH, SERVER_ENDPOINT_ID, payload, payload_len, -1);
    }
}

static void broadcast_block_destroyed(server_ctx_t *ctx, uint16_t row, uint16_t col) {
    uint8_t payload[16];
    size_t payload_len = 0;
    msg_block_destroyed_t msg;

    msg.cell_index = make_cell_index(row, col, ctx->state.map.cols);
    if (proto_encode_block_destroyed_payload(&msg, payload, sizeof(payload), &payload_len) == 0) {
        server_broadcast(ctx, MSG_BLOCK_DESTROYED, SERVER_ENDPOINT_ID, payload, payload_len, -1);
    }
}

static void maybe_spawn_bonus(server_ctx_t *ctx, uint16_t row, uint16_t col) {
    int roll = rand() % 100;

    if (roll < 25) {
        int pick = (rand() % 3) + 1;
        uint8_t cell = CELL_BONUS_SPEED;
        uint8_t payload[16];
        size_t payload_len = 0;
        msg_bonus_available_t msg;

        if (pick == BONUS_RADIUS) {
            cell = CELL_BONUS_RADIUS;
        } else if (pick == BONUS_TIMER) {
            cell = CELL_BONUS_TIMER;
        }

        gs_cell_set(&ctx->state, row, col, cell);

        msg.bonus_type = (uint8_t)pick;
        msg.cell_index = make_cell_index(row, col, ctx->state.map.cols);
        if (proto_encode_bonus_available_payload(&msg, payload, sizeof(payload), &payload_len) == 0) {
            server_broadcast(ctx, MSG_BONUS_AVAILABLE, SERVER_ENDPOINT_ID, payload, payload_len, -1);
        }
    }
}

static void mark_players_dead_on_cell(server_ctx_t *ctx, uint16_t row, uint16_t col) {
    size_t i;

    for (i = 0; i < MAX_PLAYERS; ++i) {
        server_client_t *client = &ctx->clients[i];
        player_t *player = &ctx->state.players[i];

        if (!client->active || !player->alive) {
            continue;
        }

        if (player->row == row && player->col == col) {
            player->alive = false;
            broadcast_death(ctx, player->id);
        }
    }
}

static void chain_bombs_on_cell(server_ctx_t *ctx, uint16_t row, uint16_t col, size_t except_index) {
    size_t i;

    for (i = 0; i < MAX_BOMBS; ++i) {
        bomb_t *bomb = &ctx->state.bombs[i];
        if (!bomb->active || i == except_index) {
            continue;
        }
        if (bomb->row == row && bomb->col == col && bomb->timer_ticks > 0) {
            bomb->timer_ticks = 0;
        }
    }
}

static void apply_explosion_cell(server_ctx_t *ctx, uint16_t row, uint16_t col, size_t bomb_index, bool *stop_here) {
    uint8_t cell = gs_cell_get(&ctx->state, row, col);

    chain_bombs_on_cell(ctx, row, col, bomb_index);
    mark_players_dead_on_cell(ctx, row, col);

    if (cell == CELL_SOFT) {
        gs_cell_set(&ctx->state, row, col, CELL_EMPTY);
        broadcast_block_destroyed(ctx, row, col);
        maybe_spawn_bonus(ctx, row, col);
        *stop_here = true;
    }
}

void server_process_bomb_explosion(server_ctx_t *ctx, size_t bomb_index) {
    static const int dr[4] = {-1, 1, 0, 0};
    static const int dc[4] = {0, 0, -1, 1};
    bomb_t bomb;
    msg_explosion_start_t start_msg;
    msg_explosion_end_t end_msg;
    uint8_t payload[16];
    size_t payload_len;
    size_t dir;

    if (bomb_index >= MAX_BOMBS || !ctx->state.bombs[bomb_index].active) {
        return;
    }

    bomb = ctx->state.bombs[bomb_index];

    start_msg.radius = bomb.radius;
    start_msg.cell_index = make_cell_index(bomb.row, bomb.col, ctx->state.map.cols);
    payload_len = 0;
    if (proto_encode_explosion_start_payload(&start_msg, payload, sizeof(payload), &payload_len) == 0) {
        server_broadcast(ctx, MSG_EXPLOSION_START, SERVER_ENDPOINT_ID, payload, payload_len, -1);
    }

    apply_explosion_cell(ctx, bomb.row, bomb.col, bomb_index, &(bool){false});

    for (dir = 0; dir < 4; ++dir) {
        uint8_t step;

        for (step = 1; step <= bomb.radius; ++step) {
            int next_row = (int)bomb.row + dr[dir] * (int)step;
            int next_col = (int)bomb.col + dc[dir] * (int)step;
            bool stop_here = false;
            uint8_t cell;

            if (next_row < 0 || next_col < 0 ||
                next_row >= (int)ctx->state.map.rows || next_col >= (int)ctx->state.map.cols) {
                break;
            }

            cell = gs_cell_get(&ctx->state, (uint16_t)next_row, (uint16_t)next_col);
            if (cell == CELL_HARD) {
                break;
            }

            apply_explosion_cell(ctx, (uint16_t)next_row, (uint16_t)next_col, bomb_index, &stop_here);

            if (stop_here) {
                break;
            }
        }
    }

    gs_cell_set(&ctx->state, bomb.row, bomb.col, CELL_EMPTY);
    gs_clear_bomb(&ctx->state, bomb_index);

    {
        int owner_slot = gs_find_player_slot_by_id(&ctx->state, bomb.owner_id);
        if (owner_slot >= 0 && ctx->state.players[owner_slot].bomb_count < 200) {
            ctx->state.players[owner_slot].bomb_count++;
        }
    }

    end_msg.cell_index = make_cell_index(bomb.row, bomb.col, ctx->state.map.cols);
    payload_len = 0;
    if (proto_encode_explosion_end_payload(&end_msg, payload, sizeof(payload), &payload_len) == 0) {
        server_broadcast(ctx, MSG_EXPLOSION_END, SERVER_ENDPOINT_ID, payload, payload_len, -1);
    }
}
