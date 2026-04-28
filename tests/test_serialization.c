#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../common/include/serialization.h"

static void test_hello_roundtrip(void) {
    msg_hello_t in;
    msg_hello_t out;
    uint8_t payload[64];
    size_t payload_len = 0;

    memset(&in, 0, sizeof(in));
    strncpy(in.client_id, "client/1.0", MAX_ID_LEN);
    strncpy(in.player_name, "Alice", MAX_NAME_LEN);

    assert(proto_encode_hello_payload(&in, payload, sizeof(payload), &payload_len) == 0);
    assert(payload_len == (MAX_ID_LEN + MAX_NAME_LEN));
    assert(proto_decode_hello_payload(&out, payload, payload_len) == 0);
    assert(strcmp(out.client_id, "client/1.0") == 0);
    assert(strcmp(out.player_name, "Alice") == 0);
}

static void test_welcome_roundtrip(void) {
    msg_welcome_t in;
    msg_welcome_t out;
    uint8_t payload[512];
    size_t payload_len = 0;

    memset(&in, 0, sizeof(in));
    strncpy(in.server_id, "server/1.0", MAX_ID_LEN);
    in.game_status = GAME_LOBBY;
    in.player_count = 2;

    in.players[0].id = 1;
    in.players[0].ready = true;
    strncpy(in.players[0].name, "Alice", MAX_NAME_LEN);

    in.players[1].id = 2;
    in.players[1].ready = false;
    strncpy(in.players[1].name, "Bob", MAX_NAME_LEN);

    assert(proto_encode_welcome_payload(&in, payload, sizeof(payload), &payload_len) == 0);
    assert(proto_decode_welcome_payload(&out, payload, payload_len) == 0);

    assert(strcmp(out.server_id, "server/1.0") == 0);
    assert(out.player_count == 2);
    assert(out.players[0].id == 1);
    assert(out.players[0].ready == true);
    assert(strcmp(out.players[1].name, "Bob") == 0);
}

static void test_map_roundtrip(void) {
    msg_map_t in;
    msg_map_t out;
    uint8_t payload[64];
    size_t payload_len = 0;

    memset(&in, 0, sizeof(in));
    in.rows = 3;
    in.cols = 3;
    in.cell_count = 9;
    in.cells[0] = CELL_EMPTY;
    in.cells[1] = CELL_HARD;
    in.cells[2] = CELL_EMPTY;
    in.cells[3] = CELL_SOFT;
    in.cells[4] = CELL_EMPTY;
    in.cells[5] = CELL_BONUS_RADIUS;
    in.cells[6] = CELL_EMPTY;
    in.cells[7] = CELL_BOMB;
    in.cells[8] = CELL_EMPTY;

    assert(proto_encode_map_payload(&in, payload, sizeof(payload), &payload_len) == 0);
    assert(proto_decode_map_payload(&out, payload, payload_len) == 0);

    assert(out.rows == 3 && out.cols == 3 && out.cell_count == 9);
    assert(memcmp(in.cells, out.cells, 9) == 0);
}

static void test_sync_roundtrip(void) {
    msg_sync_board_t in;
    msg_sync_board_t out;
    uint8_t payload[128];
    size_t payload_len = 0;

    memset(&in, 0, sizeof(in));
    in.status = GAME_RUNNING;
    in.player_count = 2;

    in.players[0].id = 1;
    in.players[0].lives = 1;
    in.players[0].cell_index = 4;
    in.players[0].alive = true;
    in.players[0].ready = true;
    in.players[0].bomb_count = 1;
    in.players[0].bomb_radius = 2;
    in.players[0].bomb_timer_ticks = 60;
    in.players[0].speed = 3;

    in.players[1].id = 2;
    in.players[1].lives = 0;
    in.players[1].cell_index = 7;
    in.players[1].alive = false;
    in.players[1].ready = true;
    in.players[1].bomb_count = 0;
    in.players[1].bomb_radius = 1;
    in.players[1].bomb_timer_ticks = 45;
    in.players[1].speed = 4;

    assert(proto_encode_sync_board_payload(&in, payload, sizeof(payload), &payload_len) == 0);
    assert(proto_decode_sync_board_payload(&out, payload, payload_len) == 0);

    assert(out.status == GAME_RUNNING);
    assert(out.player_count == 2);
    assert(out.players[0].alive == true);
    assert(out.players[1].cell_index == 7);
    assert(out.players[0].lives == 1);
    assert(out.players[0].bomb_radius == 2);
    assert(out.players[0].bomb_timer_ticks == 60);
    assert(out.players[1].speed == 4);
}

int main(void) {
    test_hello_roundtrip();
    test_welcome_roundtrip();
    test_map_roundtrip();
    test_sync_roundtrip();

    printf("test_serialization: OK\n");
    return 0;
}
