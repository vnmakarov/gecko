/* This file is a part of Gecko (a GLR parser) project.
   Copyright (C) 2026 Vladimir Makarov <vmakarov.gcc@gmail.com>.
*/

#include <stdlib.h>

#include "common.h"

static const char *input = "s";

static const char *description
  = "\n"
    "P  : 's' opt                # prog (1)\n"
    "opt :                       # opt (-)\n"
    "opt : 'c'                   # optc (-)";

int main (int argc, char **argv) {
  test_complex_parse (0, 0, 0, 1, argc, argv);
  exit (0);
}
