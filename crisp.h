#define _GNU_SOURCE
#include <gc.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// The top 16 bits of the 64 bit address space are unused
// Type information is stored there instead
#define CELL_TYPE(x) (((uint64_t)x) & 0xffff000000000000)
#define CELL_PTR(x) ((_cell*)((x)&0xffffffffffff))
#define CELL_DEREF(x) (*CELL_PTR(x))
#define SYM_STR(c) ((char*)CELL_PTR(c))
#define S64_VAL(c) (*((int64_t*)CELL_PTR(c)))
#define FFI_FN(c) ((int64_t(*)())CELL_PTR(c))
#define FN_PTR(c) ((cell(*)())CELL_PTR(c))
#define car(c) CELL_PTR(c)->car
#define cdr(c) CELL_PTR(c)->cdr
#define caar(c) car(car(c))
#define cadr(c) cdr(car(c))
#define cdar(c) car(cdr(c))
#define cddr(c) cdr(cdr(c))
#define cddar(c) car(cdr(cdr(c)))
#define cdddr(c) cdr(cdr(cdr(c)))
#define LIST1(a) cons((a), NIL)
#define LIST2(a, b) cons((a), cons((b), NIL))
#define IS_CALLABLE(c) (CELL_TYPE(c) == LAMBDA || \
                        CELL_TYPE(c) == FFI_FUNCTION || \
                        CELL_TYPE(c) == NATIVE_FN || \
                        CELL_TYPE(c) == NATIVE_FN_ENV || \
                        CELL_TYPE(c) == NATIVE_FN_ENV_HELD_ARGS || \
                        CELL_TYPE(c) == NATIVE_FN_HELD_ARGS)
#define IS_S64(c) (CELL_TYPE(c) == S64)
#define IS_PAIR(c) (CELL_TYPE(c) == PAIR)

#define DPRINTF(fmt, ...) do { if (debug) fprintf(stderr, fmt, __VA_ARGS__); } while (0)

#define NIL (0LL << 48)
#define PAIR ((uint64_t)1LL << 48)
#define SYMBOL (2LL << 48)
#define LAMBDA (3LL << 48)
#define FFI_FUNCTION (4LL << 48)
#define FFI_LIBRARY (5LL << 48)
#define S64 (6LL << 48)
#define NATIVE_FN (7LL << 48)
#define NATIVE_FN_ENV (8LL << 48)
#define NATIVE_FN_HELD_ARGS (9LL << 48)
#define NATIVE_FN_ENV_HELD_ARGS (10LL << 48)

typedef uintptr_t cell;

typedef union _cell {
    struct { // PAIR
        cell car;
        cell cdr;
    };
    struct { // LAMBDA
        cell args;
        cell body;
        cell env;
    };
} _cell;

typedef struct {
  size_t len;
  size_t max_len;
  char* str;
  bool in_comment;
  int parens;
} logical_line;

extern bool debug;
extern cell global_env;
extern void* stack_base;
void* malloc_or_die(size_t size);

void reset_logical_line(logical_line* line);
bool logical_line_ingest(logical_line* line, char c);

cell make_s64(int64_t x);
cell make_native_function(void* fn, bool with_env, bool hold_args);
cell cons(cell car, cell cdr);
cell sym(char* symbol);
cell str(cell args, cell env);
cell car_fn(cell args, cell env);
cell cdr_fn(cell args, cell env);
cell cons_fn(cell args, cell env);
cell same(cell args, cell env);
cell equal_fn(cell args, cell env);
cell assoc(cell key, cell dict);
cell find_ffi_function(char* sym_name, cell env);
cell dlopen_fn(cell args, cell env);
cell dlsym_fn(cell args, cell env);
cell concat(cell first, cell rest);
cell zip(cell args, cell env);
cell eval(cell c, cell env);
cell apply(cell fn, cell args, cell env);
cell lambda(cell args, cell env);
cell quote(cell args, cell env);
cell if_fn(cell args, cell env);
cell evalmap(cell args, cell env);
cell def(cell args, cell env);
cell with(cell args, cell env);
cell import(cell args, cell env);
cell native_function(cell args, cell env);
int main(int argc, char** argv);
cell parse(char** s);
char* print_cell(cell c);
char* print_env(cell c);
cell apply_ffi_function(int64_t (*fn)(), cell args);

