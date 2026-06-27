/* Test: gecko hangs on unexpected EOF during error recovery.
   Grammar has recursive stmt_list inside a block.
   Input "a {" is missing the closing "}".
   Expected: gp_parse terminates (with or without error).
   Actual: gp_parse loops forever in recovery. */
#include <stdio.h>
#include <stdlib.h>
#include "gecko.h"

static const char *grammar =
  "TERM a ;\n"
  "\n"
  "program : stmt_list # 0\n"
  "        ;\n"
  "\n"
  "stmt_list :                  # -\n"
  "          | stmt_list stmt   # 0\n"
  "          ;\n"
  "\n"
  "stmt : a ';'                 # 0\n"
  "     | '{' stmt_list '}'    # 0\n"
  "     ;\n";

enum { T_A = 256 };

static int tok_pos;
/* Input: a ; { <EOF>  —  missing '}' */
static int tokens[] = { T_A, ';', '{', -1 };

static int read_token (void **attr) {
  int code;
  if (tok_pos >= 4) return -1;
  code = tokens[tok_pos++];
  *attr = NULL;
  return code;
}

static void *my_alloc (size_t n) { return malloc (n); }
static void my_free (void *p) { free (p); }

static void my_error (const char *nt, bool after, const char *tok,
                      void *attr, const char *stop, void *sattr) {
  fprintf (stderr, "syntax error %s %s on %s\n",
           after ? "after" : "in", nt, tok);
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
