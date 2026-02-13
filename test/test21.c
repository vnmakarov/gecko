/* This file is a part of Gecko Parser (GLR Parser) project.
   Copyright (C) 2026 Vladimir Makarov <vmakarov.gcc@gmail.com>.
*/

#include <stdlib.h>

#include "common.h"

static const char *input = "a+a*(a*a+a)";

static const char *description
  = "\n"
    "TERM\n"
    "LEFT 'a'\n"
    "S : 'b';\n";

int main (int argc, char **argv) {
  test_complex_parse (false, false, 3, false, argc, argv);
  exit (0);
}
