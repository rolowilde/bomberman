#ifndef BOMBER_CLIENT_H
#define BOMBER_CLIENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../common/include/game_state.h"
#include "../common/include/protocol.h"

typedef struct {
    int fd;
    uint8_t player_id;
    bool has_welcome;
    bool running;
    game_state_t state;
    char player_name[MAX_NAME_LEN + 1];
} client_ctx_t;

int client_handle_server_message(
    client_ctx_t *ctx,
    const msg_header_t *header,
    const uint8_t *payload,
    size_t payload_len
);

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
);

void client_render_state(const client_ctx_t *ctx);

#endif
