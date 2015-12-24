#include <crisp.h>

cell sum(cell args, cell env) {
    // sum 1 2 -> 3
    // sum 1 () 2 -> 1
    // sum -> 0
    if (!args || CELL_TYPE(car(args)) != S64) return make_s64(0);
    int64_t first = S64_VAL(car(args));
    if (!IS_PAIR(cdr(args))) return make_s64(first);
    return make_s64(first + S64_VAL(sum(cdr(args), NIL)));
}

cell product(cell args, cell env) {
    // product 2 3 -> 6
    // product 4 () 2 -> 4
    // product -> 1
    if (!args || CELL_TYPE(car(args)) != S64) return make_s64(1);
    return make_s64(S64_VAL(car(args)) * S64_VAL(product(cdr(args), NIL)));
}

cell modulus(cell args, cell env) {
    // modulus 7 3 -> 1
    // modulus 7 0 ->
    // modulus () 2 ->
    // modulus 2 () ->
    // modulus 2 ->
    if (!args || !IS_PAIR(cdr(args))) return NIL;
    cell a = car(args);
    cell b = cdar(args);
    if (!IS_S64(a) || !IS_S64(b)) return NIL;
    if (S64_VAL(b) == 0) return NIL;
    return make_s64(S64_VAL(a) % S64_VAL(b));
}

// Returns the first arg if args are strictly ascending
cell asc(cell args, cell env) {
    // asc ->
    // asc 1 -> 1
    // asc 4 2 ->
    // asc 4 4 ->
    // asc 2 4 -> 2
    if (!args || !IS_S64(car(args))) return NIL;
    if (!cdr(args)) return car(args);
    if (!IS_PAIR(cdr(args))) return NIL;
    cell next = asc(cdr(args), NIL);
    if (!next) return NIL;
    if (S64_VAL(car(args)) < S64_VAL(next)) return car(args);
    return NIL;
}
