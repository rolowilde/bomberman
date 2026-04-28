#include "client.h"

#include <bits/time.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../common/include/serialization.h"
#include "config.h"

#define TIMESPEC_FS "%ld.%09ld"
#define TIMESPEC_FARGS(_ts) (long)_ts.tv_sec, (long)_ts.tv_nsec

static int ts_cmp(const struct timespec a, const struct timespec b) {
    if (a.tv_sec < b.tv_sec)
        return -1;
    if (a.tv_sec > b.tv_sec)
        return 1;

    if (a.tv_nsec < b.tv_nsec)
        return -1;
    if (a.tv_nsec > b.tv_nsec)
        return 1;

    return 0;
}

static struct timespec ts_norm(struct timespec ts) {
#define SECOND_NS 1000000000L
    if (ts.tv_nsec >= SECOND_NS) {
        ts.tv_sec += ts.tv_nsec / SECOND_NS;
        ts.tv_nsec %= SECOND_NS;
    } else if (ts.tv_nsec < 0) {
        long sec = (-ts.tv_nsec / SECOND_NS) + 1;
        ts.tv_sec -= sec;
        ts.tv_nsec += sec * SECOND_NS;
    }

    return ts;
#undef SECOND_NS
}

static struct timespec ts_diff(struct timespec a, struct timespec b) {
    a.tv_sec -= b.tv_sec;
    a.tv_nsec -= b.tv_nsec;

    return ts_norm(a);
}

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
        struct timespec ts;
        int speed = ctx->state.players[ctx->player_id - 1].speed;
        struct timespec ts_min_interval = {.tv_sec = 0, .tv_nsec = 1000000000 / (speed == 0 ? 1 : speed)};
        struct timespec ts_passed;

        if (ctx->state.status != GAME_RUNNING)
            return CLIENT_BUILD_COMMAND_ERR_INVALID_INPUT;

        if (speed == 0) {
            qlogf(ctx, "speed is 0");
            return CLIENT_BUILD_COMMAND_ERR_INVALID_INPUT;
        }

        clock_gettime(CLOCK_MONOTONIC, &ts);
        ts_passed = ts_diff(ts, ctx->ts_last_move);

        if (ts_cmp(ts_passed, ts_min_interval) < 0) {
            fprintf(stderr, "throttling movement, min intereval: " TIMESPEC_FS "s, passed: " TIMESPEC_FS "s\n",
                  TIMESPEC_FARGS(ts_min_interval), TIMESPEC_FARGS(ts_passed));
            return CLIENT_BUILD_COMMAND_ERR_INVALID_INPUT;
        }

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

        ctx->ts_last_move = ts;

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
    if (cmd == 'q') {
        *should_quit = true;
        return CLIENT_BUILD_COMMAND_ERR_NO_COMMAND;
    }

    /* help */
    if (cmd == 'h') {
        qlogf(ctx, CONTROLS_STR);
        return CLIENT_BUILD_COMMAND_ERR_NO_COMMAND;
    }

    return CLIENT_BUILD_COMMAND_ERR_INVALID_INPUT;
}
