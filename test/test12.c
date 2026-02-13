/* This file is a part of Gecko Parser (GLR Parser) project.
   Copyright (C) 2026 Vladimir Makarov <vmakarov.gcc@gmail.com>.
*/

#include <stdlib.h>

#include "common.h"

static const char *input = "a+a*(a*a+a)";

static const char *description
  = "E : A O O\n"
    "  | E 'a'\n"
    "  | 'b'\n"
    "  |\n"
    "  ;\n"
    "\n"
    "A : O O N\n"
    "  ;\n"
    "\n"
    "N : O E E O\n"
    "  ;\n"
    "\n"
    "O :\n"
    "  ;\n";

int main (void) {
  test_standard_parse ();
  exit (0);
}
