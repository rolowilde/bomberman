#ifndef GAME_SERIALIZATION_H
#define GAME_SERIALIZATION_H

#include <stddef.h>
#include <stdint.h>

#include "protocol.h"

int proto_write_header(const msg_header_t *header, uint8_t *buffer, size_t buffer_len);
int proto_read_header(msg_header_t *header, const uint8_t *buffer, size_t buffer_len);

size_t proto_welcome_payload_size(const msg_welcome_t *msg);
size_t proto_map_payload_size(const msg_map_t *msg);
size_t proto_sync_board_payload_size(const msg_sync_board_t *msg);

int proto_encode_hello_payload(const msg_hello_t *msg, uint8_t *buffer, size_t buffer_len, size_t *written);
int proto_decode_hello_payload(msg_hello_t *msg, const uint8_t *buffer, size_t buffer_len);

int proto_encode_welcome_payload(const msg_welcome_t *msg, uint8_t *buffer, size_t buffer_len, size_t *written);
int proto_decode_welcome_payload(msg_welcome_t *msg, const uint8_t *buffer, size_t buffer_len);

int proto_encode_set_status_payload(const msg_set_status_t *msg, uint8_t *buffer, size_t buffer_len, size_t *written);
int proto_decode_set_status_payload(msg_set_status_t *msg, const uint8_t *buffer, size_t buffer_len);

int proto_encode_move_attempt_payload(const msg_move_attempt_t *msg, uint8_t *buffer, size_t buffer_len, size_t *written);
int proto_decode_move_attempt_payload(msg_move_attempt_t *msg, const uint8_t *buffer, size_t buffer_len);

int proto_encode_moved_payload(const msg_moved_t *msg, uint8_t *buffer, size_t buffer_len, size_t *written);
int proto_decode_moved_payload(msg_moved_t *msg, const uint8_t *buffer, size_t buffer_len);

int proto_encode_bomb_attempt_payload(const msg_bomb_attempt_t *msg, uint8_t *buffer, size_t buffer_len, size_t *written);
int proto_decode_bomb_attempt_payload(msg_bomb_attempt_t *msg, const uint8_t *buffer, size_t buffer_len);

int proto_encode_bomb_payload(const msg_bomb_t *msg, uint8_t *buffer, size_t buffer_len, size_t *written);
int proto_decode_bomb_payload(msg_bomb_t *msg, const uint8_t *buffer, size_t buffer_len);

int proto_encode_explosion_start_payload(const msg_explosion_start_t *msg, uint8_t *buffer, size_t buffer_len, size_t *written);
int proto_decode_explosion_start_payload(msg_explosion_start_t *msg, const uint8_t *buffer, size_t buffer_len);

int proto_encode_explosion_end_payload(const msg_explosion_end_t *msg, uint8_t *buffer, size_t buffer_len, size_t *written);
int proto_decode_explosion_end_payload(msg_explosion_end_t *msg, const uint8_t *buffer, size_t buffer_len);

int proto_encode_death_payload(const msg_death_t *msg, uint8_t *buffer, size_t buffer_len, size_t *written);
int proto_decode_death_payload(msg_death_t *msg, const uint8_t *buffer, size_t buffer_len);

int proto_encode_bonus_available_payload(const msg_bonus_available_t *msg, uint8_t *buffer, size_t buffer_len, size_t *written);
int proto_decode_bonus_available_payload(msg_bonus_available_t *msg, const uint8_t *buffer, size_t buffer_len);

int proto_encode_bonus_retrieved_payload(const msg_bonus_retrieved_t *msg, uint8_t *buffer, size_t buffer_len, size_t *written);
int proto_decode_bonus_retrieved_payload(msg_bonus_retrieved_t *msg, const uint8_t *buffer, size_t buffer_len);

int proto_encode_block_destroyed_payload(const msg_block_destroyed_t *msg, uint8_t *buffer, size_t buffer_len, size_t *written);
int proto_decode_block_destroyed_payload(msg_block_destroyed_t *msg, const uint8_t *buffer, size_t buffer_len);

int proto_encode_winner_payload(const msg_winner_t *msg, uint8_t *buffer, size_t buffer_len, size_t *written);
int proto_decode_winner_payload(msg_winner_t *msg, const uint8_t *buffer, size_t buffer_len);

int proto_encode_error_payload(const msg_error_t *msg, uint8_t *buffer, size_t buffer_len, size_t *written);
int proto_decode_error_payload(msg_error_t *msg, const uint8_t *buffer, size_t buffer_len);

int proto_encode_map_payload(const msg_map_t *msg, uint8_t *buffer, size_t buffer_len, size_t *written);
int proto_decode_map_payload(msg_map_t *msg, const uint8_t *buffer, size_t buffer_len);

int proto_encode_sync_board_payload(const msg_sync_board_t *msg, uint8_t *buffer, size_t buffer_len, size_t *written);
int proto_decode_sync_board_payload(msg_sync_board_t *msg, const uint8_t *buffer, size_t buffer_len);

#endif
