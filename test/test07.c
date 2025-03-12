/* This file is a part of Gecko Parser (GLR Parser) project.
   Copyright (C) 2025 Vladimir Makarov <vmakarov.gcc@gmail.com>.
*/

#include <stdlib.h>

#include "common.h"

static const char *input = "a+a*(a*a+a)";

static const char *description = "TERM ident; ident : ;\n";

int main (void) {
  test_standard_parse ();
  exit (0);
}
