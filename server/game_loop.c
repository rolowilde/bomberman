#include "server.h"

#include <stdio.h>
#include <string.h>

#include "../common/include/serialization.h"

static void maybe_finish_round(server_ctx_t *ctx) {
    size_t i;
    size_t alive_count = 0;
    uint8_t winner_id = 0;
    const char *winner_name = NULL;

    if (ctx->state.status != GAME_RUNNING) {
        return;
    }

    for (i = 0; i < MAX_PLAYERS; ++i) {
        const server_client_t *client = &ctx->clients[i];
        const player_t *player = &ctx->state.players[i];
        if (client->active && player->alive) {
            alive_count++;
            winner_id = player->id;
            winner_name = player->name;
        }
    }

    if (alive_count <= 1 && server_active_client_count(ctx) >= 2) {
        uint8_t payload[16];
        size_t payload_len = 0;
        msg_winner_t winner;

        server_transition_status(ctx, GAME_END);

        if (alive_count == 1) {
            winner.winner_id = winner_id;
            if (proto_encode_winner_payload(&winner, payload, sizeof(payload), &payload_len) == 0) {
                server_broadcast(ctx, MSG_WINNER, SERVER_ENDPOINT_ID, payload, payload_len, -1);
            }

            fprintf(stdout, "winner id=%u", winner_id);
            if (winner_name != NULL && strlen(winner_name) > 0) {
                fprintf(stdout, " name=%s", winner_name);
            }
            fprintf(stdout, "\n");
        }

        fprintf(stdout, "round finished\n");
    }
}

void server_tick(server_ctx_t *ctx) {
    size_t i;
    bool exploded;

    ctx->state.tick++;

    if (ctx->state.status != GAME_RUNNING) {
        return;
    }

    for (i = 0; i < MAX_BOMBS; ++i) {
        if (ctx->state.bombs[i].active && ctx->state.bombs[i].timer_ticks > 0) {
            ctx->state.bombs[i].timer_ticks--;
        }
    }

    do {
        exploded = false;
        for (i = 0; i < MAX_BOMBS; ++i) {
            if (ctx->state.bombs[i].active && ctx->state.bombs[i].timer_ticks == 0) {
                server_process_bomb_explosion(ctx, i);
                exploded = true;
            }
        }
    } while (exploded);

    maybe_finish_round(ctx);
}
