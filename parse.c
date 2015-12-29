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
            return cons(cons(sym("quote"), rest), cdr(rest));
        }
        case '.': {
            (*s)++;
            cell rest = parse(s);
            if (!rest) return NIL;
            if (TYPE(rest) != PAIR) return NIL;
            return car(rest);
        }
        default: {
            char* i = *s;
            while (*i && !isspace(*i) && *i != '(' && *i != ')') i++;
            size_t token_len = i - *s;

            char* token = strncpy(malloc(token_len + 1), *s, token_len);
            token[token_len] = '\0';
            *s = i;
            cell c;

            // Try to turn the token into a number
            char* endptr;
            long val = strtol(token, &endptr, 0);
            if (endptr != token) c = make_s64(val);
            else c = sym(token);
            free(token);
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
    switch (TYPE(c)) {
        case PAIR:
            if (TYPE(car(c)) == PAIR) {
                catf("(");
                print(car(c));
                catf(")");
            }
            else print(car(c));
            if (!cdr(c)) return 0;

            catf(" ");
            if (TYPE(cdr(c)) != PAIR)
                catf(". ");
            return print(cdr(c));
        case S64:
            return catf("%ld", S64_VAL(c));
        case SYMBOL:
            return catf("%s", SYM_STR(c));
        case NATIVE_FN:
        case NATIVE_FN_TAILCALL:
        case NATIVE_FN_HELD_ARGS:
            return catf("NATIVE_FUNCTION<%p>", PTR(c));
        case FFI_FUNCTION:
            return catf("FFI_FUNCTION<%p>", PTR(c));
        case FFI_LIBRARY:
            return catf("FFI_LIBRARY<%p>", PTR(c));
        case MACRO:
            catf("(macro (");
            goto print_args_body;
        case LAMBDA:
            catf("(lambda (");
        print_args_body:
            print(((lambda_t*)PTR(c))->args);
            catf(") ");
            print(((lambda_t*)PTR(c))->body);
            return catf(")");
        case CONS:
            return catf("CONS");
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
        if (!IS_PAIR(car(c))) break;
        if (TYPE(caar(c)) != SYMBOL) break;
        if (!strcmp(SYM_STR(caar(c)), "GLOBALS")) break;
        catf("\n%20s . ", SYM_STR(caar(c)));
        print(cadr(c));
        c = cdr(c);
    }
    catf(")");
    return buf;
}

void reset_logical_line(logical_line* line) {
    line->in_comment = false;
    line->parens = 0;
    line->len = 0;
    line->str = GC_MALLOC(128);
    line->max_len = 128;
}

// ingest a new character into a logical line, and return true
// if the logical line is complete and can be parsed
bool logical_line_ingest(logical_line* line, char c) {
    switch (c) {
        case '\n':
            line->in_comment = false;
            if (line->parens != 0) {
                c = ' ';
                break;
            }
        case '\0':
            line->str[line->len] = '\0';
            return true;
        case ';':
            line->in_comment = true;
            return false;
        case '(':
            if(!line->in_comment) line->parens++;
            break;
        case ')':
            if(!line->in_comment) line->parens--;
            break;
        default:
            break;
    }
    if (line->in_comment) return false;

    line->str[line->len++] = c;
    if (line->len >= line->max_len)
        line->str = GC_REALLOC(line->str, line->max_len *= 2);
    return false;
}
