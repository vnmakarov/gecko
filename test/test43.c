/* This file is a part of Gecko Parser (GLR Parser) project.
   Copyright (C) 2026 Vladimir Makarov <vmakarov.gcc@gmail.com>.
*/

#include <stdlib.h>

#include "common.h"

static const char *input = "a";

static const char *description
  = "\n"
    "TERM;\n"
    "ANODE plus=10 plus=20;\n"
    "S : 'a' # 0\n"
    "  ;\n";

int main (void) {
  test_standard_parse ();
  exit (0);
}
