/* This file is a part of Gecko Parser (GLR Parser) project.
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
  *priority = -1;
  *assoc = GP_NON_ASSOC;
  switch (nterm) {
  case 1: *code = 'a'; return "a";
  case 2: *code = '+'; return "+";
  case 3: *code = '*'; return "*";
  case 4: *code = '('; return "(";
  case 5: *code = ')'; return ")";
  case 6: *code = 'e'; return "$eof";
  default: return NULL;
  }
}

/* The following variable is the current number of next rule grammar
   terminal. */
static int nrule;

/* The following function imported by Gecko (see comments in the interface file). */
const char *read_rule (const char ***rhs, const char **anode, int **transl, int *guard_num) {
  static const char *rhs_1[] = {"T", NULL};
  static int tr_1[] = {0, -1};
  static const char *rhs_2[] = {"E", "+", "T", NULL};
  static int tr_2[] = {0, 2, -1};
  static const char *rhs_3[] = {"F", NULL};
  static int tr_3[] = {0, -1};
  static const char *rhs_4[] = {"T", "*", "F", NULL};
  static int tr_4[] = {0, 2, -1};
  static const char *rhs_5[] = {"a", NULL};
  static int tr_5[] = {0, -1};
  static const char *rhs_6[] = {"(", "E", ")", NULL};
  static int tr_6[] = {1, -1};

  *guard_num = -1;
  nrule++;
  switch (nrule) {
  case 1:
    *rhs = rhs_1;
    *anode = NULL;
    *transl = tr_1;
    return "E";
  case 2:
    *rhs = rhs_2;
    *anode = "plus";
    *transl = tr_2;
    return "E";
  case 3:
    *rhs = rhs_3;
    *anode = NULL;
    *transl = tr_3;
    return "T";
  case 4:
    *rhs = rhs_4;
    *anode = "mult";
    *transl = tr_4;
    return "T";
  case 5:
    *rhs = rhs_5;
    *anode = NULL;
    *transl = tr_5;
    return "F";
  case 6:
    *rhs = rhs_6;
    *anode = NULL;
    *transl = tr_6;
    return "F";
  default: return NULL;
  }
}

static const char *input = "a+a*(a*a+a)";

int main (void) {
  test_standard_read (false, read_terminal, read_rule);
  exit (0);
}
