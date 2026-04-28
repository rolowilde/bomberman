#include "client.h"
#include "sigthread.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include "../common/include/serialization.h"
#include "../common/include/socket_io.h"

pthread_t signal_tid;
struct termios orig_termios;

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

    return sock_send_message(ctx->fd, MSG_HELLO, SERVER_ENDPOINT_ID, SERVER_ENDPOINT_ID, payload, payload_len);
}

/* must be called before creating any other threads */
static int init_signals(pthread_t *tid) {
    static sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGWINCH);
    sigaddset(&set, SIGINT);

    /* make signals from the set be handled by sigwait in the signal thread */
    if (pthread_sigmask(SIG_BLOCK, &set, NULL) != 0) {
        perror("pthread_sigmask");
        return 1;
    }

    st_pipe_init();

    if (0 != pthread_create(tid, NULL, signal_thread, &set)) {
        perror("pthread_create");
        return 1;
    }

    return 0;
}

static void handle_fd_sigthread(client_ctx_t *ctx, int sig_pipe_fd) {
    int sig;
    int n;

    while ((n = read(sig_pipe_fd, &sig, sizeof(int))) == sizeof(int)) {
        if (sig == SIGWINCH) {
            client_ui_update_screen_size();
        } else if (sig == SIGINT) {
            ctx->running = false;
        }
    }

    if (n == -1 && errno != EAGAIN)
        perror("read");
}

static void handle_fd_server(client_ctx_t *ctx) {
    msg_header_t header;
    uint8_t payload[65536];
    size_t payload_len = 0;

    if (sock_recv_header(ctx->fd, &header, -1) != 0) {
        fprintf(stderr, "connection closed by server\n");
        goto fail;
    }
    if (sock_recv_payload_by_type(ctx->fd, header.msg_type, payload, sizeof(payload), &payload_len, -1) != 0) {
        fprintf(stderr, "failed to read payload\n");
        goto fail;
    }
    if (client_handle_server_message(ctx, &header, payload, payload_len) != 0) {
        goto fail;
    }

    return;
fail:
    ctx->running = false;
    return;
}

static void handle_fd_input(client_ctx_t *ctx) {
    uint8_t msg_type;
    uint8_t sender_id;
    uint8_t target_id;
    uint8_t payload[64];
    size_t payload_len = 0;
    bool should_quit = false;
    client_build_command_err_t build_command_ret;
    char c;

    if (read(STDIN_FILENO, &c, 1) < 0) {
        perror("read");
        return;
    }

    build_command_ret = client_build_command(ctx, c, &msg_type, &sender_id, &target_id, payload, sizeof(payload),
                                             &payload_len, &should_quit);

    if (build_command_ret == CLIENT_BUILD_COMMAND_ERR_FAIL) {
        fprintf(stderr, "command encoding failed\n");
        ctx->running = false;
        return;
    }

    if (build_command_ret != CLIENT_BUILD_COMMAND_ERR_OK)
        return;

    if (sock_send_message(ctx->fd, msg_type, sender_id, target_id, payload, payload_len) != 0) {
        fprintf(stderr, "failed to send command\n");
        ctx->running = false;
        return;
    }

    if (should_quit) {
        ctx->running = false;
    }
}

static void restore_orig_input_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

static void enable_raw_input_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);

    struct termios raw = orig_termios;

    raw.c_lflag &= ~(ECHO | ICANON); // no echo, no line buffering
    raw.c_iflag &= ~(IXON | ICRNL);  // disable ctrl-s/q and CR->NL
    raw.c_cc[VMIN] = 1;              // read returns after 1 byte
    raw.c_cc[VTIME] = 0;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void qlogf(client_ctx_t *ctx, const char *fmt, ...) {
    va_list args;
    char *buf = ctx->qlog[ctx->qlog_end];

    va_start(args, fmt);
    vsnprintf(buf, sizeof(ctx->qlog[0]), fmt, args);
    va_end(args);

    ctx->qlog_end = (ctx->qlog_end + 1) % MAX_CLIENT_LOG_COUNT;
    if (ctx->qlog_beg == ctx->qlog_end)
        ctx->qlog_beg = (ctx->qlog_beg + 1) % MAX_CLIENT_LOG_COUNT;
}

int main(int argc, char **argv) {
    client_ctx_t ctx;
    long port;

    if (argc < 4) {
        fprintf(stderr, "usage: %s <host> <port> <name>\n", argv[0]);
        return 1;
    }

    freopen("/tmp/bomberman.log", "a", stderr);
    fprintf(stderr, "Starting...\n");

    if (init_signals(&signal_tid) != 0)
        return 1;

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
    ctx.qlog_beg = ctx.qlog_end = 0;

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

    client_ui_init();
    enable_raw_input_mode();

    qlogf(&ctx, "client started");

    client_ui_render(&ctx);

    while (ctx.running) {
        fd_set readfds;
        int max_fd = ctx.fd;
        int rv;
        int sig_pipe_fd = st_pipe_read_fd();

        FD_ZERO(&readfds);
        FD_SET(ctx.fd, &readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(sig_pipe_fd, &readfds);

        if (STDIN_FILENO > max_fd) {
            max_fd = STDIN_FILENO;
        }

        rv = select(max_fd + 1, &readfds, NULL, NULL, NULL);
        if (rv < 0) {
            perror("select");
            break;
        }

        if (FD_ISSET(sig_pipe_fd, &readfds))
            handle_fd_sigthread(&ctx, sig_pipe_fd);

        if (FD_ISSET(ctx.fd, &readfds))
            handle_fd_server(&ctx);

        if (FD_ISSET(STDIN_FILENO, &readfds))
            handle_fd_input(&ctx);

        if (ctx.running)
            client_ui_render(&ctx);
    }

    /* TODO: send MSG_LEAVE */

    restore_orig_input_mode();
    client_ui_deinit();
    close(ctx.fd);
    gs_free(&ctx.state);

    return 0;
}
