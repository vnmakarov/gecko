/* This file is a part of Gecko (a GLR parser) project.
   Copyright (C) 2026 Vladimir Makarov <vmakarov.gcc@gmail.com>.
*/

#include <stdlib.h>

#include "common.h"

static const char *input = "a+a*(a*a+a)";

static const char *description
  = "\n"
    "E : E '+' E   # plus (0 2)\n"
    "  | E '*' E   # mult (0 2)\n"
    "  | 'a'       # 0\n"
    "  | '(' E ')' # 1\n"
    "  ;\n";

int main (int argc, char **argv) {
  test_complex_parse (1, 0, 0, 1, argc, argv);
  exit (0);
}
