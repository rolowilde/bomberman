#include "server.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../common/include/serialization.h"

static void send_error(server_client_t *client, uint8_t sender_id, const char *text) {
    msg_error_t msg;
    uint8_t payload[512];
    size_t payload_len = 0;

    memset(&msg, 0, sizeof(msg));
    strncpy(msg.text, text, MAX_ERROR_LEN);
    msg.text[MAX_ERROR_LEN] = '\0';
    msg.len = (uint16_t)strnlen(msg.text, MAX_ERROR_LEN);

    if (proto_encode_error_payload(&msg, payload, sizeof(payload), &payload_len) == 0) {
        server_send_to_client(client, MSG_ERROR, sender_id, client->id, payload, payload_len);
    }
}

void server_send_map_to_client(server_ctx_t *ctx, server_client_t *client) {
    msg_map_t map_msg;
    uint8_t *payload = NULL;
    size_t payload_len = 0;

    memset(&map_msg, 0, sizeof(map_msg));
    map_msg.rows = (uint8_t)ctx->state.map.rows;
    map_msg.cols = (uint8_t)ctx->state.map.cols;
    map_msg.cell_count = (uint16_t)((uint16_t)map_msg.rows * (uint16_t)map_msg.cols);

    memcpy(map_msg.cells, ctx->state.map.cells, map_msg.cell_count);

    payload = (uint8_t *)malloc(2 + map_msg.cell_count);
    if (payload == NULL) {
        return;
    }

    if (proto_encode_map_payload(&map_msg, payload, 2 + map_msg.cell_count, &payload_len) == 0) {
        server_send_to_client(client, MSG_MAP, SERVER_ENDPOINT_ID, client->id, payload, payload_len);
    }

    free(payload);
}

void server_send_sync_to_all(server_ctx_t *ctx) {
    msg_sync_board_t sync;
    uint8_t payload[128];
    size_t payload_len = 0;
    size_t i;

    memset(&sync, 0, sizeof(sync));
    sync.status = (uint8_t)ctx->state.status;

    for (i = 0; i < MAX_PLAYERS; ++i) {
        if (!ctx->clients[i].active) {
            continue;
        }

        sync.players[sync.player_count].id = ctx->state.players[i].id;
        sync.players[sync.player_count].lives = ctx->state.players[i].lives;
        sync.players[sync.player_count].cell_index =
            make_cell_index(ctx->state.players[i].row, ctx->state.players[i].col, ctx->state.map.cols);
        sync.players[sync.player_count].alive = ctx->state.players[i].alive;
        sync.players[sync.player_count].ready = ctx->state.players[i].ready;
        sync.players[sync.player_count].bomb_count = ctx->state.players[i].bomb_count;
        sync.players[sync.player_count].bomb_radius = ctx->state.players[i].bomb_radius;
        sync.players[sync.player_count].bomb_timer_ticks = ctx->state.players[i].bomb_timer_ticks;
        sync.players[sync.player_count].speed = ctx->state.players[i].speed;
        sync.player_count++;
    }

    if (proto_encode_sync_board_payload(&sync, payload, sizeof(payload), &payload_len) != 0) {
        return;
    }

    for (i = 0; i < MAX_PLAYERS; ++i) {
        if (!ctx->clients[i].active) {
            continue;
        }
        server_send_to_client(
            &ctx->clients[i],
            MSG_SYNC_BOARD,
            SERVER_ENDPOINT_ID,
            ctx->clients[i].id,
            payload,
            payload_len
        );
    }
}

static void send_welcome(server_ctx_t *ctx, server_client_t *client) {
    msg_welcome_t welcome;
    uint8_t payload[512];
    size_t payload_len = 0;
    size_t i;

    memset(&welcome, 0, sizeof(welcome));
    strncpy(welcome.server_id, ctx->server_id, MAX_ID_LEN);
    welcome.server_id[MAX_ID_LEN] = '\0';
    welcome.game_status = (uint8_t)ctx->state.status;

    for (i = 0; i < MAX_PLAYERS; ++i) {
        if (!ctx->clients[i].active || ctx->clients[i].id == client->id) {
            continue;
        }

        welcome.players[welcome.player_count].id = ctx->clients[i].id;
        welcome.players[welcome.player_count].ready = ctx->state.players[i].ready;
        strncpy(welcome.players[welcome.player_count].name, ctx->clients[i].name, MAX_NAME_LEN);
        welcome.players[welcome.player_count].name[MAX_NAME_LEN] = '\0';
        welcome.player_count++;
    }

    if (proto_encode_welcome_payload(&welcome, payload, sizeof(payload), &payload_len) != 0) {
        send_error(client, SERVER_ENDPOINT_ID, "could not encode WELCOME");
        return;
    }

    server_send_to_client(client, MSG_WELCOME, client->id, client->id, payload, payload_len);
}

static bool occupied_by_other_player(const server_ctx_t *ctx, uint8_t self_id, uint16_t row, uint16_t col) {
    size_t i;

    for (i = 0; i < MAX_PLAYERS; ++i) {
        const player_t *player = &ctx->state.players[i];
        if (!ctx->clients[i].active || !player->alive || player->id == self_id) {
            continue;
        }
        if (player->row == row && player->col == col) {
            return true;
        }
    }

    return false;
}

static void maybe_start_game(server_ctx_t *ctx) {
    size_t i;
    size_t active = server_active_client_count(ctx);
    size_t ready = server_ready_client_count(ctx);

    if (ctx->state.status != GAME_LOBBY || active < 2 || ready < active) {
        return;
    }

    for (i = 0; i < MAX_PLAYERS; ++i) {
        if (ctx->clients[i].active) {
            ctx->state.players[i].alive = true;
            ctx->state.players[i].lives = 1;
        }
    }

    server_transition_status(ctx, GAME_RUNNING);

    for (i = 0; i < MAX_PLAYERS; ++i) {
        if (ctx->clients[i].active) {
            server_send_map_to_client(ctx, &ctx->clients[i]);
        }
    }
    server_send_sync_to_all(ctx);

    fprintf(stdout, "game started\n");
}

static int handle_hello(server_ctx_t *ctx, server_client_t *client, const uint8_t *payload, size_t payload_len) {
    msg_hello_t hello;
    size_t slot = (size_t)(client->id - 1);
    size_t i;

    if (proto_decode_hello_payload(&hello, payload, payload_len) != 0) {
        send_error(client, SERVER_ENDPOINT_ID, "invalid HELLO payload");
        return -1;
    }

    for (i = 0; i < MAX_PLAYERS; ++i) {
        if (!ctx->clients[i].active || ctx->clients[i].id == client->id) {
            continue;
        }
        if (strncmp(ctx->clients[i].name, hello.player_name, MAX_NAME_LEN) == 0) {
            send_error(client, SERVER_ENDPOINT_ID, "player name already in use");
            return -1;
        }
    }

    strncpy(client->client_id, hello.client_id, MAX_ID_LEN);
    client->client_id[MAX_ID_LEN] = '\0';

    strncpy(client->name, hello.player_name, MAX_NAME_LEN);
    client->name[MAX_NAME_LEN] = '\0';

    strncpy(ctx->state.players[slot].name, hello.player_name, MAX_NAME_LEN);
    ctx->state.players[slot].name[MAX_NAME_LEN] = '\0';

    send_welcome(ctx, client);
    server_broadcast(ctx, MSG_HELLO, client->id, payload, payload_len, client->fd);

    if (ctx->state.status == GAME_RUNNING) {
        server_send_map_to_client(ctx, client);
        server_send_sync_to_all(ctx);
    }

    return 0;
}

static int handle_set_ready(server_ctx_t *ctx, server_client_t *client) {
    size_t slot = (size_t)(client->id - 1);

    if (!ctx->state.players[slot].ready) {
        ctx->state.players[slot].ready = true;
        server_broadcast(ctx, MSG_SET_READY, client->id, NULL, 0, -1);
    }

    maybe_start_game(ctx);
    return 0;
}

static int handle_move_attempt(server_ctx_t *ctx, server_client_t *client, const uint8_t *payload, size_t payload_len) {
    msg_move_attempt_t move;
    player_t *player;
    int next_row;
    int next_col;
    msg_moved_t moved;
    uint8_t moved_payload[16];
    size_t moved_payload_len = 0;
    uint8_t cell;

    if (proto_decode_move_attempt_payload(&move, payload, payload_len) != 0) {
        send_error(client, SERVER_ENDPOINT_ID, "invalid MOVE_ATTEMPT payload");
        return 0;
    }

    if (move.direction > DIR_RIGHT) {
        send_error(client, SERVER_ENDPOINT_ID, "invalid movement direction");
        return 0;
    }

    player = &ctx->state.players[client->id - 1];
    if (ctx->state.status != GAME_RUNNING || !player->alive) {
        send_error(client, SERVER_ENDPOINT_ID, "cannot move in current state");
        return 0;
    }

    next_row = player->row;
    next_col = player->col;
    if (move.direction == DIR_UP) {
        next_row--;
    } else if (move.direction == DIR_DOWN) {
        next_row++;
    } else if (move.direction == DIR_LEFT) {
        next_col--;
    } else if (move.direction == DIR_RIGHT) {
        next_col++;
    }

    if (next_row < 0 || next_col < 0 || next_row >= (int)ctx->state.map.rows || next_col >= (int)ctx->state.map.cols) {
        send_error(client, SERVER_ENDPOINT_ID, "target cell out of bounds");
        return 0;
    }

    if (server_cell_blocked(ctx, (uint16_t)next_row, (uint16_t)next_col) ||
        occupied_by_other_player(ctx, client->id, (uint16_t)next_row, (uint16_t)next_col)) {
        send_error(client, SERVER_ENDPOINT_ID, "target cell is blocked");
        return 0;
    }

    player->row = (uint16_t)next_row;
    player->col = (uint16_t)next_col;

    moved.player_id = player->id;
    moved.cell_index = make_cell_index(player->row, player->col, ctx->state.map.cols);
    if (proto_encode_moved_payload(&moved, moved_payload, sizeof(moved_payload), &moved_payload_len) == 0) {
        server_broadcast(ctx, MSG_MOVED, client->id, moved_payload, moved_payload_len, -1);
    }

    cell = gs_cell_get(&ctx->state, player->row, player->col);
    if (cell == CELL_BONUS_SPEED || cell == CELL_BONUS_RADIUS || cell == CELL_BONUS_TIMER) {
        msg_bonus_retrieved_t bonus_msg;
        uint8_t bonus_payload[16];
        size_t bonus_payload_len = 0;

        if (cell == CELL_BONUS_SPEED) {
            player->speed++;
        } else if (cell == CELL_BONUS_RADIUS) {
            player->bomb_radius++;
        } else {
            player->bomb_timer_ticks++;
        }

        gs_cell_set(&ctx->state, player->row, player->col, CELL_EMPTY);

        bonus_msg.player_id = player->id;
        bonus_msg.cell_index = make_cell_index(player->row, player->col, ctx->state.map.cols);
        if (proto_encode_bonus_retrieved_payload(&bonus_msg, bonus_payload, sizeof(bonus_payload), &bonus_payload_len) == 0) {
            server_broadcast(ctx, MSG_BONUS_RETRIEVED, client->id, bonus_payload, bonus_payload_len, -1);
        }

        server_send_sync_to_all(ctx);
    }

    return 0;
}

static int handle_bomb_attempt(server_ctx_t *ctx, server_client_t *client, const uint8_t *payload, size_t payload_len) {
    msg_bomb_attempt_t attempt;
    player_t *player;
    uint16_t current_cell;
    size_t i;
    bomb_t bomb;
    msg_bomb_t bomb_msg;
    uint8_t bomb_payload[16];
    size_t bomb_payload_len = 0;

    if (proto_decode_bomb_attempt_payload(&attempt, payload, payload_len) != 0) {
        send_error(client, SERVER_ENDPOINT_ID, "invalid BOMB_ATTEMPT payload");
        return 0;
    }

    player = &ctx->state.players[client->id - 1];
    if (ctx->state.status != GAME_RUNNING || !player->alive) {
        send_error(client, SERVER_ENDPOINT_ID, "cannot place bomb in current state");
        return 0;
    }

    current_cell = make_cell_index(player->row, player->col, ctx->state.map.cols);
    if (attempt.cell_index != current_cell) {
        send_error(client, SERVER_ENDPOINT_ID, "bomb must be placed on current cell");
        return 0;
    }

    if (player->bomb_count == 0) {
        send_error(client, SERVER_ENDPOINT_ID, "no bombs available");
        return 0;
    }

    for (i = 0; i < MAX_BOMBS; ++i) {
        if (ctx->state.bombs[i].owner_id != 0 &&
            ctx->state.bombs[i].row == player->row &&
            ctx->state.bombs[i].col == player->col) {
            send_error(client, SERVER_ENDPOINT_ID, "cell already contains a bomb");
            return 0;
        }
    }

    memset(&bomb, 0, sizeof(bomb));
    bomb.owner_id = player->id;
    bomb.row = player->row;
    bomb.col = player->col;
    bomb.radius = player->bomb_radius;
    bomb.timer_ticks = player->bomb_timer_ticks;

    if (gs_add_bomb(&ctx->state, &bomb) < 0) {
        send_error(client, SERVER_ENDPOINT_ID, "bomb storage full");
        return 0;
    }

    gs_cell_set(&ctx->state, player->row, player->col, CELL_BOMB);
    player->bomb_count--;

    bomb_msg.player_id = player->id;
    bomb_msg.cell_index = current_cell;
    if (proto_encode_bomb_payload(&bomb_msg, bomb_payload, sizeof(bomb_payload), &bomb_payload_len) == 0) {
        server_broadcast(ctx, MSG_BOMB, client->id, bomb_payload, bomb_payload_len, -1);
    }

    server_send_sync_to_all(ctx);

    return 0;
}

static int handle_set_status(server_ctx_t *ctx, server_client_t *client, const uint8_t *payload, size_t payload_len) {
    msg_set_status_t msg;
    size_t i;

    if (proto_decode_set_status_payload(&msg, payload, payload_len) != 0) {
        send_error(client, SERVER_ENDPOINT_ID, "invalid SET_STATUS payload");
        return 0;
    }

    if (ctx->state.status == GAME_END && msg.status == GAME_LOBBY) {
        server_reset_round(ctx);
        server_transition_status(ctx, GAME_LOBBY);
        for (i = 0; i < MAX_PLAYERS; ++i) {
            if (ctx->clients[i].active) {
                server_send_map_to_client(ctx, &ctx->clients[i]);
            }
        }
        server_send_sync_to_all(ctx);
        return 0;
    }

    send_error(client, SERVER_ENDPOINT_ID, "SET_STATUS not allowed in current state");
    return 0;
}

int server_handle_client_message(
    server_ctx_t *ctx,
    server_client_t *client,
    const msg_header_t *header,
    const uint8_t *payload,
    size_t payload_len
) {
    if (header == NULL) {
        return -1;
    }

    switch (header->msg_type) {
        case MSG_HELLO:
            return handle_hello(ctx, client, payload, payload_len);

        case MSG_SET_READY:
            return handle_set_ready(ctx, client);

        case MSG_MOVE_ATTEMPT:
            return handle_move_attempt(ctx, client, payload, payload_len);

        case MSG_BOMB_ATTEMPT:
            return handle_bomb_attempt(ctx, client, payload, payload_len);

        case MSG_SET_STATUS:
            return handle_set_status(ctx, client, payload, payload_len);

        case MSG_SYNC_REQUEST:
            server_send_sync_to_all(ctx);
            return 0;

        case MSG_PING:
            server_send_to_client(client, MSG_PONG, SERVER_ENDPOINT_ID, client->id, NULL, 0);
            return 0;

        case MSG_PONG:
            return 0;

        case MSG_LEAVE:
            return 1;

        default:
            send_error(client, SERVER_ENDPOINT_ID, "unsupported client message type");
            return 0;
    }
}
