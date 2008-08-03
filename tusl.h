/* TUSL -- the ultimate scripting language.
   Copyright 2003-2005 Darius Bacon under the terms of the MIT X license
   found at http://www.opensource.org/licenses/mit-license.html */

#ifndef TUSL_H
#define TUSL_H

#include <stdarg.h>
#include <stdio.h>
#include <setjmp.h>

/* The `inline' keyword varies across compilers. */
#if defined(__GNUC__)
# define INLINE __inline__
#elif defined(_MSC_VER)
# define INLINE __inline
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
# define INLINE inline
#else
# define INLINE
#endif

/* Configuration constants */
enum { ts_stack_size = 1024 };	/* Max. depth of the data stack */
enum { ts_data_size = 65536 };	/* Max. # of bytes in the data area */
				/*  (must be a multiple of sizeof(int) */
enum { ts_dictionary_size = 2048 }; /* Max. # of dictionary entries */

/* Forward declarations */
typedef struct ts_Handler_frame ts_Handler_frame;
typedef struct ts_Stream ts_Stream;
typedef struct ts_Word ts_Word;
typedef struct ts_VM ts_VM;
typedef void ts_Action (ts_VM *, ts_Word *);
typedef int ts_Streamer (ts_VM *);
typedef int ts_TraceFn (ts_VM *vm, unsigned word);
typedef int ts_CTraceFn (ts_VM *vm, ts_Word *);
typedef const char *ts_ErrorFn (ts_VM *vm, const char *format, va_list args);

/* Chain of exception handlers */
struct ts_Handler_frame {
  jmp_buf state;		/* longjmp() target to the handler */
  ts_Handler_frame *next;	/* Enclosing handler frames */
  const char *complaint;	/* The exception being raised */
  /* TODO: add a saved data stack pointer to all frames? */
};

/* Macros to set up and run exception handlers. */

void ts_escape (ts_VM *vm, const char *complaint);

#define ts_TRY(vm, frame) \
      ts_Handler_frame frame; \
      frame.next = (vm)->handler_stack; \
      (vm)->handler_stack = &frame; \
      if (0 == setjmp (frame.state))

#define ts_EXCEPT(vm, frame) \
      else

#define ts_POP_TRY(vm, frame) \
      (vm)->handler_stack = (frame).next

/* A source location */
typedef struct ts_Place {
  int line;
  int column;
  const char *opt_filename;
} ts_Place;

/* An input/output source/sink */
struct ts_Stream {
  char buffer[256];		/* Holding area for input/output bytes */
  char *ptr;			/* The next available byte in buffer */
  char *limit;			/* The first unavailable byte in buffer */
  ts_Streamer *streamer;	/* How to flush or refill buffer */
  void *data;			/* streamer's private data */
  ts_Place place;		/* Position of next byte to be processed */
				/*  (we only keep place current for inputs) */
};

/* A dictionary entry */
struct ts_Word {
  ts_Action *action;		/* How to execute this word */
  int datum;			/* Private argument for action */
  char *name;			/* This word's name */
};

/* A TUSL virtual machine */
struct ts_VM {
  int stack[ts_stack_size];	/* The data stack; grows upwards */
  int sp;			/* Offset in bytes of the top stack entry */
  int *pc;			/* Ptr to the next instruction to execute */
  char data[ts_data_size];	/* The data area; holds instructions, etc. */
  int here;			/* The next free byte within data[] */
  int there;			/* The first occupied byte of string space */
  ts_Word words[ts_dictionary_size]; /* The dictionary */
  int where;			/* The next free entry in words[] */
  int local_words;		/* # of locals at the end of words[] */
  char local_names[256];	/* Space for the names of locals */
  int local_names_ptr;		/* The next free index in local_names[] */
  char mode;			/* How to interpret the next source token */
  ts_Stream output;		/* The current output sink */
  ts_Stream input;		/* The current input source */
  ts_Place token_place;	        /* The position of the last token scanned */
  ts_ErrorFn *error;		/* How to report an error */
  void *error_data;		/* Private data for error() */
  ts_TraceFn *tracer;		/* How to trace an instruction execution */
  void *tracer_data;		/* Private data for tracer() */
  ts_CTraceFn *colon_tracer;    /* How to trace a colon definition */
  void *colon_tracer_data;	/* Private data for colon_tracer() */
  ts_Handler_frame *handler_stack; /* Currently ready exception handlers */
};

ts_VM *ts_vm_make (void);
void   ts_vm_unmake (ts_VM *vm);

void ts_push (ts_VM *vm, int c);
int  ts_pop (ts_VM *vm);

void ts_install (ts_VM *vm, char *name, ts_Action *action, int datum);
int  ts_lookup (ts_VM *vm, const char *name);
enum { ts_not_found = -1 };

void ts_install_standard_words (ts_VM *vm);
void ts_install_unsafe_words (ts_VM *vm);

void ts_run (ts_VM *vm, int word);
void ts_error (ts_VM *vm, const char *format, ...);

void ts_set_stream (ts_Stream *stream, ts_Streamer *streamer, void *data,
		    const char *opt_filename);
void ts_set_input_file_stream (ts_VM *vm, FILE *stream, 
			       const char *opt_filename);
void ts_set_output_file_stream (ts_VM *vm, FILE *stream, 
				const char *opt_filename);
void ts_set_input_string (ts_VM *vm, const char *string);

void ts_interactive_loop (ts_VM *vm);
void ts_loading_loop (ts_VM *vm);

void ts_load (ts_VM *vm, const char *filename);
void ts_load_interactive (ts_VM *vm, FILE *stream);
void ts_load_string (ts_VM *vm, const char *string);

void ts_put_char (ts_VM *vm, char c);
void ts_put_string (ts_VM *vm, const char *string, int size);
void ts_flush_output (ts_VM *vm);

ts_Action ts_with_io_on_file;

ts_Action ts_run_void_0;
ts_Action ts_run_void_1;
ts_Action ts_run_void_2;
ts_Action ts_run_void_3;
ts_Action ts_run_void_4;

ts_Action ts_run_int_0;
ts_Action ts_run_int_1;
ts_Action ts_run_int_2;
ts_Action ts_run_int_3;
ts_Action ts_run_int_4;

ts_Action ts_do_push;

ts_Action ts_prim_load;

/* Return a native pointer to byte i in vm's data space. */
static INLINE char *
ts_data_byte (ts_VM *vm, int i)
{
  if (ts_data_size <= (unsigned)i)
    ts_error (vm, "Data reference out of range: %d", i);
  return (char *)(vm->data + i);
}


/* Stack accesses */

static INLINE int
ts__spadd (ts_VM *vm, int i)
{
  return vm->sp + i * sizeof vm->stack[0];
}

static INLINE int
ts__popping (ts_VM *vm, int n)
{
  if (0 < n && ts__spadd (vm, 1 - n) < 0)
    ts_error (vm, "Stack underflow");
  return n;
}

static INLINE void
ts__fix_stack_fn (ts_VM *vm, int delta)
{
  if (0 < delta && 
      ts__spadd (vm, delta) >= ts_stack_size * sizeof vm->stack[0])
    ts_error (vm, "Stack overflow");
  vm->sp = ts__spadd (vm, delta);
}

#define ts__sr(vm, i) (*(int *)((char *) vm->stack + ts__spadd (vm,i)))

#define ts__pop0(vm, n)        ts_VM *ts__vm = (vm); \
                               const int ts__popped=ts__popping(ts__vm,n)
#define ts__pop1(vm,n,z)       ts__pop0(vm,n);      int z = ts__sr(ts__vm,1-(n))
#define ts__pop2(vm,n,y,z)     ts__pop1(vm,n,y);    int z = ts__sr(ts__vm,2-(n))
#define ts__pop3(vm,n,x,y,z)   ts__pop2(vm,n,x,y);  int z = ts__sr(ts__vm,3-(n))
#define ts__pop4(vm,n,w,x,y,z) ts__pop3(vm,n,w,x,y);int z = ts__sr(ts__vm,4-(n))

#define ts__fix_stack(pushing) ts__fix_stack_fn (ts__vm,(pushing)-ts__popped)

#define ts__push1(c)            (ts__sr (ts__vm, 0)=(c))
#define ts__push2(c,d)          (ts__sr (ts__vm,-1)=(c), ts__push1 (d))
#define ts__push3(c,d,e)        (ts__sr (ts__vm,-2)=(c), ts__push2 (d,e))
#define ts__push4(c,d,e,f)      (ts__sr (ts__vm,-3)=(c), ts__push3 (d,e,f))

#define ts_INPUT_0(vm)          ts__pop0 (vm,0)
#define ts_INPUT_1(vm, c)       ts__pop1 (vm,1,c)
#define ts_INPUT_2(vm, c,d)     ts__pop2 (vm,2,c,d)
#define ts_INPUT_3(vm, c,d,e)   ts__pop3 (vm,3,c,d,e)
#define ts_INPUT_4(vm, c,d,e,f) ts__pop4 (vm,4,c,d,e,f)

#define ts_OUTPUT_0()           (ts__fix_stack (0))
#define ts_OUTPUT_1(z)          (ts__fix_stack (1), ts__push1 (z))
#define ts_OUTPUT_2(y,z)        (ts__fix_stack (2), ts__push2 (y,z))
#define ts_OUTPUT_3(x,y,z)      (ts__fix_stack (3), ts__push3 (x,y,z))
#define ts_OUTPUT_4(w,x,y,z)    (ts__fix_stack (4), ts__push4 (w,x,y,z))

#endif /* TUSL_H */
