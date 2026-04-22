#ifndef BOMBER_CLIENT_H
#define BOMBER_CLIENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../common/include/game_state.h"
#include "../common/include/protocol.h"

#define CONTROLS_STR "controls: r (ready), w/a/s/d (move), b (bomb), p (ping), l (lobby), q (quit)"

typedef struct {
    int fd;
    uint8_t player_id;
    bool has_welcome;
    bool running;
    game_state_t state;
    char player_name[MAX_NAME_LEN + 1];
} client_ctx_t;

typedef enum {
    CLIENT_BUILD_COMMAND_ERR_OK = 0,
    CLIENT_BUILD_COMMAND_ERR_FAIL,
    CLIENT_BUILD_COMMAND_ERR_INVALID_INPUT,
    CLIENT_BUILD_COMMAND_ERR_NO_COMMAND,
} client_build_command_err_t;

int client_handle_server_message(client_ctx_t *ctx, const msg_header_t *header, const uint8_t *payload,
                                 size_t payload_len);

client_build_command_err_t client_build_command(client_ctx_t *ctx, char c, uint8_t *msg_type, uint8_t *sender_id,
                                                uint8_t *target_id, uint8_t *payload, size_t payload_capacity,
                                                size_t *payload_len, bool *should_quit);

int client_ui_init(void);
int client_ui_deinit(void);
void client_ui_update_screen_size(void);
int client_ui_render(const client_ctx_t *ctx);

#endif
