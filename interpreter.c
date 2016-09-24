#include "crisp.h"

// This is the crisp REPL. All of the evaluation logic is in crisp.c.
// First we set up a global environment mapping symbols to builtin functions
// Then we read logical lines and print the result of their evaluation until EOF

int main(int argc, char** argv) {
    if (argc >= 2 && !strcmp(argv[1], "debug"))
        debug = true;

    stack_base = &argc;

    global_env = cons(cons(sym("eval"), CAST(eval, NATIVE_FN)), global_env);
    global_env = cons(cons(sym("quote"), CAST(quote, NATIVE_MACRO)), global_env);
    global_env = cons(cons(sym("lambda"), CAST(lambda, NATIVE_MACRO)), global_env);
    global_env = cons(cons(sym("car"), CAST(car_fn, NATIVE_FN)), global_env);
    global_env = cons(cons(sym("cdr"), CAST(cdr_fn, NATIVE_FN)), global_env);
    global_env = cons(cons(sym("list"), CAST(quote, NATIVE_FN)), global_env);
    global_env = cons(cons(sym("equal"), CAST(equal_fn, NATIVE_FN)), global_env);
    global_env = cons(cons(sym("same"), CAST(same, NATIVE_FN)), global_env);
    global_env = cons(cons(sym("def"), CAST(def, NATIVE_MACRO)), global_env);
    global_env = cons(cons(sym("macro"), CAST(macro, NATIVE_MACRO)), global_env);
    global_env = cons(cons(sym("typeof"), CAST(typeof_fn, NATIVE_FN)), global_env);

#ifndef DISABLE_FFI
    global_env = cons(cons(sym("dlopen"), CAST(dlopen_fn, NATIVE_FN)), global_env);
    global_env = cons(cons(sym("dlsym"), CAST(dlsym_fn, NATIVE_FN)), global_env);
    global_env = cons(cons(sym("import"), CAST(import, NATIVE_FN)), global_env);
#endif

    global_env = cons(cons(sym("apply"), CAST(apply_fn, NATIVE_FN_TCO)), global_env);
    global_env = cons(cons(sym("if"), CAST(if_fn, NATIVE_FN_TCO)), global_env);
    global_env = cons(cons(sym("with"), CAST(with, NATIVE_FN_TCO)), global_env);
    global_env = cons(cons(sym("cons"), CAST(NIL, CONS)), global_env);

    global_env = cons(cons(sym("GLOBALS"), NIL), global_env);

    // The global env looks like this:
    //     (y . 5) (z . 5) ..       (GLOBALS) (foo . 5) (identity . FN(x)<x>) .. (hash . NATIVE_FUNCTION<...>) (asc . NATIVE_FUNCTION<...>) ...
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

#ifdef FUZZ
    int i;
    for(i = 0; i<5; i++){
        if (-1 == getline(&line, &len, stdin))
            return 0;
        if(strlen(line) > 50) return 0;
        char* i = line;
        while (*i) {
            if (!logical_line_ingest(&ll, *i++)) {
                if (ll.parens < 0) {
                    reset_logical_line(&ll);
                    break;
                }
                continue;
            }
            cell expr = parse(&ll.str);
            if (expr) {
                DPRINTF("Parsed %s\n", print_cell(expr));
                cell evalled = eval(expr, global_env);
                if (evalled) puts(print_cell(evalled));
            }
            reset_logical_line(&ll);
        }
    }
    return 0;
#endif

    while (1) {
        if (-1 == getline(&line, &len, stdin))
            return 0;
        char* i = line;
        while (*i) {
            if (!logical_line_ingest(&ll, *i++)) {
                if (ll.parens < 0) {
                    reset_logical_line(&ll);
                    break;
                }
                continue;
            }
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

