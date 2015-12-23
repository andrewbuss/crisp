#include "crisp.h"

// This file contains definitions for the core of crisp

bool debug = false;
cell global_env = NIL;
char* stack_base = NULL;
cell sym_list = NIL;

cell malloc_or_die(size_t size) {
    cell rv = (cell) GC_MALLOC(size);
    if (!rv) {
        puts("malloc failed");
        exit(-1);
    }
    return rv;
}

cell make_s64(int64_t x) {
    cell rv = malloc_or_die(8);
    *(int64_t*) rv = x;
    return rv | S64;
}

cell make_native_function(void* fn, int with_env, int hold_args) {
    cell c = (cell) malloc_or_die(16);
    CELL_DEREF(c).fn = fn;
    CELL_DEREF(c).with_env = with_env;
    CELL_DEREF(c).hold_args = hold_args;
    return c | NATIVE_FUNCTION;
}

cell cons(cell car, cell cdr) {
    cell c = (cell) malloc_or_die(16);
    CELL_DEREF(c).car = car;
    CELL_DEREF(c).cdr = cdr;
    return c | PAIR;
}

cell lambda(cell args, cell env) {
    if (!args) return NIL;
    cell lambda_args = car(args);
    if (CELL_TYPE(lambda_args) == SYMBOL)
        lambda_args = LIST1(lambda_args);
    cell body = cdr(args);
    DPRINTF("Making lambda %s = %s in %s\n", print_cell(lambda_args), print_cell(body), print_env(env));
    cell c = (cell) malloc_or_die(24);
    CELL_DEREF(c).args = lambda_args;
    CELL_DEREF(c).body = body;
    CELL_DEREF(c).env = env;
    return c | LAMBDA;
}

// Look up symbol in list
cell sym_dedupe(cell list, char* symbol) {
    if (!IS_PAIR(list)) return NIL;
    if (!strcmp(SYM_STR(car(list)), symbol)) return car(list);
    return sym_dedupe(cdr(list), symbol);
}


cell sym(char* symbol) {
    cell deduped = sym_dedupe(sym_list, symbol);
    if(deduped) return deduped;
    sym_list = cons((cell)strdup(symbol) | SYMBOL, sym_list);
    return car(sym_list);
}

// Builtin functions

// The contract for these functions is that the argument
// may be either NIL or a PAIR - atomic types are not
// allowed

// Some functions take an additional `env` argument. The
// interpreter must create the wrapping cell for these
// functions using hold_args = true.

cell car_fn(cell args, cell env) {
    // car 4 ->
    // car (a b) -> a
    if (!args || !IS_PAIR(car(args))) return NIL;
    return caar(args);
}

cell cdr_fn(cell args, cell env) {
    // cdr 4 ->
    // cdr (a b) -> b
    if (!args || !IS_PAIR(car(args))) return NIL;
    return cadr(args);
}

cell cons_fn(cell args, cell env) {
    // cons 4 -> 4
    // cons 4 5 -> 4 . 5
    // cons a . b ->
    if (!args) return NIL;
    if (!cdr(args)) return args;
    if (!IS_PAIR(cdr(args))) return NIL;
    return cons(car(args), cdar(args));
}

cell ispair(cell args, cell env) {
    // ispair 4 ->
    // ispair () -> ()
    // ispair (a b) -> (a b)
    if (!args) return NIL;
    return IS_PAIR(car(args)) ? car(args) : (cell) NIL;
}

// Check for pointer equality among args
// If one argument is not equal, returns NIL
// Otherwise returns the first argument
cell same(cell args, cell env) {
    // same ->
    // same 2 -> 2

    // If 2 and 2 were instantiated separately they may not be same
    // same 2 2 ->

    // For any x:
    // (lambda y same y y) x -> x
    if (!args) return NIL;
    cell first = car(args);
    cell rest = cdr(args);
    if (!rest || (ispair(rest, NIL) && (first == same(rest, NIL)))) return first;
    return NIL;
}

static inline cell equal(cell left, cell right) {
    if (left == right) {
        return left;
    }
    if (IS_S64(left) && IS_S64(right) && S64_VAL(left) == S64_VAL(right))
        return left;
    return NIL;
}

// Check for value equality among args, or equality of symbols
// If one argument is not equal, returns NIL
// Otherwise returns the first argument
cell equal_fn(cell args, cell env) {
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
    cell right = equal_fn(cdr(args), NIL);
    if (!right) return NIL;
    if (equal(left, right)) return left;
    return NIL;
}

cell concat(cell first, cell rest) {
    if (!first) return rest;
    if (IS_PAIR(first))
        return cons(car(first), concat(cdr(first), rest));
    else
        return cons(first, rest);
}

// Concatenate arguments
cell concat_fn(cell args, cell env) {
    // concat (a b) c (d e) -> a b c d e
    // concat ((a b) (c d)) e -> (a b) (c d) e
    // concat a -> a
    // concat a . b -> a b
    if (!args) return NIL;
    if (!IS_PAIR(args)) return LIST1(args);
    cell first = car(args);
    if (!IS_PAIR(first)) first = LIST1(first);
    return concat(first, concat_fn(cdr(args), NIL));
}

// Pair up items from a left and right list into a
// new list of cons pairs until one list runs out
cell zip_fn(cell args, cell env) {
    // zip (a b c) (d e) -> (a . d) (b . e)
    // zip () () ->
    // zip ->
    // zip a b ->
    // zip (a b) (c . d) -> (a . c)
    if (!args || !IS_PAIR(cdr(args))) return NIL;
    cell a = car(args), b = cdar(args);
    return zip(a, b);
}

cell zip(cell a, cell b) {
    if (!IS_PAIR(a) || !IS_PAIR(b)) return NIL;
    return cons(cons(car(a), car(b)), zip(cdr(a), cdr(b)));
}

cell assoc(cell key, cell dict) {
    if (!IS_PAIR(dict) || !IS_PAIR(car(dict))) return NIL;
    if (equal(key, caar(dict))) return car(dict);
    return assoc(key, cdr(dict));
}

cell assoc_fn(cell args, cell env) {
    if (!args || !IS_PAIR(cdr(args))) return NIL;
    cell key = car(args), dict = cdar(args);
    return assoc(key, dict);
}

cell apply_fn(cell args, cell env) {
    if (!args) return NIL;
    cell fn = car(args);
    if (!IS_CALLABLE(fn)) return NIL;
    if (!IS_PAIR(cdr(args))) args = NIL;
    else args = cdar(args);
    return apply(fn, args, env);
}

cell apply(cell fn, cell args, cell env) {
    if (!IS_PAIR(args)) args = LIST1(args);
    DPRINTF("Applying %s to %s\n", print_cell(fn), print_cell(args));
    if (CELL_TYPE(fn) == LAMBDA) {
        cell new_def = zip(CELL_PTR(fn)->args, args);
        cell new_env = concat(new_def, CELL_PTR(fn)->env);
        return eval(CELL_PTR(fn)->body, new_env);
    }
    else if (CELL_TYPE(fn) == NATIVE_FUNCTION) {
        return CELL_DEREF(fn).fn(args, env);
    }
#ifndef DISABLE_FFI
    else if (CELL_TYPE(fn) == FFI_FUNCTION) {
        return apply_ffi_function(fn, args);
    }
#endif
    return NIL;
}


cell quote(cell args, cell env) { return args; }

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
    // Hack to limit recursion depth
    // It is otherwise trivial to crash the interpeter with infinite recursion
    if (stack_base - (char*) (&c) > 0x40000) {
        puts("Stack overflowed");
        exit(-1);
    }
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
            if (CELL_TYPE(first) != NATIVE_FUNCTION || !CELL_DEREF(first).hold_args)
                rest = evalmap(rest, env);
            // apply f to args
            return apply(first, rest, env);
        }
        // x y -> 1 2
        return cons(first, evalmap(rest, env));
    }
    else if (CELL_TYPE(c) == SYMBOL) {
        cell resolved_symbol = assoc(c, env);
        //DPRINTF("Looked up %s in %s, found %s\n", print_cell(c), print_cell(env), print_cell(resolved_symbol));
        // x -> 1
        if (resolved_symbol) return cdr(resolved_symbol);

#ifndef DISABLE_FFI
        cell ffi_fn = find_ffi_function((char*) CELL_PTR(c), env);

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
    if (!IS_PAIR(cdr(args))) return NIL;
    cell referent = eval(cdar(args), env);
    if (!IS_PAIR(cddr(args))) return NIL;
    cell var_name = car(args);
    if (CELL_TYPE(var_name) != SYMBOL) return NIL;
    DPRINTF("With %s -> %s\n", print_cell(var_name), print_cell(referent));
    return eval(cddr(args), cons(cons(var_name, referent), env));
}

cell hash(cell args, cell env) {
    if (!args) return make_s64(0);
    if (CELL_TYPE(car(args)) != SYMBOL) return make_s64((uint64_t) car(args));

    // djb2 best hash
    uint64_t hash = 5381;
    char* s = SYM_STR(car(args));
    while (*s)
        hash = ((hash << 5) + hash) + *(s++);

    return make_s64(hash);
}
