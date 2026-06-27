/* Minimal reproducer: gecko hangs on '{' END_OF_FILE (missing '}').
   Grammar: program -> stmt_list END_OF_FILE
            stmt_list -> (empty) | '{' stmt_list '}'
   Input: '{' END_OF_FILE
   Expected: gp_parse terminates with error.
   Actual: gp_parse loops forever in error recovery. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gecko.h"

static const char *grammar =
  "TERM END_OF_FILE;\n"
  "program : stmt_list END_OF_FILE # 0 ;\n"
  "stmt_list :                     # -\n"
  "          | '{' stmt_list '}'   # 0\n"
  "          ;\n";

enum { T_END_OF_FILE = 256 };

static int tok_pos;

static int read_token (void **attr) {
  *attr = NULL;
  if (tok_pos == 0) { tok_pos++; return '{'; }
  if (tok_pos == 1) { tok_pos++; return T_END_OF_FILE; }
  return -1;
}

static void *my_alloc (size_t n) { return malloc (n); }
static void my_free (void *p) { free (p); }

static void my_error (const char *nt, bool after, const char *tok,
                      void *attr, const char *stop, void *sattr) {
  fprintf (stderr, "syntax error %s %s on %s",
           after ? "after" : "in", nt, tok);
  if (stop) fprintf (stderr, ", stopped on %s", stop);
  fprintf (stderr, "\n");
}

int main (void) {
  struct grammar *g = gp_create_grammar ();
  struct gp_tree_node *root;
  int ambiguity;

  if (gp_parse_grammar (g, true, grammar) != 0) {
    fprintf (stderr, "grammar error: %s\n", gp_error_message (g));
    return 1;
  }
  gp_set_parse_alloc (g, my_alloc);
  gp_set_parse_free (g, my_free);
  gp_set_syntax_error (g, my_error);
  fprintf (stderr, "calling gp_parse...\n");
  int rc = gp_parse (g, read_token, &root, &ambiguity, NULL);
  fprintf (stderr, "gp_parse returned %d\n", rc);
  gp_fin (g);
  return rc != 0 ? 1 : 0;
}
