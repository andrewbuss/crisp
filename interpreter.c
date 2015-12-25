#include "crisp.h"

// This is the crisp REPL. All of the evaluation logic is in crisp.c.
// First we set up a global environment mapping symbols to builtin functions
// Then we read logical lines and print the result of their evaluation until EOF

int main(int argc, char** argv) {
    if (argc >= 2 && !strcmp(argv[1], "debug"))
        debug = true;

    stack_base = &argc;


    global_env = cons(cons(sym("if"), make_native_function(if_fn, true, true)), global_env);
    global_env = cons(cons(sym("eval"), make_native_function(eval, true, false)), global_env);
    global_env = cons(cons(sym("quote"), make_native_function(quote, false, true)), global_env);
    global_env = cons(cons(sym("lambda"), make_native_function(lambda, true, true)), global_env);
    global_env = cons(cons(sym("car"), make_native_function(car_fn, false, false)), global_env);
    global_env = cons(cons(sym("cdr"), make_native_function(cdr_fn, false, false)), global_env);
    global_env = cons(cons(sym("cons"), make_native_function(cons_fn, false, false)), global_env);
    global_env = cons(cons(sym("list"), make_native_function(quote, false, false)), global_env);
    global_env = cons(cons(sym("equal"), make_native_function(equal_fn, false, false)), global_env);
    global_env = cons(cons(sym("same"), make_native_function(same, false, false)), global_env);
    global_env = cons(cons(sym("def"), make_native_function(def, true, true)), global_env);
    global_env = cons(cons(sym("with"), make_native_function(with, true, true)), global_env);

#ifndef DISABLE_FFI
    global_env = cons(cons(sym("dlopen"), make_native_function(dlopen_fn, false, false)), global_env);
    global_env = cons(cons(sym("dlsym"), make_native_function(dlsym_fn, false, false)), global_env);
    global_env = cons(cons(sym("import"), make_native_function(import, true, false)), global_env);
#endif

    global_env = cons(cons(sym("GLOBALS"), NIL), global_env);

    // The global env looks like this:
    //     (y . 5) (z . 5) ..       (GLOBALS) (foo . 5) (identity . LAMBDA(x)<x>) .. (hash . NATIVE_FUNCTION<...>) (asc . NATIVE_FUNCTION<...>) ...
    // local variables-^  just a marker-^       ^-global var  ^-builtins

    // Lookups proceed left to right so local variables occlude global variables,
    // global variables occlude builtins, and more recent global var definitions
    // occlude older definitions

    // global_env always points at the GLOBALS marker and new definitions
    // are inserted just under it by the def function

    logical_line ll;
    reset_logical_line(&ll);
    char* line = NULL;
    size_t len = 0;

    while (1) {
        if (-1 == getline(&line, &len, stdin))
            return 0;
        char* i = line;
        while (*i) {
            if (!logical_line_ingest(&ll, *i++)) continue;
            cell expr = parse(&ll.str);
            if (expr) {
                DPRINTF("Parsed %s\n", print_cell(expr));
                cell evalled = eval(expr, global_env);
                if (evalled) puts(print_cell(evalled));
            }
            reset_logical_line(&ll);
        }
    }
}

