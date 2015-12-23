#include "crisp.h"

// This file contains code for the foreign function interface.
//

cell find_ffi_function(char* sym_name, cell env) {
    char* dot = sym_name;
    char* cur = sym_name;
    do {
        dot = cur + 1;
        cur = strstr(dot, ".");
    } while (cur);
    if (!dot) return NIL;

    size_t libname_len = dot - sym_name - 1;
    char* libname = strncpy(malloc(libname_len + 1), sym_name, libname_len);
    libname[libname_len] = 0;
    cell lib = assoc(sym(libname), env);
    if (!lib) return NIL;

    lib = cdr(lib);
    if (CELL_TYPE(lib) != FFI_LIBRARY)
        return NIL;

    void* sym = dlsym(CELL_DEREF(lib).handle, dot);
    if (!sym) return NIL;

    cell c = malloc_or_die(8);
    CELL_DEREF(c).fn = sym;
    return c | FFI_FUNCTION;
}

cell dlopen_fn(cell args, cell env) {
    if (!args || CELL_TYPE(car(args)) != SYMBOL) return NIL;
    void* handle = dlopen(SYM_STR(car(args)), RTLD_LAZY);
    if (handle) {
        cell c = malloc_or_die(8);
        CELL_DEREF(c).handle = handle;
        return c | FFI_LIBRARY;
    }
    return NIL;
}

cell apply_ffi_function(cell fn, cell args) {
    // Hardcode cases for up to 5 args
    void* ffi_args[5];
    int i = 0;
    for (; args && i < 5; args = cdr(args), i++) {
        cell arg = car(args);
        if (CELL_TYPE(arg) == SYMBOL)
            ffi_args[i] = SYM_STR(arg);
        else if (CELL_TYPE(arg) == S64)
            ffi_args[i] = (void*) S64_VAL(arg);
        else if (CELL_TYPE(arg) == FFI_FUNCTION)
            ffi_args[i] = (void*) CELL_DEREF(arg).fn;
        else
            ffi_args[i] = NULL;
    }
    switch (i) {
        case 0:
            return make_s64(CELL_DEREF(fn).fn());
        case 1:
            return make_s64(CELL_DEREF(fn).fn(ffi_args[0]));
        case 2:
            return make_s64(CELL_DEREF(fn).fn(ffi_args[0], ffi_args[1]));
        case 3:
            return make_s64(CELL_DEREF(fn).fn(ffi_args[0], ffi_args[1], ffi_args[2]));
        case 4:
            return make_s64(CELL_DEREF(fn).fn(ffi_args[0], ffi_args[1], ffi_args[2], ffi_args[3]));
        case 5:
        default:
            return make_s64(CELL_DEREF(fn).fn(ffi_args[0], ffi_args[1], ffi_args[2], ffi_args[3], ffi_args[4]));
    }
}