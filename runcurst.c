#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <curses.h>

#include "tusl.h"

static void
panic (void)
{
  fprintf (stderr, "%s", strerror (errno));
  exit (1);
}

static void
setup (void)
{
  initscr ();
  raw ();
  noecho ();
  nonl ();
  intrflush (stdscr, FALSE);
  keypad (stdscr, TRUE);
}

static void
teardown (void)
{
  endwin ();
}

static int
clip (int i, int limit)
{
  if (i < 0) return 0;
  if (limit <= i) return limit;
  return i;
}

static void
blast (int x, int y, const char *buffer, int length)
{
  int x1 = clip (x, COLS);
  int y1 = clip (y, LINES);
  int i, L = length;  /* clip (length, COLS - x1); */
  move (y1, x1);
  for (i = 0; i < L; ++i)
    addch (buffer[i]);
}

static void
redisplay (int cursor_x, int cursor_y)
{
  move (clip (cursor_y, LINES), clip (cursor_x, COLS));
  refresh ();
}

static void
do_blast (ts_VM *vm, ts_Word *pw)
{
  ts_INPUT_4 (vm, w, x, y, z);
  ts_OUTPUT_0 ();
  blast (w, x, ts_data_byte (vm, y), z);
}

static void
do_refresh (ts_VM *vm, ts_Word *pw)
{
  ts_INPUT_2 (vm, y, z);
  ts_OUTPUT_0 ();
  redisplay (y, z);
}

static void
do_screen_size (ts_VM *vm, ts_Word *pw)
{
  ts_INPUT_0 (vm);
  ts_OUTPUT_2 (COLS, LINES);
}

static void
install_curses_words (ts_VM *vm)
{
  ts_install (vm, "screen-setup",    ts_run_void_0, (int) setup);
  ts_install (vm, "screen-teardown", ts_run_void_0, (int) teardown);
  ts_install (vm, "screen-blast",    do_blast, 0);
  ts_install (vm, "screen-refresh",  do_refresh, 0);
  ts_install (vm, "screen-size",     do_screen_size, 0);
  ts_install (vm, "get-key",         ts_run_int_0, (int) getch);
}

int
main (int argc, char **argv)
{
  ts_VM *vm = ts_vm_make ();
  if (NULL == vm)
    panic ();
  ts_set_output_file_stream (vm, stdout, NULL);
  ts_set_input_file_stream (vm, stdin, NULL);
  ts_install_standard_words (vm);
  ts_install_unsafe_words (vm);	/* XXX needed only for 'load' and
				   'with-io-on-file' */
  install_curses_words (vm);

  ts_load (vm, "tuslrc.ts");

  // TODO: catch errors and teardown() if needed
  // (currently it's up to your Tusl program to do this)

  if (1 == argc)
    ts_load_interactive (vm, stdin);
  else
    {
      int i;
      for (i = 1; i < argc; ++i)
	ts_load_string (vm, argv[i]);
    }

  ts_vm_unmake (vm);
  return 0;
}
