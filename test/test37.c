/* This file is a part of Gecko (a GLR parser) project.
   Copyright (C) 2026 Vladimir Makarov <vmakarov.gcc@gmail.com>.
*/

#include <stdlib.h>

#include "common.h"

/* The following variable is the current number of next input grammar
   terminal. */
static int nterm;

/* The following function imported by Gecko (see comments in the interface file). */
const char *read_terminal (int *code, int *priority, enum gp_assoc *assoc) {
  nterm++;
  *priority = 0;
  *assoc = GP_NON_ASSOC;
  switch (nterm) {
  case 1: *code = 'a'; return "a";
  case 2:
    *code = '+';
    *priority = 0;
    *assoc = GP_LEFT_ASSOC;
    return "+";
  case 3:
    *code = '*';
    *priority = 1;
    *assoc = GP_LEFT_ASSOC;
    return "*";
  case 4: *code = '('; return "(";
  case 5: *code = ')'; return ")";
  default: return NULL;
  }
}

/* The following variable is the current number of next rule grammar
   terminal. */
static int nrule;

/* The following function imported by Gecko (see comments in the interface file). */
const char *read_rule (const char ***rhs, const char **anode, int **transl) {
  static const char *rhs_1[] = {"E", "+", "E", NULL};
  static int tr_1[] = {0, 2, -1};
  static const char *rhs_2[] = {"E", "*", "E", NULL};
  static int tr_2[] = {0, 2, -1};
  static const char *rhs_3[] = {"a", NULL};
  static int tr_3[] = {0, -1};
  static const char *rhs_4[] = {"(", "E", ")", NULL};
  static int tr_4[] = {1, -1};

  nrule++;
  switch (nrule) {
  case 1:
    *rhs = rhs_1;
    *anode = "plus";
    *transl = tr_1;
    return "E";
  case 2:
    *rhs = rhs_2;
    *anode = "mult";
    *transl = tr_2;
    return "E";
  case 3:
    *rhs = rhs_3;
    *anode = NULL;
    *transl = tr_3;
    return "E";
  case 4:
    *rhs = rhs_4;
    *anode = NULL;
    *transl = tr_4;
    return "E";
  default: return NULL;
  }
}

static const char *input = "a+a*(a*a+a)";

int main (void) {
  test_standard_read (true, read_terminal, read_rule);
  exit (0);
}
