#include "crisp.h"

int main(int argc, char** argv) {
    char* line = NULL;
    size_t len = 0;
    if (-1 == getline(&line, &len, stdin))
        return 0;

    char* to_parse = line;
    cell expr = parse(&to_parse);
    char* formatted_1 = strdup(print_cell(expr));
    to_parse = formatted_1;
    expr = parse(&to_parse);
    char* formatted_2 = print_cell(expr);

    printf("%s\n%s\n", formatted_1, formatted_2);

    if (strcmp(formatted_1, formatted_2)) {
        *(int*) 0 = 0;
    }
}
