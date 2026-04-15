#include "client.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "../common/include/serialization.h"
#include "../common/include/socket_io.h"

static int connect_to_server(const char *host, uint16_t port) {
    struct addrinfo hints;
    struct addrinfo *result = NULL;
    struct addrinfo *it;
    char port_text[16];
    int fd = -1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    snprintf(port_text, sizeof(port_text), "%u", port);
    if (getaddrinfo(host, port_text, &hints, &result) != 0) {
        return -1;
    }

    for (it = result; it != NULL; it = it->ai_next) {
        fd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (fd < 0) {
            continue;
        }
        if (connect(fd, it->ai_addr, it->ai_addrlen) == 0) {
            break;
        }
        close(fd);
        fd = -1;
    }

    freeaddrinfo(result);
    return fd;
}

static int send_hello(client_ctx_t *ctx) {
    msg_hello_t hello;
    uint8_t payload[128];
    size_t payload_len = 0;

    memset(&hello, 0, sizeof(hello));
    strncpy(hello.client_id, "bomberman-client/1.0", MAX_ID_LEN);
    hello.client_id[MAX_ID_LEN] = '\0';
    strncpy(hello.player_name, ctx->player_name, MAX_NAME_LEN);
    hello.player_name[MAX_NAME_LEN] = '\0';

    if (proto_encode_hello_payload(&hello, payload, sizeof(payload), &payload_len) != 0) {
        return -1;
    }

    return sock_send_message(
        ctx->fd,
        MSG_HELLO,
        SERVER_ENDPOINT_ID,
        SERVER_ENDPOINT_ID,
        payload,
        payload_len
    );
}

static void apply_map_payload(client_ctx_t *ctx, const msg_map_t *map_msg) {
    if (gs_resize_map(&ctx->state, map_msg->rows, map_msg->cols) != 0) {
        return;
    }
    memcpy(ctx->state.map.cells, map_msg->cells, map_msg->cell_count);
}

int client_handle_server_message(
    client_ctx_t *ctx,
    const msg_header_t *header,
    const uint8_t *payload,
    size_t payload_len
) {
    if (ctx == NULL || header == NULL) {
        return -1;
    }

    switch (header->msg_type) {
        case MSG_HELLO: {
            msg_hello_t hello;
            if (proto_decode_hello_payload(&hello, payload, payload_len) == 0) {
                if (header->sender_id >= 1 && header->sender_id <= MAX_PLAYERS) {
                    player_t *player = &ctx->state.players[header->sender_id - 1];
                    player->id = header->sender_id;
                    strncpy(player->name, hello.player_name, MAX_NAME_LEN);
                    player->name[MAX_NAME_LEN] = '\0';
                }
                printf("[server] player joined: %s\n", hello.player_name);
            }
            break;
        }

        case MSG_WELCOME: {
            msg_welcome_t welcome;
            if (proto_decode_welcome_payload(&welcome, payload, payload_len) != 0) {
                fprintf(stderr, "invalid WELCOME payload\n");
                return -1;
            }

            ctx->player_id = header->sender_id;
            ctx->has_welcome = true;
            ctx->state.status = (game_status_t)welcome.game_status;

            if (ctx->player_id >= 1 && ctx->player_id <= MAX_PLAYERS) {
                player_t *self = &ctx->state.players[ctx->player_id - 1];
                self->id = ctx->player_id;
                strncpy(self->name, ctx->player_name, MAX_NAME_LEN);
                self->name[MAX_NAME_LEN] = '\0';
            }

            for (size_t i = 0; i < welcome.player_count; ++i) {
                uint8_t id = welcome.players[i].id;
                if (id >= 1 && id <= MAX_PLAYERS) {
                    player_t *player = &ctx->state.players[id - 1];
                    player->id = id;
                    player->ready = welcome.players[i].ready;
                    strncpy(player->name, welcome.players[i].name, MAX_NAME_LEN);
                    player->name[MAX_NAME_LEN] = '\0';
                }
            }

            printf("[server] welcome, assigned id=%u status=%u\n", ctx->player_id, welcome.game_status);
            break;
        }

        case MSG_SET_STATUS: {
            msg_set_status_t status;
            if (proto_decode_set_status_payload(&status, payload, payload_len) == 0) {
                ctx->state.status = (game_status_t)status.status;
                printf("[server] status changed to %u\n", status.status);
            }
            break;
        }

        case MSG_SET_READY:
            if (header->sender_id >= 1 && header->sender_id <= MAX_PLAYERS) {
                player_t *player = &ctx->state.players[header->sender_id - 1];
                player->id = header->sender_id;
                player->ready = true;
                printf("[server] player %u is ready\n", header->sender_id);
            }
            break;

        case MSG_MAP: {
            msg_map_t map_msg;
            if (proto_decode_map_payload(&map_msg, payload, payload_len) == 0) {
                apply_map_payload(ctx, &map_msg);
                client_render_state(ctx);
            }
            break;
        }

        case MSG_SYNC_BOARD: {
            msg_sync_board_t sync;
            if (proto_decode_sync_board_payload(&sync, payload, payload_len) == 0) {
                size_t i;

                ctx->state.status = (game_status_t)sync.status;
                for (i = 0; i < MAX_PLAYERS; ++i) {
                    ctx->state.players[i].alive = false;
                    ctx->state.players[i].ready = false;
                }

                for (i = 0; i < sync.player_count; ++i) {
                    uint8_t id = sync.players[i].id;
                    uint16_t row;
                    uint16_t col;
                    player_t *player;
                    if (id < 1 || id > MAX_PLAYERS || ctx->state.map.cols == 0) {
                        continue;
                    }
                    player = &ctx->state.players[id - 1];
                    player->id = id;
                    split_cell_index(sync.players[i].cell_index, ctx->state.map.cols, &row, &col);
                    player->row = row;
                    player->col = col;
                    player->alive = sync.players[i].alive;
                    player->ready = sync.players[i].ready;
                }

                client_render_state(ctx);
            }
            break;
        }

        case MSG_MOVED: {
            msg_moved_t moved;
            if (proto_decode_moved_payload(&moved, payload, payload_len) == 0 &&
                moved.player_id >= 1 && moved.player_id <= MAX_PLAYERS && ctx->state.map.cols > 0) {
                uint16_t row;
                uint16_t col;
                split_cell_index(moved.cell_index, ctx->state.map.cols, &row, &col);
                ctx->state.players[moved.player_id - 1].row = row;
                ctx->state.players[moved.player_id - 1].col = col;
                client_render_state(ctx);
            }
            break;
        }

        case MSG_BOMB: {
            msg_bomb_t bomb;
            if (proto_decode_bomb_payload(&bomb, payload, payload_len) == 0 && ctx->state.map.cols > 0) {
                uint16_t row;
                uint16_t col;
                split_cell_index(bomb.cell_index, ctx->state.map.cols, &row, &col);
                gs_cell_set(&ctx->state, row, col, CELL_BOMB);
                client_render_state(ctx);
            }
            break;
        }

        case MSG_EXPLOSION_START: {
            msg_explosion_start_t explosion;
            if (proto_decode_explosion_start_payload(&explosion, payload, payload_len) == 0) {
                printf("[server] explosion start at cell=%u radius=%u\n", explosion.cell_index, explosion.radius);
            }
            break;
        }

        case MSG_EXPLOSION_END: {
            msg_explosion_end_t explosion;
            if (proto_decode_explosion_end_payload(&explosion, payload, payload_len) == 0 && ctx->state.map.cols > 0) {
                uint16_t row;
                uint16_t col;
                split_cell_index(explosion.cell_index, ctx->state.map.cols, &row, &col);
                gs_cell_set(&ctx->state, row, col, CELL_EMPTY);
                client_render_state(ctx);
            }
            break;
        }

        case MSG_DEATH: {
            msg_death_t death;
            if (proto_decode_death_payload(&death, payload, payload_len) == 0 && death.player_id >= 1 && death.player_id <= MAX_PLAYERS) {
                ctx->state.players[death.player_id - 1].alive = false;
                printf("[server] player %u died\n", death.player_id);
                client_render_state(ctx);
            }
            break;
        }

        case MSG_BONUS_AVAILABLE: {
            msg_bonus_available_t bonus;
            if (proto_decode_bonus_available_payload(&bonus, payload, payload_len) == 0 && ctx->state.map.cols > 0) {
                uint16_t row;
                uint16_t col;
                uint8_t cell = CELL_BONUS_SPEED;
                if (bonus.bonus_type == BONUS_RADIUS) {
                    cell = CELL_BONUS_RADIUS;
                } else if (bonus.bonus_type == BONUS_TIMER) {
                    cell = CELL_BONUS_TIMER;
                }
                split_cell_index(bonus.cell_index, ctx->state.map.cols, &row, &col);
                gs_cell_set(&ctx->state, row, col, cell);
                client_render_state(ctx);
            }
            break;
        }

        case MSG_BONUS_RETRIEVED: {
            msg_bonus_retrieved_t bonus;
            if (proto_decode_bonus_retrieved_payload(&bonus, payload, payload_len) == 0 && ctx->state.map.cols > 0) {
                uint16_t row;
                uint16_t col;
                split_cell_index(bonus.cell_index, ctx->state.map.cols, &row, &col);
                gs_cell_set(&ctx->state, row, col, CELL_EMPTY);
                client_render_state(ctx);
            }
            break;
        }

        case MSG_BLOCK_DESTROYED: {
            msg_block_destroyed_t block;
            if (proto_decode_block_destroyed_payload(&block, payload, payload_len) == 0 && ctx->state.map.cols > 0) {
                uint16_t row;
                uint16_t col;
                split_cell_index(block.cell_index, ctx->state.map.cols, &row, &col);
                gs_cell_set(&ctx->state, row, col, CELL_EMPTY);
                client_render_state(ctx);
            }
            break;
        }

        case MSG_WINNER: {
            msg_winner_t winner;
            if (proto_decode_winner_payload(&winner, payload, payload_len) == 0) {
                printf("[server] winner is player %u\n", winner.winner_id);
            }
            break;
        }

        case MSG_ERROR: {
            msg_error_t err;
            if (proto_decode_error_payload(&err, payload, payload_len) == 0) {
                printf("[server][error] %s\n", err.text);
            }
            break;
        }

        case MSG_DISCONNECT:
            printf("[server] disconnect requested\n");
            ctx->running = false;
            return 1;

        case MSG_LEAVE:
            if (header->sender_id >= 1 && header->sender_id <= MAX_PLAYERS) {
                size_t slot = (size_t)(header->sender_id - 1);
                ctx->state.players[slot].alive = false;
                ctx->state.players[slot].ready = false;
                ctx->state.players[slot].name[0] = '\0';
                printf("[server] player %u left\n", header->sender_id);
                client_render_state(ctx);
            }
            break;

        case MSG_PING:
            sock_send_message(ctx->fd, MSG_PONG, ctx->player_id, SERVER_ENDPOINT_ID, NULL, 0);
            break;

        case MSG_PONG:
            printf("[server] pong\n");
            break;

        default:
            printf("[server] unhandled message type=%u\n", header->msg_type);
            break;
    }

    return 0;
}

int main(int argc, char **argv) {
    client_ctx_t ctx;
    long port;

    if (argc < 4) {
        fprintf(stderr, "usage: %s <host> <port> <name>\n", argv[0]);
        return 1;
    }

    port = strtol(argv[2], NULL, 10);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "invalid port\n");
        return 1;
    }

    memset(&ctx, 0, sizeof(ctx));
    gs_init(&ctx.state);
    strncpy(ctx.player_name, argv[3], MAX_NAME_LEN);
    ctx.player_name[MAX_NAME_LEN] = '\0';
    ctx.running = true;
    ctx.fd = connect_to_server(argv[1], (uint16_t)port);

    if (ctx.fd < 0) {
        fprintf(stderr, "could not connect to server\n");
        gs_free(&ctx.state);
        return 1;
    }

    if (send_hello(&ctx) != 0) {
        fprintf(stderr, "could not send HELLO\n");
        close(ctx.fd);
        gs_free(&ctx.state);
        return 1;
    }

    printf("connected. commands: ready, w/a/s/d, b, ping, lobby, quit\n");

    while (ctx.running) {
        fd_set readfds;
        int max_fd = ctx.fd;
        int rv;

        FD_ZERO(&readfds);
        FD_SET(ctx.fd, &readfds);
        FD_SET(STDIN_FILENO, &readfds);
        if (STDIN_FILENO > max_fd) {
            max_fd = STDIN_FILENO;
        }

        rv = select(max_fd + 1, &readfds, NULL, NULL, NULL);
        if (rv < 0) {
            perror("select");
            break;
        }

        if (FD_ISSET(ctx.fd, &readfds)) {
            msg_header_t header;
            uint8_t payload[65536];
            size_t payload_len = 0;

            if (sock_recv_header(ctx.fd, &header, -1) != 0) {
                fprintf(stderr, "connection closed by server\n");
                break;
            }
            if (sock_recv_payload_by_type(ctx.fd, header.msg_type, payload, sizeof(payload), &payload_len, -1) != 0) {
                fprintf(stderr, "failed to read payload\n");
                break;
            }
            if (client_handle_server_message(&ctx, &header, payload, payload_len) != 0) {
                break;
            }
        }

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            char line[128];
            uint8_t msg_type;
            uint8_t sender_id;
            uint8_t target_id;
            uint8_t payload[64];
            size_t payload_len = 0;
            bool should_quit = false;
            int command_status;

            if (fgets(line, sizeof(line), stdin) == NULL) {
                break;
            }

            command_status = client_build_command(
                &ctx,
                line,
                &msg_type,
                &sender_id,
                &target_id,
                payload,
                sizeof(payload),
                &payload_len,
                &should_quit
            );

            if (command_status == 0) {
                if (sock_send_message(ctx.fd, msg_type, sender_id, target_id, payload, payload_len) != 0) {
                    fprintf(stderr, "failed to send command\n");
                    break;
                }
                if (should_quit) {
                    break;
                }
            } else if (command_status < 0) {
                fprintf(stderr, "command encoding failed\n");
            }
        }
    }

    close(ctx.fd);
    gs_free(&ctx.state);
    return 0;
}
