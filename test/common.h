/* This file is a part of Gecko Parser (GLR Parser) project.
   Copyright (C) 2026 Vladimir Makarov <vmakarov.gcc@gmail.com>.
*/

#ifndef GP_TEST_C_COMMON_H_
#define GP_TEST_C_COMMON_H_

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef YAEP
#include "yaep.h"
#define ambig_type int
#define tree_node yaep_tree_node
#define set_debug_level yaep_set_debug_level
#define set_error_recovery_flag yaep_set_error_recovery_flag
#define set_cost_flag yaep_set_cost_flag
#define create_grammar yaep_create_grammar
#define read_grammar yaep_read_grammar
#define set_recovery_match yaep_set_recovery_match
#define create_grammar yaep_create_grammar
#define parse_grammar yaep_parse_grammar
#define free_grammar yaep_free_grammar
#define error_message yaep_error_message
#else
#include "gecko.h"
#define ambig_type bool
#define tree_node gp_tree_node
#define set_debug_level gp_set_debug_level
#define set_error_recovery_flag gp_set_error_recovery_flag
#define set_cost_flag gp_set_cost_flag
#define create_grammar gp_create_grammar
#define read_grammar gp_read_grammar
#define set_recovery_match gp_set_recovery_match
#define parse_grammar gp_parse_grammar
#define free_grammar gp_fin
#define error_message gp_error_message
#endif

static void *test_parse_alloc (int size) {
  void *result;

  assert ((size > 0) && ((unsigned int) size == (size_t) size));
  result = malloc (size);
  assert (result != NULL);

  return result;
}

static void test_parse_free (void *mem) { free (mem); }

/* Printing syntax error. */
#ifdef YAEP
static void test_syntax_error (int err_tok_num, void *err_tok_attr, int start_ignored_tok_num,
                               void *start_ignored_tok_attr, int start_recovered_tok_num,
                               void *start_recovered_tok_attr) {
  if (start_ignored_tok_num < 0)
    fprintf (stderr, "Syntax error on token %d\n", err_tok_num);
  else
    fprintf (stderr, "Syntax error on token %d:ignore %d tokens starting with token = %d\n", err_tok_num,
             start_recovered_tok_num - start_ignored_tok_num, start_ignored_tok_num);
}
#else
static void test_syntax_error (const char *err_tok_repr, void *err_tok_attr GP_UNUSED,
                               const char *stop_tok_repr, void *stop_tok_attr GP_UNUSED) {
  if (stop_tok_repr == NULL)
    fprintf (stderr, "Syntax error on token %s (pos=%d)\n", err_tok_repr, (int) (ptrdiff_t) err_tok_attr);
  else
    fprintf (stderr, "Syntax error on token %s (pos=%d) and stopping on token %s (pos=%d)\n", err_tok_repr,
             (int) (ptrdiff_t) err_tok_attr, stop_tok_repr, (int) (ptrdiff_t) stop_tok_attr);
}
#endif

static const char *input;
static const char *description;

/* The following function imported by GP (see comments in the interface file). */
static int test_read_token (void **attr) {
  static int ntok = 0;

  *attr = (void *) (ptrdiff_t) ntok;
  if (input[ntok]) {
    return input[ntok++];
  } else {
    return -1;
  }
}

static void test_standard_parse (void) {
  struct grammar *g;
  struct tree_node *root;
  ambig_type ambiguous_p;

  if ((g = create_grammar ()) == NULL) {
    fprintf (stderr, "create_grammar: No memory\n");
    exit (1);
  }
  if (parse_grammar (g, 1, description) != 0) {
    fprintf (stderr, "%s\n", error_message (g));
    exit (1);
  }
#ifdef YAEP
  bool fail = yaep_parse (g, test_read_token, test_syntax_error, test_parse_alloc, test_parse_free, &root,
                          &ambiguous_p);
#else
  gp_set_parse_alloc (g, test_parse_alloc);
  gp_set_parse_free (g, test_parse_free);
  gp_set_syntax_error (g, test_syntax_error);
  bool fail = gp_parse (g, test_read_token, &root, &ambiguous_p);
#endif
  if (fail) {
    fprintf (stderr, "gp parse: %s\n", error_message (g));
    exit (1);
  }
  free_grammar (g);
}

static bool test_standard_read (bool print_transl,
#ifdef YAEP
                                const char *(*read_terminal) (int *),
                                const char *(*read_rule) (const char ***, const char **, int *, int **) ) {
#else
                                const char *(*read_terminal) (int *, int *, enum gp_assoc *),
                                const char *(*read_rule) (const char ***, const char **, int **) ) {
#endif
  struct grammar *g;
  struct tree_node *root;
  ambig_type ambiguous_p;

  if ((g = create_grammar ()) == NULL) {
    fprintf (stderr, "create_grammar: No memory\n");
    exit (1);
  }
  if (read_grammar (g, 1, read_terminal, read_rule) != 0) {
    fprintf (stderr, "%s\n", error_message (g));
    exit (1);
  }
#ifdef YAEP
  bool fail = yaep_parse (g, test_read_token, test_syntax_error, test_parse_alloc, test_parse_free, &root,
                          &ambiguous_p);
#else
  gp_set_parse_alloc (g, test_parse_alloc);
  gp_set_parse_free (g, test_parse_free);
  gp_set_syntax_error (g, test_syntax_error);
  bool fail = gp_parse (g, test_read_token, &root, &ambiguous_p);
#endif
  if (fail) {
    fprintf (stderr, "gp parse: %s\n", error_message (g));
    exit (1);
  }
#ifndef YAEP
  if (print_transl) gp_print_translation (g, stderr, root);
#endif
  free_grammar (g);
  return ambiguous_p;
}

#ifdef __GNUC__
#define UNUSED __attribute__ ((unused))
#else
#define UNUSED
#endif

static void test_complex_parse (bool ambiguous, bool print_cost UNUSED, int recovery_match, bool print_transl,
                                int argc, char **argv) {
  struct grammar *g;
  struct tree_node *root;
  ambig_type ambiguous_p;

  if ((g = create_grammar ()) == NULL) {
    fprintf (stderr, "create_grammar: No memory\n");
    exit (1);
  }
#ifdef YAEP
  if (print_cost) set_cost_flag (g, true);
#endif
  if (argc > 1)
    set_debug_level (g, atoi (argv[1]));
  else
    set_debug_level (g, 3);
#ifdef YAEP
  if (argc > 2) set_error_recovery_flag (g, atoi (argv[2]));
#endif
  if (recovery_match) set_recovery_match (g, recovery_match);
#ifdef YAEP
  yaep_set_one_parse_flag (g, false);
#endif
  if (parse_grammar (g, 1, description) != 0) {
    fprintf (stderr, "%s\n", error_message (g));
    exit (1);
  }
#ifdef YAEP
  bool fail = yaep_parse (g, test_read_token, test_syntax_error, test_parse_alloc, test_parse_free, &root,
                          &ambiguous_p);
#else
  gp_set_parse_alloc (g, test_parse_alloc);
  gp_set_parse_free (g, test_parse_free);
  gp_set_syntax_error (g, test_syntax_error);
  bool fail = gp_parse (g, test_read_token, &root, &ambiguous_p);
#endif
  if (fail) {
    fprintf (stderr, "gp parse: %s\n", error_message (g));
    exit (1);
  }
  if (ambiguous != ambiguous_p) {
    fprintf (stderr, "Grammar should be %sambiguous\n", ambiguous ? "" : "un");
    exit (1);
  }
#ifndef YAEP
  if (print_transl) gp_print_translation (g, stderr, root);
#endif
  free_grammar (g);
}

#endif
