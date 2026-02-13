/* This file is a part of Gecko Parser (GLR Parser) project.
   Copyright (C) 2026 Vladimir Makarov <vmakarov.gcc@gmail.com>.
*/

#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "ticker.h"

#define S10 "+a+a+a+a+a+a+a+a+a+a"
#define S100 S10 S10 S10 S10 S10 S10 S10 S10 S10 S10
static const char *input = "a" S100 S100;
// static const char *input = "a+a+a+a";

static const char *description
  = "\n"
    "E : E '+' E\n"
    "  | 'a'\n"
    "  ;\n";

#ifdef linux
#include <unistd.h>
#endif

int main (int argc, char **argv) {
  ticker_t t = create_ticker ();
#ifdef linux
  char *start = sbrk (0);
#endif
  test_complex_parse (true, false, 3, false, argc, argv);
#ifdef linux
  printf ("parse time %.2f, memory=%.1fkB\n", active_time (t), ((char *) sbrk (0) - start) / 1024.);
#else
  printf ("parse time %.2f\n", active_time (t));
#endif
  exit (0);
}
