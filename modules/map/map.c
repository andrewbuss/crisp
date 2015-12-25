#include <crisp.h>

// This is a type code which will be filled in when loaded
uint64_t MAP;
//
//cell map(cell args, cell env) {
//    return assoc_fn(args, NIL);
//}

cell mkmap(cell args, cell env) {
    if (!args) return NIL;
    return CAST(car(args), MAP);
}