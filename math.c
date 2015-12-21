#include "crisp.h"

cell sum(cell args) {
    // sum 1 2 -> 3
    // sum 1 () 2 -> 1
    // sum -> 0
    if (!args || CELL_TYPE(car(args)) != S64) return make_s64(0);
    return make_s64(CELL_DEREF(car(args)).s64 + CELL_DEREF(sum(cdr(args))).s64);
}

cell product(cell args) {
    // product 2 3 -> 6
    // product 4 () 2 -> 4
    // product -> 1
    if (!args || CELL_TYPE(car(args)) != S64) return make_s64(1);
    return make_s64(CELL_DEREF(car(args)).s64 * CELL_DEREF(product(cdr(args))).s64);
}

cell modulus(cell args) {
    // modulus 7 3 -> 1
    // modulus 7 0 ->
    // modulus () 2 ->
    // modulus 2 () ->
    // modulus 2 ->
    if (!args || !IS_PAIR(cdr(args))) return NIL;
    cell a = car(args);
    cell b = cdar(args);
    if (!IS_S64(a) || !IS_S64(b)) return NIL;
    if (CELL_DEREF(b).s64 == 0) return NIL;
    return make_s64(CELL_DEREF(a).s64 % CELL_DEREF(b).s64);
}

// Returns the first arg if args are strictly ascending
cell asc(cell args) {
    // asc ->
    // asc 1 -> 1
    // asc 4 2 ->
    // asc 4 4 ->
    // asc 2 4 -> 2
    if (!args || !IS_S64(car(args))) return NIL;
    if (!cdr(args)) return car(args);
    if (!IS_PAIR(cdr(args))) return NIL;
    cell next = asc(cdr(args));
    if (!next) return NIL;
    if (CELL_DEREF(car(args)).s64 < CELL_DEREF(next).s64) return car(args);
    return NIL;
}
