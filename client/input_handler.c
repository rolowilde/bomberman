#include "client.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "../common/include/serialization.h"

int client_build_command(
    client_ctx_t *ctx,
    const char *line,
    uint8_t *msg_type,
    uint8_t *sender_id,
    uint8_t *target_id,
    uint8_t *payload,
    size_t payload_capacity,
    size_t *payload_len,
    bool *should_quit
) {
    char command[64];

    if (ctx == NULL || line == NULL || msg_type == NULL || sender_id == NULL ||
        target_id == NULL || payload == NULL || payload_len == NULL || should_quit == NULL) {
        return -1;
    }

    memset(command, 0, sizeof(command));
    if (sscanf(line, "%63s", command) != 1) {
        return 1;
    }

    command[0] = (char)tolower((unsigned char)command[0]);

    *sender_id = ctx->has_welcome ? ctx->player_id : SERVER_ENDPOINT_ID;
    *target_id = SERVER_ENDPOINT_ID;
    *payload_len = 0;
    *should_quit = false;

    if (strcmp(command, "ready") == 0) {
        *msg_type = MSG_SET_READY;
        return 0;
    }

    if (strcmp(command, "w") == 0 || strcmp(command, "a") == 0 || strcmp(command, "s") == 0 || strcmp(command, "d") == 0) {
        msg_move_attempt_t move;

        if (strcmp(command, "w") == 0) {
            move.direction = DIR_UP;
        } else if (strcmp(command, "s") == 0) {
            move.direction = DIR_DOWN;
        } else if (strcmp(command, "a") == 0) {
            move.direction = DIR_LEFT;
        } else {
            move.direction = DIR_RIGHT;
        }

        *msg_type = MSG_MOVE_ATTEMPT;
        if (proto_encode_move_attempt_payload(&move, payload, payload_capacity, payload_len) != 0) {
            return -1;
        }
        return 0;
    }

    if (strcmp(command, "b") == 0) {
        msg_bomb_attempt_t bomb;
        int slot;

        if (!ctx->has_welcome) {
            return 1;
        }

        slot = gs_find_player_slot_by_id(&ctx->state, ctx->player_id);
        if (slot < 0) {
            return 1;
        }

        bomb.cell_index = make_cell_index(
            ctx->state.players[slot].row,
            ctx->state.players[slot].col,
            ctx->state.map.cols
        );

        *msg_type = MSG_BOMB_ATTEMPT;
        if (proto_encode_bomb_attempt_payload(&bomb, payload, payload_capacity, payload_len) != 0) {
            return -1;
        }
        return 0;
    }

    if (strcmp(command, "lobby") == 0) {
        msg_set_status_t status;

        status.status = GAME_LOBBY;
        *msg_type = MSG_SET_STATUS;
        if (proto_encode_set_status_payload(&status, payload, payload_capacity, payload_len) != 0) {
            return -1;
        }
        return 0;
    }

    if (strcmp(command, "ping") == 0) {
        *msg_type = MSG_PING;
        return 0;
    }

    if (strcmp(command, "quit") == 0) {
        *msg_type = MSG_LEAVE;
        *should_quit = true;
        return 0;
    }

    if (strcmp(command, "help") == 0) {
        printf("commands: ready, w/a/s/d, b, ping, lobby, quit\n");
        return 1;
    }

    printf("unknown command. list command with 'help'.\n");
    return 1;
}
