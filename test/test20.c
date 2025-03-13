/* This file is a part of Gecko Parser (GLR Parser) project.
   Copyright (C) 2025 Vladimir Makarov <vmakarov.gcc@gmail.com>.
*/

#include <stdlib.h>

#include "common.h"

static const char *input = "a+a*(a*a+a)";

static const char *description
  = "\n"
    "TERM ID = 0 KW = 1000000;\n"
    "S : ID KW;\n";

int main (int argc, char **argv) {
  test_complex_parse (false, false, false, 3, false, argc, argv);
  exit (0);
}
