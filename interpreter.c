#include "crisp.h"

// This is the crisp REPL. All of the evaluation logic is in crisp.c.
// First we set up a global environment mapping symbols to builtin functions
// Then we read logical lines and print the result of their evaluation until EOF
int main(int argc, char** argv) {
    if (argc >= 2 && !strcmp(argv[1], "debug"))
        debug = true;

    //                                                         with_env, hold_args
    global_env = cons(cons(sym("if"), make_builtin_function(if_fn, true, true)), NIL);
    global_env = cons(cons(sym("eval"), make_builtin_function(eval, true, false)), global_env);
    global_env = cons(cons(sym("quote"), make_builtin_function(quote, false, true)), global_env);
    global_env = cons(cons(sym("lambda"), make_builtin_function(lambda, true, true)), global_env);
    global_env = cons(cons(sym("apply"), make_builtin_function(apply, true, false)), global_env);
    global_env = cons(cons(sym("car"), make_builtin_function(car_fn, false, false)), global_env);
    global_env = cons(cons(sym("cdr"), make_builtin_function(cdr_fn, false, false)), global_env);
    global_env = cons(cons(sym("cons"), make_builtin_function(cons_fn, false, false)), global_env);
    global_env = cons(cons(sym("sum"), make_builtin_function(sum, false, false)), global_env);
    global_env = cons(cons(sym("product"), make_builtin_function(product, false, false)), global_env);
    global_env = cons(cons(sym("modulus"), make_builtin_function(modulus, false, false)), global_env);
    global_env = cons(cons(sym("list"), make_builtin_function(quote, false, false)), global_env);
    global_env = cons(cons(sym("concat"), make_builtin_function(concat, false, false)), global_env);
    global_env = cons(cons(sym("equal"), make_builtin_function(equal, false, false)), global_env);
    global_env = cons(cons(sym("ispair"), make_builtin_function(ispair, false, false)), global_env);
    global_env = cons(cons(sym("same"), make_builtin_function(same, false, false)), global_env);
    global_env = cons(cons(sym("def"), make_builtin_function(def, true, true)), global_env);
    global_env = cons(cons(sym("with"), make_builtin_function(with, true, true)), global_env);
    global_env = cons(cons(sym("asc"), make_builtin_function(asc, false, false)), global_env);
    global_env = cons(cons(sym("hash"), make_builtin_function(hash, false, false)), global_env);

#ifndef DISABLE_FFI
    global_env = cons(cons(sym("dlopen"), make_builtin_function(dlopen_fn, false, false)), global_env);
#endif

    global_env = cons(cons(sym("GLOBALS"), NIL), global_env);

    // The global env looks like this:
    //     (y . 5) (z . 5) ..       (GLOBALS) (foo . 5) (identity . LAMBDA(x)<x>) .. (hash . BUILTIN_FUNCTION<...>) (asc . BUILTIN_FUNCTION<...>) ...
    // local variables-^  just a marker-^       ^-global var  ^-builtins

    // Lookups proceed left to right so local variables occlude global variables,
    // global variables occlude builtins, and more recent global var definitions
    // occlude older definitions

    // global_env always points at the GLOBALS marker and new definitions
    // are inserted just under it by the def function

    char c;
    stack_base = &c;

    char* line = NULL;
    size_t len = 0;

    char* parse_buf = malloc(64);
    size_t parse_buf_len = 64;

    while (1) {
        // Keep reading lines until a newline, and until all parens are closed
        int j = 0;
        int parens = 0;
        do {
            if (-1 == getline(&line, &len, stdin))
                return 0;
            char* i = line;
            while (*i) {
                // comment; ignore the rest of the line
                if (*i == ';') break;

                if (*i == '(') parens++;
                else if (*i == ')') parens--;

                // expand the buffer if it can't fit the new line
                if (j + 1 == parse_buf_len) {
                    parse_buf = realloc(parse_buf, parse_buf_len += 64);
                }

                // copy over the new character
                parse_buf[j++] = *(i++);
            }
        } while (parens > 0);

        // if we have an extra closing paren, don't bother parsing
        if (parens < 0) continue;
        parse_buf[j] = '\0';

        // parse takes a char** and alters the char*, so we need to copy this
        char* parse_buf_ = parse_buf;
        cell expr = parse(&parse_buf_);

        if (!expr) continue;
        DPRINTF("Parsed %s\n", print_cell(expr));

        cell evalled = eval(expr, global_env);
        // don't print anything if the result is NIL
        if (evalled) puts(print_cell(evalled));
    }
}
