#include <crisp.h>
#include <err.h>

cell assert(cell args, cell env){
    if(!args)
        errx(-1, "assert called without an argument");
    if(!eval(args, env))
        errx(-1, "fatal assertion failure: %s", print_cell(args));
    return NIL;
}
