/* Arena-allocation benchmark for Gecko.

   Parses a large generated expression with a tree-building grammar (an anode per operator) and
   compares parse time and parse_alloc call count with the arena ON (the default) vs OFF
   (per-node malloc).

   Usage: bench_arena [n_tokens [rounds]]    (defaults: 2000000 tokens, 7 rounds) */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "gecko.h"
#include "ticker.h"

static long n_tokens; /* total tokens to generate */
static long pos;

/* Deterministic expression token stream:  a (+|*) a (+|*) a ...  (always a valid expression). */
static int read_token (void **attr) {
  *attr = NULL;
  if (pos >= n_tokens) return -1; /* EOF */
  long i = pos++;
  if (i % 2 == 0) return 'a';
  return (i % 4 == 1) ? '+' : '*';
}

static void syntax_err (const char *a, bool b, const char *c, void *d, const char *e, void *f) {
  (void) a, (void) b, (void) c, (void) d, (void) e, (void) f;
}

static const char *description
  = "E : T         # 0\n"
    "  | E '+' T   # plus (0 2)\n"
    "  ;\n"
    "T : F         # 0\n"
    "  | T '*' F   # mult (0 2)\n"
    "  ;\n"
    "F : 'a'       # 0\n"
    "  ;\n";

static uint64_t alloc_count;
static void *counting_alloc (size_t size) {
  alloc_count++;
  return malloc (size);
}

/* Parse the stream once with the arena on (ARENA_P is true) or off.  When COUNT is non-NULL, install
   a counting parse_alloc and store the parse_alloc call count there (the timed value from that
   pass is meaningless and discarded).  Returns parse time in seconds, or -1 on error. */
static double parse_once (bool arena_p, uint64_t *count) {
  struct grammar *g = gp_create_grammar ();
  if (g == NULL) return -1;
  gp_set_syntax_error (g, syntax_err);
  gp_set_parse_arena (g, arena_p);
  if (count != NULL) {
    alloc_count = 0;
    gp_set_parse_alloc (g, counting_alloc);
  }
  if (gp_parse_grammar (g, 1, description)) {
    gp_fin (g);
    return -1;
  }
  pos = 0;
  struct gp_tree_node *root;
  int ambiguity;
  ticker_t t = create_ticker ();
  int r = gp_parse (g, read_token, &root, &ambiguity, NULL);
  double elapsed = active_time (t);
  if (count != NULL) *count = alloc_count;
  gp_fin (g);
  return r ? -1 : elapsed;
}

int main (int argc, char **argv) {
  n_tokens = argc > 1 ? atol (argv[1]) : 2000000;
  int rounds = argc > 2 ? atoi (argv[2]) : 7;
  if (n_tokens <= 0 || rounds <= 0) {
    fprintf (stderr, "usage: %s [n_tokens (>0) [rounds (>0)]]\n", argv[0]);
    return 2;
  }

  double off_best = 1e9, on_best = 1e9;
  for (int i = 0; i < rounds; i++) {
    double t = parse_once (false, NULL);
    if (t >= 0 && t < off_best) off_best = t;
    t = parse_once (true, NULL);
    if (t >= 0 && t < on_best) on_best = t;
  }
  long off_count = -1, on_count = -1;
  parse_once (false, &off_count);
  parse_once (true, &on_count);

  printf ("Gecko arena-allocation benchmark: %ld tokens, best of %d\n", n_tokens, rounds);
  printf ("                              %16s %16s\n", "non-arena", "arena");
  printf ("  best parse time (s)         %16.4f %16.4f", off_best, on_best);
  if (on_best > 0 && off_best < 1e9)
    printf ("  -> arena is %.2fx faster (%.1f%% less time)\n", off_best / on_best,
            (1.0 - on_best / off_best) * 100.0);
  printf ("  parse_alloc calls           %16ld %16ld", off_count, on_count);
  if (on_count > 0 && off_count > 0)
    printf ("  -> arena makes %.0fx fewer allocations (%.1f%% fewer)\n", (double) off_count / on_count,
            (1.0 - (double) on_count / off_count) * 100.0);
  return (off_best >= 1e9 || on_best >= 1e9) ? 1 : 0;
}
