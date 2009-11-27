/* TUSL -- the ultimate scripting language.
   Copyright 2003-2005 Darius Bacon under the terms of the MIT X license
   found at http://www.opensource.org/licenses/mit-license.html */

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tusl.h"

/* We try to leave this much space free in the data area for error messages
   to get formatted into. */
enum { reserved_space = 128 };

/* Boolean values.  We use these unusual names because true and false
   may be already taken, yet we can't rely on that either. */
typedef enum { no = 0, yes = 1 } boolean;


/* Source locations */

/* Represent the beginning of a file. */
static ts_Place
make_origin_place (const char *opt_filename)
{
  ts_Place place;
  place.line = 1;
  place.column = 1;
  place.opt_filename = opt_filename;
  return place;
}

/* Update place to reflect reading one character, c. */
static void
advance (ts_Place *place, char c)
{
  if ('\n' == c)
    ++(place->line), place->column = 0;
  else
    ++(place->column);
}

static INLINE int
min (int x, int y)
{
  return x < y ? x : y;
}

/* Format a place the way Emacs likes to see them in error messages. */
static void
print_place (char **dest, int *dest_size, const ts_Place *place)
{
  if (NULL != place->opt_filename && '\0' != place->opt_filename[0])
    {
      int n = min (*dest_size,
                   snprintf (*dest, *dest_size, 
                             "%s:", place->opt_filename));
      *dest += n, *dest_size -= n;
    }
    {
      int n = min (*dest_size,
                   snprintf (*dest, *dest_size, 
                             "%d.%d: ", place->line, place->column));
      *dest += n, *dest_size -= n;
    }
}


/* Exceptions */

/* Complain and terminate the process. */
void
ts_die (const char *plaint)
{
  fprintf (stderr, "%s\n", plaint);
  exit (1);
}

/* Pop the current exception handler and jump to it. */
void 
ts_escape (ts_VM *vm, const char *complaint)
{
  ts_Handler_frame *frame = vm->handler_stack;
  if (NULL == frame)
    ts_die (complaint);
  vm->handler_stack = frame->next;
  frame->complaint = complaint;
  longjmp (frame->state, 1);
}

/* The default error action: format complaint with place. */
/* TODO: elevate the place info into the exception structure
   instead of formatting it into a string here.  This gives us
   that info even in the last resort. */
static const char *
default_error (ts_VM *vm, const char *message, va_list args)
{
  /* We place the complaint inside vm's data space so it can be accessed
     from within the vm without importing unsafe operations. */
  char *buffer = vm->data + vm->here, *scan = buffer;
  int size = vm->there - vm->here;
  if (size < 8)
    return vm->data + 1; /* Holds last-resort error message */
  print_place (&scan, &size, &vm->token_place);
  vsnprintf (scan, size, message, args);
  return buffer;
}

/* Perform vm's current error action on `message' and the following
   (printf-style) arguments, then escape to the current exception
   handler. */
void
ts_error (ts_VM *vm, const char *message, ...)
{
  va_list args;
  const char *complaint;

  /* TODO: It would be nice to flush output here, but that could raise
     another error.  How best to handle this?  Maybe we should just
     forget about buffering, for now. */

  va_start (args, message);
  complaint = vm->error (vm, message, args);
  va_end (args);

  ts_escape (vm, complaint);
}


/* Misc VM operations */

/* Return the index of the top of vm's stack. */
static INLINE int
stack_pointer (ts_VM *vm)
{
  return vm->sp / (int)sizeof vm->stack[0];
}

/* Return a native pointer to cell i in vm's data space. */
static INLINE int *
data_cell (ts_VM *vm, int i)
{
  return (int *)ts_data_byte (vm, i);
}

/* Return the first cell boundary at or after n. */
static INLINE int
cell_align (int n)
{
  return (n + sizeof(int) - 1) & ~(sizeof(int) - 1);
}

static INLINE void
align_here (ts_VM *vm)
{
  vm->here = cell_align (vm->here);
}

/* Raise an error unless we can allot 'size' bytes. */
static void
ensure_space (ts_VM *vm, size_t size)
{
  if (vm->there < vm->here + reserved_space + size)
    ts_error (vm, "Out of space");
}

/* Append an int to vm's data area. */
static void
compile (ts_VM *vm, int c)
{
  align_here (vm);
  ensure_space (vm, sizeof c);
  *data_cell (vm, vm->here) = c;
  vm->here += sizeof c;
}

/* Prepend string to vm's string area, returning its index in data space. */
static int
compile_string (ts_VM *vm, const char *string)
{
  int size = strlen (string) + 1;
  ensure_space (vm, size);
  vm->there -= size;
  strcpy (vm->data + vm->there, string);
  return vm->there;
}

/* Return a copy of s allocated in vm's string space. */
static char *
save_string (ts_VM *vm, const char *s)
{
  return (char *) ts_data_byte (vm, compile_string (vm, s));
}

void ts_print_stack (ts_VM *vm, ts_Word *pw);

/* The default tracing action: print the current word and stack to
   stderr. */
int
ts_default_tracer (ts_VM *vm, unsigned word)
{
  char buffer[80];
  if (word < (unsigned) vm->where)
    snprintf (buffer, sizeof buffer, "trace: %-12s", vm->words[word].name);
  else
    snprintf (buffer, sizeof buffer, "trace: bad word #%u", word);
  ts_put_string (vm, buffer, strlen (buffer));
  ts_print_stack (vm, NULL);
  return no;
}

/* Push `c' onto vm's stack. */
void
ts_push (ts_VM *vm, int c)
{
  ts_INPUT_0 (vm);
  ts_OUTPUT_1 (c);
}

/* Return the top popped off vm's stack. */
int
ts_pop (ts_VM *vm)
{
  ts_INPUT_1 (vm, z);
  ts_OUTPUT_0 ();
  return z;
}


/* I/O streams */

/* Initialize a stream with the given closure. */
void
ts_set_stream (ts_Stream *stream, ts_Streamer *streamer, void *data,
               const char *opt_filename)
{
  stream->ptr = stream->limit = stream->buffer;
  stream->streamer = streamer;
  stream->data = data;
  stream->place = make_origin_place (opt_filename);
}

/* Throw away any characters already buffered from vm's input. */
static void
discard_input (ts_VM *vm)
{
  ts_Stream *input = &vm->input;
  for (; input->ptr < input->limit; ++(input->ptr))
    advance (&input->place, input->ptr[0]);
  input->ptr = input->limit = input->buffer;
}

/* Refill vm's input buffer from its input source, return the first
   new character, and consume it if delta == 1.
   Pre: 0 <= delta <= 1 */
static int
refill (ts_VM *vm, int delta)
{
  ts_Stream *input = &vm->input;
  int nread = input->streamer (vm);
  if (nread <= 0)
    return EOF;
  input->ptr = input->buffer;
  input->limit = input->buffer + nread;
  {
    int result = input->ptr[0];
    if (delta)
      {
        advance (&input->place, result);
        ++(input->ptr);
      }
    return result;
  }
}

/* Force any buffered output characters onto the output sink. */
void
ts_flush_output (ts_VM *vm)
{
  ts_Stream *output = &vm->output;
  /* TODO: allow streamer to only partially flush the buffer? */
  output->streamer (vm);
  output->ptr = output->buffer;
  output->limit = output->buffer + sizeof output->buffer;
}

/* A ts_Streamer that always errors. */
static int
null_streamer (ts_VM *vm)
{
  /* Watch out!  This very null_streamer might be the sink the error
     handler tries to write to. */
  ts_error (vm, "No source or sink set for I/O stream");
  return 0;
}

/* A ts_Streamer that reads from a FILE *. */
static int
read_from_file (ts_VM *vm)
{
  ts_Stream *input = &vm->input;
  FILE *fp = (FILE *) input->data;
  /* FIXME: handle null bytes */
  if (NULL != fgets (input->buffer, sizeof input->buffer, fp))
    return strlen (input->buffer); 
  if (ferror (fp))
    ts_error (vm, "Read error: %s", strerror (errno));
  return 0;
}

/* A ts_Streamer that writes to a FILE *. */
static int
write_to_file (ts_VM *vm)
{
  ts_Stream *output = &vm->output;
  FILE *fp = (FILE *) output->data;
  int n = output->ptr - output->buffer;
  if (n != fwrite (output->buffer, 1, n, fp))
    ts_error (vm, "Write error: %s", strerror (errno));
  return n;
}

/* Disable vm's input & output streams. */
void
ts_disable_IO (ts_VM *vm)
{
  ts_set_stream (&vm->input, null_streamer, NULL, NULL);
  ts_set_stream (&vm->output, null_streamer, NULL, NULL);
}

/* Set vm's input to come from fp. */
void
ts_set_input_file_stream (ts_VM *vm, FILE *fp, const char *opt_filename)
{
  ts_set_stream (&vm->input, read_from_file, (void *) fp, opt_filename);
}

/* Set vm's output to go to fp. */
void
ts_set_output_file_stream (ts_VM *vm, FILE *fp, const char *opt_filename)
{
  ts_set_stream (&vm->output, write_to_file, (void *) fp, opt_filename);
}

/* A ts_Streamer for inputs that never need refilling. */
static int
never_refill (ts_VM *vm)
{
  return 0;
}

/* Set vm's input to come from string.  You should not mutate the string
   after this until the input has all been read. */
void
ts_set_input_string (ts_VM *vm, const char *string)
{
  ts_Stream *input = &vm->input;
  /* Normally ptr and limit are always within buffer, but this time
     we cheat and use the string directly without copying it. */
  input->ptr = (char *)string;
  input->limit = (char *)string + strlen (string);
  input->streamer = never_refill;
  input->data = NULL;
}

/* Consume and return one character (or EOF) from vm's input source. */
static INLINE int
get_char (ts_VM *vm)
{
  ts_Stream *input = &vm->input;
  if (input->ptr == input->limit)
    return refill (vm, 1);
  {
    int result = input->ptr++[0];
    advance (&input->place, result);
    return result;
  }
}

/* Return one character (or EOF) from vm's input source, without
   consuming it. */
static INLINE int
peek_char (ts_VM *vm)
{
  ts_Stream *input = &vm->input;
  if (input->ptr == input->limit)
    return refill (vm, 0);
  return input->ptr[0];
}

#if 0
void
ts_put_string (ts_VM *vm, const char *string, int size)
{
  ts_Stream *output = &vm->output;
  int i, newline = no;
  for (i = 0; i < size; ++i)
    {
      if (output->ptr == output->limit)
        ts_flush_output (vm);
      output->ptr++[0] = string[i];
      if ('\n' == string[i])
        newline = yes;
    }
  if (newline)
    ts_flush_output (vm);
}
#endif

/* Write string (of length size) to vm's output. */
void
ts_put_string (ts_VM *vm, const char *string, int size)
{
  ts_Stream *output = &vm->output;
  int i;
  for (i = 0; i < size; ++i)
    {
      if (output->ptr == output->limit)
        ts_flush_output (vm);
      output->ptr++[0] = string[i];
      if ('\n' == string[i])
        ts_flush_output (vm);
    }
}

/* Write c to vm's output. */
void
ts_put_char (ts_VM *vm, char c)
{
  ts_put_string (vm, &c, 1);
}

/* Write n, formatted as a decimal number, to vm's output. */
static void
put_decimal (ts_VM *vm, int n)
{
  char s[42];
  ts_put_string (vm, s, sprintf (s, "%d", n));
}

/* Write d, formatted as a decimal float number, to vm's output. */
static void
put_double (ts_VM *vm, double d)
{
  char s[42];
  ts_put_string (vm, s, sprintf (s, "%.20g", d));
}


/* The dictionary */

enum { 
  EXIT = 0,                     /* Dictionary index of the ";" word */
  LITERAL,            /* Dictionary index of the "<<literal>>" word */
  BRANCH,              /* Dictionary index of the "<<branch>>" word */
  LOCAL0,
  LOCAL1,
  LOCAL2,
  LOCAL3,
  LOCAL4,
  GRAB1,
  GRAB2,
  GRAB3,
  GRAB4,
  GRAB5,
  WILL,
  DO_WILL,
  LAST_SPECIAL_PRIM = DO_WILL
};

/* Return the index of the last-defined word named `name', or else
   ts_not_found. */
int
ts_lookup (ts_VM *vm, const char *name)
{
  int i;
  /* First check if it's a local */
  for (i = 0; i < vm->local_words; ++i)
    {
      int j = ts_dictionary_size - i - 1;
      if (NULL != vm->words[j].name && 
          0 == strcmp (name, vm->words[j].name))
        return LOCAL0 + vm->local_words - i - 1; 
                        /* TODO: understand this reversal */
    }
  /* Otherwise check the main dictionary */
  for (i = vm->where - 1; 0 <= i; --i)
    if (NULL != vm->words[i].name && 
        0 == strcmp (name, vm->words[i].name))
      return i;
  return ts_not_found;
}

/* Add a word named `name' to vm's dictionary.  The name is not copied,
   so you shouldn't reuse its characters for anything else. */
void
ts_install (ts_VM *vm, char *name, ts_Action *action, int datum)
{
  if (ts_dictionary_size <= vm->where + vm->local_words)
    ts_error (vm, "Too many words");
  if (0)
    if (ts_not_found != ts_lookup (vm, name))
      /* FIXME: this is pretty crude -- sometimes we won't want to be
         bothered by these warnings */
      fprintf (stderr, "Warning: redefinition of %s\n", name);
  {
    ts_Word *w = vm->words + vm->where++;
    w->action = action;
    w->datum = datum;
    w->name = name;
  }
}

enum { max_locals = 5 };

/* Add 'name' to the current set of local variables. */
static void
install_local (ts_VM *vm, const char *name)
{
  if (ts_dictionary_size <= vm->where + vm->local_words + 1)
    ts_error (vm, "Too many words");
  if (max_locals <= vm->local_words)
    ts_error (vm, "Too many locals");
  {
    int size = strlen (name) + 1;
    if (sizeof vm->local_names < vm->local_names_ptr + size)
      ts_error (vm, "Local names too long");

    vm->local_words++;
    {
      ts_Word *w = vm->words + ts_dictionary_size - vm->local_words;
      w->action = NULL;
      w->datum = 0;
      w->name = vm->local_names + vm->local_names_ptr;
      strcpy (w->name, name);
      vm->local_names_ptr += size;
    }
  }
}


/* VM creation/destruction and special primitives */

/* Primitive to push a literal value. */
static void
ts_do_literal (ts_VM *vm, ts_Word *pw)
{
  ts_INPUT_0 (vm); 
  ts_OUTPUT_1 (vm->pc++[0]); 
}

/* Primitive to pop, then jump if zero. */
void
ts_do_branch (ts_VM *vm, ts_Word *pw) 
{
  ts_INPUT_1 (vm, z);
  int y = vm->pc++[0];
  if (0 == z)
    vm->pc = data_cell (vm, y);
  ts_OUTPUT_0 ();
}

static void
ts_do_will (ts_VM *vm, ts_Word *pw);

/* Reclaim a vm. */
void
ts_vm_unmake (ts_VM *vm)
{
  ts_Stream *output = &vm->output;
  if (output->buffer < output->ptr)
    ts_flush_output (vm);
  free (vm);
}

#define last_resort_error_message "No space for complaint"

/* Return a newly malloc'd vm, or NULL if out of memory.  Its
   dictionary and data area are empty except for certain reserved
   entries. */
ts_VM *
ts_vm_make (void)
{
  ts_VM *vm = malloc (sizeof *vm);
  if (NULL == vm)
    return NULL;

  strcpy (vm->data + 1, last_resort_error_message);
  vm->sp = -((int) sizeof vm->stack[0]);
  vm->pc = NULL;
  vm->here = cell_align (1 + sizeof last_resort_error_message);
  vm->there = ts_data_size;
  vm->where = 0;
  vm->local_words = 0;
  vm->mode = '(';
  ts_disable_IO (vm);
  vm->token_place = vm->input.place;
  vm->error = default_error;
  vm->error_data = NULL;
  vm->tracer = NULL;
  vm->tracer_data = NULL;
  vm->colon_tracer = NULL;
  vm->colon_tracer_data = NULL;
  vm->handler_stack = NULL;

  /* Internals depend on the order of these first definitions;
     see enums below. */
  ts_install (vm, ";",            NULL, 0);
  ts_install (vm, "<<literal>>",  ts_do_literal, 0);
  ts_install (vm, "<<branch>>",   ts_do_branch, 0);
  ts_install (vm, "z",            NULL, 0);
  ts_install (vm, "y",            NULL, 0);
  ts_install (vm, "x",            NULL, 0);
  ts_install (vm, "w",            NULL, 0);
  ts_install (vm, "v",            NULL, 0);
  ts_install (vm, "z-",           NULL, 0);
  ts_install (vm, "yz-",          NULL, 0);
  ts_install (vm, "xyz-",         NULL, 0);
  ts_install (vm, "wxyz-",        NULL, 0);
  ts_install (vm, "vwxyz-",       NULL, 0);
  ts_install (vm, ";will",        NULL, 0);
  ts_install (vm, "<<will>>",     ts_do_will, 0);
  /* XXX I should initialize the actions of all remaining dictionary
     entries to an undefined_word() action... or at least null them
     out.  Though the 'where check' in do_sequence() makes running an
     undefined word at least very unlikely... is it the only check we
     really need? */

  return vm;
}

/* Compile a literal value to be pushed at runtime. */
static void
compile_push (ts_VM *vm, int c)
{
  compile (vm, LITERAL);
  compile (vm, c);
}


/* Primitives */

/* Execute a colon definition. */
static void 
do_sequence (ts_VM *vm, ts_Word *pw) 
{
  if (NULL != vm->colon_tracer && vm->colon_tracer (vm, pw))
    return;
  {
    int locals[max_locals];
    int *old_pc = vm->pc;
    vm->pc = data_cell (vm, pw->datum);

    {                    /* TODO: eliminate overhead of setjmp here */
      ts_TRY (vm, frame)
        {
          for (;;)
            {               /* This code also appears in ts_run(). */
              unsigned word = *(vm->pc)++;

              if (NULL != vm->tracer && vm->tracer (vm, word))
                break;

              if (EXIT == word)
                break;
              else if ((unsigned)(word - LOCAL0) < (unsigned)max_locals)
                ts_push (vm, locals[word - LOCAL0]);
              else if ((unsigned)(word - GRAB1) < (unsigned)max_locals)
                {                               /* Grab locals */
                  int i, count = 1 + (word - GRAB1);
                  for (i = 0; i < count; ++i)
                    locals[i] = ts_pop (vm); /* TODO: speed up */
                }
              else if (WILL == word)
                {
                  /*
                    Post:
                    word: action = ts_do_will, datum = p
                    p: script_location
                  */
                  ts_Word *w = vm->words + vm->where - 1;
                  w->action = ts_do_will;
                  *data_cell (vm, w->datum) = (char*)vm->pc - vm->data;
                  break;
                }
              else if (word < (unsigned)(vm->where))
                {
                  ts_Action *action = vm->words[word].action;
                  if (do_sequence == action && EXIT == vm->pc[0])
                    {           /* tail call */
                      if (NULL != vm->colon_tracer && 
                          vm->colon_tracer (vm, pw))
                        return;
                      vm->pc = data_cell (vm, vm->words[word].datum);
                    }
                  else
                    action (vm, &(vm->words[word]));
                }
              else
                ts_error (vm, "Invoked an undefined word, #%d", word);
            }

          vm->pc = old_pc;
          ts_POP_TRY (vm, frame);
        }
      ts_EXCEPT (vm, frame)
        {
          vm->pc = old_pc;
          ts_escape (vm, frame.complaint);
        }
    }
  }
}

/* Like ts_run, but catching exceptions, restoring the stack pointer,
   and pushing an indicator of whether an exception was caught. */
static void
ts_catch (ts_VM *vm, ts_Word *pw)
{
  ts_INPUT_1 (vm, word);
  ts_OUTPUT_0 ();
  {
    int sp = vm->sp;
    ts_TRY (vm, frame)
      {
        ts_run (vm, word);
        ts_push (vm, 0);
      }
    ts_EXCEPT (vm, frame)
      {
        vm->sp = sp;
        ts_push (vm, frame.complaint - vm->data);
      }
  }
}

/* Throw an exception. */
static void
ts_throw (ts_VM *vm, ts_Word *pw)
{
  ts_INPUT_1 (vm, complaint);
  ts_OUTPUT_0 ();
  if (0 != complaint)
    ts_escape (vm, (const char *) ts_data_byte (vm, complaint));
}

/* Execute the word that's at the given dictionary index. */
void
ts_run (ts_VM *vm, int word)
{
  /* This is from do_sequence, minus the looping and the words that
     make no sense outside an instruction sequence. */
  do {
    if (NULL != vm->tracer && vm->tracer (vm, word))
      break;

    if ((unsigned)word <= LAST_SPECIAL_PRIM)
      ts_error (vm, "execute of a sequential-only word: %d", word);
    else if ((unsigned)word < (unsigned)(vm->where))
      vm->words[word].action (vm, &(vm->words[word]));
    else
      ts_error (vm, "Invoked an undefined word, #%d", word);
  } while (0);
}

/* The behavior of a word whose action was set by ;will. */
static void
ts_do_will (ts_VM *vm, ts_Word *pw)
{
  /*
    Pre:
    word: action = ts_do_will, datum = p
    p: script_location
  */
  ts_Word phony;
  phony.action = NULL;
  phony.datum = *data_cell (vm, pw->datum);
  phony.name = NULL;

  ts_push (vm, pw->datum + sizeof(int));
  do_sequence (vm, &phony);
}

/* define<n> defines a primitive with <n> inputs popped from the
   stack.  Those top-of-stack inputs are named x, y, and z within the
   body (with z being the top, y being next, etc.). */
#define define0(name, code) \
  void name (ts_VM *vm, ts_Word *pw) { ts_INPUT_0 (vm); code }
#define define1(name, code) \
  void name (ts_VM *vm, ts_Word *pw) { ts_INPUT_1 (vm, z); code }
#define define2(name, code) \
  void name (ts_VM *vm, ts_Word *pw) { ts_INPUT_2 (vm, y, z); code }

define0 (ts_do_push,      ts_OUTPUT_1 (pw->datum); )

define1 (ts_make_literal, ts_OUTPUT_0 (); compile_push (vm, z); )
define1 (ts_execute,      ts_OUTPUT_0 (); ts_run (vm, z); )
define1 (ts_to_data,      ts_OUTPUT_1 ((int) ts_data_byte (vm, z)); )
define1 (ts_comma,        ts_OUTPUT_0 (); compile (vm, z); )
define1 (ts_allot,        ts_OUTPUT_0 (); ensure_space (vm, z); vm->here += z;)
define0 (ts_align_bang,   ts_OUTPUT_0 (); align_here (vm); )
define0 (ts_here,         ts_OUTPUT_1 (vm->here); )
define0 (ts_there,        ts_OUTPUT_1 (vm->there); )
define0 (ts_where,        ts_OUTPUT_1 (vm->where); )
define1 (ts_string_comma, ts_OUTPUT_1 (compile_string (vm, 
                                                       ts_data_byte (vm, 
                                                                     z))); )

static INLINE void
nonzero (ts_VM *vm, int z)
{
  if (0 == z)
    ts_error (vm, "Division by 0");
}

define2 (ts_add,          ts_OUTPUT_1 (y + z); )
define2 (ts_sub,          ts_OUTPUT_1 (y - z); )
define2 (ts_mul,          ts_OUTPUT_1 (y * z); )
define2 (ts_umul,         ts_OUTPUT_1 ((unsigned)y * (unsigned)z); )
define2 (ts_idiv, nonzero (vm, z); ts_OUTPUT_1 (y / z); )
define2 (ts_imod, nonzero (vm, z); ts_OUTPUT_1 (y % z); )
define2 (ts_udiv, nonzero (vm, z); ts_OUTPUT_1 ((unsigned)y / (unsigned)z); )
define2 (ts_umod, nonzero (vm, z); ts_OUTPUT_1 ((unsigned)y % (unsigned)z); )
define2 (ts_eq,           ts_OUTPUT_1 (-(y == z)); )
define2 (ts_lt,           ts_OUTPUT_1 (-(y < z)); )
define2 (ts_ult,          ts_OUTPUT_1 (-((unsigned)y < (unsigned)z)); )
define2 (ts_and,          ts_OUTPUT_1 (y & z); )
define2 (ts_or,           ts_OUTPUT_1 (y | z); )
define2 (ts_xor,          ts_OUTPUT_1 (y ^ z); )
define2 (ts_lshift,       ts_OUTPUT_1 (y << z); )
define2 (ts_rshift,       ts_OUTPUT_1 (y >> z); )
define2 (ts_urshift,      ts_OUTPUT_1 ((unsigned)y >> (unsigned)z); )

define1 (ts_fetchu,       ts_OUTPUT_1 (*(int *)z); )
define1 (ts_cfetchu,      ts_OUTPUT_1 (*(unsigned char *)z); )
define2 (ts_storeu,       ts_OUTPUT_0 (); *(int *)z = y; )
define2 (ts_cstoreu,      ts_OUTPUT_0 (); *(unsigned char *)z = y; )
define2 (ts_plus_storeu,  ts_OUTPUT_0 (); *(int *)z += y; )

define1 (ts_fetch,        ts_OUTPUT_1 (*data_cell (vm, z)); )
define1 (ts_cfetch,       ts_OUTPUT_1 (*(unsigned char*)ts_data_byte (vm, z)); )
define2 (ts_store,        ts_OUTPUT_0 (); *data_cell (vm, z) = y; )
define2 (ts_cstore,       ts_OUTPUT_0 (); *ts_data_byte (vm, z) = y; )
define2 (ts_plus_store,   ts_OUTPUT_0 (); *data_cell (vm, z) += y; )

define0 (ts_start_tracing,ts_OUTPUT_0 (); vm->tracer = ts_default_tracer; )
define0 (ts_stop_tracing, ts_OUTPUT_0 (); vm->tracer = NULL; )

define1 (ts_add2,         ts_OUTPUT_1 (z + 2); )
define1 (ts_add1,         ts_OUTPUT_1 (z + 1); )
define1 (ts_sub1,         ts_OUTPUT_1 (z - 1); )
define1 (ts_sub2,         ts_OUTPUT_1 (z - 2); )
define1 (ts_is_negative,  ts_OUTPUT_1 (-(z < 0)); )
define1 (ts_is_zero,      ts_OUTPUT_1 (-(0 == z)); )
define1 (ts_times2,       ts_OUTPUT_1 (z << 1); )
define1 (ts_times4,       ts_OUTPUT_1 (z << 2); )
define1 (ts_div2,         ts_OUTPUT_1 (z >> 1); )
define1 (ts_div4,         ts_OUTPUT_1 (z >> 2); )

define1 (ts_emit,         ts_OUTPUT_0 (); ts_put_char (vm, z); )
define1 (ts_print,        ts_OUTPUT_0 (); 
                          put_decimal (vm, z); ts_put_char (vm, ' '); )
define0 (ts_absorb,       ts_OUTPUT_1 (get_char (vm)); )

define1 (ts_prim_error,   ts_OUTPUT_0 (); 
                          ts_error (vm, "%s", ts_data_byte (vm, z)); )

define0 (ts_repl,         ts_OUTPUT_0 (); ts_load_interactive (vm, stdin); )
define1 (ts_prim_load,    ts_OUTPUT_0 (); ts_load (vm, ts_data_byte (vm, z)); )

/* Pop the top of stack (call it z), and change the last-defined word
   to be a constant with value z. */
void
ts_make_constant (ts_VM *vm, ts_Word *pw)
{
  ts_Word *w = vm->words + vm->where - 1;
  ts_INPUT_1 (vm, z);
  ts_OUTPUT_0 ();
  w->action = ts_do_push;
  w->datum = z;
}

/* Given a name, define a new word (as a colon definition). */
void
ts_create (ts_VM *vm, ts_Word *pw)
{
  ts_INPUT_1 (vm, z);
  ts_OUTPUT_0 ();
  ts_install (vm, ts_data_byte (vm, z), do_sequence, vm->here);
}

/* Given a name, add it to the current set of local variables. */
void
ts_create_local (ts_VM *vm, ts_Word *pw)
{
  ts_INPUT_1 (vm, z);
  ts_OUTPUT_0 ();
  install_local (vm, ts_data_byte (vm, z));
}

/* Look up a word by name. */
void
ts_find (ts_VM *vm, ts_Word *pw)
{
  ts_INPUT_1 (vm, z);
  int w = ts_lookup (vm, (const char *)ts_data_byte (vm, z));
  if (ts_not_found == w)
    ts_OUTPUT_2 (z, no);
  else
    ts_OUTPUT_2 (w, -yes);
}

static void
compile_grab (ts_VM *vm, ts_Word *pw)
{
  if (0 < vm->local_words)
    compile (vm, GRAB1 + vm->local_words - 1);
}

static void
reset_locals (ts_VM *vm, ts_Word *pw)
{
  vm->local_words = 0;
  vm->local_names_ptr = 0;
}

/* Print vm's stack as decimal numbers to vm's output. */
void
ts_print_stack (ts_VM *vm, ts_Word *pw)
{
  int i;
  for (i = 0; i <= stack_pointer (vm); ++i)
    {
      if (0 < i)
        ts_put_char (vm, ' ');
      put_decimal (vm, vm->stack[i]);
    }
  ts_put_char (vm, '\n');
}

/* Make vm's stack empty. */
void
ts_clear_stack (ts_VM *vm, ts_Word *pw)
{
  vm->sp = -((int) sizeof vm->stack[0]);
}


/* Return yes iff string s is all whitespace. */
static boolean
all_blank (const char *s)
{
  for (; *s; ++s)
    if (!isspace (*s))
      return no;
  return yes;
}

/* Try to parse text as a number (either signed or unsigned).
   Return yes iff successful, and set *result to the value. */
static boolean
parse_number (int *result, const char *text)
{
  char *endptr;
  int value;

  if ('\0' == text[0])
    return no;

  errno = 0;
  value = strtol (text, &endptr, 0);
  if (!all_blank (endptr) || ERANGE == errno)
    {
      errno = 0;
      value = strtoul (text, &endptr, 0);
      if ('\0' != *endptr || ERANGE == errno)
	{
	  /* Ugly hack to more or less support float constants */
	  float fvalue;
	  errno = 0, fvalue = (float) strtod (text, &endptr);
	  if (!all_blank (endptr) || ERANGE == errno)
	    return no;
	  value = *(int *)&fvalue;
	}
    }

  *result = value;
  return yes;
}

/* Convert a string to number; push the result and a success/failure flag. 
   (On failure, the 'result' is the original string.) */
void 
ts_parse_number (ts_VM *vm, ts_Word *pw)
{
  ts_INPUT_1 (vm, z);
  int n = 0;
  /* TODO: change this to have an interface more like strtol():
     output end_ptr, n, -ok */
  if (parse_number (&n, (const char *)ts_data_byte (vm, z)))
    ts_OUTPUT_2 (n, yes);
  else
    ts_OUTPUT_2 (z, no);
}


/* C interface primitives.  Each run_foo_n primitive calls its
   pw->datum as a C function pointer taking n arguments and returning
   foo (either void or a single int value).  The n arguments are first
   popped from the stack (with the topmost as the rightmost argument).
   If there's a return value, it is then pushed on the stack. */

void
ts_run_void_0 (ts_VM *vm, ts_Word *pw)
{
  void (*f)(void) = (void (*)(void)) pw->datum;
  f ();
}

void
ts_run_void_1 (ts_VM *vm, ts_Word *pw)
{
  void (*f)(int) = (void (*)(int)) pw->datum;
  ts_INPUT_1 (vm, z);
  ts_OUTPUT_0 ();
  f (z);
}

void
ts_run_void_2 (ts_VM *vm, ts_Word *pw)
{
  void (*f)(int, int) = (void (*)(int, int)) pw->datum;
  ts_INPUT_2 (vm, y, z);
  ts_OUTPUT_0 ();
  f (y, z);
}

void
ts_run_void_3 (ts_VM *vm, ts_Word *pw)
{
  void (*f)(int, int, int) = (void (*)(int, int, int)) pw->datum;
  ts_INPUT_3 (vm, x, y, z);
  ts_OUTPUT_0 ();
  f (x, y, z);
}

void
ts_run_void_4 (ts_VM *vm, ts_Word *pw)
{
  void (*f)(int, int, int, int) = (void (*)(int, int, int, int)) pw->datum;
  ts_INPUT_4 (vm, w, x, y, z);
  ts_OUTPUT_0 ();
  f (w, x, y, z);
}

void
ts_run_void_5 (ts_VM *vm, ts_Word *pw)
{
  void (*f)(int, int, int, int, int) = 
    (void (*)(int, int, int, int, int)) pw->datum;
  ts_INPUT_5 (vm, v, w, x, y, z);
  ts_OUTPUT_0 ();
  f (v, w, x, y, z);
}

void
ts_run_int_0 (ts_VM *vm, ts_Word *pw)
{
  int (*f)(void) = (int (*)(void)) pw->datum;
  ts_INPUT_0 (vm);
  ts_OUTPUT_1 (f ());
}

void
ts_run_int_1 (ts_VM *vm, ts_Word *pw)
{
  int (*f)(int) = (int (*)(int)) pw->datum;
  ts_INPUT_1 (vm, z);
  ts_OUTPUT_1 (f (z));
}

void
ts_run_int_2 (ts_VM *vm, ts_Word *pw)
{
  int (*f)(int, int) = (int (*)(int, int)) pw->datum;
  ts_INPUT_2 (vm, y, z);
  ts_OUTPUT_1 (f (y, z));
}

void
ts_run_int_3 (ts_VM *vm, ts_Word *pw)
{
  int (*f)(int, int, int) = (int (*)(int, int, int)) pw->datum;
  ts_INPUT_3 (vm, x, y, z);
  ts_OUTPUT_1 (f (x, y, z));
}

void
ts_run_int_4 (ts_VM *vm, ts_Word *pw)
{
  int (*f)(int, int, int, int) = (int (*)(int, int, int, int)) pw->datum;
  ts_INPUT_4 (vm, w, x, y, z);
  ts_OUTPUT_1 (f (w, x, y, z));
}


/* Floating-point primitives.  These are easy to misuse since floats
   get mixed with ints on the stack without any typechecking. */

static INLINE float i2f (int i) { return *(float*)&i; }
static INLINE int f2i (float f) { return *(int*)&f; }

define2 (ts_fadd, ts_OUTPUT_1 (f2i (i2f (y) + i2f (z))); )
define2 (ts_fsub, ts_OUTPUT_1 (f2i (i2f (y) - i2f (z))); )
define2 (ts_fmul, ts_OUTPUT_1 (f2i (i2f (y) * i2f (z))); )
define2 (ts_fdiv, ts_OUTPUT_1 (f2i (i2f (y) / i2f (z))); )

define1 (ts_fprint, ts_OUTPUT_0 (); 
	            put_double (vm, i2f (z)); ts_put_char (vm, ' '); )


/* Add all the safe built-in primitives to vm's dictionary. */
void
ts_install_standard_words (ts_VM *vm)
{
  ts_install (vm, "+",            ts_add, 0);
  ts_install (vm, "-",            ts_sub, 0);
  ts_install (vm, "*",            ts_mul, 0);
  ts_install (vm, "/",            ts_idiv, 0);
  ts_install (vm, "mod",          ts_imod, 0); /* TODO: rename to % ? */
  ts_install (vm, "u*",           ts_umul, 0);
  ts_install (vm, "u/",           ts_udiv, 0);
  ts_install (vm, "umod",         ts_umod, 0);
  ts_install (vm, "=",            ts_eq, 0);
  ts_install (vm, "<",            ts_lt, 0);
  ts_install (vm, "u<",           ts_ult, 0);
  ts_install (vm, "and",          ts_and, 0);
  ts_install (vm, "or",           ts_or, 0);
  ts_install (vm, "xor",          ts_xor, 0);
  ts_install (vm, "<<",           ts_lshift, 0);
  ts_install (vm, ">>",           ts_rshift, 0);
  ts_install (vm, "u>>",          ts_urshift, 0);

  ts_install (vm, "@",            ts_fetch, 0);
  ts_install (vm, "!",            ts_store, 0);
  ts_install (vm, "c@",           ts_cfetch, 0);
  ts_install (vm, "c!",           ts_cstore, 0);
  ts_install (vm, "+!",           ts_plus_store, 0);

  ts_install (vm, "literal",      ts_make_literal, 0);
  ts_install (vm, ",",            ts_comma, 0);
  ts_install (vm, "here",         ts_here, 0);
  ts_install (vm, "there",        ts_there, 0);
  ts_install (vm, "where",        ts_where, 0);
  ts_install (vm, "allot",        ts_allot, 0);
  ts_install (vm, "align!",       ts_align_bang, 0);
  ts_install (vm, "constant",     ts_make_constant, 0);
  ts_install (vm, "create",       ts_create, 0);
  ts_install (vm, "create-local", ts_create_local, 0);
  ts_install (vm, "reset-locals", reset_locals, 0);
  ts_install (vm, "compile-grab", compile_grab, 0);
  ts_install (vm, "find",         ts_find, 0);
  ts_install (vm, "string,",      ts_string_comma, 0);

  ts_install (vm, "parse-number", ts_parse_number, 0);

  ts_install (vm, "emit",         ts_emit, 0);
  ts_install (vm, ".",            ts_print, 0);
  ts_install (vm, "absorb",       ts_absorb, 0);

  ts_install (vm, "execute",      ts_execute, 0);

  ts_install (vm, "catch",        ts_catch, 0);
  ts_install (vm, "throw",        ts_throw, 0);
  ts_install (vm, "error",        ts_prim_error, 0);

  ts_install (vm, "clear-stack",  ts_clear_stack, 0);
  ts_install (vm, ".s",           ts_print_stack, 0);
  ts_install (vm, "start-tracing",ts_start_tracing, 0);
  ts_install (vm, "stop-tracing", ts_stop_tracing, 0);

  ts_install (vm, "f+",           ts_fadd, 0);
  ts_install (vm, "f-",           ts_fsub, 0);
  ts_install (vm, "f*",           ts_fmul, 0);
  ts_install (vm, "f/",           ts_fdiv, 0);
  ts_install (vm, "f.",           ts_fprint, 0);

  /* Extras for efficiency */
  ts_install (vm, "0<",           ts_is_negative, 0);
  ts_install (vm, "0=",           ts_is_zero, 0);
  ts_install (vm, "2+",           ts_add2, 0);
  ts_install (vm, "1+",           ts_add1, 0);
  ts_install (vm, "1-",           ts_sub1, 0);
  ts_install (vm, "2-",           ts_sub2, 0);
  ts_install (vm, "cells",        ts_times4, 0);
  ts_install (vm, "4*",           ts_times4, 0);
  ts_install (vm, "2*",           ts_times2, 0);
  ts_install (vm, "2/",           ts_div2, 0);
  ts_install (vm, "4/",           ts_div4, 0);
}

/* Add all the unsafe built-in primitives to vm's dictionary.  That
   more or less means anything that could corrupt memory or open a
   file, at the moment. */
void
ts_install_unsafe_words (ts_VM *vm)
{
  ts_install (vm, ">data",        ts_to_data, 0);
  ts_install (vm, "@u",           ts_fetchu, 0);
  ts_install (vm, "!u",           ts_storeu, 0);
  ts_install (vm, "c@u",          ts_cfetchu, 0);
  ts_install (vm, "c!u",          ts_cstoreu, 0);
  ts_install (vm, "+!u",          ts_plus_storeu, 0);

  ts_install (vm, "with-io-on-file", ts_with_io_on_file, 0);
  ts_install (vm, "repl",         ts_repl, 0);
  ts_install (vm, "load",         ts_prim_load, 0);
}


/* Input scanning/parsing */

/* Add c to buf at position i. 
   Pre: i < size */
static void
append (ts_VM *vm, char *buf, int size, int i, char c)
{
  if (size == i + 1)
    {
      buf[i] = '\0';
      ts_error (vm, "Token too long: %s...", buf);
    }
  buf[i] = c;
}

#define punctuation "\\:(){}"

/* Scan the next token of input (consuming up to size-1 bytes) and
   copy it into `buf'.  Return yes if successful, or no if we reach
   EOF.
   Pre: 3 <= size. */
static boolean
get_token (ts_VM *vm, char *buf, int size)
{
  int c, i = 0;
  do {
    c = get_char (vm);
  } while (isspace (c) && '\n' != c);

  vm->token_place = vm->input.place;

  if (EOF == c)
    return no;

  if ('$' == c)
    {
      buf[i++] = c;
      c = get_char (vm);
      if (EOF == c)
        {
          buf[i] = '\0';
          ts_error (vm, "Unterminated character constant: %s", buf);
        }
      buf[i++] = c;
    }
  else if (NULL != strchr ("\n" punctuation, c))
    buf[i++] = c;
  else if ('"' == c || '`' == c) /* Scan a string literal */
    {
      /* We allow both " and ` as string delimiters because idiotic
         libsdl parses command-line arguments wrong (at least in
         Windows), messing up double-quoted strings.  Backquoted
         strings are a hack around that. */
      int delim = c;
      do {
        append (vm, buf, size, i++, c);
        c = get_char (vm);
        if (EOF == c)
          {
            buf[i] = '\0';
            ts_error (vm, "Unterminated string constant: %s", buf);
          }
      } while (delim != c);     /* TODO: fancier string syntax */
    }
  else
    {           /* Other tokens extend to whitespace, quote, or punctuation */
      do {
        append (vm, buf, size, i++, c);
        c = peek_char (vm);
        if (NULL != strchr (" \t\r\n\"`" punctuation, c))
          break;
        get_char (vm);
      } while (EOF != c);
    }
  buf[i] = '\0';
  return yes;
}

/* Skip past the end of the current line of input. */
static void
skip_line (ts_VM *vm)
{
  int c;
  do {
    c = get_char (vm);
  } while (EOF != c && '\n' != c);
}

/* Act on one source-code token as the current mode directs. */
static void
dispatch (ts_VM *vm, const char *token)
{
  /* XXX We could simplify this overall if get_token() performed the
     actions in the cases below, couldn't we? And that just left the
     code in our default case here.
   */
  switch (token[0])
    {
    case '\\':                  /* a comment */
      skip_line (vm); 
      break;

    case ':': case '(': case ')': /* mode-changing characters */
      /* XXX how about making ':' not mode-changing, but simply acting
         immediately on the token it's part of?  No spaces allowed then
         before the word to be defined, but I've never had them anyway. */
      vm->mode = token[0]; 
      break;

    case '{':                   /* start defining locals */
      reset_locals (vm, NULL);
      vm->mode = '{';
      break;

    case '}':                   /* finish defining locals */
      compile_grab (vm, NULL);
      vm->mode = ')';
      break;

    case '$':                   /* a character literal */
      ('(' == vm->mode ? ts_push : compile_push) (vm, token[1]);
      break;

    case '"':                   /* a string literal */
    case '`':
      {
        int string_index = compile_string (vm, token + 1);
        ('(' == vm->mode ? ts_push : compile_push) (vm, string_index);
        break;
      }

    case '\'':                  /* a tick literal */
      {
        int word = ts_lookup (vm, token + 1);
        if (ts_not_found == word)
          ts_error (vm, "Undefined word:\n:%s ;", token + 1);
        else
          ('(' == vm->mode ? ts_push : compile_push) (vm, word);
        break;
      }

    default:
      if (':' == vm->mode)      /* define word */
        {
          align_here (vm);
          ts_install (vm, save_string (vm, token), do_sequence, vm->here);
          reset_locals (vm, NULL);
          vm->mode = ')'; 
        }
      else if ('{' == vm->mode) /* define local */
        install_local (vm, token);
      else
        {                       /* handle it if it's a defined word */
          int word = ts_lookup (vm, token);
          if (ts_not_found != word)
            ('(' == vm->mode ? ts_run : compile) (vm, word);
          else
            {         /* handle it if it's a literal number */
              int value;
              if (parse_number (&value, token))
                ('(' == vm->mode ? ts_push : compile_push) (vm, value);
              else
                ts_error (vm, "Undefined word:\n:%s ;", token);
            }
        }
    }
}


/* Input loading */

/* Print a prompt with the current mode and stack height. */
static void
prompt (ts_VM *vm)
{
  int height = stack_pointer (vm) + 1;
  ts_put_char (vm, vm->mode);
  ts_put_char (vm, ' ');
  if (0 < height)
    {
      ts_put_char (vm, '<');
      put_decimal (vm, height);
      ts_put_string (vm, "> ", 2);
    }
  ts_flush_output (vm);
}

/* Read and execute source code interactively, starting in interpret
   mode.  Interactively means: we print a prompt, and errors only
   abort the current line. */
void
ts_interactive_loop (ts_VM *vm)
{
  char token[1024];
  vm->mode = '(';

  prompt (vm);
  for (;;)
    {
      ts_TRY (vm, frame)
        {
          if (!get_token (vm, token, sizeof token))
            {
              ts_POP_TRY (vm, frame);
              break;
            }
          else if ('\n' == token[0])
            prompt (vm);
          else
            dispatch (vm, token);
          ts_POP_TRY (vm, frame);
        }
      ts_EXCEPT (vm, frame)
        {
          /* TODO: we should have a separate 'stderr' type of stream 
             to send complaints to. */
          ts_put_string (vm, frame.complaint, strlen (frame.complaint));
          ts_put_char (vm, '\n');
          discard_input (vm);
          prompt (vm);
        }
    }

  ts_put_char (vm, '\n');
}

/* Read and execute source code from the current input stream till EOF,
   starting in interpret mode. */
void
ts_loading_loop (ts_VM *vm)
{
  char token[1024];
  vm->mode = '(';
  while (get_token (vm, token, sizeof token))
    if ('\n' != token[0])
      dispatch (vm, token);
}

/* Set the current input stream from the file named `filename'. */
/* XXX */
static void
with_io_on_file (ts_VM *vm, 
                 const char *filename, const char *mode, int word)
{
  ts_Stream input = vm->input;
  ts_Stream output = vm->output;
  FILE *fp = fopen (filename, mode);
  if (NULL == fp)
    ts_error (vm, "%s: %s\n", filename, strerror (errno));
  else
    {
      if ('r' == mode[0])
        ts_set_input_file_stream (vm, fp, filename);
      else
        ts_set_output_file_stream (vm, fp, filename);
      {
        ts_TRY (vm, frame)
          {
            ts_run (vm, word);
            
            fclose (fp);
            vm->output = output;
            vm->input = input;
            ts_POP_TRY (vm, frame);
          }
        ts_EXCEPT (vm, frame)
          {
            fclose (fp);
            vm->output = output;
            vm->input = input;
            ts_escape (vm, frame.complaint);
          }
      }
    }
}

/* XXX */
void
ts_with_io_on_file (ts_VM *vm, ts_Word *pw)
{
  ts_INPUT_3 (vm, filename, mode, word);
  ts_OUTPUT_0 ();
  with_io_on_file (vm,
                   ts_data_byte (vm, filename), 
                   ts_data_byte (vm, mode), 
                   word);
}

/* Read and execute source code from the file named `filename',
   starting and ending in interpret mode. */
void
ts_load (ts_VM *vm, const char *filename)
{
  ts_Stream saved = vm->input;
  FILE *fp = fopen (filename, "r");
  if (NULL == fp)
    ts_error (vm, "%s: %s\n", filename, strerror (errno));
  else
    {
      ts_TRY (vm, frame)
        {
          ts_set_input_file_stream (vm, fp, filename);
          ts_loading_loop (vm);

          /* FIXME: on return, token_place could still point at filename,
             which might get freed anytime after.  Null it out or something. */
          fclose (fp);
          vm->mode = '(';       /* should probably move this into callee */
          vm->input = saved;
          ts_POP_TRY (vm, frame);
        }
      ts_EXCEPT (vm, frame)
        {
          fclose (fp);
          vm->mode = '(';
          vm->input = saved;
          ts_escape (vm, frame.complaint);
        }
    }
}

/* Read and execute the contents of string. */
void
ts_load_string (ts_VM *vm, const char *string)
{
  ts_set_input_string (vm, string);
  ts_loading_loop (vm);
}

/* Do an interactive loop with stream as the input. */
void
ts_load_interactive (ts_VM *vm, FILE *stream)
{
  ts_set_input_file_stream (vm, stream, NULL);
  ts_interactive_loop (vm);
}
