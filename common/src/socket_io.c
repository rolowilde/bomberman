#include "../include/socket_io.h"

#include <errno.h>
#include <poll.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../include/serialization.h"

#define HEADER_SIZE 3

static uint16_t read_u16_be(const uint8_t *bytes) {
    return (uint16_t)(((uint16_t)bytes[0] << 8) | (uint16_t)bytes[1]);
}

static int wait_for_readable(int fd, int timeout_ms) {
    struct pollfd pfd;
    int rv;

    if (timeout_ms < 0) {
        return 0;
    }

    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    while (1) {
        rv = poll(&pfd, 1, timeout_ms);
        if (rv > 0) {
            return 0;
        }
        if (rv == 0) {
            return 2;
        }
        if (errno == EINTR) {
            continue;
        }
        return -1;
    }
}

int sock_send_all(int fd, const uint8_t *buffer, size_t length) {
    size_t sent = 0;

    while (sent < length) {
        ssize_t chunk = send(fd, buffer + sent, length - sent, 0);
        if (chunk > 0) {
            sent += (size_t)chunk;
            continue;
        }
        if (chunk < 0 && errno == EINTR) {
            continue;
        }
        return -1;
    }

    return 0;
}

int sock_recv_all(int fd, uint8_t *buffer, size_t length, int timeout_ms) {
    size_t received = 0;

    while (received < length) {
        int wait_status = wait_for_readable(fd, timeout_ms);
        if (wait_status != 0) {
            return wait_status;
        }

        ssize_t chunk = recv(fd, buffer + received, length - received, 0);
        if (chunk > 0) {
            received += (size_t)chunk;
            continue;
        }
        if (chunk == 0) {
            return 1;
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            continue;
        }
        return -1;
    }

    return 0;
}

int sock_send_message(
    int fd,
    uint8_t msg_type,
    uint8_t sender_id,
    uint8_t target_id,
    const uint8_t *payload,
    size_t payload_len
) {
    msg_header_t header;
    uint8_t header_buf[HEADER_SIZE];

    header.msg_type = msg_type;
    header.sender_id = sender_id;
    header.target_id = target_id;

    if (proto_write_header(&header, header_buf, sizeof(header_buf)) != 0) {
        return -1;
    }

    if (sock_send_all(fd, header_buf, sizeof(header_buf)) != 0) {
        return -1;
    }
    if (payload_len > 0 && sock_send_all(fd, payload, payload_len) != 0) {
        return -1;
    }

    return 0;
}

int sock_recv_header(int fd, msg_header_t *header, int timeout_ms) {
    uint8_t header_buf[HEADER_SIZE];
    int rc;

    rc = sock_recv_all(fd, header_buf, sizeof(header_buf), timeout_ms);
    if (rc != 0) {
        return rc;
    }

    if (proto_read_header(header, header_buf, sizeof(header_buf)) != 0) {
        return -1;
    }

    return 0;
}

int sock_recv_payload_by_type(
    int fd,
    uint8_t msg_type,
    uint8_t *payload,
    size_t max_len,
    size_t *payload_len,
    int timeout_ms
) {
    size_t fixed = msg_type_fixed_payload_size(msg_type);

    if (payload == NULL || payload_len == NULL) {
        return -1;
    }

    *payload_len = 0;

    if (!msg_type_variable_payload(msg_type)) {
        if (fixed == SIZE_MAX) {
            return -1;
        }
        if (fixed > max_len) {
            return -1;
        }
        if (fixed == 0) {
            return 0;
        }
        if (sock_recv_all(fd, payload, fixed, timeout_ms) != 0) {
            return -1;
        }
        *payload_len = fixed;
        return 0;
    }

    if (msg_type == MSG_WELCOME) {
        size_t base_len = MAX_ID_LEN + 1 + 1;
        size_t extra_len;
        uint8_t player_count;

        if (base_len > max_len) {
            return -1;
        }
        if (sock_recv_all(fd, payload, base_len, timeout_ms) != 0) {
            return -1;
        }
        player_count = payload[MAX_ID_LEN + 1];
        extra_len = (size_t)player_count * (1 + 1 + MAX_NAME_LEN);
        if (player_count > MAX_PLAYERS || base_len + extra_len > max_len) {
            return -1;
        }
        if (extra_len > 0 && sock_recv_all(fd, payload + base_len, extra_len, timeout_ms) != 0) {
            return -1;
        }
        *payload_len = base_len + extra_len;
        return 0;
    }

    if (msg_type == MSG_MAP) {
        size_t base_len = 2;
        size_t cells_len;

        if (base_len > max_len) {
            return -1;
        }
        if (sock_recv_all(fd, payload, base_len, timeout_ms) != 0) {
            return -1;
        }

        cells_len = (size_t)payload[0] * (size_t)payload[1];
        if (cells_len == 0 || base_len + cells_len > max_len || cells_len > MAX_MAP_CELLS) {
            return -1;
        }
        if (sock_recv_all(fd, payload + base_len, cells_len, timeout_ms) != 0) {
            return -1;
        }
        *payload_len = base_len + cells_len;
        return 0;
    }

    if (msg_type == MSG_ERROR) {
        size_t base_len = 2;
        uint16_t text_len;

        if (base_len > max_len) {
            return -1;
        }
        if (sock_recv_all(fd, payload, base_len, timeout_ms) != 0) {
            return -1;
        }

        text_len = read_u16_be(payload);
        if (text_len > MAX_ERROR_LEN || base_len + text_len > max_len) {
            return -1;
        }
        if (text_len > 0 && sock_recv_all(fd, payload + base_len, text_len, timeout_ms) != 0) {
            return -1;
        }
        *payload_len = base_len + text_len;
        return 0;
    }

    if (msg_type == MSG_SYNC_BOARD) {
        size_t base_len = 2;
        size_t extra_len;
        uint8_t player_count;

        if (base_len > max_len) {
            return -1;
        }
        if (sock_recv_all(fd, payload, base_len, timeout_ms) != 0) {
            return -1;
        }

        player_count = payload[1];
        extra_len = (size_t)player_count * 5;
        if (player_count > MAX_PLAYERS || base_len + extra_len > max_len) {
            return -1;
        }

        if (extra_len > 0 && sock_recv_all(fd, payload + base_len, extra_len, timeout_ms) != 0) {
            return -1;
        }
        *payload_len = base_len + extra_len;
        return 0;
    }

    return -1;
}
