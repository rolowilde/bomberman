#include "client.h"

#include <signal.h>
#include <stdio.h>
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

/* TODO: logging: add logging function to use instead of perror/fprintf */

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

int client_ui_render_state_v2(const client_ctx_t *ctx) {
    if (screen_resized) {
        update_screen_size();
        screen_resized = false;
    }

    sb_clear();

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
