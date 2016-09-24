#define _GNU_SOURCE

#include <gc.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef unsigned long long int uintptr_t;
typedef unsigned long long int uint64_t;
typedef uintptr_t cell;

// The top 16 bits of the 64 bit address space are unused
// Type information is stored there instead
#define TYPE(x) (((unsigned long long int)x) & 0xffff000000000000)
#define PTR(x) ((void*)((x) & 0xffffffffffff))
#define CAST(c, t) ((cell)( PTR((cell)c) )| (t))
#define SYM_STR(c) ((char*)PTR(c))
#define S64_VAL(c) (*((int64_t*)PTR(c)))
#define FFI_FN_PTR(c) ((int64_t(*)())PTR(c))
#define FN_PTR(c) ((cell(*)())PTR(c))
#define car(c) ((cell)(((pair* )(PTR(c)))->car))
#define cdr(c) ((cell)(((pair* )(PTR(c)))->cdr))
#define caar(c) car(car(c))
#define cadr(c) cdr(car(c))
#define cdar(c) car(cdr(c))
#define cddr(c) cdr(cdr(c))
#define cddar(c) car(cdr(cdr(c)))
#define cdddr(c) cdr(cdr(cdr(c)))
#define LIST1(a) cons((a), NIL)
#define LIST2(a, b) cons((a), cons((b), NIL))
#define IS_CALLABLE(c) (TYPE(c) == FN || \
                        TYPE(c) == MACRO || \
                        TYPE(c) == CONS || \
                        TYPE(c) == FFI_FN || \
                        TYPE(c) == NATIVE_FN || \
                        TYPE(c) == NATIVE_FN_TCO || \
                        TYPE(c) == NATIVE_MACRO)

#define IS_S64(c) (TYPE(c) == S64)
#define IS_PAIR(c) (TYPE(c) == PAIR)

#define DPRINTF(fmt, ...) do { if (debug) fprintf(stderr, fmt, __VA_ARGS__); } while (0)

#define TC_RETURN(val) do {*args = val; return false;} while(0)
#define TC_SLIDE(val) do {*args = val; return true;} while(0)

#define NIL           (0LL << 48)
#define PAIR          (1LL << 48)
#define SYMBOL        (2LL << 48)
#define FN            (3LL << 48)
#define FFI_SYM       (4LL << 48)
#define FFI_LIBRARY   (5LL << 48)
#define FFI_FN        (6LL << 48)
#define S64           (7LL << 48)
#define NATIVE_FN     (8LL << 48)
#define NATIVE_MACRO  (9LL << 48)
#define NATIVE_FN_TCO (10LL << 48)
#define MACRO         (11LL << 48)
#define CONS          (12LL << 48)
#define BUILTIN_TYPE_COUNT ((CONS >> 48) + 1)

typedef struct {
    cell car;
    cell cdr;
} pair;

typedef struct {
    cell args;
    cell body;
    cell env;
} fn_t;

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

bool apply_fn(cell* args, cell* env);
bool apply(cell fn, cell* args, cell* env);
cell apply_ffi_function(int64_t (* fn)(), cell args);
cell assoc(cell key, cell dict);
cell car_fn(cell args, cell env);
cell cdr_fn(cell args, cell env);
cell concat(cell first, cell rest);
cell cons(cell car, cell cdr);
cell cons_fn(cell args, cell env);
cell def(cell args, cell env);
cell dlopen_fn(cell args, cell env);
cell dlsym_fn(cell args, cell env);
cell equal_fn(cell args, cell env);
cell eval(cell c, cell env);
cell evalmap(cell args, cell env);
cell find_ffi_sym(char* sym_name, cell env);
bool if_fn(cell* args, cell* env);
cell import(cell args, cell env);
cell lambda(cell args, cell env);
cell macro(cell args, cell env);
cell make_s64(int64_t x);
cell typeof_fn(cell args, cell env);
cell parse(char** s);
cell quote(cell args, cell env);
cell same(cell args, cell env);
cell str(cell args, cell env);
cell sym(char* symbol);
bool with(cell* args, cell* env);
cell zip(cell args, cell env);
char* print_cell(cell c);
char* print_env(cell c);