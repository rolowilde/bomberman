#include "client.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define ASCII_ESC_STR "\033"

#define ESC_CLEAR ASCII_ESC_STR "[2J"
#define ESC_SET_CURSOR_HOME ASCII_ESC_STR "[H"
#define ESC_SET_CURSOR_ROW_COL_FS ASCII_ESC_STR "[%zu;%zuH"
#define ESC_HIDE_CURSOR ASCII_ESC_STR "[?25l"
#define ESC_SHOW_CURSOR ASCII_ESC_STR "[?25h"
#define ESC_ENABLE_ALT_SCREEN_BUF ASCII_ESC_STR "[?1049h"
#define ESC_DISABLE_ALT_SCREEN_BUF ASCII_ESC_STR "[?1049l"

#define OUT_FD STDOUT_FILENO
#define MAX_ROWS 512
#define MAX_COLS 512

#define CONST_STRLEN(s) (sizeof(s) - 1)

static bool screen_resized = false;

static size_t screen_rows = 128;
static size_t screen_cols = 128;

/* screen buffer */
static char sb[MAX_ROWS][MAX_COLS];

static char sb_flush_buf[MAX_ROWS * (MAX_COLS + 64) + 64];

static int write_esc(const char *str, size_t len) {
    if (write(OUT_FD, str, len) < 0) {
        perror("write");
        return 1;
    }
    return 0;
}

#define WRITE_ESC(_str_lit) write_esc(_str_lit, CONST_STRLEN(_str_lit))

static void sb_clear(void) {
    for (size_t y = 0; y < screen_rows; y++) {
        for (size_t x = 0; x < screen_cols; x++) {
            sb[y][x] = ' ';
        }
    }
}

static void sb_flush(void) {
    size_t i = 0;
    i += sprintf(sb_flush_buf + i, ESC_SET_CURSOR_ROW_COL_FS, (size_t)1, (size_t)1);
    for (size_t y = 0; y < screen_rows; y++) {

        memcpy(sb_flush_buf + i, sb[y], screen_cols);
        i += screen_cols;

        if (y != screen_rows - 1)
            sb_flush_buf[i++] = '\n';
    }

    write(OUT_FD, sb_flush_buf, i);
}

/* TODO: logging: add logging function to use instead of perror/fprintf, include PID in logging, log player info on
 * start */

static int bound(int x, int bound_min, int bound_max) {
    if (x < bound_min)
        return bound_min;
    if (x > bound_max)
        return bound_max;

    return x;
}

static int update_screen_size(void) {
    /* TODO: clamp to some min screen size (maybe dependent of the map size) */

    struct winsize ws;
    if (ioctl(OUT_FD, TIOCGWINSZ, &ws) < 0) {
        perror("ioctl");
        return 1;
    }

    screen_rows = bound(ws.ws_row, 0, MAX_ROWS);
    screen_cols = bound(ws.ws_col, 0, MAX_COLS);

    return 0;
}

__attribute__((format(printf, 3, 4))) static void draw_text_line_format(size_t row, size_t col, const char *fmt, ...) {
    static char buf[MAX_ROWS];
    va_list args;
    int len;

    va_start(args, fmt);
    len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    memcpy(&sb[row][col], buf, bound(len, 0, screen_cols - col));
}

static void draw_screen_lobby(const client_ctx_t *ctx __attribute__((unused))) {
    /* FIXME: placeholder, should be redone */
    draw_text_line_format(5, 10, "LOBBY");
    draw_text_line_format(6, 10, "commands: ready, w/a/s/d, b, ping, lobby, quit");
}

static char status_char(game_status_t status) {
    if (status == GAME_LOBBY) {
        return 'L';
    }
    if (status == GAME_RUNNING) {
        return 'R';
    }
    return 'E';
}

static void draw_screen_game(const client_ctx_t *ctx) {
    /* FIXME: placeholder, should be redone */
    uint16_t rows;
    uint16_t cols;
    char *grid;
    size_t i;

    if (ctx == NULL || ctx->state.map.cells == NULL) {
        draw_text_line_format(5, 5, "[render] map not available yet");
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

    draw_text_line_format(3, 5, "GAME (status=%c, self=%u)", status_char(ctx->state.status), ctx->player_id);

    for (uint16_t r = 0; r < rows; ++r) {
        for (uint16_t c = 0; c < cols; ++c) {
            sb[r + 5][c * 2 + 5] = grid[make_cell_index(r, c, cols)];
            sb[r + 5][c * 2 + 1 + 5] = ' ';
        }
    }

    draw_text_line_format(rows + 6, 5, "players:");
    for (i = 0; i < MAX_PLAYERS; ++i) {
        const player_t *player = &ctx->state.players[i];
        if (player->id == 0) {
            continue;
        }
        draw_text_line_format(rows + 7 + i, 7, "id=%u alive=%d ready=%d pos=(%u,%u) bombs=%u radius=%u timer=%u speed=%u name=%s", player->id,
               player->alive ? 1 : 0, player->ready ? 1 : 0, player->row, player->col, player->bomb_count,
               player->bomb_radius, player->bomb_timer_ticks, player->speed, player->name);
    }

    free(grid);
}

int client_ui_init(void) {
    int ret = 0;
    ret |= WRITE_ESC(ESC_ENABLE_ALT_SCREEN_BUF);
    ret |= WRITE_ESC(ESC_HIDE_CURSOR);
    ret |= update_screen_size();

    return ret;
}

int client_ui_deinit(void) {
    int ret = 0;
    ret |= WRITE_ESC(ESC_DISABLE_ALT_SCREEN_BUF);
    ret |= WRITE_ESC(ESC_SHOW_CURSOR);

    return ret;
}

void client_ui_update_screen_size(void) {
    screen_resized = true;
}

int client_ui_render(const client_ctx_t *ctx) {
    if (screen_resized) {
        update_screen_size();
        screen_resized = false;
    }

    sb_clear();

    switch (ctx->state.status) {
    case GAME_LOBBY:
        draw_screen_lobby(ctx);
        break;
    case GAME_RUNNING:
        draw_screen_game(ctx);
        break;
    case GAME_END:
        /* FIXME: placeholder */
        draw_text_line_format(5, 5, "GAME END");
        break;
    }

    /* border */
    for (size_t y = 0; y < screen_rows; y++) {
        for (size_t x = 0; x < screen_cols; x++) {
            if (y != 0 && y != screen_rows - 1 && x != 0 && x != screen_cols - 1)
                continue;

            sb[y][x] = '#';
        }
    }

    sb_flush();

    return 0;
}
