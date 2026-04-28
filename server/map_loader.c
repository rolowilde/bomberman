#include "server.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static bool is_supported_cell(char c) {
    return c == CELL_EMPTY ||
           c == CELL_HARD ||
           c == CELL_SOFT ||
           c == CELL_BOMB ||
           c == CELL_BONUS_SPEED ||
           c == CELL_BONUS_RADIUS ||
           c == CELL_BONUS_TIMER;
}

int server_load_map(server_ctx_t *ctx, const char *path) {
    FILE *fp;
    int rows;
    int cols;
    int speed;
    int explosion_duration;
    int radius;
    int timer_ticks;
    int r;
    int c;

    fp = fopen(path, "r");
    if (fp == NULL) {
        perror("fopen map");
        return -1;
    }

    if (fscanf(fp, "%d %d %d %d %d %d", &rows, &cols, &speed, &explosion_duration, &radius, &timer_ticks) != 6) {
        fclose(fp);
        fprintf(stderr, "invalid map header\n");
        return -1;
    }

    if (rows <= 0 || cols <= 0 || rows > MAX_MAP_SIDE || cols > MAX_MAP_SIDE) {
        fclose(fp);
        fprintf(stderr, "invalid map dimensions\n");
        return -1;
    }

    if (gs_resize_map(&ctx->state, (uint16_t)rows, (uint16_t)cols) != 0) {
        fclose(fp);
        fprintf(stderr, "cannot allocate map\n");
        return -1;
    }

    memset(ctx->spawn_defined, 0, sizeof(ctx->spawn_defined));

    for (r = 0; r < rows; ++r) {
        for (c = 0; c < cols; ++c) {
            char token[16];
            char cell;

            if (fscanf(fp, " %15s", token) != 1) {
                fclose(fp);
                fprintf(stderr, "not enough map cells\n");
                return -1;
            }

            cell = token[0];
            if (isdigit((unsigned char)cell)) {
                int player_slot = cell - '1';
                if (player_slot < 0 || player_slot >= MAX_PLAYERS) {
                    fclose(fp);
                    fprintf(stderr, "invalid player spawn in map\n");
                    return -1;
                }

                ctx->spawn_defined[player_slot] = true;
                ctx->spawn_rows[player_slot] = (uint16_t)r;
                ctx->spawn_cols[player_slot] = (uint16_t)c;

                if (gs_cell_set(&ctx->state, (uint16_t)r, (uint16_t)c, CELL_EMPTY) != 0) {
                    fclose(fp);
                    return -1;
                }
            } else if (is_supported_cell(cell)) {
                if (gs_cell_set(&ctx->state, (uint16_t)r, (uint16_t)c, (uint8_t)cell) != 0) {
                    fclose(fp);
                    return -1;
                }
            } else {
                fclose(fp);
                fprintf(stderr, "unsupported map token: %c\n", cell);
                return -1;
            }
        }
    }

    fclose(fp);

    ctx->default_speed = (speed <= 0) ? 3 : (uint16_t)speed;
    ctx->explosion_duration_ticks = (explosion_duration <= 0) ? 3 : (uint16_t)explosion_duration;
    ctx->default_bomb_radius = (radius <= 0) ? 1 : (uint8_t)radius;
    ctx->default_bomb_timer_ticks = (timer_ticks <= 0) ? (uint16_t)(3 * TICKS_PER_SECOND) : (uint16_t)timer_ticks;

    ctx->initial_cell_count = (uint16_t)((uint16_t)ctx->state.map.rows * (uint16_t)ctx->state.map.cols);
    memcpy(ctx->initial_cells, ctx->state.map.cells, ctx->initial_cell_count);

    for (c = 0; c < MAX_PLAYERS; ++c) {
        player_t *player = &ctx->state.players[c];
        player->id = (uint8_t)(c + 1);
        player->lives = 0;
        player->alive = false;
        player->ready = false;
        player->bomb_count = 1;
        player->bomb_radius = ctx->default_bomb_radius;
        player->bomb_timer_ticks = ctx->default_bomb_timer_ticks;
        player->speed = ctx->default_speed;

        if (ctx->spawn_defined[c]) {
            player->row = ctx->spawn_rows[c];
            player->col = ctx->spawn_cols[c];
        } else {
            player->row = 0;
            player->col = 0;
        }
    }

    return 0;
}
