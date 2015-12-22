#define _GNU_SOURCE

#include "crisp.h"
#include <stdarg.h>

// Returns the cell represented by the string starting at *s
// The mechanics of this function are strange. *s advances
// along the string while parse constructs the corresponding
// lists
cell parse(char** s) {
    // Skip whitespace
    while (isspace(**s)) (*s)++;
    if (!**s) return NIL;
    switch (**s) {
        case ')':
            (*s)++;
            return NIL;
        case '(': {
            (*s)++;
            cell first = parse(s);
            return cons(first, parse(s));
        }
        case '\'': {
            (*s)++;
            cell rest = parse(s);
            // ' -> ()
            if (!rest) return NIL;

            // '.a -> ()
            // ' -> ()
            if (!IS_PAIR(rest)) return NIL;

            // 'a -> (quote a)
            if (!IS_PAIR(car(rest))) return cons(LIST2(sym("quote"), car(rest)), cdr(rest));

            // '(a b c) -> (quote a b c)
            return cons(cons(sym("quote"), car(rest)), cdr(rest));
        }
        case '.': {
            (*s)++;
            cell rest = parse(s);
            if (!rest) return NIL;
            if (CELL_TYPE(rest) != PAIR) return NIL;
            return car(rest);
        }
        default: {
            char* i = *s;
            while (*i && !isspace(*i) && *i != '(' && *i != ')') i++;
            size_t token_len = i - *s;

            char* token = strncpy(GC_MALLOC_ATOMIC(token_len + 1), *s, token_len);
            token[token_len] = '\0';
            *s = i;
            cell c;

            // Try to turn the token into a number
            char* endptr;
            long val = strtol(token, &endptr, 0);
            if (endptr != token) c = make_s64(val);
            else c = sym(token);

            return cons(c, parse(s));
        }
    }
}

static char* buf = NULL;
static size_t buf_len = 0;
static int buf_index = 0;

// A combination of sprintf and strcat, catf safely appends formatted
// strings to the end of the buffer, enlarging the buffer as needed
inline static int catf(char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char* new_part;
    vasprintf(&new_part, fmt, args);
    va_end(args);
    size_t extra_len = strlen(new_part);
    if (buf_index + extra_len >= buf_len) {
        buf = GC_REALLOC(buf, buf_len = (buf_index + extra_len) * 2);
    }
    memcpy(buf + buf_index, new_part, extra_len + 1);
    free(new_part);
    return buf_index += extra_len;
}

// Recursive print function - updates buf_index as appropriate
// during its traversal of c
static int print(cell c) {
    switch (CELL_TYPE(c)) {
        case PAIR:
            if (CELL_TYPE(car(c)) == PAIR) {
                catf("(");
                print(car(c));
                catf(")");
            }
            else print(car(c));
            if (!cdr(c)) return 0;

            catf(" ");
            if (CELL_TYPE(cdr(c)) != PAIR)
                catf(". ");
            return print(cdr(c));
        case S64:
            return catf("%ld", S64_VAL(c));
        case SYMBOL:
            return catf("%s", SYM_STR(c));
        case BUILTIN_FUNCTION:
            return catf("BUILTIN_FUNCTION<%p>", CELL_DEREF(c).fn);
        case FFI_FUNCTION:
            return catf("FFI_FUNCTION<%p>", CELL_DEREF(c).fn);
        case FFI_LIBRARY:
            return catf("FFI_LIBRARY<%p>", CELL_DEREF(c).handle);
        case LAMBDA:
            catf("(lambda (");
            print(CELL_PTR(c)->args);
            catf(") ");
            print(CELL_PTR(c)->body);
            return catf(")");
        case NIL:
            return catf("()");
        default:
            return catf("UNKNOWN<%p>", c);
    }
}

char* print_cell(cell c) {
    buf = GC_MALLOC(64);
    buf_len = 64;
    buf_index = 0;
    print(c);
    return buf;
}

// Handy for pretty-printing local variables in an env
char* print_env(cell c) {
    buf = GC_MALLOC(64);
    buf_len = 64;
    buf_index = 0;
    catf("(");
    while (IS_PAIR(c)) {
        if(!IS_PAIR(car(c))) break;
        if(CELL_TYPE(caar(c)) != SYMBOL) break;
        if(!strcmp(SYM_STR(caar(c)), "GLOBALS")) break;
        catf("\n");
        print(car(c));
        c = cdr(c);
    }
    catf(")");
    return buf;
}