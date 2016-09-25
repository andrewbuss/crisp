#include <crisp.h>

cell sum(cell args, cell env) {
    // sum 1 2 -> 3
    // sum 1 () 2 -> 1
    // sum -> 0
    if (!args || !IS_INT(car(args))) return make_int(0);
    int64_t first = INT_VAL(car(args));
    DPRINTF("doing math\n", 0);
    if (!IS_PAIR(cdr(args))) return make_int(first);
    return make_int(first + INT_VAL(sum(cdr(args), NIL)));
}

cell product(cell args, cell env) {
    // product 2 3 -> 6
    // product 4 () 2 -> 4
    // product -> 1
    if (!args || !IS_INT(car(args))) return make_int(1);
    return make_int(INT_VAL(car(args)) * INT_VAL(product(cdr(args), NIL)));
}

cell quotient(cell args, cell env) {
  // quotient 7 4 -> 1
  // quotient 7 2 -> 3
  if (!args || !IS_PAIR(cdr(args))) return NIL;
  cell a = car(args);
  cell b = cdar(args);
  if (!IS_INT(a) || !IS_INT(b)) return NIL;
  if (INT_VAL(b) == 0) return NIL;
  return make_int(INT_VAL(a) / INT_VAL(b));
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
    if (!IS_INT(a) || !IS_INT(b)) return NIL;
    if (INT_VAL(b) == 0) return NIL;
    return make_int(INT_VAL(a) % INT_VAL(b));
}

// Returns the first arg if args are strictly ascending
cell asc(cell args, cell env) {
    // asc ->
    // asc 1 -> 1
    // asc 4 2 ->
    // asc 4 4 ->
    // asc 2 4 -> 2
    if (!args || !IS_INT(car(args))) return NIL;
    if (!cdr(args)) return car(args);
    if (!IS_PAIR(cdr(args))) return NIL;
    cell next = asc(cdr(args), NIL);
    if (!next) return NIL;
    if (INT_VAL(car(args)) < INT_VAL(next)) return car(args);
    return NIL;
}
