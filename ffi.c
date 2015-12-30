#include "crisp.h"

// This file contains code for the foreign function interface.
//

// Try to resolve a symbol from a string of the form libname.symname
// libname must be defined in env
cell find_ffi_function(char* sym_name, cell env) {
    char* dot = sym_name;
    char* cur = sym_name;

    // split sym_name at the last dot
    // ab.cd.ef -> libname: ab.cd
    //             dot: ef
    do {
        dot = cur + 1;
        cur = strstr(dot, ".");
    } while (cur);
    if (!dot) return NIL;

    size_t libname_len = dot - sym_name - 1;
    char* libname = strncpy(malloc_or_die(libname_len + 1), sym_name, libname_len);
    libname[libname_len] = 0;
    cell lib = assoc(sym(libname), env);
    if (!lib) return NIL;

    lib = cdr(lib);
    if (TYPE(lib) != FFI_LIBRARY)
        return NIL;

    return dlsym_fn(LIST2(lib, (cell) dot | SYMBOL), NIL);
}

// Try to resolve a symbol in a specific shared library
// dlsym (dlopen libc.so.6) puts -> FFI_FUNCTION<...>
cell dlsym_fn(cell args, cell env) {
    if (!args || TYPE(car(args)) != FFI_LIBRARY || !IS_PAIR(cdr(args)))
        return NIL;
    if (!IS_PAIR(cdr(args)) || TYPE(cdar(args)) != SYMBOL)
        return NIL;
    void* sym = dlsym((void*) PTR(car(args)), SYM_STR(cdar(args)));

    if (!sym) return NIL;
    return (cell) sym | FFI_FUNCTION;
}

// Open a shared library by name
cell dlopen_fn(cell args, cell env) {
    if (!IS_PAIR(args) || TYPE(car(args)) != SYMBOL) return NIL;
    void* handle = dlopen(SYM_STR(car(args)), RTLD_LAZY);
    if (!handle) return NIL;
    return (cell) handle | FFI_LIBRARY;
}

// Apply an FFI_FUNCTION to up to 5 arguments
// Symbols are passed as strings, S64's are passed as longs
cell apply_ffi_function(int64_t (* fn)(), cell args) {
    // Hardcode cases for up to 5 args
    void* ffi_args[5];
    int i = 0;

    for (; IS_PAIR(args) && i < 5; args = cdr(args), i++) {
        cell arg = car(args);
        if (TYPE(arg) == SYMBOL)
            ffi_args[i] = SYM_STR(arg);
        else if (TYPE(arg) == S64)
            ffi_args[i] = (void*) S64_VAL(arg);
        else if (TYPE(arg) == FFI_FUNCTION)
            ffi_args[i] = (void*) PTR(arg);
        else
            ffi_args[i] = NULL;
    }
    switch (i) {
        case 0:
            return make_s64(fn());
        case 1:
            return make_s64(fn(ffi_args[0]));
        case 2:
            return make_s64(fn(ffi_args[0], ffi_args[1]));
        case 3:
            return make_s64(fn(ffi_args[0], ffi_args[1], ffi_args[2]));
        case 4:
            return make_s64(fn(ffi_args[0], ffi_args[1], ffi_args[2], ffi_args[3]));
        case 5:
        default:
            return make_s64(fn(ffi_args[0], ffi_args[1], ffi_args[2], ffi_args[3], ffi_args[4]));
    }
}

void* try_load(char* filename, bool* already_loaded) {
    *already_loaded = (bool) dlopen(filename, RTLD_LAZY | RTLD_NOLOAD);
    if (*already_loaded) {
        DPRINTF("Already loaded %s\n", filename);
        return NULL;
    }

    void* handle = dlopen(filename, RTLD_LAZY);
    if (handle) {
        DPRINTF("Loaded from %s: %p\n", filename, handle);
        return handle;
    }

    DPRINTF("Failed to load %s\n", filename);
    return NULL;
}

cell import(cell args, cell env) {
    if (!args || TYPE(car(args)) != SYMBOL) return NIL;
    char* lib_name = SYM_STR(car(args));

    char* lib_filename = NULL;
    void* handle;
    char* module_path = getenv("CRISP_MODULE_PATH");
    bool already_loaded = false;
    if (!module_path) module_path = "./modules";
    module_path = realpath(module_path, NULL);
    asprintf(&lib_filename, "%s/%s/lib%s.crisp.so", module_path, lib_name, lib_name);
    handle = try_load(lib_filename, &already_loaded);
    if (!already_loaded && !handle) {
        asprintf(&lib_filename, "%s/lib%s.crisp.so", module_path, lib_name);
        handle = try_load(lib_filename, &already_loaded);
    }
    if (!already_loaded && !handle) {
        asprintf(&lib_filename, "lib%s.crisp.so", lib_name);
        handle = try_load(lib_filename, &already_loaded);
    }
    free(lib_filename);
    free(module_path);
    if (!handle) return NIL;

    char* sym_name = NULL;
    asprintf(&sym_name, "_binary_%s_crisp_start", lib_name);
    char* script = dlsym(handle, sym_name);

    asprintf(&sym_name, "_binary_%s_crisp_end", lib_name);
    char* script_end = dlsym(handle, sym_name);
    free(sym_name);

    if (!script || !script_end) return NIL;

    cell this_lib = (cell) handle | FFI_LIBRARY;
    cell mapping_this_lib = cons(sym("this"), this_lib);
    cell mapping_native_function = cons(sym("native-function"), CAST(native_function, NATIVE_FN));
    cell new_env = cons(mapping_this_lib, env);
    new_env = cons(mapping_native_function, new_env);
    logical_line ll;
    reset_logical_line(&ll);

    cell evalled = NIL;

    while (script < script_end) {
        if (!logical_line_ingest(&ll, *script++)) continue;
        cell expr = parse(&ll.str);
        if (expr) {
            DPRINTF("Parsed %s\n", print_cell(expr));
            evalled = eval(expr, new_env);
        }
        reset_logical_line(&ll);
    }

    return evalled;
}

cell native_function(cell args, cell env) {
    if (!args) return NIL;
    return (cell) PTR(car(args)) | NATIVE_FN;
}


