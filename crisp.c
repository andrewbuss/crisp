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
    return CAST(rv, S64);
}

cell cons(cell car, cell cdr) {
    cell c = (cell) malloc_or_die(16);
    ((pair*) c)->car = car;
    ((pair*) c)->cdr = cdr;
    return CAST(c, PAIR);
}

cell make_fn(cell args, cell body, cell env) {
    DPRINTF("\x1b[33m" "Making lambda %s = %s in %s\n" "\x1b[0m", print_cell(args), print_cell(body), print_env(env));
    cell c = (cell) malloc_or_die(sizeof(fn_t));
    ((fn_t*) c)->args = args;
    ((fn_t*) c)->body = body;
    ((fn_t*) c)->env = env;
    return CAST(c, FN);
}

cell lambda(cell args, cell env) {
    if (!args) return NIL;
    cell body = cdr(args);
    args = car(args);
    if (!IS_PAIR(args)) args = LIST1(args);
    return make_fn(args, body, env);
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
    cell interned = sym_dedupe(sym_list, symbol);
    if (interned) return interned;
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

// zip yields a list containing pairs of corresponding elements from
// a and b. This implementation of zip handles improper left lists
// by pairing the terminal element of a with the remaining elements of b
cell zip(cell a, cell b) {
    // zip (a b) (c d) -> (a . c) (b . d)
    // zip (a b) (c d e) -> (a . c) (b . d)
    // zip (a . b) (c d e) -> (a . c) (b . (d e))
    if (!a) return NIL;
    if (!IS_PAIR(a)) return LIST1(cons(a, b));
    if (!IS_PAIR(b)) return NIL;
    return cons(cons(car(a), car(b)), zip(cdr(a), cdr(b)));
}

// assoc yields the pair in dict whose car is equal to key
// We need to distinguish between the cases where dict[key]
// is nil and when key is not in dict
cell assoc(cell key, cell dict) {
    if (!IS_PAIR(dict) || !IS_PAIR(car(dict))) return NIL;
    if (equal(key, caar(dict))) return car(dict);
    return assoc(key, cdr(dict));
}

// apply a callable to a list of args in a given environment
// To support tail call optimization, apply can mutate args and env
// and return true to signal `eval` to use the existing stack frame
// to continue executing.
bool apply(cell fn, cell* args, cell* env) {
    DPRINTF("\x1b[32m" "Applying %s\n      to %s\n" "\x1b[0m", print_cell(fn), print_cell(*args));
    switch ((uint64_t) TYPE(fn)) {
        case FN:
        case MACRO: {
            // For lambdas and macros we simply "slide" the evaluation
            // sideways into the body of the lambda, adding some new
            // definitions to the environment
            fn_t* l = (fn_t*) PTR(fn);
            cell new_env;
            // If our arguments of the form () . rest, we just
            // set up the single argument
            if (!car(l->args) && TYPE(cdr(l->args)) == SYMBOL)
                new_env = LIST1(cons(cdr(l->args), *args));
            else
                new_env = zip(l->args, *args);
            *env = concat(new_env, TYPE(fn) == MACRO ? *env : l->env);
            // we haven't finished evaluating, so signal that we should continue
            // in the loop

            // Update the evaluation context and environment
            TC_SLIDE(l->body);
        }
        case NATIVE_FN_TCO:
            // The function we are calling can itself potentially invoke a
            // tail call, so just passb in a pointer to the evaluation context
            return (bool) FN_PTR(fn)(args, env);
        case NATIVE_FN:
        case NATIVE_MACRO:
            // We can't make any assumptions about the native function, so
            // evaluate it normally in a new stack frame
            TC_RETURN(FN_PTR(fn)(*args, *env));
        case CONS:
            // TODO: TCO modulo cons does not work when (apply cons (a b))
            // is evaluated. Fixing this would muddy the already confusing
            // interface of apply, since it'd need to modify the caller's
            // new_cons value, so that'd need to be piped in all the way
            // through apply_fn
            TC_RETURN(cons_fn(*args, NIL));
#ifndef DISABLE_FFI
        case FFI_FN:
            TC_RETURN(apply_ffi_function(FFI_FN_PTR(fn), *args));
#endif
        default:
            puts("Tried to apply something uncallable");
            exit(-1);
            *args = NIL;
            return false;
    }
}

// Apply a callable first argument to a second argument
// Arguments following the second are not evaluated
// This function is tail call optimized
bool apply_fn(cell* args, cell* env) {
    if (!*args) TC_RETURN(NIL);
    cell fn = eval(car(*args), *env);
    // apply f . x ->
    if (!IS_CALLABLE(fn) || !IS_PAIR(cdr(*args))) TC_RETURN(NIL);
    *args = eval(cdar(*args), *env);
    if (!IS_PAIR(*args)) *args = LIST1(*args);
    DPRINTF("\x1b[35m" "apply_fn got %s from evalmap\n" "\x1b[0m", print_cell(*args));
    return apply(fn, args, env);
}

cell quote(cell args, cell env) { return args ? car(args) : NIL; }

// Evaluate the first argument as a predicate
// If non-nil, slide to evaluate the second argument
// Otherwise slide to the third argument and beyond
bool if_fn(cell* args, cell* env) {
    if (!*args) TC_RETURN(NIL);
    // We can't avoid creating a new stack frame here
    cell predicate = eval(car(*args), *env);

    // Return nil if there was no "then" branch
    if (!IS_PAIR(cdr(*args))) TC_RETURN(NIL);

    // The predicate was true; slide the evaluation context to the second arg
    if (predicate) TC_SLIDE(cdar(*args));

    // The predicate was false but no "else" branch was provided
    if (!IS_PAIR(cddr(*args))) TC_RETURN(NIL);

    // The predicate was false; slide the evaluation context sideways to the fourth arg
    if (cdddr(*args))
        TC_SLIDE(cddr(*args));
    else
        TC_SLIDE(cddar(*args));
}

// apply eval to each element of a list individually
cell evalmap(cell args, cell env) {
    if (!args) return NIL;
    if (!IS_PAIR(args)) return eval(args, env);

    // explicitly evaluate in argument order
    // .. important for FFI functions
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
//    printf("Depth %d\n", depth);
    if (depth > 0x200000) {
        puts("Stack overflowed");
        exit(-1);
    }

    // TCO notes:
    // As far as possible, we try to avoid calling eval from eval.
    // For basic tail call optimization, crisp supports a type NATIVE_FN_TCO
    // Functions of this type accept references to their args and to the environment
    // and return true or false.

    // Returning true indicates that the function has mutated
    // args and eval should repeat its work in the same stack frame but the new
    // evaluation context.

    // Returning false indicates that eval should simply return the value in args

    // For extra challenge, this supports TCO modulo cons. This allows optimization
    // of calls whose result is used as an argument to cons.

    // For example, (cons (f 1 2) (g 4 5)) can be evaluated by creating a new pair,
    // evaluating (f 1 2) and storing the result in the car of the pair, and then
    // sliding the execution context right to (g 4 5) without deepening the stack.
    // We need only keep track of which value to eventually return, and where to
    // store the result of a particular evaluation when it's complete. These are
    // rv and new_cons below. This allows us to nest this feature, evaluating
    // cons a cons b cons c f d in constant stack space. When we evaluate the top
    // level, we create a new pair and store it in rv. Then we evaluate a and
    // store it on the left, save the address of the right side of the pair in cur_cons
    // and then slide the evaluation context right to cons b cons c f d.
    // Eventually, when f d is evaluated, the result is stored in the right side
    // of the pair containing c on the left, and the original rv is returned

    // rv is the value to eventually return from eval
    cell rv = NIL;

    // new_cons is either inserted into the cdr of cur_cons or returned as rv
    // depending on whether we choose to continue or return
    cell new_cons = NIL;

    // cur_cons is a pointer to a cell that should be filled in before returning
    // or continuing
    cell* cur_cons = NULL;

    // We use a loop so that
    while (1) {
        if IS_PAIR(c) {
            DPRINTF("\x1b[0m" "Evalling %s in %s\n" "\x1b[0m", print_cell(c), print_env(env));
            // () x y -> () 1 2
            if (!car(c)) {
                // evalmap rather than slide because we don't want to apply
                // the second element of the list to the others if it is callable
                new_cons = cons(NIL, evalmap(cdr(c), env));
                goto eval_return;
            }
            // (x) -> eval x -> 1
            if (!cdr(c)) {
                c = car(c);
                continue;
            }
            cell first = eval(car(c), env);
            c = cdr(c);
            // (x y) -> 1 2
            if (!c) {
                // We evaluated the first element already
                new_cons = first;
                goto eval_return;
            }
            // x . y -> 1 . (eval y) -> 1 . 2
            if (!IS_PAIR(c)) {
                // Evaluate the cdr of the dotted pair
                // and put the result into the cdr of the pair we create here
                new_cons = cons(first, NIL);
                goto eval_slide;
            }
            // cons x y -> 1 . (eval y) -> 1 . 2
            if (TYPE(first) == CONS) {
                new_cons = cons(eval(car(c), env), NIL);
                c = cdr(c);
                goto eval_slide;
            }

            if (TYPE(first) == FFI_SYM) first = CAST(first, FFI_FN);
            // sum x y -> sum (eval x) (eval y) -> apply sum (1 2) -> 3
            if (IS_CALLABLE(first)) {
                cell fn = first;
                switch (TYPE(fn)) {
                    // These types expect their arguments to be evaluated first
                    case NATIVE_FN:
                    case FN:
                    case FFI_FN:
                        c = evalmap(c, env);
                    default:
                        break;
                }

                if (!IS_PAIR(c)) c = LIST1(c);

                // apply fn to args

                if (apply(fn, &c, &env))
                    // if apply returned true, c and env have been updated to a new context
                    // so we can slide right without deepening the stack
                    goto eval_slide;
                // otherwise new_cons has been updated to the final result of evaluation
                // so we can return from this frame of eval
                new_cons = c;
                goto eval_return;
            }
            // x y -> 1 2
            new_cons = cons(first, evalmap(c, env));
            goto eval_return;
        }
        else if (TYPE(c) == SYMBOL) {
            cell resolved_symbol = assoc(c, env);
//        DPRINTF("Looked up %s in %s, found %s\n", print_cell(c), print_cell(env), print_cell(resolved_symbol));
            // x -> 1
            if (resolved_symbol) {
                new_cons = cdr(resolved_symbol);
                goto eval_return;
            }

#ifndef DISABLE_FFI
            cell ffi_sym = find_ffi_sym(SYM_STR(c), env);
            if (ffi_sym) {
                new_cons = ffi_sym;
                goto eval_return;
            }
#endif
        }

        new_cons = c;
        goto eval_return;

        eval_slide:

        if (new_cons) {
            // Slide down and right, and remember to save the result in the cdr of new_cons
            DPRINTF("\x1b[34m" "Slide into cdr of new cons %p\n" "\x1b[0m", new_cons);
            if (cur_cons)
                *cur_cons = new_cons;
            else
                rv = new_cons;
            cur_cons = (cell*) ((cell) (PTR(new_cons)) + 8);
            new_cons = NIL;
        }
        else {
            // Slide sideways into a new execution context
            DPRINTF("\x1b[34m" "Slide sideways... \n" "\x1b[0m", NULL);
        }

        continue;
    }
    eval_return:

    if (cur_cons) {
        DPRINTF("\x1b[34m" "Return %s into %p\n" "\x1b[0m", print_cell(new_cons), cur_cons);
        *cur_cons = new_cons;
    }
    else rv = new_cons;
    return rv;
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

bool with(cell* args, cell* env) {
    // with x ->
    if (!IS_PAIR(cdr(*args))) TC_RETURN(NIL);
    cell referent = eval(cdar(*args), *env);
    // with x 2 ->
    if (!IS_PAIR(cddr(*args))) TC_RETURN(NIL);
    cell var_name = car(*args);
    // with 4 x ->
    if (TYPE(var_name) != SYMBOL) var_name = eval(var_name, *env);
    DPRINTF("\x1b[31m" "With %s -> %s\n" "\x1b[0m", print_cell(var_name), print_cell(referent));
    // with x 2 (sum x 4) -> sum 2 4
    *env = cons(cons(var_name, referent), *env);
    *args = cddr(*args);
    return true;
}
