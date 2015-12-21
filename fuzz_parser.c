#include "crisp.h"

// This shell program reads a line from stdin, parses it,
// prints it out again, parses it once more, prints it again,
// then checks whether the two outputs of print are identical
// If they are not, the parse and print functions are not stable
// The program crashes to signal this so a fuzzer can generate and
// minimize a test case for an error
int main(int argc, char** argv) {
    char* line = NULL;
    size_t len = 0;
    if (-1 == getline(&line, &len, stdin))
        return 0;

    char* to_parse = line;
    cell expr = parse(&to_parse);
    char* formatted_1 = print_cell(expr);
    to_parse = formatted_1;
    expr = parse(&to_parse);
    char* formatted_2 = print_cell(expr);

    printf("%s\n%s\n", formatted_1, formatted_2);

    if (strcmp(formatted_1, formatted_2)) *(int*) 0 = 0;
}
