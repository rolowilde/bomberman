#include "client.h"

#include <stdio.h>
#include <stdlib.h>

static char status_char(game_status_t status) {
    if (status == GAME_LOBBY) {
        return 'L';
    }
    if (status == GAME_RUNNING) {
        return 'R';
    }
    return 'E';
}

void client_render_state(const client_ctx_t *ctx) {
    uint16_t rows;
    uint16_t cols;
    char *grid;
    size_t i;

    if (ctx == NULL || ctx->state.map.cells == NULL) {
        printf("[render] map not available yet\n");
        return;
    }

    rows = ctx->state.map.rows;
    cols = ctx->state.map.cols;
    grid = (char *)malloc((size_t)rows * (size_t)cols);
    if (grid == NULL) {
        return;
    }

    for (i = 0; i < (size_t)rows * (size_t)cols; ++i) {
        grid[i] = (char)ctx->state.map.cells[i];
    }

    for (i = 0; i < MAX_PLAYERS; ++i) {
        const player_t *player = &ctx->state.players[i];
        uint16_t index;

        if (player->id == 0 || (!player->alive && !player->ready)) {
            continue;
        }
        if (player->row >= rows || player->col >= cols) {
            continue;
        }

        index = make_cell_index(player->row, player->col, cols);
        grid[index] = (char)('0' + (player->id % 10));
    }

    printf("\nGAME (status=%c, self=%u)\n", status_char(ctx->state.status), ctx->player_id);

    for (uint16_t r = 0; r < rows; ++r) {
        for (uint16_t c = 0; c < cols; ++c) {
            putchar(grid[make_cell_index(r, c, cols)]);
            putchar(' ');
        }
        putchar('\n');
    }

    printf("players:\n");
    for (i = 0; i < MAX_PLAYERS; ++i) {
        const player_t *player = &ctx->state.players[i];
        if (player->id == 0) {
            continue;
        }
        printf(
            "  id=%u alive=%d ready=%d pos=(%u,%u) bombs=%u radius=%u timer=%u speed=%u name=%s\n",
            player->id,
            player->alive ? 1 : 0,
            player->ready ? 1 : 0,
            player->row,
            player->col,
            player->bomb_count,
            player->bomb_radius,
            player->bomb_timer_ticks,
            player->speed,
            player->name
        );
    }

    free(grid);
}
