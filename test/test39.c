/* This file is a part of Gecko (a GLR parser) project.
   Copyright (C) 2026 Vladimir Makarov <vmakarov.gcc@gmail.com>.
*/

#include <stdlib.h>

#include "common.h"

static const char *input = "a+(a*a+a)*a";

static const char *description
  = "\n"
    "LEFT '+'\n"
    "RIGHT '*'\n"
    "E : E '+' E   # plus (- 2)\n"
    "  | E '*' E   # mult (0 -)\n"
    "  | E '+' E   # bad (- 2) ?1\n"
    "  | E '*' E   # bad (0 -) ?2\n"
    "  | 'a'       # 0\n"
    "  | '(' E ')' # 1\n"
    "  ;\n";

static bool test_rule_guard (int guard_num, void *arg GP_UNUSED) {
  fprintf (stderr, "guard=%d\n", guard_num);
  return false;
}

int main (int argc, char **argv) {
  rule_guard = test_rule_guard;
  test_complex_parse (0, 0, 0, 1, argc, argv);
  exit (0);
}
