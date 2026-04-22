#include "client.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "../common/include/serialization.h"

client_build_command_err_t client_build_command(client_ctx_t *ctx, char cmd, uint8_t *msg_type, uint8_t *sender_id,
                                                uint8_t *target_id, uint8_t *payload, size_t payload_capacity,
                                                size_t *payload_len, bool *should_quit) {
    if (ctx == NULL || msg_type == NULL || sender_id == NULL || target_id == NULL || payload == NULL ||
        payload_len == NULL || should_quit == NULL) {
        return CLIENT_BUILD_COMMAND_ERR_FAIL;
    }

    *sender_id = ctx->has_welcome ? ctx->player_id : SERVER_ENDPOINT_ID;
    *target_id = SERVER_ENDPOINT_ID;
    *payload_len = 0;
    *should_quit = false;

    /* ready */
    if (cmd == 'r') {
        if (ctx->state.status != GAME_LOBBY)
            return CLIENT_BUILD_COMMAND_ERR_INVALID_INPUT;

        *msg_type = MSG_SET_READY;
        return CLIENT_BUILD_COMMAND_ERR_OK;
    }

    /* wasd */
    if (cmd == 'w' || cmd == 'a' || cmd == 's' || cmd == 'd') {
        msg_move_attempt_t move;

        if (ctx->state.status != GAME_RUNNING)
            return CLIENT_BUILD_COMMAND_ERR_INVALID_INPUT;

        if (cmd == 'w') {
            move.direction = DIR_UP;
        } else if (cmd == 's') {
            move.direction = DIR_DOWN;
        } else if (cmd == 'a') {
            move.direction = DIR_LEFT;
        } else {
            move.direction = DIR_RIGHT;
        }

        *msg_type = MSG_MOVE_ATTEMPT;
        if (proto_encode_move_attempt_payload(&move, payload, payload_capacity, payload_len) != 0) {
            return CLIENT_BUILD_COMMAND_ERR_FAIL;
        }
        return CLIENT_BUILD_COMMAND_ERR_OK;
    }

    /* bomb */
    if (cmd == 'b') {
        msg_bomb_attempt_t bomb;
        int slot;

        if (!ctx->has_welcome) {
            return CLIENT_BUILD_COMMAND_ERR_FAIL;
        }

        if (ctx->state.status != GAME_RUNNING)
            return CLIENT_BUILD_COMMAND_ERR_INVALID_INPUT;

        slot = gs_find_player_slot_by_id(&ctx->state, ctx->player_id);
        if (slot < 0) {
            return CLIENT_BUILD_COMMAND_ERR_FAIL;
        }

        bomb.cell_index =
            make_cell_index(ctx->state.players[slot].row, ctx->state.players[slot].col, ctx->state.map.cols);

        *msg_type = MSG_BOMB_ATTEMPT;
        if (proto_encode_bomb_attempt_payload(&bomb, payload, payload_capacity, payload_len) != 0) {
            return CLIENT_BUILD_COMMAND_ERR_FAIL;
        }
        return CLIENT_BUILD_COMMAND_ERR_OK;
    }

    /* lobby */
    if (cmd == 'l') {
        msg_set_status_t status;

        if (ctx->state.status == GAME_LOBBY)
            return CLIENT_BUILD_COMMAND_ERR_INVALID_INPUT;

        status.status = GAME_LOBBY;
        *msg_type = MSG_SET_STATUS;
        if (proto_encode_set_status_payload(&status, payload, payload_capacity, payload_len) != 0) {
            return CLIENT_BUILD_COMMAND_ERR_FAIL;
        }
        return CLIENT_BUILD_COMMAND_ERR_OK;
    }

    /* ping */
    if (cmd == 'p') {
        *msg_type = MSG_PING;
        return CLIENT_BUILD_COMMAND_ERR_OK;
    }

    /* quit */
    /* TODO: send MSG_LEAVE in main() after the main loop instead and return CLIENT_BUILD_COMMAND_ERR_NO_COMMAND here */
    if (cmd == 'q') {
        *msg_type = MSG_LEAVE;
        *should_quit = true;
        return CLIENT_BUILD_COMMAND_ERR_OK;
    }

    /* help */
    /* TODO: either remove this or change the behavior: maybe make a permanent help message the visibility of which can
     * be toggled with this (or just send  msg to clien-facing logs) */
    if (cmd == 'h') {
        printf(CONTROLS_STR "\n");
        return CLIENT_BUILD_COMMAND_ERR_NO_COMMAND;
    }

    return CLIENT_BUILD_COMMAND_ERR_INVALID_INPUT;
}
