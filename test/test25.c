/* This file is a part of Gecko Parser (GLR Parser) project.
   Copyright (C) 2025 Vladimir Makarov <vmakarov.gcc@gmail.com>.
*/

#include <stdlib.h>

#include "common.h"

static const char *input = "a+a**(a+a)";

static const char *description
  = "\n"
    "TERM;\n"
    "LEFT '+';\n"
    "LEFT '*';\n"
    "E : 'a'         # 0\n"
    "  | '(' E ')'   # 1\n"
    "  | E '+' E     # plus (0 2)\n"
    "  | E '*' E     # mult (0 2)\n"
    "  ;\n";

int main (int argc, char **argv) {
  test_complex_parse (false, false, 3, true, argc, argv);
  exit (0);
}
