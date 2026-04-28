#include "client.h"
#include "config.h"

#include <stdarg.h>
#include <stdint.h>
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

#define CHAR_NOCHAR '\0'

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
    draw_text_line_format(5, 10, "LOBBY");
    draw_text_line_format(7, 10, "- " CONTROLS_STR);
    draw_text_line_format(
        9, 10, "%s", ctx->state.players[ctx->player_id - 1].ready ? "[YOU ARE READY]" : "- press 'r' to get ready");
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
    uint16_t rows;
    uint16_t cols;
    char *grid;
    size_t i;
    const int map_offset_r = 5, map_offset_c = 5;

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
            sb[r + map_offset_r][c * 2 + map_offset_c] = grid[make_cell_index(r, c, cols)];
            sb[r + map_offset_r][c * 2 + 1 + map_offset_c] = ' ';
        }
    }

    for (uint16_t r = 0; r < rows; ++r) {
        for (uint16_t c = 0; c < cols; ++c) {
            int rad = ctx->explosions[r * MAX_MAP_SIDE + c];
            static const int dr[4] = {-1, 1, 0, 0};
            static const int dc[4] = {0, 0, -1, 1};

            if (rad == 0)
                continue;

            for (int dir = 0; dir < 4; dir++) {
                for (int i = 0; i <= rad; i++) {
                    int r2 = r + i * dr[dir];
                    int c2 = c + i * dc[dir];

                    if (r2 < 0 || r2 >= rows || c2 < 0 || c2 >= cols)
                        continue;

                    if (grid[make_cell_index(r2, c2, cols)] != '.')
                        continue;

                    sb[r2 + map_offset_r][c2 * 2 + map_offset_c] = '#';
                }
            }
        }
    }

    draw_text_line_format(rows + 6, 5, "players:");
    for (i = 0; i < MAX_PLAYERS; ++i) {
        const player_t *player = &ctx->state.players[i];
        if (player->id == 0) {
            continue;
        }
        draw_text_line_format(
            rows + 7 + i, 5, "%sid=%u alive=%d ready=%d pos=(%u,%u) bombs=%u radius=%u timer=%u speed=%u name=%s%s",
            player->id == ctx->player_id ? "[" : " ", player->id, player->alive ? 1 : 0, player->ready ? 1 : 0,
            player->row, player->col, player->bomb_count, player->bomb_radius, player->bomb_timer_ticks, player->speed,
            player->name, player->id == ctx->player_id ? "]" : " ");
    }

    free(grid);
}

static void draw_screen_game_over(const client_ctx_t *ctx __attribute__((unused))) {
    draw_text_line_format(5, 5, "GAME OVER");
    draw_text_line_format(7, 5, "- press 'l' to return to lobby");
}

static void draw_rect(char ch_fill, char ch_border, size_t r1, size_t c1, size_t r2, size_t c2) {
    for (size_t y = r1; y <= r2; y++) {
        for (size_t x = c1; x <= c2; x++) {
            if (y != r1 && y != r2 && x != c1 && x != c2) {
                if (ch_fill != CHAR_NOCHAR)
                    sb[y][x] = ch_fill;
            } else {
                if (ch_border != CHAR_NOCHAR)
                    sb[y][x] = ch_border;
            }
        }
    }
}

static void draw_qlog(const client_ctx_t *ctx, size_t r1, size_t c1, size_t r2, size_t c2) {
    draw_rect(' ', '#', r1, c1, r2, c2);
    size_t row = r1 + 1;
    size_t logs_total = (ctx->qlog_end + MAX_CLIENT_LOG_COUNT - ctx->qlog_beg) % MAX_CLIENT_LOG_COUNT;
    size_t logs_visible = bound(r2 - r1 - 1, 0, logs_total);

    for (size_t i = (ctx->qlog_end + MAX_CLIENT_LOG_COUNT - logs_visible) % MAX_CLIENT_LOG_COUNT; i != ctx->qlog_end;
         i = (i + 1) % MAX_CLIENT_LOG_COUNT) {
        if (row >= r2)
            break;

        draw_text_line_format(row, c1 + 2, "%s", ctx->qlog[i]);
        row++;
    }
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
        draw_screen_game_over(ctx);
        break;
    }

    draw_qlog(ctx, screen_rows - 5 - 2 - 1, 0, screen_rows - 1, screen_cols - 1);

    /* border */
    draw_rect(CHAR_NOCHAR, '#', 0, 0, screen_rows - 1, screen_cols - 1);

    sb_flush();

    return 0;
}
