#include "../include/serialization.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define HEADER_SIZE 3

static int ensure_capacity(size_t offset, size_t need, size_t capacity) {
    if (offset > capacity || need > (capacity - offset)) {
        return -1;
    }
    return 0;
}

static int write_u8(uint8_t *buffer, size_t capacity, size_t *offset, uint8_t value) {
    if (ensure_capacity(*offset, 1, capacity) != 0) {
        return -1;
    }
    buffer[*offset] = value;
    *offset += 1;
    return 0;
}

static int write_u16_be(uint8_t *buffer, size_t capacity, size_t *offset, uint16_t value) {
    if (ensure_capacity(*offset, 2, capacity) != 0) {
        return -1;
    }
    buffer[*offset] = (uint8_t)((value >> 8) & 0xFFu);
    buffer[*offset + 1] = (uint8_t)(value & 0xFFu);
    *offset += 2;
    return 0;
}

static int read_u8(const uint8_t *buffer, size_t length, size_t *offset, uint8_t *value) {
    if (ensure_capacity(*offset, 1, length) != 0) {
        return -1;
    }
    *value = buffer[*offset];
    *offset += 1;
    return 0;
}

static int read_u16_be(const uint8_t *buffer, size_t length, size_t *offset, uint16_t *value) {
    if (ensure_capacity(*offset, 2, length) != 0) {
        return -1;
    }
    *value = (uint16_t)(((uint16_t)buffer[*offset] << 8) | (uint16_t)buffer[*offset + 1]);
    *offset += 2;
    return 0;
}

static int write_fixed_string(uint8_t *buffer, size_t capacity, size_t *offset, const char *input, size_t field_len) {
    size_t copy_len = 0;

    if (ensure_capacity(*offset, field_len, capacity) != 0) {
        return -1;
    }

    if (input != NULL) {
        copy_len = strnlen(input, field_len);
        memcpy(buffer + *offset, input, copy_len);
    }
    if (field_len > copy_len) {
        memset(buffer + *offset + copy_len, 0, field_len - copy_len);
    }

    *offset += field_len;
    return 0;
}

static int read_fixed_string(char *output, size_t output_capacity, const uint8_t *buffer, size_t length, size_t *offset, size_t field_len) {
    size_t copy_len = 0;

    if (output_capacity == 0) {
        return -1;
    }
    if (ensure_capacity(*offset, field_len, length) != 0) {
        return -1;
    }

    copy_len = field_len;
    if (copy_len > output_capacity - 1) {
        copy_len = output_capacity - 1;
    }

    memcpy(output, buffer + *offset, copy_len);
    output[copy_len] = '\0';

    while (copy_len > 0 && output[copy_len - 1] == '\0') {
        output[copy_len - 1] = '\0';
        copy_len--;
    }

    *offset += field_len;
    return 0;
}

int proto_write_header(const msg_header_t *header, uint8_t *buffer, size_t buffer_len) {
    size_t offset = 0;

    if (header == NULL || buffer == NULL) {
        return -1;
    }
    if (buffer_len < HEADER_SIZE) {
        return -1;
    }

    if (write_u8(buffer, buffer_len, &offset, header->msg_type) != 0 ||
        write_u8(buffer, buffer_len, &offset, header->sender_id) != 0 ||
        write_u8(buffer, buffer_len, &offset, header->target_id) != 0) {
        return -1;
    }

    return 0;
}

int proto_read_header(msg_header_t *header, const uint8_t *buffer, size_t buffer_len) {
    size_t offset = 0;

    if (header == NULL || buffer == NULL || buffer_len < HEADER_SIZE) {
        return -1;
    }

    if (read_u8(buffer, buffer_len, &offset, &header->msg_type) != 0 ||
        read_u8(buffer, buffer_len, &offset, &header->sender_id) != 0 ||
        read_u8(buffer, buffer_len, &offset, &header->target_id) != 0) {
        return -1;
    }

    return 0;
}

size_t proto_welcome_payload_size(const msg_welcome_t *msg) {
    if (msg == NULL || msg->player_count > MAX_PLAYERS) {
        return 0;
    }
    return (size_t)MAX_ID_LEN + 1 + 1 + ((size_t)msg->player_count * (1 + 1 + MAX_NAME_LEN));
}

size_t proto_map_payload_size(const msg_map_t *msg) {
    uint16_t expected_count;

    if (msg == NULL) {
        return 0;
    }

    expected_count = (uint16_t)((uint16_t)msg->rows * (uint16_t)msg->cols);
    if (expected_count == 0 || expected_count > MAX_MAP_CELLS) {
        return 0;
    }
    return 2 + expected_count;
}

size_t proto_sync_board_payload_size(const msg_sync_board_t *msg) {
    if (msg == NULL || msg->player_count > MAX_PLAYERS) {
        return 0;
    }
    return 2 + ((size_t)msg->player_count * 11);
}

int proto_encode_hello_payload(const msg_hello_t *msg, uint8_t *buffer, size_t buffer_len, size_t *written) {
    size_t offset = 0;

    if (msg == NULL || buffer == NULL || written == NULL) {
        return -1;
    }

    if (write_fixed_string(buffer, buffer_len, &offset, msg->client_id, MAX_ID_LEN) != 0 ||
        write_fixed_string(buffer, buffer_len, &offset, msg->player_name, MAX_NAME_LEN) != 0) {
        return -1;
    }

    *written = offset;
    return 0;
}

int proto_decode_hello_payload(msg_hello_t *msg, const uint8_t *buffer, size_t buffer_len) {
    size_t offset = 0;

    if (msg == NULL || buffer == NULL || buffer_len != (MAX_ID_LEN + MAX_NAME_LEN)) {
        return -1;
    }

    if (read_fixed_string(msg->client_id, sizeof(msg->client_id), buffer, buffer_len, &offset, MAX_ID_LEN) != 0 ||
        read_fixed_string(msg->player_name, sizeof(msg->player_name), buffer, buffer_len, &offset, MAX_NAME_LEN) != 0) {
        return -1;
    }

    return 0;
}

int proto_encode_welcome_payload(const msg_welcome_t *msg, uint8_t *buffer, size_t buffer_len, size_t *written) {
    size_t offset = 0;
    size_t index;

    if (msg == NULL || buffer == NULL || written == NULL || msg->player_count > MAX_PLAYERS) {
        return -1;
    }

    if (write_fixed_string(buffer, buffer_len, &offset, msg->server_id, MAX_ID_LEN) != 0 ||
        write_u8(buffer, buffer_len, &offset, msg->game_status) != 0 ||
        write_u8(buffer, buffer_len, &offset, msg->player_count) != 0) {
        return -1;
    }

    for (index = 0; index < msg->player_count; ++index) {
        if (write_u8(buffer, buffer_len, &offset, msg->players[index].id) != 0 ||
            write_u8(buffer, buffer_len, &offset, msg->players[index].ready ? 1u : 0u) != 0 ||
            write_fixed_string(buffer, buffer_len, &offset, msg->players[index].name, MAX_NAME_LEN) != 0) {
            return -1;
        }
    }

    *written = offset;
    return 0;
}

int proto_decode_welcome_payload(msg_welcome_t *msg, const uint8_t *buffer, size_t buffer_len) {
    size_t offset = 0;
    size_t expected;
    size_t index;
    uint8_t ready_byte;

    if (msg == NULL || buffer == NULL || buffer_len < (MAX_ID_LEN + 2)) {
        return -1;
    }

    if (read_fixed_string(msg->server_id, sizeof(msg->server_id), buffer, buffer_len, &offset, MAX_ID_LEN) != 0 ||
        read_u8(buffer, buffer_len, &offset, &msg->game_status) != 0 ||
        read_u8(buffer, buffer_len, &offset, &msg->player_count) != 0) {
        return -1;
    }

    if (msg->player_count > MAX_PLAYERS) {
        return -1;
    }

    expected = (size_t)MAX_ID_LEN + 1 + 1 + ((size_t)msg->player_count * (1 + 1 + MAX_NAME_LEN));
    if (buffer_len != expected) {
        return -1;
    }

    for (index = 0; index < msg->player_count; ++index) {
        if (read_u8(buffer, buffer_len, &offset, &msg->players[index].id) != 0 ||
            read_u8(buffer, buffer_len, &offset, &ready_byte) != 0 ||
            read_fixed_string(msg->players[index].name, sizeof(msg->players[index].name), buffer, buffer_len, &offset, MAX_NAME_LEN) != 0) {
            return -1;
        }
        msg->players[index].ready = (ready_byte != 0);
    }

    return 0;
}

int proto_encode_set_status_payload(const msg_set_status_t *msg, uint8_t *buffer, size_t buffer_len, size_t *written) {
    size_t offset = 0;
    if (msg == NULL || buffer == NULL || written == NULL) {
        return -1;
    }
    if (write_u8(buffer, buffer_len, &offset, msg->status) != 0) {
        return -1;
    }
    *written = offset;
    return 0;
}

int proto_decode_set_status_payload(msg_set_status_t *msg, const uint8_t *buffer, size_t buffer_len) {
    size_t offset = 0;
    if (msg == NULL || buffer == NULL || buffer_len != 1) {
        return -1;
    }
    return read_u8(buffer, buffer_len, &offset, &msg->status);
}

int proto_encode_move_attempt_payload(const msg_move_attempt_t *msg, uint8_t *buffer, size_t buffer_len, size_t *written) {
    size_t offset = 0;
    if (msg == NULL || buffer == NULL || written == NULL) {
        return -1;
    }
    if (write_u8(buffer, buffer_len, &offset, msg->direction) != 0) {
        return -1;
    }
    *written = offset;
    return 0;
}

int proto_decode_move_attempt_payload(msg_move_attempt_t *msg, const uint8_t *buffer, size_t buffer_len) {
    size_t offset = 0;
    if (msg == NULL || buffer == NULL || buffer_len != 1) {
        return -1;
    }
    return read_u8(buffer, buffer_len, &offset, &msg->direction);
}

int proto_encode_moved_payload(const msg_moved_t *msg, uint8_t *buffer, size_t buffer_len, size_t *written) {
    size_t offset = 0;
    if (msg == NULL || buffer == NULL || written == NULL) {
        return -1;
    }
    if (write_u8(buffer, buffer_len, &offset, msg->player_id) != 0 ||
        write_u16_be(buffer, buffer_len, &offset, msg->cell_index) != 0) {
        return -1;
    }
    *written = offset;
    return 0;
}

int proto_decode_moved_payload(msg_moved_t *msg, const uint8_t *buffer, size_t buffer_len) {
    size_t offset = 0;
    if (msg == NULL || buffer == NULL || buffer_len != 3) {
        return -1;
    }
    if (read_u8(buffer, buffer_len, &offset, &msg->player_id) != 0 ||
        read_u16_be(buffer, buffer_len, &offset, &msg->cell_index) != 0) {
        return -1;
    }
    return 0;
}

int proto_encode_bomb_attempt_payload(const msg_bomb_attempt_t *msg, uint8_t *buffer, size_t buffer_len, size_t *written) {
    size_t offset = 0;
    if (msg == NULL || buffer == NULL || written == NULL) {
        return -1;
    }
    if (write_u16_be(buffer, buffer_len, &offset, msg->cell_index) != 0) {
        return -1;
    }
    *written = offset;
    return 0;
}

int proto_decode_bomb_attempt_payload(msg_bomb_attempt_t *msg, const uint8_t *buffer, size_t buffer_len) {
    size_t offset = 0;
    if (msg == NULL || buffer == NULL || buffer_len != 2) {
        return -1;
    }
    return read_u16_be(buffer, buffer_len, &offset, &msg->cell_index);
}

int proto_encode_bomb_payload(const msg_bomb_t *msg, uint8_t *buffer, size_t buffer_len, size_t *written) {
    size_t offset = 0;
    if (msg == NULL || buffer == NULL || written == NULL) {
        return -1;
    }
    if (write_u8(buffer, buffer_len, &offset, msg->player_id) != 0 ||
        write_u16_be(buffer, buffer_len, &offset, msg->cell_index) != 0) {
        return -1;
    }
    *written = offset;
    return 0;
}

int proto_decode_bomb_payload(msg_bomb_t *msg, const uint8_t *buffer, size_t buffer_len) {
    size_t offset = 0;
    if (msg == NULL || buffer == NULL || buffer_len != 3) {
        return -1;
    }
    if (read_u8(buffer, buffer_len, &offset, &msg->player_id) != 0 ||
        read_u16_be(buffer, buffer_len, &offset, &msg->cell_index) != 0) {
        return -1;
    }
    return 0;
}

int proto_encode_explosion_start_payload(const msg_explosion_start_t *msg, uint8_t *buffer, size_t buffer_len, size_t *written) {
    size_t offset = 0;
    if (msg == NULL || buffer == NULL || written == NULL) {
        return -1;
    }
    if (write_u8(buffer, buffer_len, &offset, msg->radius) != 0 ||
        write_u16_be(buffer, buffer_len, &offset, msg->cell_index) != 0) {
        return -1;
    }
    *written = offset;
    return 0;
}

int proto_decode_explosion_start_payload(msg_explosion_start_t *msg, const uint8_t *buffer, size_t buffer_len) {
    size_t offset = 0;
    if (msg == NULL || buffer == NULL || buffer_len != 3) {
        return -1;
    }
    if (read_u8(buffer, buffer_len, &offset, &msg->radius) != 0 ||
        read_u16_be(buffer, buffer_len, &offset, &msg->cell_index) != 0) {
        return -1;
    }
    return 0;
}

int proto_encode_explosion_end_payload(const msg_explosion_end_t *msg, uint8_t *buffer, size_t buffer_len, size_t *written) {
    size_t offset = 0;
    if (msg == NULL || buffer == NULL || written == NULL) {
        return -1;
    }
    if (write_u16_be(buffer, buffer_len, &offset, msg->cell_index) != 0) {
        return -1;
    }
    *written = offset;
    return 0;
}

int proto_decode_explosion_end_payload(msg_explosion_end_t *msg, const uint8_t *buffer, size_t buffer_len) {
    size_t offset = 0;
    if (msg == NULL || buffer == NULL || buffer_len != 2) {
        return -1;
    }
    return read_u16_be(buffer, buffer_len, &offset, &msg->cell_index);
}

int proto_encode_death_payload(const msg_death_t *msg, uint8_t *buffer, size_t buffer_len, size_t *written) {
    size_t offset = 0;
    if (msg == NULL || buffer == NULL || written == NULL) {
        return -1;
    }
    if (write_u8(buffer, buffer_len, &offset, msg->player_id) != 0) {
        return -1;
    }
    *written = offset;
    return 0;
}

int proto_decode_death_payload(msg_death_t *msg, const uint8_t *buffer, size_t buffer_len) {
    size_t offset = 0;
    if (msg == NULL || buffer == NULL || buffer_len != 1) {
        return -1;
    }
    return read_u8(buffer, buffer_len, &offset, &msg->player_id);
}

int proto_encode_bonus_available_payload(const msg_bonus_available_t *msg, uint8_t *buffer, size_t buffer_len, size_t *written) {
    size_t offset = 0;
    if (msg == NULL || buffer == NULL || written == NULL) {
        return -1;
    }
    if (write_u8(buffer, buffer_len, &offset, msg->bonus_type) != 0 ||
        write_u16_be(buffer, buffer_len, &offset, msg->cell_index) != 0) {
        return -1;
    }
    *written = offset;
    return 0;
}

int proto_decode_bonus_available_payload(msg_bonus_available_t *msg, const uint8_t *buffer, size_t buffer_len) {
    size_t offset = 0;
    if (msg == NULL || buffer == NULL || buffer_len != 3) {
        return -1;
    }
    if (read_u8(buffer, buffer_len, &offset, &msg->bonus_type) != 0 ||
        read_u16_be(buffer, buffer_len, &offset, &msg->cell_index) != 0) {
        return -1;
    }
    return 0;
}

int proto_encode_bonus_retrieved_payload(const msg_bonus_retrieved_t *msg, uint8_t *buffer, size_t buffer_len, size_t *written) {
    size_t offset = 0;
    if (msg == NULL || buffer == NULL || written == NULL) {
        return -1;
    }
    if (write_u8(buffer, buffer_len, &offset, msg->player_id) != 0 ||
        write_u16_be(buffer, buffer_len, &offset, msg->cell_index) != 0) {
        return -1;
    }
    *written = offset;
    return 0;
}

int proto_decode_bonus_retrieved_payload(msg_bonus_retrieved_t *msg, const uint8_t *buffer, size_t buffer_len) {
    size_t offset = 0;
    if (msg == NULL || buffer == NULL || buffer_len != 3) {
        return -1;
    }
    if (read_u8(buffer, buffer_len, &offset, &msg->player_id) != 0 ||
        read_u16_be(buffer, buffer_len, &offset, &msg->cell_index) != 0) {
        return -1;
    }
    return 0;
}

int proto_encode_block_destroyed_payload(const msg_block_destroyed_t *msg, uint8_t *buffer, size_t buffer_len, size_t *written) {
    size_t offset = 0;
    if (msg == NULL || buffer == NULL || written == NULL) {
        return -1;
    }
    if (write_u16_be(buffer, buffer_len, &offset, msg->cell_index) != 0) {
        return -1;
    }
    *written = offset;
    return 0;
}

int proto_decode_block_destroyed_payload(msg_block_destroyed_t *msg, const uint8_t *buffer, size_t buffer_len) {
    size_t offset = 0;
    if (msg == NULL || buffer == NULL || buffer_len != 2) {
        return -1;
    }
    return read_u16_be(buffer, buffer_len, &offset, &msg->cell_index);
}

int proto_encode_winner_payload(const msg_winner_t *msg, uint8_t *buffer, size_t buffer_len, size_t *written) {
    size_t offset = 0;
    if (msg == NULL || buffer == NULL || written == NULL) {
        return -1;
    }
    if (write_u8(buffer, buffer_len, &offset, msg->winner_id) != 0) {
        return -1;
    }
    *written = offset;
    return 0;
}

int proto_decode_winner_payload(msg_winner_t *msg, const uint8_t *buffer, size_t buffer_len) {
    size_t offset = 0;
    if (msg == NULL || buffer == NULL || buffer_len != 1) {
        return -1;
    }
    return read_u8(buffer, buffer_len, &offset, &msg->winner_id);
}

int proto_encode_error_payload(const msg_error_t *msg, uint8_t *buffer, size_t buffer_len, size_t *written) {
    size_t offset = 0;
    uint16_t message_len;

    if (msg == NULL || buffer == NULL || written == NULL) {
        return -1;
    }

    message_len = msg->len;
    if (message_len == 0) {
        message_len = (uint16_t)strnlen(msg->text, MAX_ERROR_LEN);
    }
    if (message_len > MAX_ERROR_LEN) {
        return -1;
    }

    if (write_u16_be(buffer, buffer_len, &offset, message_len) != 0) {
        return -1;
    }

    if (ensure_capacity(offset, message_len, buffer_len) != 0) {
        return -1;
    }

    memcpy(buffer + offset, msg->text, message_len);
    offset += message_len;

    *written = offset;
    return 0;
}

int proto_decode_error_payload(msg_error_t *msg, const uint8_t *buffer, size_t buffer_len) {
    size_t offset = 0;

    if (msg == NULL || buffer == NULL || buffer_len < 2) {
        return -1;
    }

    if (read_u16_be(buffer, buffer_len, &offset, &msg->len) != 0) {
        return -1;
    }
    if (msg->len > MAX_ERROR_LEN || buffer_len != (size_t)(2 + msg->len)) {
        return -1;
    }

    memcpy(msg->text, buffer + offset, msg->len);
    msg->text[msg->len] = '\0';
    return 0;
}

int proto_encode_map_payload(const msg_map_t *msg, uint8_t *buffer, size_t buffer_len, size_t *written) {
    size_t offset = 0;
    uint16_t expected_count;

    if (msg == NULL || buffer == NULL || written == NULL) {
        return -1;
    }

    expected_count = (uint16_t)((uint16_t)msg->rows * (uint16_t)msg->cols);
    if (expected_count == 0 || expected_count > MAX_MAP_CELLS) {
        return -1;
    }

    if (write_u8(buffer, buffer_len, &offset, msg->rows) != 0 ||
        write_u8(buffer, buffer_len, &offset, msg->cols) != 0) {
        return -1;
    }
    if (ensure_capacity(offset, expected_count, buffer_len) != 0) {
        return -1;
    }

    memcpy(buffer + offset, msg->cells, expected_count);
    offset += expected_count;

    *written = offset;
    return 0;
}

int proto_decode_map_payload(msg_map_t *msg, const uint8_t *buffer, size_t buffer_len) {
    size_t offset = 0;
    uint16_t expected_count;

    if (msg == NULL || buffer == NULL || buffer_len < 2) {
        return -1;
    }

    if (read_u8(buffer, buffer_len, &offset, &msg->rows) != 0 ||
        read_u8(buffer, buffer_len, &offset, &msg->cols) != 0) {
        return -1;
    }

    expected_count = (uint16_t)((uint16_t)msg->rows * (uint16_t)msg->cols);
    if (expected_count == 0 || expected_count > MAX_MAP_CELLS || buffer_len != (size_t)(2 + expected_count)) {
        return -1;
    }

    msg->cell_count = expected_count;
    memcpy(msg->cells, buffer + offset, expected_count);
    return 0;
}

int proto_encode_sync_board_payload(const msg_sync_board_t *msg, uint8_t *buffer, size_t buffer_len, size_t *written) {
    size_t offset = 0;
    size_t index;

    if (msg == NULL || buffer == NULL || written == NULL || msg->player_count > MAX_PLAYERS) {
        return -1;
    }

    if (write_u8(buffer, buffer_len, &offset, msg->status) != 0 ||
        write_u8(buffer, buffer_len, &offset, msg->player_count) != 0) {
        return -1;
    }

    for (index = 0; index < msg->player_count; ++index) {
        if (write_u8(buffer, buffer_len, &offset, msg->players[index].id) != 0 ||
            write_u16_be(buffer, buffer_len, &offset, msg->players[index].cell_index) != 0 ||
            write_u8(buffer, buffer_len, &offset, msg->players[index].alive ? 1u : 0u) != 0 ||
            write_u8(buffer, buffer_len, &offset, msg->players[index].ready ? 1u : 0u) != 0 ||
            write_u8(buffer, buffer_len, &offset, msg->players[index].bomb_count) != 0 ||
            write_u8(buffer, buffer_len, &offset, msg->players[index].bomb_radius) != 0 ||
            write_u16_be(buffer, buffer_len, &offset, msg->players[index].bomb_timer_ticks) != 0 ||
            write_u16_be(buffer, buffer_len, &offset, msg->players[index].speed) != 0) {
            return -1;
        }
    }

    *written = offset;
    return 0;
}

int proto_decode_sync_board_payload(msg_sync_board_t *msg, const uint8_t *buffer, size_t buffer_len) {
    size_t offset = 0;
    size_t expected;
    size_t index;
    uint8_t alive_byte;
    uint8_t ready_byte;

    if (msg == NULL || buffer == NULL || buffer_len < 2) {
        return -1;
    }

    if (read_u8(buffer, buffer_len, &offset, &msg->status) != 0 ||
        read_u8(buffer, buffer_len, &offset, &msg->player_count) != 0) {
        return -1;
    }

    if (msg->player_count > MAX_PLAYERS) {
        return -1;
    }

    expected = 2 + ((size_t)msg->player_count * 11);
    if (buffer_len != expected) {
        return -1;
    }

    for (index = 0; index < msg->player_count; ++index) {
        if (read_u8(buffer, buffer_len, &offset, &msg->players[index].id) != 0 ||
            read_u16_be(buffer, buffer_len, &offset, &msg->players[index].cell_index) != 0 ||
            read_u8(buffer, buffer_len, &offset, &alive_byte) != 0 ||
            read_u8(buffer, buffer_len, &offset, &ready_byte) != 0 ||
            read_u8(buffer, buffer_len, &offset, &msg->players[index].bomb_count) != 0 ||
            read_u8(buffer, buffer_len, &offset, &msg->players[index].bomb_radius) != 0 ||
            read_u16_be(buffer, buffer_len, &offset, &msg->players[index].bomb_timer_ticks) != 0 ||
            read_u16_be(buffer, buffer_len, &offset, &msg->players[index].speed) != 0) {
            return -1;
        }
        msg->players[index].alive = (alive_byte != 0);
        msg->players[index].ready = (ready_byte != 0);
    }

    return 0;
}
