#include "../include/protocol.h"

#include <stddef.h>

const char *msg_type_name(uint8_t msg_type) {
    switch (msg_type) {
        case MSG_HELLO:
            return "HELLO";
        case MSG_WELCOME:
            return "WELCOME";
        case MSG_DISCONNECT:
            return "DISCONNECT";
        case MSG_PING:
            return "PING";
        case MSG_PONG:
            return "PONG";
        case MSG_LEAVE:
            return "LEAVE";
        case MSG_ERROR:
            return "ERROR";
        case MSG_MAP:
            return "MAP";
        case MSG_SYNC_BOARD:
            return "SYNC_BOARD";
        case MSG_SET_READY:
            return "SET_READY";
        case MSG_SET_STATUS:
            return "SET_STATUS";
        case MSG_WINNER:
            return "WINNER";
        case MSG_MOVE_ATTEMPT:
            return "MOVE_ATTEMPT";
        case MSG_BOMB_ATTEMPT:
            return "BOMB_ATTEMPT";
        case MSG_MOVED:
            return "MOVED";
        case MSG_BOMB:
            return "BOMB";
        case MSG_EXPLOSION_START:
            return "EXPLOSION_START";
        case MSG_EXPLOSION_END:
            return "EXPLOSION_END";
        case MSG_DEATH:
            return "DEATH";
        case MSG_BONUS_AVAILABLE:
            return "BONUS_AVAILABLE";
        case MSG_BONUS_RETRIEVED:
            return "BONUS_RETRIEVED";
        case MSG_BLOCK_DESTROYED:
            return "BLOCK_DESTROYED";
        default:
            return "UNKNOWN";
    }
}

bool msg_type_variable_payload(uint8_t msg_type) {
    switch (msg_type) {
        case MSG_WELCOME:
        case MSG_ERROR:
        case MSG_MAP:
        case MSG_SYNC_BOARD:
            return true;
        default:
            return false;
    }
}

size_t msg_type_fixed_payload_size(uint8_t msg_type) {
    switch (msg_type) {
        case MSG_HELLO:
            return MAX_ID_LEN + MAX_NAME_LEN;
        case MSG_DISCONNECT:
        case MSG_PING:
        case MSG_PONG:
        case MSG_LEAVE:
        case MSG_SET_READY:
            return 0;
        case MSG_SET_STATUS:
            return 1;
        case MSG_MOVE_ATTEMPT:
            return 1;
        case MSG_BOMB_ATTEMPT:
            return 2;
        case MSG_MOVED:
            return 3;
        case MSG_BOMB:
            return 3;
        case MSG_EXPLOSION_START:
            return 3;
        case MSG_EXPLOSION_END:
            return 2;
        case MSG_DEATH:
            return 1;
        case MSG_BONUS_AVAILABLE:
            return 3;
        case MSG_BONUS_RETRIEVED:
            return 3;
        case MSG_BLOCK_DESTROYED:
            return 2;
        case MSG_WINNER:
            return 1;
        case MSG_WELCOME:
        case MSG_ERROR:
        case MSG_MAP:
        case MSG_SYNC_BOARD:
        default:
            return SIZE_MAX;
    }
}
