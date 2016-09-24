#include <crisp.h>

// This is a type code which will be filled in when loaded
uint64_t MAP;

// For now we just use assoc to implement our map as a PoC
cell map_lookup(cell args, cell env) {
    if (!args || TYPE(cdr(args)) != PAIR) return NIL;
    cell key = car(args), dict = cdar(args);
    DPRINTF("Deferring to assoc\n", 0);
    return assoc(key, CAST(dict, PAIR));
}

cell mkmap(cell args, cell env) {
    if (!args || TYPE(car(args)) != PAIR) return NIL;
    DPRINTF("yielding map type %d\n", MAP >> 48);
    return CAST(car(args), MAP);
}
