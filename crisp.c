#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define CELL_TYPE(x) ((unsigned char)(((cell)x)&0xf))
#define CELL_DEREFERENCE(x) (*(_cell*)((x)&0xfffffffffffffff0))
#define car(c) CELL_DEREFERENCE(c).car
#define cdr(c) CELL_DEREFERENCE(c).cdr
#define caar(c) car(car(c))
#define cadr(c) cdr(car(c))
#define cdar(c) car(cdr(c))
#define cddr(c) cdr(cdr(c))
#define cddar(c) car(cdr(cdr(c)))
#define cdddr(c) cdr(cdr(cdr(c)))
#define LIST1(a) cons((a), NIL)
#define LIST2(a, ...) cons((a), LIST1(__VA_ARGS__))
#define LIST3(a, ...) cons((a), LIST2(__VA_ARGS__))

#ifndef DEBUG
#define DEBUG 0
#endif

#define CELL_POOL_SIZE 10000000
#define DPRINTF(fmt, ...) \
            do { if (DEBUG) fprintf(stderr, fmt, __VA_ARGS__); } while (0)

enum celltype {
    NIL, PAIR, SYMBOL, S64, LAMBDA, C_FUNCTION, STRING
};

typedef uintptr_t cell;
typedef union _cell {
    struct {
        cell car;
        cell cdr;
    };
    struct {
        cell l_args;
        cell l_expr;
    };
    struct {
        int64_t s64;
    };
    struct {
        char* symbol;
    };

    struct {
        union {
            cell(* fn)(cell);
            cell(* fn_env)(cell, cell);
        };

        unsigned int with_env:1;
        unsigned int hold_args:1;
    };
} _cell;

_cell* arena;
cell env_base = NIL;
cell global_env = NIL;
cell symbols = NIL;

/* Cell manipulation functions ---------------- */

cell allocate_cell() {
    return (cell) (arena += sizeof(cell));
}

cell make_s64(uint64_t x) {
    cell c = allocate_cell();
    CELL_DEREFERENCE(c).s64 = x;
    return c | S64;
}

cell make_lambda(cell params, cell expr) {
    cell c = allocate_cell();
    CELL_DEREFERENCE(c).l_args = params;
    CELL_DEREFERENCE(c).l_expr = expr;
    return c | LAMBDA;
}

cell make_c_function(void* fn, int with_env, int hold_args) {
    cell c = allocate_cell();
    CELL_DEREFERENCE(c).fn = fn;
    CELL_DEREFERENCE(c).with_env = with_env;
    CELL_DEREFERENCE(c).hold_args = hold_args;
    return c | C_FUNCTION;
}

cell cons(cell car, cell cdr) {
    cell c = allocate_cell();
    CELL_DEREFERENCE(c).car = car;
    CELL_DEREFERENCE(c).cdr = cdr;
    return c | PAIR;
}

cell sym(char* symbol) {
    cell c = allocate_cell();
    cell sym_cell = (cell) malloc(sizeof(_cell));
    CELL_DEREFERENCE(sym_cell).symbol = CELL_DEREFERENCE(c).symbol = strdup(symbol);
    cdr(sym_cell) = symbols;
    symbols = sym_cell;
    return c | SYMBOL;
}

cell deep_copy(cell c) {
    if (!c) return NIL;
    if (CELL_TYPE(c) == PAIR)
        return cons(deep_copy(car(c)), deep_copy(cdr(c)));
    else if (CELL_TYPE(c) == LAMBDA)
        return make_lambda(deep_copy(car(c)), deep_copy(cdr(c)));
    return CELL_TYPE(c) | (cell) memcpy((_cell*) allocate_cell(), (_cell*) (c & (~0xF)), sizeof(_cell));
}

cell car_fn(cell args) { return caar(args); }
cell cdr_fn(cell args) { return cadr(args); }
cell cons_fn(cell args) { return cons(car(args), cdar(args)); }
cell ispair(cell args) { return CELL_TYPE(cdar(args)) == PAIR ? cdar(args) : NIL; }
cell same(cell args) {
    cell first = car(args);
    cell rest = cdr(args);
    if (!rest || first == same(rest)) return first;
    return NIL;
}

char* leaky_print(cell c);

cell equal(cell args) {
    cell left = car(args);
    if (!cdr(args)) return left;
    cell right = equal(cdr(args));
//    if (left == right) return left;
    if (!right) return NIL;
    if (CELL_TYPE(left) == SYMBOL && CELL_TYPE(right) == SYMBOL) {
        if (!strcmp(CELL_DEREFERENCE(left).symbol, CELL_DEREFERENCE(right).symbol))
            return left;
    }
    if (CELL_TYPE(left) == S64 && CELL_TYPE(right) == S64 && CELL_DEREFERENCE(left).s64 == CELL_DEREFERENCE(right).s64)
        return left;
    return NIL;
}

cell asc(cell args) {
    // are the arguments strictly ascending?
    if (!cdr(args)) return car(args);
    cell next = asc(cdr(args));
    if (!next) return next;
    if (CELL_DEREFERENCE(car(args)).s64 < CELL_DEREFERENCE(next).s64) return car(args);
    return NIL;
}

/* text serdes ---------------- */

cell parse(char** s) {
    while (isspace(**s)) (*s)++;
    if (!**s) return NIL;
    switch (**s) {
        case ')':
            (*s)++;
            return NIL;
        case '(':
            (*s)++;
            cell first = parse(s);
            return cons(first, parse(s));
        case '\'':
            (*s)++;
            cell rest = parse(s);
            return cons(cons(sym("quote"), car(rest)), cdr(rest));
        case '.':
            (*s)++;
            return car(parse(s));
        default: {
            char* i = *s;
            while (*i && !isspace(*i) && *i != '(' && *i != ')') i++;
            int token_len = i - *s;
            char* token = strncpy(malloc(token_len + 1), *s, token_len);
            token[token_len] = '\0';
            *s = i;
            cell c;

            char* endptr;
            long val = strtol(token, &endptr, 0);

            if (endptr != token) c = make_s64(val);
            else c = sym(token);

            free(token);
            return cons(c, parse(s));
        }
    }
}

int print(char* buf, cell c) {
    char* i = buf;
    switch (CELL_TYPE(c)) {
        case NIL:
            return sprintf(buf, "()");
        case PAIR: {
            if (CELL_TYPE(car(c)) == PAIR) {
                i += sprintf(i, "(");
                i += print(i, car(c));
                i += sprintf(i, ")");
            }
            else i += print(i, car(c));
            if (!cdr(c)) return i - buf;
            i += sprintf(i, " ");
            if (CELL_TYPE(cdr(c)) != PAIR) i += sprintf(i, ". ");
            return i - buf + print(i, cdr(c));
        }
        case S64:
            return sprintf(buf, "%ld", CELL_DEREFERENCE(c).s64);
        case SYMBOL:
            return sprintf(buf, "%s", CELL_DEREFERENCE(c).symbol);
        case C_FUNCTION:
            return sprintf(buf, "C_FUNCTION<%p>", CELL_DEREFERENCE(c).fn);
        case LAMBDA:
            i += sprintf(i, "LAMBDA(");
            i += print(i, car(c));
            i += sprintf(i, ")<");
            i += print(i, cdr(c));
            return i - buf + sprintf(i, ">");
    }
    return 0;
}

char* leaky_print(cell c) {
    char* buf = malloc(4096);
    print(buf, c);
    return buf;
}

/* List manipulation functions ---------------- */

cell concat(cell args) {
    cell first = car(args);
    cell rest = cdr(args);
    if (!rest) return first;
    if (!first) return concat(rest);
    return cons(car(first), concat(cons(cdr(first), rest)));
}

cell zip(cell args) {
    cell a = car(args), b = cdar(args);
    if (!a || !b) return NIL;
    DPRINTF("zipping %s %s\n", leaky_print(a), leaky_print(b));
    if (!cdr(a) && cdr(b)) return cons(car(a), b);
    cell rv = cons(cons(car(a), car(b)), zip(LIST2(cdr(a), cdr(b))));
    return rv;
}

cell keys(cell args) {
    if (!args) return NIL;
    return cons(caar(args), keys(cdr(args)));
}

cell assoc(cell args) {
    cell key = car(args), dict = cdar(args);
    if (!dict) return NIL;
    if (equal(LIST2(key, caar(dict)))) return car(dict);
    return assoc(LIST2(key, cdr(dict)));
}

cell sum(cell args) {
    if (!args) return make_s64(0);
    return make_s64(CELL_DEREFERENCE(car(args)).s64 + CELL_DEREFERENCE(sum(cdr(args))).s64);
}

cell product(cell args) {
    if (!args) return make_s64(1);
    return make_s64(CELL_DEREFERENCE(car(args)).s64 * CELL_DEREFERENCE(product(cdr(args))).s64);
}

/* Special functions which require held variables or env access ------- */

cell eval(cell c, cell env);

cell apply(cell args, cell env) {
    cell fn = car(args);
    args = cdar(args);
    DPRINTF("Applying %s to %s\n", leaky_print(fn), leaky_print(args));
    if (CELL_TYPE(fn) == LAMBDA) {
        cell result = eval(cdr(fn), concat(LIST2(zip(LIST2(car(fn), args)), env)));
        return result;
    }
    else if (CELL_TYPE(fn) == C_FUNCTION) {
        if (CELL_DEREFERENCE(fn).with_env)
            return CELL_DEREFERENCE(fn).fn_env(args, env);
        else
            return CELL_DEREFERENCE(fn).fn(args);
    }
    printf("Can't apply %s\n", leaky_print(fn));
    return NIL;
}


cell lambda(cell args) {
    cell vars = car(args);
    if (CELL_TYPE(vars) == SYMBOL) vars = LIST1(vars);
    cell expr = cdr(args);
    DPRINTF("Making lambda %s = %s\n", leaky_print(vars), leaky_print(expr));
    return make_lambda(vars, expr);
}

cell quote(cell args) { return args; }

cell if_fn(cell args, cell env) {
    cell predicate = eval(car(args), env);
    if (predicate)
        return eval(cdar(args), env);
    else if (cdddr(args))
        // if a "fourth argument" is provided, eval it as part of the "else"
        return eval(cddr(args), env);
    else
        return eval(cddar(args), env);
}

cell evalmap(cell args, cell env) {
    if (!args) return NIL;
    return cons(eval(car(args), env), evalmap(cdr(args), env));
}

cell eval(cell c, cell env) {
    switch (CELL_TYPE(c)) {
        case PAIR: {
            DPRINTF("Evalling %s\n", leaky_print(c));
            // () -> ()
            if (!car(c)) return c;
            cell first = eval(car(c), env);
            cell rest = cdr(c);
            // (f x) -> f x
            if (!cdr(c)) return first;
            if (CELL_TYPE(first) == LAMBDA || CELL_TYPE(first) == C_FUNCTION) {
                if (CELL_TYPE(first) == LAMBDA || !CELL_DEREFERENCE(first).hold_args)
                    rest = evalmap(rest, env);
                return apply(LIST2(first, rest), env);
            }
            return cons(first, evalmap(rest, env));
        }
        case SYMBOL: {
            cell resolved_symbol = assoc(LIST2(c, env));
            if (resolved_symbol) return cdr(resolved_symbol);
        }
        case C_FUNCTION:
        case NIL:
        case LAMBDA:
        case S64:
            return c;
        default:
            return NIL;
    }
}

cell def(cell args, cell env) {
    cell referent = eval(cdr(args), env);
    cell var_name = car(args);
    cdr(global_env) = cons(cons(var_name, referent), cdr(global_env));
    return NIL;
}

int main() {
    _cell* arena_base = (_cell*) malloc((sizeof(_cell)) * CELL_POOL_SIZE);
    arena = arena_base;


    //                                                   with_env, hold_args
    global_env = cons(cons(sym("if"), make_c_function(if_fn, true, true)), env_base);
    global_env = cons(cons(sym("eval"), make_c_function(eval, true, false)), global_env);
    global_env = cons(cons(sym("quote"), make_c_function(quote, false, true)), global_env);
    global_env = cons(cons(sym("lambda"), make_c_function(lambda, false, true)), global_env);
    global_env = cons(cons(sym("apply"), make_c_function(apply, true, false)), global_env);
    global_env = cons(cons(sym("car"), make_c_function(car_fn, false, false)), global_env);
    global_env = cons(cons(sym("cdr"), make_c_function(cdr_fn, false, false)), global_env);
    global_env = cons(cons(sym("cons"), make_c_function(cons_fn, false, false)), global_env);
    global_env = cons(cons(sym("sum"), make_c_function(sum, false, false)), global_env);
    global_env = cons(cons(sym("product"), make_c_function(product, false, false)), global_env);
    global_env = cons(cons(sym("list"), make_c_function(quote, false, false)), global_env);
    global_env = cons(cons(sym("concat"), make_c_function(concat, false, false)), global_env);
    global_env = cons(cons(sym("equal"), make_c_function(equal, false, false)), global_env);
    global_env = cons(cons(sym("ispair"), make_c_function(ispair, false, false)), global_env);
    global_env = cons(cons(sym("same"), make_c_function(same, false, false)), global_env);
    global_env = cons(cons(sym("def"), make_c_function(def, true, true)), global_env);
    global_env = cons(cons(sym("asc"), make_c_function(asc, false, false)), global_env);
    global_env = cons(cons(sym("LOCALS-OVERHEAD"), NIL), global_env);

    // The global env looks like this:
    //     (y . 5) (z . 5) .. (LOCALS-OVERHEAD) (identity . LAMBDA(x)<x>) .. (asc . C_FUNCTION<0x123>) (def . C_FUNCTION<0x456>) ...
    // local variables-^       just a marker-^      global vars-^              builtins-^

    // Lookups proceed left to right so local variables occlude global variables,
    // global variables occlude builtins, and more recent global var definitions
    // occlude older definitions

    // global_env always points at the LOCALS-OVERHEAD marker

    char* line = NULL;
    size_t len = 0;
    while (1) {
        char buf[4096] = "";
        char* j = buf;
        int parens = 0;
        do {
            if (-1 == getline(&line, &len, stdin))
                goto cleanup;
            char* i = line;
            while (*i) {
                if (*i == ';') break;
                if (*i == '(') parens++;
                else if (*i == ')') parens--;
                *(j++) = *(i++);
            }
        } while (parens);
        *j = '\0';
        char* buf_ = buf;
        cell expr = parse(&buf_);
        if (!expr) continue;
        DPRINTF("Parsed %s\n", leaky_print(expr));
        cell evalled = eval(expr, global_env);
        if (evalled) {
            print(buf, evalled);
            puts(buf);
        }

        // Switch to a new arena, deep-copy the entire global environment, then free the old arena
        // This is totally a legitimate GC strategy.
        _cell* old_arena = arena_base;
        arena = arena_base = (_cell*) malloc(sizeof(_cell) * CELL_POOL_SIZE);
        global_env = deep_copy(global_env);
        free(old_arena);
    }
    cleanup:
    free(line);
    // Free all the symbol strings
    for (cell cur = symbols; cur;) {
        free((char*) car(cur));
        cell old = cur;
        cur = cdr(cur);
        free((_cell*) old);
    }
    free(arena_base);
    return 0;
}
