#include <crisp.h>

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

cell ispair(cell args, cell env) {
    // ispair 4 ->
    // ispair () -> ()
    // ispair (a b) -> (a b)
    if (!args) return NIL;
    return IS_PAIR(car(args)) ? car(args) : (cell) NIL;
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