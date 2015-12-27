#include "crisp.h"

// This file contains definitions for the core of crisp

bool debug = false;
cell global_env = NIL;
void* stack_base = NULL;
cell sym_list = NIL;

void* malloc_or_die(size_t size) {
    void* rv = GC_MALLOC(size);
    if (!rv) {
        puts("malloc failed");
        exit(-1);
    }
    return rv;
}

cell make_s64(int64_t x) {
    cell rv = (cell) malloc_or_die(8);
    *(int64_t*) rv = x;
    return rv | S64;
}

cell make_native_function(void* fn, bool with_env, bool hold_args) {
    cell type = NATIVE_FN;
    if (with_env) type += (1LL << 48);
    if (hold_args) type += (2LL << 48);
    return CAST(fn, type);
}

cell cons(cell car, cell cdr) {
    cell c = (cell) malloc_or_die(16);
    ((pair*) c)->car = car;
    ((pair*) c)->cdr = cdr;
    return c | PAIR;
}

cell make_lambda(cell args, cell body, cell env) {
    DPRINTF("\x1b[33m" "Making lambda %s = %s in %s\n" "\x1b[0m", print_cell(args), print_cell(body), print_env(env));
    cell c = (cell) malloc_or_die(sizeof(lambda_t));
    ((lambda_t*) c)->args = args;
    ((lambda_t*) c)->body = body;
    ((lambda_t*) c)->env = env;
    return c | LAMBDA;
}

cell lambda(cell args, cell env) {
    if (!args) return NIL;
    cell body = cdr(args);
    args = car(args);
    if (!IS_PAIR(args)) args = LIST1(args);
    return make_lambda(args, body, env);
}

// A macro is identical to a lambda except that args are left
// unevaluated before being passed in, so the macro has full
// control over the evaluation of its inputs
cell macro(cell args, cell env) {
    return CAST(lambda(args, env), MACRO);
}

// Look up symbol in list
cell sym_dedupe(cell list, char* symbol) {
    if (!IS_PAIR(list)) return NIL;
    if (!strcmp(SYM_STR(car(list)), symbol)) return car(list);
    return sym_dedupe(cdr(list), symbol);
}

// Create a new symbol from the passed string
// If the symbol already exists, return the already created one
cell sym(char* symbol) {
    cell deduped = sym_dedupe(sym_list, symbol);
    if (deduped) return deduped;
    sym_list = cons(CAST(strdup(symbol), SYMBOL), sym_list);
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
    if (!rest || (IS_PAIR(rest) && (first == same(rest, NIL)))) return first;
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

cell zip(cell a, cell b) {
    if (!IS_PAIR(a) || !IS_PAIR(b)) return NIL;
    return cons(cons(car(a), car(b)), zip(cdr(a), cdr(b)));
}

static cell zip_args(cell a, cell b){
    if(!a) return NIL;
    if(!IS_PAIR(a)) return LIST1(cons(a, b));
    if (!IS_PAIR(b)) return NIL;
    return cons(cons(car(a), car(b)), zip_args(cdr(a), cdr(b)));
}

cell assoc(cell key, cell dict) {
    if (!IS_PAIR(dict) || !IS_PAIR(car(dict))) return NIL;
    if (equal(key, caar(dict))) return car(dict);
    return assoc(key, cdr(dict));
}

cell apply(cell fn, cell args, cell env) {
    if (!IS_PAIR(args)) args = LIST1(args);
    DPRINTF("\x1b[32m" "Applying %s\n      to %s\n" "\x1b[0m", print_cell(fn), print_cell(args));
    switch (TYPE(fn)) {
        case LAMBDA:
        case MACRO: {
            lambda_t* l = (lambda_t*) PTR(fn);
            cell new_env;
            if(!car(l->args) && TYPE(cdr(l->args)) == SYMBOL)
                new_env = LIST1(cons(cdr(l->args), args));
            else
                new_env = zip_args(l->args, args);
            return eval(l->body, concat(new_env, TYPE(fn) == MACRO ? env : l->env));
        }
        case NATIVE_FN:
        case NATIVE_FN_HELD_ARGS:
            return FN_PTR(fn)(args);
        case NATIVE_FN_ENV:
        case NATIVE_FN_ENV_HELD_ARGS:
            return FN_PTR(fn)(args, env);
#ifndef DISABLE_FFI
        case FFI_FUNCTION:
            return apply_ffi_function(FFI_FN(fn), args);
#endif
        default:
            return NIL;
    }
}

cell apply_fn(cell args, cell env) {
    if (!args) return NIL;
    cell fn = car(args);
    if (!IS_CALLABLE(fn)) return NIL;
    if (!IS_PAIR(cdr(args))) args = NIL;
    else args = cdar(args);
    return apply(fn, args, env);
}

cell quote(cell args, cell env) { return args ? car(args) : NIL; }

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
    uint64_t depth = stack_base - (void*) &c;
    if (depth > 0x200000) {
        puts("Stack overflowed");
        exit(-1);
    }
    if IS_PAIR(c) {
        DPRINTF("\x1b[0m" "Evalling %s in %s\n" "\x1b[0m", print_cell(c), print_env(env));
        // () x y -> () 1 2
        if (!car(c)) return cons(NIL, evalmap(cdr(c), env));
        cell first = eval(car(c), env);
        cell rest = cdr(c);
        // (x y) -> 1 2
        if (!rest) return first;
        // x . y -> 1 . 2
        if (!IS_PAIR(rest)) return cons(first, eval(rest, env));
        if (IS_CALLABLE(first)) {
            switch (TYPE(first)) {
                case NATIVE_FN:
                case NATIVE_FN_ENV:
                case LAMBDA:
                case FFI_FUNCTION:
                    rest = evalmap(rest, env);
                default:
                    break;
            }
            // apply f to args
            return apply(first, rest, env);
        }
        // x y -> 1 2
        return cons(first, evalmap(rest, env));
    }
    else if (TYPE(c) == SYMBOL) {
        cell resolved_symbol = assoc(c, env);
//        DPRINTF("Looked up %s in %s, found %s\n", print_cell(c), print_cell(env), print_cell(resolved_symbol));
        // x -> 1
        if (resolved_symbol) return cdr(resolved_symbol);

#ifndef DISABLE_FFI
        cell ffi_fn = find_ffi_function(SYM_STR(c), env);
        if (ffi_fn) return ffi_fn;
#endif
    }
    return c;
}

cell def(cell args, cell env) {
    cell referent = eval(cdr(args), env);
    cell var_name = eval(car(args), env);
    if (TYPE(var_name) != SYMBOL) var_name = car(args);
    DPRINTF("\x1b[31m" "Defining %s -> %s\n" "\x1b[0m", print_cell(var_name), print_cell(referent));
    cell old_defs = ((pair*) PTR(global_env))->cdr;
    ((pair*) PTR(global_env))->cdr = cons(cons(var_name, referent), old_defs);
    return NIL;
}

cell with(cell args, cell env) {
    // with x ->
    if (!IS_PAIR(cdr(args))) return NIL;
    cell referent = eval(cdar(args), env);
    // with x 2 ->
    if (!IS_PAIR(cddr(args))) return NIL;
    cell var_name = car(args);
    // with 4 x ->
    if (TYPE(var_name) != SYMBOL) var_name = eval(var_name, env);
    DPRINTF("\x1b[31m" "With %s -> %s\n" "\x1b[0m", print_cell(var_name), print_cell(referent));
    // with x 2 (sum x 4) -> sum 2 4
    return eval(cddr(args), cons(cons(var_name, referent), env));
}
