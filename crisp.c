#include "crisp.h"

// This file contains definitions for the core of crisp

bool debug = false;
cell global_env = NIL;
char* stack_base = NULL;

cell allocate_cell() {
    cell rv = (cell) GC_MALLOC(sizeof(_cell));
    if (!rv) DIE("Failed to allocate memory for new cell");
    return rv;
}

cell make_s64(int64_t x) {
    cell c = allocate_cell();
    CELL_DEREF(c).s64 = x;
    return c | S64;
}

cell make_builtin_function(void* fn, int with_env, int hold_args) {
    cell c = allocate_cell();
    CELL_DEREF(c).fn = fn;
    CELL_DEREF(c).with_env = with_env;
    CELL_DEREF(c).hold_args = hold_args;
    return c | BUILTIN_FUNCTION;
}

cell cons(cell car, cell cdr) {
    cell c = allocate_cell();
    CELL_DEREF(c).car = car;
    CELL_DEREF(c).cdr = cdr;
    return c | PAIR;
}

// Notably this does not strdup the passed symbol
cell sym(char* symbol) {
    cell c = allocate_cell();
    CELL_DEREF(c).symbol = symbol;
    return c | SYMBOL;
}

// Builtin functions

// The contract for these functions is that the argument
// may be either NIL or a PAIR - atomic types are not
// allowed

// Some functions take an additional `env` argument. The
// interpreter must create the wrapping cell for these
// functions using hold_args = true.

cell car_fn(cell args) {
    // car 4 ->
    // car (a b) -> a
    if (!args || CELL_TYPE(car(args)) != PAIR) return NIL;
    return caar(args);
}

cell cdr_fn(cell args) {
    // cdr 4 ->
    // cdr (a b) -> b
    if (!args || CELL_TYPE(car(args)) != PAIR) return NIL;
    return cadr(args);
}

cell cons_fn(cell args) {
    // cons 4 -> 4
    // cons 4 5 -> 4 . 5
    // cons a . b ->
    if (!args) return NIL;
    if (!cdr(args)) return args;
    if (CELL_TYPE(cdr(args)) != PAIR) return NIL;
    return cons(car(args), cdar(args));
}

cell ispair(cell args) {
    // ispair 4 ->
    // ispair () -> ()
    // ispair (a b) -> (a b)
    if (!args) return NIL;
    return IS_PAIR(car(args)) ? car(args) : (cell) NIL;
}

// Check for pointer equality among args
// If one argument is not equal, returns NIL
// Otherwise returns the first argument
cell same(cell args) {
    // same ->
    // same 2 -> 2

    // If 2 and 2 were instantiated separately they may not be same
    // same 2 2 ->

    // For any x:
    // (lambda y same y y) x -> x
    if (!args) return NIL;
    cell first = car(args);
    cell rest = cdr(args);
    if (!rest || (ispair(rest) && (first == same(rest)))) return first;
    return NIL;
}

// Check for value equality among args, or equality of symbols
// If one argument is not equal, returns NIL
// Otherwise returns the first argument
cell equal(cell args) {
    // equal ->
    // equal 1 -> 1
    // equal 2 1 ->
    // equal () () ->
    // equal a a -> a
    // equal 2 y ->

    // For any x:
    // equal x -> x
    if (!args) return NIL;
    cell left = car(args);
    if (!cdr(args)) return left;
    if (!IS_PAIR(cdr(args))) return NIL;
    cell right = equal(cdr(args));
    if (!right) return NIL;
    if (CELL_TYPE(left) == SYMBOL && CELL_TYPE(right) == SYMBOL) {
        if (!strcmp(CELL_DEREF(left).symbol, CELL_DEREF(right).symbol))
            return left;
    }
    if (CELL_TYPE(left) == S64 && CELL_TYPE(right) == S64 && CELL_DEREF(left).s64 == CELL_DEREF(right).s64)
        return left;
    return NIL;
}

// Concatenate arguments
cell concat(cell args) {
    // concat (a b) c (d e) -> a b c d e
    // concat ((a b) (c d)) e -> (a b) (c d) e
    // concat a -> a
    // concat a . b -> a b
    if (!args) return NIL;
    cell first = car(args);
    cell rest = cdr(args);
    if (!rest) return first;
    if (!IS_PAIR(rest)) rest = LIST1(rest);
    if (!first) return concat(rest);
    if (!IS_PAIR(first)) first = LIST1(first);
    return cons(car(first), concat(cons(cdr(first), rest)));
}

// Pair up items from a left and right list into a
// new list of cons pairs until one list runs out
cell zip(cell args) {
    // zip (a b c) (d e) -> (a . d) (b . e)
    // zip () () ->
    // zip ->
    // zip a b ->
    // zip (a b) (c . d) -> (a . c)
    if (!args || !IS_PAIR(cdr(args))) return NIL;
    cell a = car(args), b = cdar(args);
    if (!IS_PAIR(a) || !IS_PAIR(b)) return NIL;
    cell rv = cons(cons(car(a), car(b)), zip(LIST2(cdr(a), cdr(b))));
    return rv;
}

cell assoc(cell args) {
    if (!args || !IS_PAIR(cdr(args))) return NIL;
    cell key = car(args), dict = cdar(args);
    if (!IS_PAIR(dict) || !IS_PAIR(car(dict))) return NIL;
    if (equal(LIST2(key, caar(dict)))) return car(dict);
    return assoc(LIST2(key, cdr(dict)));
}

cell apply(cell args, cell env) {
    if (!args) return NIL;
    cell fn = car(args);
    if (!IS_CALLABLE(fn)) return NIL;
    if (!IS_PAIR(cdr(args))) args = NIL;
    else args = cdar(args);
    if (!IS_PAIR(args)) args = LIST1(args);
    DPRINTF("Applying %s to %s\n", print_cell(fn), print_cell(args));
    if (CELL_TYPE(fn) == LAMBDA) {
        // eval (cdr fn) (concat (zip (car fn) args) env)
        cell lambda_body = cdr(CELL_PTR(fn)->def);
        cell lambda_args = car(CELL_PTR(fn)->def);
        cell lambda_env = CELL_PTR(fn)->env;
        cell new_def = zip(LIST2(lambda_args, args));
        cell new_env = concat(LIST2(new_def, lambda_env));
        return eval(lambda_body, new_env);
    }
    else if (CELL_TYPE(fn) == BUILTIN_FUNCTION) {
        if (CELL_DEREF(fn).with_env)
            return CELL_DEREF(fn).fn(args, env);
        else
            return CELL_DEREF(fn).fn(args);
    }
#ifndef DISABLE_FFI
    else if (CELL_TYPE(fn) == FFI_FUNCTION) {
        return apply_ffi_function(fn, args);
    }
#endif
    return NIL;
}


cell lambda(cell args, cell env) {
    if (!args) return NIL;
    cell vars = car(args);
    if (CELL_TYPE(vars) == SYMBOL)
        vars = LIST1(vars);
    cell expr = cdr(args);
    DPRINTF("Making lambda %s = %s in %s\n", print_cell(vars), print_cell(expr), print_env(env));
    cell c = allocate_cell();
    CELL_DEREF(c).def = cons(vars, expr);
    CELL_DEREF(c).env = env;
    return c | LAMBDA;
}

cell quote(cell args) { return args; }

cell if_fn(cell args, cell env) {
    if (!args) return NIL;
    cell predicate = eval(car(args), env);
    if (!IS_PAIR(cdr(args))) return NIL;
    if (predicate) return eval(cdar(args), env);
    if (!IS_PAIR(cddr(args))) return NIL;
    if (cdddr(args))
        // if a "fourth argument" is provided, eval it as part of the "else"
        return eval(cddr(args), env);
    else
        return eval(cddar(args), env);
}


cell evalmap(cell args, cell env) {
    if (!args) return NIL;
    if (!IS_PAIR(args)) return eval(args, env);

    // explicitly evaluate in argument order
    cell first = eval(car(args), env);

    if (IS_PAIR(cdr(args)))
        return cons(first, evalmap(cdr(args), env));
    else
        return cons(first, eval(cdr(args), env));
}

cell eval(cell c, cell env) {
    if (stack_base - (char*) (&c) > 0x40000) DIE("Stack overflowed");
    if IS_PAIR(c) {
        DPRINTF("Evalling %s in %s\n", print_cell(c), print_env(env));
        // () x y -> () 1 2
        if (!car(c)) return evalmap(cdr(c), env);
        cell first = eval(car(c), env);
        cell rest = cdr(c);
        // (x y) -> 1 2
        if (!rest) return first;
        // x . y -> 1 . 2
        if (!IS_PAIR(rest)) return cons(first, eval(rest, env));
        if (IS_CALLABLE(first)) {
            if (CELL_TYPE(first) != BUILTIN_FUNCTION || !CELL_DEREF(first).hold_args)
                rest = evalmap(rest, env);
            // apply f to args
            return apply(LIST2(first, rest), env);
        }
        // x y -> 1 2
        return cons(first, evalmap(rest, env));
    }
    else if (CELL_TYPE(c) == SYMBOL) {
        cell resolved_symbol = assoc(LIST2(c, env));
        //DPRINTF("Looked up %s in %s, found %s\n", print_cell(c), print_cell(env), print_cell(resolved_symbol));
        // x -> 1
        if (resolved_symbol) return cdr(resolved_symbol);
        char* sym_name = CELL_DEREF(c).symbol;
#ifndef DISABLE_FFI
        cell ffi_fn = find_ffi_function(sym_name, env);

        // libc.puts -> FFI_FUNCTION<...>
        if (ffi_fn) return ffi_fn;
#endif
    }
    return c;
}

cell def(cell args, cell env) {
    cell referent = eval(cdr(args), env);
    cell var_name = eval(car(args), env);
    if (CELL_TYPE(var_name) != SYMBOL) var_name = car(args);
    DPRINTF("Defining %s -> %s\n", print_cell(var_name), print_cell(referent));
    cdr(global_env) = cons(cons(var_name, referent), cdr(global_env));
    return NIL;
}

cell with(cell args, cell env) {
    if (!cdr(args)) return NIL;
    cell referent = eval(cdar(args), env);
    cell var_name = car(args);
    DPRINTF("With %s -> %s\n", print_cell(var_name), print_cell(referent));
    return eval(cddr(args), concat(LIST2(LIST1(cons(var_name, referent)), env)));
}

cell hash(cell args) {
    if (!args) return make_s64(0);
    if (CELL_TYPE(car(args)) != SYMBOL) return make_s64((uint64_t) car(args));

    // djb2 best hash
    uint64_t hash = 5381;
    char* s = CELL_PTR(car(args))->symbol;
    while (*s)
        hash = ((hash << 5) + hash) + *(s++);

    return make_s64(hash);
}
