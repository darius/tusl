/* TUSL -- the ultimate scripting language.
   Copyright 2003 Darius Bacon under the terms of the MIT X license
   found at http://www.opensource.org/licenses/mit-license.html */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tusl.h"

static void
panic (void)
{
  ts_die (strerror (errno));
}

static int
file_exists (const char *filename)
{
  FILE *f = fopen (filename, "r");
  fclose (f);
  return f != NULL;
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
  ts_install_unsafe_words (vm);
  
  // XXX refactor ts_load so you can pass in a FILE*
  if (file_exists ("tuslrc.ts"))
    ts_load (vm, "tuslrc.ts");
  else
    ts_load (vm, "/usr/local/share/tusl/tuslrc.ts");

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
