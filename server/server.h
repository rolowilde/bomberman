#ifndef BOMBER_SERVER_H
#define BOMBER_SERVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../common/include/game_state.h"
#include "../common/include/protocol.h"

typedef struct {
    int fd;
    bool active;
    uint8_t id;
    char client_id[MAX_ID_LEN + 1];
    char name[MAX_NAME_LEN + 1];
    uint64_t last_seen_tick;
} server_client_t;

typedef struct {
    bool active;
    uint16_t cell_index;
    uint16_t remaining_ticks;
} explosion_t;

typedef struct {
    int listen_fd;
    uint16_t port;
    bool running;
    game_state_t state;
    server_client_t clients[MAX_PLAYERS];

    uint16_t default_speed;
    uint8_t default_bomb_radius;
    uint16_t default_bomb_timer_ticks;
    uint16_t explosion_duration_ticks;

    bool spawn_defined[MAX_PLAYERS];
    uint16_t spawn_rows[MAX_PLAYERS];
    uint16_t spawn_cols[MAX_PLAYERS];

    explosion_t explosions[MAX_BOMBS];

    uint8_t initial_cells[MAX_MAP_CELLS];
    uint16_t initial_cell_count;

    char server_id[MAX_ID_LEN + 1];
} server_ctx_t;

int server_load_map(server_ctx_t *ctx, const char *path);
int server_run_loop(server_ctx_t *ctx);
int server_handle_client_message(
    server_ctx_t *ctx,
    server_client_t *client,
    const msg_header_t *header,
    const uint8_t *payload,
    size_t payload_len
);

void server_tick(server_ctx_t *ctx);
void server_process_bomb_explosion(server_ctx_t *ctx, size_t bomb_index);

int server_send_to_client(
    server_client_t *client,
    uint8_t msg_type,
    uint8_t sender_id,
    uint8_t target_id,
    const uint8_t *payload,
    size_t payload_len
);

void server_broadcast(
    server_ctx_t *ctx,
    uint8_t msg_type,
    uint8_t sender_id,
    const uint8_t *payload,
    size_t payload_len,
    int except_fd
);

server_client_t *server_client_by_id(server_ctx_t *ctx, uint8_t id);
size_t server_active_client_count(const server_ctx_t *ctx);
size_t server_ready_client_count(const server_ctx_t *ctx);
bool server_cell_blocked(const server_ctx_t *ctx, uint16_t row, uint16_t col);

void server_send_map_to_client(server_ctx_t *ctx, server_client_t *client);
void server_send_sync_to_all(server_ctx_t *ctx);

void server_disconnect_client(server_ctx_t *ctx, server_client_t *client, bool notify);
void server_transition_status(server_ctx_t *ctx, game_status_t status);
void server_reset_round(server_ctx_t *ctx);

#endif
