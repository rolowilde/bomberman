#ifndef BOMBER_CLIENT_H
#define BOMBER_CLIENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "../common/include/game_state.h"
#include "../common/include/protocol.h"

#define CONTROLS_STR "controls: r (ready), w/a/s/d (move), b (bomb), p (ping), l (lobby), q (quit)"
#define MAX_CLIENT_LOG_COUNT 16
#define MAX_CLIENT_LOG_STRLEN 512

typedef struct {
    int fd;
    uint8_t player_id;
    bool has_welcome;
    bool running;
    game_state_t state;
    uint16_t explosions[MAX_MAP_CELLS]; /* currently drawn explosion radiuses, 0 - no explosion */
    struct timespec ts_last_move;
    char qlog[MAX_CLIENT_LOG_COUNT][MAX_CLIENT_LOG_STRLEN];
    size_t qlog_beg, qlog_end;
    char player_name[MAX_NAME_LEN + 1];
} client_ctx_t;

typedef enum {
    CLIENT_BUILD_COMMAND_ERR_OK = 0,
    CLIENT_BUILD_COMMAND_ERR_FAIL,
    CLIENT_BUILD_COMMAND_ERR_INVALID_INPUT,
    CLIENT_BUILD_COMMAND_ERR_NO_COMMAND,
} client_build_command_err_t;

__attribute__((format(printf, 2, 3))) void qlogf(client_ctx_t *ctx, const char *fmt, ...);

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
