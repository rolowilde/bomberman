#include "client.h"

#include <signal.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define ASCII_ESC_STR "\033"

#define ESC_CLEAR ASCII_ESC_STR "[2J"
#define ESC_SET_CURSOR_HOME ASCII_ESC_STR "[H"
#define ESC_SET_CURSOR_ROW_COL_FS ASCII_ESC_STR "[%zu;%zuH"
#define ESC_HIDE_CURSOR ASCII_ESC_STR "[?25l"
#define ESC_SHOW_CURSOR ASCII_ESC_STR "[?25h"

#define OUT_FD STDOUT_FILENO
#define MAX_ROWS 512
#define MAX_COLS 512

#define CONST_STRLEN(s) (sizeof(s) - 1)

volatile sig_atomic_t resized = 0;

size_t screen_rows = 128;
size_t screen_cols = 128;

/* screen buffer */
char sb[MAX_ROWS][MAX_COLS];

static int clear_screen(void) {
    if (write(OUT_FD, ESC_CLEAR, CONST_STRLEN(ESC_CLEAR)) < 0) {
        perror("write");
        return -1;
    }

    if (write(OUT_FD, ESC_SET_CURSOR_HOME, CONST_STRLEN(ESC_SET_CURSOR_HOME)) < 0) {
        perror("write");
        return -1;
    }

    return 0;
}

static int hide_cursor(void) {
    if (write(OUT_FD, ESC_HIDE_CURSOR, CONST_STRLEN(ESC_HIDE_CURSOR)) < 0) {
        perror("write");
        return -1;
    }
}

static int show_cursor(void) {
    if (write(OUT_FD, ESC_SHOW_CURSOR, CONST_STRLEN(ESC_SHOW_CURSOR)) < 0) {
        perror("write");
        return -1;
    }
}

static int set_cursor(size_t row, size_t col) {
    char buf[64];
    int len = snprintf(buf, sizeof(buf), ESC_SET_CURSOR_ROW_COL_FS, row, col);

    if (len < 0) {
        perror("snprintf");
        return -1;
    }

    if (write(OUT_FD, buf, len) < 0) {
        perror("write");
        return -1;
    }

    return 0;
}

static void sb_clear(void) {
    for (size_t i = 0; i < sizeof(sb); i++) {
        *((char *)sb + i) = ' ';
    }
}

static void sb_flush(void) {
    /* TODO */
}

/* TODO: redirect stderr */
/* TODO: add logging function */
/* TODO: log errors differently probably */
/* TODO: log errors differently probably */
/* TODO: handle more errors */

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
        return -1;
    }

    screen_rows = bound(ws.ws_row, 0, MAX_ROWS);
    screen_cols = bound(ws.ws_col, 0, MAX_COLS);

    return 0;
}

int client_ui_init(void) {
    update_screen_size();
    clear_screen();
    hide_cursor(); /* FIXME: is this even needed? */

    return 0;
}

int client_ui_render_state_v2(const client_ctx_t *ctx) {
    if (resized) {
        update_screen_size();
        resized = 0;
    }

    clear_screen();

    /* FIXME: placeholder */
    for (size_t y = 0; y < screen_rows; y++) {
        for (size_t x = 0; x < screen_cols; x++) {
            if (y != 0 && y != screen_rows - 1 && x != 0 && x != screen_cols - 1)
                continue;

            set_cursor(y, x);
            write(OUT_FD, "#", 1);
        }
    }

    return 0;
}
