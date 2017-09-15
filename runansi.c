#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "tusl.h"

static void
panic(void) {
    ts_die(strerror(errno));
}

#define ANSI "\033["

enum { COLS = 80, ROWS = 25 }; // for now

static char showing[ROWS][COLS];
static char pending[ROWS][COLS];

static struct termios orig_termios;

static void
setup(void) {
    printf(ANSI "2J");  // clear, home
    memset(showing, ' ', sizeof showing);
    memset(pending, ' ', sizeof pending);

    // from http://viewsourcecode.org/snaptoken/kilo/02.enteringRawMode.html
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) panic();
    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) panic();
}

static void
teardown(void) {
    printf(ANSI "2J");  // clear, home

    // from http://viewsourcecode.org/snaptoken/kilo/02.enteringRawMode.html
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) panic();
}

static int
clip(int i, int limit) {
    if (i < 0) return 0;
    if (limit <= i) return limit;
    return i;
}

static void
blast(int x, int y, const char *buffer, int length) {
    int x1 = clip(x, COLS);
    int y1 = clip(y, ROWS);
    int i, L = clip(length, COLS - x1);
    for (i = 0; i < L; ++i)
        pending[y1][x1 + i] = buffer[i];
}

static void
redisplay(int cursor_x, int cursor_y) {
    int y;
    printf(ANSI "H" ANSI "?25l"); // home, cursor-hide
    for (y = 0; y < ROWS; ++y) {
        if (0 != memcmp(showing[y], pending[y], COLS)) {
            printf(ANSI "%d;1H", y+1); // goto(y+1, 1)
            printf("%*.*s", COLS, COLS, pending[y]);
            memcpy(showing[y], pending[y], COLS);
        }
    }
    printf(ANSI "%d;%dH", cursor_y+1, cursor_x+1); // goto(...)
    printf(ANSI "?25h");  // cursor-show
    fflush(stdout);
}

static void
do_blast(ts_VM *vm, ts_Word *pw) {
    ts_INPUT_4(vm, w, x, y, z);
    ts_OUTPUT_0();
    blast(w, x, ts_data_byte(vm, y), z);
}

static void
do_refresh(ts_VM *vm, ts_Word *pw) {
    ts_INPUT_2(vm, y, z);
    ts_OUTPUT_0();
    redisplay(y, z);
}

static void
do_screen_size(ts_VM *vm, ts_Word *pw) {
    ts_INPUT_0(vm);
    ts_OUTPUT_2(COLS, ROWS);
}

static int
get_byte(void) {
    // from http://viewsourcecode.org/snaptoken/kilo/02.enteringRawMode.html
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) panic();
    }
    return (int) (unsigned char) c;
}

static int
get_key(void) {
    int c = get_byte();
    if (c != '\x1b') return c;
    // escape sequence
    c = get_byte();
    if (c != '[') return 0x100 | c;
    c = get_byte();
    if (isdigit(c)) {
        int accum = c - '0';
        c = get_byte();
        if (isdigit(c)) {
            accum = 10 * accum + c - '0';
            c = get_byte();
        }
        if (c == '~') return 0x200 | accum;  // various special keys
        return 0xFFFF;          // XXX giving up
    }
    switch (c) {
    case 'A': 
    case 'B': 
    case 'C': 
    case 'D': 
        // TODO find some standard keycodes for these
        return 0x400 | c;  // some other special keys
    }
    return 0x800 | c;
}

static void
do_get_key(ts_VM *vm, ts_Word *pw) {
    int c = get_key();
    ts_INPUT_0(vm);
    ts_OUTPUT_1(c);
}

static void
install_curses_words(ts_VM *vm) {
    ts_install(vm, "screen-setup",    ts_run_void_0, (int) setup);
    ts_install(vm, "screen-teardown", ts_run_void_0, (int) teardown);
    ts_install(vm, "screen-blast",    do_blast, 0);
    ts_install(vm, "screen-refresh",  do_refresh, 0);
    ts_install(vm, "screen-size",     do_screen_size, 0);
    ts_install(vm, "get-key",         do_get_key, 0);
}

int
main(int argc, char **argv) {
    ts_VM *vm = ts_vm_make();
    if (NULL == vm) panic();
    ts_set_output_file_stream(vm, stdout, NULL);
    ts_set_input_file_stream(vm, stdin, NULL);
    ts_install_standard_words(vm);
    ts_install_unsafe_words(vm);	/* XXX needed only for 'load' and
                                           'with-io-on-file' */
    install_curses_words(vm);
    
    ts_load(vm, "tuslrc.ts");

    // TODO: catch errors and teardown() if needed
    // (currently it's up to your Tusl program to do this)

    if (1 == argc) {
        ts_load_interactive(vm, stdin);
    } else {
        int i;
        for (i = 1; i < argc; ++i)
            ts_load_string(vm, argv[i]);
    }

    ts_vm_unmake(vm);
    return 0;
}
