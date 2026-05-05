/* This file is a part of Gecko Parser (GLR Parser) project.
   Copyright (C) 2026 Vladimir Makarov <vmakarov.gcc@gmail.com>.
*/

#include <stdlib.h>
#include <string.h>

#include "common.h"

static const char *input = "a+a*a";

static const char *description
  = "\n"
    "TERM;\n"
    "ANODE plus=10 mult=20;\n"
    "E : T         # 0\n"
    "  | E '+' T   # plus (0 2)\n"
    "  ;\n"
    "T : F         # 0\n"
    "  | T '*' F   # mult (0 2)\n"
    "  ;\n"
    "F : 'a'       # 0\n"
    "  ;\n";

static bool check_node (struct gp_tree_node *node, struct gp_tree_node *father GP_UNUSED,
                        void *arg GP_UNUSED) {
  if (node->type == GP_ANODE) {
    if (strcmp (node->val.anode.name, "plus") == 0)
      fprintf (stderr, "plus: aux=%d\n", node->aux);
    else if (strcmp (node->val.anode.name, "mult") == 0)
      fprintf (stderr, "mult: aux=%d\n", node->aux);
  } else if (node->type == GP_TERM) {
    fprintf (stderr, "term: aux=%d\n", node->aux);
  }
  return true;
}

int main (void) {
  struct grammar *g;
  struct gp_tree_node *root;
  int ambiguity;

  if ((g = gp_create_grammar ()) == NULL) {
    fprintf (stderr, "gp_create_grammar: No memory\n");
    exit (1);
  }
  gp_set_debug_level (g, 0);
  if (gp_parse_grammar (g, 1, description) != 0) {
    fprintf (stderr, "%s\n", gp_error_message (g));
    exit (1);
  }
  gp_set_parse_alloc (g, test_parse_alloc);
  gp_set_parse_free (g, test_parse_free);
  gp_set_syntax_error (g, test_syntax_error);
  bool fail = gp_parse (g, test_read_token, &root, &ambiguity, NULL);
  if (fail) {
    fprintf (stderr, "gp parse: %s\n", gp_error_message (g));
    exit (1);
  }
  gp_traverse_tree (g, root, check_node, NULL, NULL);
  gp_free_tree (g, root);
  gp_fin (g);
  exit (0);
}
