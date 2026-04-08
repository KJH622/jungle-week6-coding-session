#include "error.h"

#include <stdio.h>

void print_error(const char *msg) { fprintf(stderr, "ERROR: %s\n", msg); }
