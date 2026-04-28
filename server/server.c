#include "server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "../common/include/serialization.h"
#include "../common/include/socket_io.h"

static int create_listener(uint16_t port) {
    int fd;
    int opt = 1;
    struct sockaddr_in addr;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(fd);
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }

    if (listen(fd, MAX_PLAYERS) < 0) {
        perror("listen");
        close(fd);
        return -1;
    }

    return fd;
}

static void server_init_context(server_ctx_t *ctx, uint16_t port) {
    memset(ctx, 0, sizeof(*ctx));
    gs_init(&ctx->state);

    ctx->port = port;
    ctx->running = true;
    ctx->default_speed = 3;
    ctx->default_bomb_radius = 1;
    ctx->default_bomb_timer_ticks = (uint16_t)(3 * TICKS_PER_SECOND);
    ctx->explosion_duration_ticks = 3;

    strncpy(ctx->server_id, "bomberman-server/1.0", MAX_ID_LEN);
    ctx->server_id[MAX_ID_LEN] = '\0';
}

static int find_free_client_slot(const server_ctx_t *ctx) {
    size_t i;

    for (i = 0; i < MAX_PLAYERS; ++i) {
        if (!ctx->clients[i].active) {
            return (int)i;
        }
    }

    return -1;
}

static bool map_has_player_at(const server_ctx_t *ctx, uint16_t row, uint16_t col) {
    size_t i;

    for (i = 0; i < MAX_PLAYERS; ++i) {
        if (!ctx->clients[i].active) {
            continue;
        }
        if (ctx->state.players[i].row == row && ctx->state.players[i].col == col) {
            return true;
        }
    }

    return false;
}

static void assign_spawn(server_ctx_t *ctx, size_t slot) {
    player_t *player = &ctx->state.players[slot];
    uint16_t r;
    uint16_t c;

    if (ctx->spawn_defined[slot]) {
        player->row = ctx->spawn_rows[slot];
        player->col = ctx->spawn_cols[slot];
        return;
    }

    for (r = 0; r < ctx->state.map.rows; ++r) {
        for (c = 0; c < ctx->state.map.cols; ++c) {
            if (gs_cell_is_walkable(&ctx->state, r, c) && !map_has_player_at(ctx, r, c)) {
                player->row = r;
                player->col = c;
                return;
            }
        }
    }

    player->row = 0;
    player->col = 0;
}

int server_send_to_client(
    server_client_t *client,
    uint8_t msg_type,
    uint8_t sender_id,
    uint8_t target_id,
    const uint8_t *payload,
    size_t payload_len
) {
    if (client == NULL || !client->active) {
        return -1;
    }
    return sock_send_message(client->fd, msg_type, sender_id, target_id, payload, payload_len);
}

void server_broadcast(
    server_ctx_t *ctx,
    uint8_t msg_type,
    uint8_t sender_id,
    const uint8_t *payload,
    size_t payload_len,
    int except_fd
) {
    size_t i;

    for (i = 0; i < MAX_PLAYERS; ++i) {
        if (!ctx->clients[i].active || ctx->clients[i].fd == except_fd) {
            continue;
        }
        server_send_to_client(&ctx->clients[i], msg_type, sender_id, BROADCAST_ENDPOINT_ID, payload, payload_len);
    }
}

server_client_t *server_client_by_id(server_ctx_t *ctx, uint8_t id) {
    size_t i;

    for (i = 0; i < MAX_PLAYERS; ++i) {
        if (ctx->clients[i].active && ctx->clients[i].id == id) {
            return &ctx->clients[i];
        }
    }

    return NULL;
}

size_t server_active_client_count(const server_ctx_t *ctx) {
    size_t i;
    size_t count = 0;

    for (i = 0; i < MAX_PLAYERS; ++i) {
        if (ctx->clients[i].active) {
            count++;
        }
    }

    return count;
}

size_t server_ready_client_count(const server_ctx_t *ctx) {
    size_t i;
    size_t count = 0;

    for (i = 0; i < MAX_PLAYERS; ++i) {
        if (ctx->clients[i].active && ctx->state.players[i].ready) {
            count++;
        }
    }

    return count;
}

bool server_cell_blocked(const server_ctx_t *ctx, uint16_t row, uint16_t col) {
    size_t i;

    if (!gs_in_bounds(&ctx->state, row, col)) {
        return true;
    }

    if (!gs_cell_is_walkable(&ctx->state, row, col)) {
        return true;
    }

    for (i = 0; i < MAX_BOMBS; ++i) {
        if (ctx->state.bombs[i].owner_id != 0 && ctx->state.bombs[i].row == row && ctx->state.bombs[i].col == col) {
            return true;
        }
    }

    return false;
}

void server_transition_status(server_ctx_t *ctx, game_status_t status) {
    msg_set_status_t msg;
    uint8_t payload[8];
    size_t payload_len = 0;

    if (ctx->state.status == status) {
        return;
    }

    ctx->state.status = status;

    msg.status = (uint8_t)status;
    if (proto_encode_set_status_payload(&msg, payload, sizeof(payload), &payload_len) == 0) {
        server_broadcast(ctx, MSG_SET_STATUS, SERVER_ENDPOINT_ID, payload, payload_len, -1);
    }
}

void server_reset_round(server_ctx_t *ctx) {
    size_t i;

    if (ctx->initial_cell_count > 0) {
        memcpy(ctx->state.map.cells, ctx->initial_cells, ctx->initial_cell_count);
    }

    for (i = 0; i < MAX_BOMBS; ++i) {
        gs_clear_bomb(&ctx->state, i);
    }

    for (i = 0; i < MAX_BOMBS; ++i) {
        ctx->explosions[i].active = false;
        ctx->explosions[i].cell_index = 0;
        ctx->explosions[i].remaining_ticks = 0;
    }

    for (i = 0; i < MAX_PLAYERS; ++i) {
        player_t *player = &ctx->state.players[i];

        player->ready = false;
        player->alive = false;
        player->lives = 0;
        player->bomb_count = 1;
        player->bomb_radius = ctx->default_bomb_radius;
        player->bomb_timer_ticks = ctx->default_bomb_timer_ticks;
        player->speed = ctx->default_speed;
        if (ctx->spawn_defined[i]) {
            player->row = ctx->spawn_rows[i];
            player->col = ctx->spawn_cols[i];
        }
    }
}

void server_disconnect_client(server_ctx_t *ctx, server_client_t *client, bool notify) {
    int slot;

    if (client == NULL || !client->active) {
        return;
    }

    slot = client->id - 1;

    if (notify) {
        server_broadcast(ctx, MSG_LEAVE, client->id, NULL, 0, client->fd);
    }

    close(client->fd);
    client->fd = -1;
    client->active = false;
    client->client_id[0] = '\0';
    client->name[0] = '\0';

    ctx->state.players[slot].alive = false;
    ctx->state.players[slot].ready = false;
    ctx->state.players[slot].lives = 0;

    fprintf(stdout, "client disconnected, slot=%d, id=%u\n", slot, client->id);
}

static void accept_connection(server_ctx_t *ctx) {
    int fd;
    struct sockaddr_in peer_addr;
    socklen_t peer_len = sizeof(peer_addr);
    int slot;
    server_client_t *client;
    player_t *player;

    fd = accept(ctx->listen_fd, (struct sockaddr *)&peer_addr, &peer_len);
    if (fd < 0) {
        if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("accept");
        }
        return;
    }

    slot = find_free_client_slot(ctx);
    if (slot < 0) {
        sock_send_message(fd, MSG_DISCONNECT, SERVER_ENDPOINT_ID, SERVER_ENDPOINT_ID, NULL, 0);
        close(fd);
        return;
    }

    client = &ctx->clients[slot];
    player = &ctx->state.players[slot];

    memset(client, 0, sizeof(*client));
    client->fd = fd;
    client->active = true;
    client->id = (uint8_t)(slot + 1);

    player->id = client->id;
    player->ready = false;
    player->alive = false;
    player->lives = 0;
    player->bomb_count = 1;
    player->bomb_radius = ctx->default_bomb_radius;
    player->bomb_timer_ticks = ctx->default_bomb_timer_ticks;
    player->speed = ctx->default_speed;

    assign_spawn(ctx, (size_t)slot);

    fprintf(stdout, "client connected, slot=%d, id=%u\n", slot, client->id);
}

int server_run_loop(server_ctx_t *ctx) {
    const int tick_timeout_us = 1000000 / TICKS_PER_SECOND;

    while (ctx->running) {
        fd_set readfds;
        int max_fd = ctx->listen_fd;
        struct timeval timeout;
        int rv;
        size_t i;

        FD_ZERO(&readfds);
        FD_SET(ctx->listen_fd, &readfds);

        for (i = 0; i < MAX_PLAYERS; ++i) {
            if (ctx->clients[i].active) {
                FD_SET(ctx->clients[i].fd, &readfds);
                if (ctx->clients[i].fd > max_fd) {
                    max_fd = ctx->clients[i].fd;
                }
            }
        }

        timeout.tv_sec = 0;
        timeout.tv_usec = tick_timeout_us;

        rv = select(max_fd + 1, &readfds, NULL, NULL, &timeout);
        if (rv < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("select");
            return -1;
        }

        if (FD_ISSET(ctx->listen_fd, &readfds)) {
            accept_connection(ctx);
        }

        for (i = 0; i < MAX_PLAYERS; ++i) {
            server_client_t *client = &ctx->clients[i];
            msg_header_t header;
            uint8_t payload[65536];
            size_t payload_len = 0;
            int recv_status;
            int handle_status;

            if (!client->active || !FD_ISSET(client->fd, &readfds)) {
                continue;
            }

            recv_status = sock_recv_header(client->fd, &header, 1000);
            if (recv_status != 0) {
                server_disconnect_client(ctx, client, true);
                continue;
            }

            if (sock_recv_payload_by_type(client->fd, header.msg_type, payload, sizeof(payload), &payload_len, 1000) != 0) {
                server_disconnect_client(ctx, client, true);
                continue;
            }

            handle_status = server_handle_client_message(ctx, client, &header, payload, payload_len);
            if (handle_status != 0) {
                server_disconnect_client(ctx, client, true);
                continue;
            }

            client->last_seen_tick = ctx->state.tick;
        }

        server_tick(ctx);
    }

    return 0;
}

int main(int argc, char **argv) {
    server_ctx_t ctx;
    long port;

    if (argc < 3) {
        fprintf(stderr, "usage: %s <port> <map_file>\n", argv[0]);
        return 1;
    }

    port = strtol(argv[1], NULL, 10);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "invalid port\n");
        return 1;
    }

    srand((unsigned int)time(NULL));

    server_init_context(&ctx, (uint16_t)port);

    if (server_load_map(&ctx, argv[2]) != 0) {
        gs_free(&ctx.state);
        return 1;
    }

    ctx.listen_fd = create_listener((uint16_t)port);
    if (ctx.listen_fd < 0) {
        gs_free(&ctx.state);
        return 1;
    }

    fprintf(stdout, "server listening on port %ld\n", port);

    if (server_run_loop(&ctx) != 0) {
        close(ctx.listen_fd);
        gs_free(&ctx.state);
        return 1;
    }

    close(ctx.listen_fd);
    gs_free(&ctx.state);
    return 0;
}
