#ifndef GAME_SOCKET_IO_H
#define GAME_SOCKET_IO_H

#include <stddef.h>
#include <stdint.h>

#include "protocol.h"

int sock_send_all(int fd, const uint8_t *buffer, size_t length);
int sock_recv_all(int fd, uint8_t *buffer, size_t length, int timeout_ms);

int sock_send_message(
    int fd,
    uint8_t msg_type,
    uint8_t sender_id,
    uint8_t target_id,
    const uint8_t *payload,
    size_t payload_len
);

int sock_recv_header(int fd, msg_header_t *header, int timeout_ms);
int sock_recv_payload_by_type(
    int fd,
    uint8_t msg_type,
    uint8_t *payload,
    size_t max_len,
    size_t *payload_len,
    int timeout_ms
);

#endif
