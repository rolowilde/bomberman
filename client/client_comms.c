#include "../common/include/serialization.h"
#include "../common/include/socket_io.h"
#include "client.h"
#include "config.h"
#include <stdio.h>
#include <string.h>

static void apply_map_payload(client_ctx_t *ctx, const msg_map_t *map_msg) {
    if (gs_resize_map(&ctx->state, map_msg->rows, map_msg->cols) != 0) {
        return;
    }
    memcpy(ctx->state.map.cells, map_msg->cells, map_msg->cell_count);
}

int client_handle_server_message(client_ctx_t *ctx, const msg_header_t *header, const uint8_t *payload,
                                 size_t payload_len) {
    if (ctx == NULL || header == NULL) {
        return -1;
    }

    switch (header->msg_type) {
    case MSG_HELLO: {
        msg_hello_t hello;
        if (proto_decode_hello_payload(&hello, payload, payload_len) == 0) {
            if (header->sender_id >= 1 && header->sender_id <= MAX_PLAYERS) {
                player_t *player = &ctx->state.players[header->sender_id - 1];
                player->id = header->sender_id;
                strncpy(player->name, hello.player_name, MAX_NAME_LEN);
                player->name[MAX_NAME_LEN] = '\0';
            }
            qlogf(ctx, "[server] player joined: %s", hello.player_name);
        }
        break;
    }

    case MSG_WELCOME: {
        msg_welcome_t welcome;
        if (proto_decode_welcome_payload(&welcome, payload, payload_len) != 0) {
            fprintf(stderr, "invalid WELCOME payload\n");
            return -1;
        }

        ctx->player_id = header->sender_id;
        ctx->has_welcome = true;
        ctx->state.status = (game_status_t)welcome.game_status;

        if (ctx->player_id >= 1 && ctx->player_id <= MAX_PLAYERS) {
            player_t *self = &ctx->state.players[ctx->player_id - 1];
            self->id = ctx->player_id;
            strncpy(self->name, ctx->player_name, MAX_NAME_LEN);
            self->name[MAX_NAME_LEN] = '\0';
        }

        for (size_t i = 0; i < welcome.player_count; ++i) {
            uint8_t id = welcome.players[i].id;
            if (id >= 1 && id <= MAX_PLAYERS) {
                player_t *player = &ctx->state.players[id - 1];
                player->id = id;
                player->ready = welcome.players[i].ready;
                strncpy(player->name, welcome.players[i].name, MAX_NAME_LEN);
                player->name[MAX_NAME_LEN] = '\0';
            }
        }

        qlogf(ctx, "[server] welcome, assigned id=%u status=%u", ctx->player_id, welcome.game_status);
        break;
    }

    case MSG_SET_STATUS: {
        msg_set_status_t status;
        if (proto_decode_set_status_payload(&status, payload, payload_len) == 0) {
            ctx->state.status = (game_status_t)status.status;
            qlogf(ctx, "[server] status changed to %u", status.status);
        }
        break;
    }

    case MSG_SET_READY:
        if (header->sender_id >= 1 && header->sender_id <= MAX_PLAYERS) {
            player_t *player = &ctx->state.players[header->sender_id - 1];
            player->id = header->sender_id;
            player->ready = true;
            qlogf(ctx, "[server] player %u is ready", header->sender_id);
        }
        break;

    case MSG_MAP: {
        msg_map_t map_msg;
        if (proto_decode_map_payload(&map_msg, payload, payload_len) == 0) {
            apply_map_payload(ctx, &map_msg);
        }
        break;
    }

    case MSG_SYNC_BOARD: {
        msg_sync_board_t sync;
        if (proto_decode_sync_board_payload(&sync, payload, payload_len) == 0) {
            size_t i;

            ctx->state.status = (game_status_t)sync.status;
            for (i = 0; i < MAX_PLAYERS; ++i) {
                ctx->state.players[i].alive = false;
                ctx->state.players[i].ready = false;
            }

            for (i = 0; i < sync.player_count; ++i) {
                uint8_t id = sync.players[i].id;
                uint16_t row;
                uint16_t col;
                player_t *player;
                if (id < 1 || id > MAX_PLAYERS || ctx->state.map.cols == 0) {
                    continue;
                }
                player = &ctx->state.players[id - 1];
                player->id = id;
                split_cell_index(sync.players[i].cell_index, ctx->state.map.cols, &row, &col);
                player->row = row;
                player->col = col;
                player->alive = sync.players[i].alive;
                player->ready = sync.players[i].ready;
                player->lives = sync.players[i].lives;
                player->bomb_count = sync.players[i].bomb_count;
                player->bomb_radius = sync.players[i].bomb_radius;
                player->bomb_timer_ticks = sync.players[i].bomb_timer_ticks;
                player->speed = sync.players[i].speed;
            }
        }
        break;
    }

    case MSG_MOVED: {
        msg_moved_t moved;
        if (proto_decode_moved_payload(&moved, payload, payload_len) == 0 && moved.player_id >= 1 &&
            moved.player_id <= MAX_PLAYERS && ctx->state.map.cols > 0) {
            uint16_t row;
            uint16_t col;
            split_cell_index(moved.cell_index, ctx->state.map.cols, &row, &col);
            ctx->state.players[moved.player_id - 1].row = row;
            ctx->state.players[moved.player_id - 1].col = col;
        }
        break;
    }

    case MSG_BOMB: {
        msg_bomb_t bomb;
        if (proto_decode_bomb_payload(&bomb, payload, payload_len) == 0 && ctx->state.map.cols > 0) {
            uint16_t row;
            uint16_t col;
            split_cell_index(bomb.cell_index, ctx->state.map.cols, &row, &col);
            gs_cell_set(&ctx->state, row, col, CELL_BOMB);
        }
        break;
    }

    case MSG_EXPLOSION_START: {
        msg_explosion_start_t explosion;
        if (proto_decode_explosion_start_payload(&explosion, payload, payload_len) == 0) {
            uint16_t row;
            uint16_t col;
            fprintf(stderr, "[server] explosion start at cell=%u radius=%u\n", explosion.cell_index, explosion.radius);
            split_cell_index(explosion.cell_index, ctx->state.map.cols, &row, &col);
            ctx->explosions[row * MAX_MAP_SIDE + col] = explosion.radius;
        }
        break;
    }

    case MSG_EXPLOSION_END: {
        msg_explosion_end_t explosion;
        if (proto_decode_explosion_end_payload(&explosion, payload, payload_len) == 0 && ctx->state.map.cols > 0) {
            uint16_t row;
            uint16_t col;
            fprintf(stderr, "[server] explosion end at cell=%u\n", explosion.cell_index);
            split_cell_index(explosion.cell_index, ctx->state.map.cols, &row, &col);
            gs_cell_set(&ctx->state, row, col, CELL_EMPTY);
            ctx->explosions[row * MAX_MAP_SIDE + col] = 0;
        }
        break;
    }

    case MSG_DEATH: {
        msg_death_t death;
        if (proto_decode_death_payload(&death, payload, payload_len) == 0 && death.player_id >= 1 &&
            death.player_id <= MAX_PLAYERS) {
            ctx->state.players[death.player_id - 1].alive = false;
            ctx->state.players[death.player_id - 1].lives = 0;
            qlogf(ctx, "[server] player %u died", death.player_id);
        }
        break;
    }

    case MSG_BONUS_AVAILABLE: {
        msg_bonus_available_t bonus;
        if (proto_decode_bonus_available_payload(&bonus, payload, payload_len) == 0 && ctx->state.map.cols > 0) {
            uint16_t row;
            uint16_t col;
            uint8_t cell = CELL_BONUS_SPEED;
            if (bonus.bonus_type == BONUS_RADIUS) {
                cell = CELL_BONUS_RADIUS;
            } else if (bonus.bonus_type == BONUS_TIMER) {
                cell = CELL_BONUS_TIMER;
            }
            split_cell_index(bonus.cell_index, ctx->state.map.cols, &row, &col);
            gs_cell_set(&ctx->state, row, col, cell);
        }
        break;
    }

    case MSG_BONUS_RETRIEVED: {
        msg_bonus_retrieved_t bonus;
        if (proto_decode_bonus_retrieved_payload(&bonus, payload, payload_len) == 0 && ctx->state.map.cols > 0) {
            uint16_t row;
            uint16_t col;
            split_cell_index(bonus.cell_index, ctx->state.map.cols, &row, &col);
            gs_cell_set(&ctx->state, row, col, CELL_EMPTY);
        }
        break;
    }

    case MSG_BLOCK_DESTROYED: {
        msg_block_destroyed_t block;
        if (proto_decode_block_destroyed_payload(&block, payload, payload_len) == 0 && ctx->state.map.cols > 0) {
            uint16_t row;
            uint16_t col;
            split_cell_index(block.cell_index, ctx->state.map.cols, &row, &col);
            gs_cell_set(&ctx->state, row, col, CELL_EMPTY);
        }
        break;
    }

    case MSG_WINNER: {
        msg_winner_t winner;
        if (proto_decode_winner_payload(&winner, payload, payload_len) == 0) {
            qlogf(ctx, "[server] winner is player %u", winner.winner_id);
        }
        break;
    }

    case MSG_ERROR: {
        msg_error_t err;
        if (proto_decode_error_payload(&err, payload, payload_len) == 0) {
            qlogf(ctx, "[server][error] %s", err.text);
        }
        break;
    }

    case MSG_DISCONNECT:
        qlogf(ctx, "[server] disconnect requested");
        ctx->running = false;
        return 1;

    case MSG_LEAVE:
        if (header->sender_id >= 1 && header->sender_id <= MAX_PLAYERS) {
            size_t slot = (size_t)(header->sender_id - 1);
            ctx->state.players[slot].alive = false;
            ctx->state.players[slot].ready = false;
            ctx->state.players[slot].name[0] = '\0';
            qlogf(ctx, "[server] player %u left", header->sender_id);
        }
        break;

    case MSG_PING:
        sock_send_message(ctx->fd, MSG_PONG, ctx->player_id, SERVER_ENDPOINT_ID, NULL, 0);
        break;

    case MSG_PONG:
        qlogf(ctx, "[server] pong");
        break;

    default:
        qlogf(ctx, "[server] unhandled message type=%u", header->msg_type);
        break;
    }

    return 0;
}
