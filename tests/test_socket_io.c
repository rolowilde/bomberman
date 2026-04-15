#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../common/include/serialization.h"
#include "../common/include/socket_io.h"

static void test_fixed_payload_message(void) {
    int fds[2];
    msg_header_t header;
    uint8_t payload[16];
    size_t payload_len = 0;
    msg_moved_t moved;
    msg_moved_t decoded;

    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    moved.player_id = 3;
    moved.cell_index = 258;
    assert(proto_encode_moved_payload(&moved, payload, sizeof(payload), &payload_len) == 0);

    assert(sock_send_message(fds[0], MSG_MOVED, 3, BROADCAST_ENDPOINT_ID, payload, payload_len) == 0);
    assert(sock_recv_header(fds[1], &header, 1000) == 0);
    assert(header.msg_type == MSG_MOVED);

    memset(payload, 0, sizeof(payload));
    assert(sock_recv_payload_by_type(fds[1], header.msg_type, payload, sizeof(payload), &payload_len, 1000) == 0);
    assert(proto_decode_moved_payload(&decoded, payload, payload_len) == 0);
    assert(decoded.player_id == 3);
    assert(decoded.cell_index == 258);

    close(fds[0]);
    close(fds[1]);
}

static void test_variable_payload_message(void) {
    int fds[2];
    msg_header_t header;
    uint8_t tx_payload[64];
    uint8_t rx_payload[64];
    size_t tx_len = 0;
    size_t rx_len = 0;
    msg_error_t err;
    msg_error_t decoded;

    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    memset(&err, 0, sizeof(err));
    strncpy(err.text, "sample error", MAX_ERROR_LEN);
    err.len = (uint16_t)strlen(err.text);

    assert(proto_encode_error_payload(&err, tx_payload, sizeof(tx_payload), &tx_len) == 0);
    assert(sock_send_message(fds[0], MSG_ERROR, SERVER_ENDPOINT_ID, 1, tx_payload, tx_len) == 0);

    assert(sock_recv_header(fds[1], &header, 1000) == 0);
    assert(header.msg_type == MSG_ERROR);

    assert(sock_recv_payload_by_type(fds[1], header.msg_type, rx_payload, sizeof(rx_payload), &rx_len, 1000) == 0);
    assert(proto_decode_error_payload(&decoded, rx_payload, rx_len) == 0);
    assert(strcmp(decoded.text, "sample error") == 0);

    close(fds[0]);
    close(fds[1]);
}

int main(void) {
    test_fixed_payload_message();
    test_variable_payload_message();
    printf("test_socket_io: OK\n");
    return 0;
}
