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
#define CELL_TYPE(x) (((uint64_t)x)&0xffff000000000000)
#define CELL_PTR(x) ((_cell*)((x)&0xffffffffffff))
#define CELL_DEREF(x) (*CELL_PTR(x))
#define SYM_STR(c) ((char*)CELL_PTR(c))
#define S64_VAL(c) (*((int64_t*)CELL_PTR(c)))
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
#define IS_CALLABLE(c) (CELL_TYPE(c) == LAMBDA || CELL_TYPE(c) == FFI_FUNCTION || CELL_TYPE(c) == NATIVE_FUNCTION)
#define IS_S64(c) (CELL_TYPE(c) == S64)
#define IS_PAIR(c) (CELL_TYPE(c) == PAIR)

#define DPRINTF(fmt, ...) do { if (debug) fprintf(stderr, fmt, __VA_ARGS__); } while (0)

enum celltype {
    NIL = 0x0000000000000,
    PAIR = 0x1000000000000,
    SYMBOL = 0x2000000000000,
    S64 = 0x3000000000000,
    LAMBDA = 0x4000000000000,
    NATIVE_FUNCTION = 0x5000000000000,
    FFI_FUNCTION = 0x6000000000000,
    FFI_LIBRARY = 0x7000000000000
};

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
    struct { // NATIVE_FUNCTION, FFI_FUNCTION
        union {
            cell(* fn)();
        };

        // Indicates whether this function accepts a second env parameter
        int with_env:1;

        // Indicates whether to hold arguments unevaluated before application
        int hold_args:1;
    };
    struct { // FFI_LIBRARY
        void* handle;
    };
} _cell;

extern bool debug;
extern cell global_env;
extern char* stack_base;
cell malloc_or_die(size_t size);
cell make_s64(int64_t x);
cell make_native_function(void* fn, int with_env, int hold_args);
cell cons(cell car, cell cdr);
cell sym(char* symbol);
cell car_fn(cell args, cell env);
cell cdr_fn(cell args, cell env);
cell cons_fn(cell args, cell env);
cell ispair(cell args, cell env);
cell same(cell args, cell env);
cell sum(cell args, cell env);
cell product(cell args, cell env);
cell modulus(cell args, cell env);
cell equal_fn(cell args, cell env);
cell asc(cell args, cell env);
cell assoc(cell args, cell env);
cell find_ffi_function(char* sym_name, cell env);
cell dlopen_fn(cell args, cell env);
cell concat_fn(cell args, cell env);
cell zip(cell args, cell env);
cell eval(cell c, cell env);
cell apply(cell fn, cell args, cell env);
cell apply_fn(cell args, cell env);
cell lambda(cell args, cell env);
cell quote(cell args, cell env);
cell if_fn(cell args, cell env);
cell evalmap(cell args, cell env);
cell def(cell args, cell env);
cell with(cell args, cell env);
cell hash(cell args, cell env);
int main(int argc, char** argv);
cell parse(char** s);
char* print_cell(cell c);
char* print_env(cell c);
cell apply_ffi_function(cell fn, cell args);