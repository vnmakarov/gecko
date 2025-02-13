/* This file is a part of GP (General Parser) project.
   Copyright (C) 2025 Vladimir Makarov <vmakarov.gcc@gmail.com>.
*/

#include <stdio.h>
#include <stdlib.h>

#include "common.h"

#define S10 "+a+a+a+a+a+a+a+a+a+a"
#define S100 S10 S10 S10 S10 S10 S10 S10 S10 S10 S10
static const char *input = "a" S100 S100;

static const char *description
  = "\n"
    "E : E '+' E # plus (0 2)\n"
    "  | 'a'     # 0\n"
    "  ;\n";

int main (int argc, char **argv) {
  test_complex_parse (false, true, false, false, argc, argv);
  exit (0);
}
