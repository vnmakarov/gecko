/* This file is a part of Gecko Parser (GLR Parser) project.
   Copyright (C) 2025 Vladimir Makarov <vmakarov.gcc@gmail.com>.
*/

#include <stdlib.h>

#include "common.h"

static const char *input = "a+a*(a*a+a)";

static const char *description
  = "\n"
    "TERM\n"
    "LEFT 'a'\n"
    "LEFT 'a'\n"
    "S : 'a';\n";

int main (int argc, char **argv) {
  test_complex_parse (false, false, false, false, argc, argv);
  exit (0);
}
