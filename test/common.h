/* This file is a part of Gecko Parser (GLR Parser) project.
   Copyright (C) 2025 Vladimir Makarov <vmakarov.gcc@gmail.com>.
*/

#ifndef GP_TEST_C_COMMON_H_
#define GP_TEST_C_COMMON_H_

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef YAEP
#include "yaep.h"
#define ambig_type int
#define tree_node yaep_tree_node
#define set_debug_level yaep_set_debug_level
#define set_error_recovery_flag yaep_set_error_recovery_flag
#define set_cost_flag yaep_set_cost_flag
#define set_one_parse_flag yaep_set_one_parse_flag
#define create_grammar yaep_create_grammar
#define read_grammar yaep_read_grammar
#define set_recovery_match yaep_set_recovery_match
#define create_grammar yaep_create_grammar
#define parse_grammar yaep_parse_grammar
#define parse yaep_parse
#define free_grammar yaep_free_grammar
#define parse yaep_parse
#define error_message yaep_error_message
#else
#include "gecko.h"
#define ambig_type bool
#define tree_node gp_tree_node
#define set_debug_level gp_set_debug_level
#define set_error_recovery_flag gp_set_error_recovery_flag
#define set_cost_flag gp_set_cost_flag
#define set_one_parse_flag gp_set_one_parse_flag
#define create_grammar gp_create_grammar
#define read_grammar gp_read_grammar
#define set_recovery_match gp_set_recovery_match
#define parse_grammar gp_parse_grammar
#define parse gp_parse
#define free_grammar gp_free_grammar
#define parse gp_parse
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
static void test_syntax_error (int err_tok_num, void *err_tok_attr, int start_ignored_tok_num,
                               void *start_ignored_tok_attr, int start_recovered_tok_num,
                               void *start_recovered_tok_attr) {
  if (start_ignored_tok_num < 0)
    fprintf (stderr, "Syntax error on token %d\n", err_tok_num);
  else
    fprintf (stderr, "Syntax error on token %d:ignore %d tokens starting with token = %d\n",
             err_tok_num, start_recovered_tok_num - start_ignored_tok_num, start_ignored_tok_num);
}

static const char *input;
static const char *description;

/* The following function imported by GP (see comments in the interface file). */
static int test_read_token (void **attr) {
  static int ntok = 0;

  *attr = NULL;
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
  if (parse (g, test_read_token, test_syntax_error, test_parse_alloc, test_parse_free, &root,
             &ambiguous_p)) {
    fprintf (stderr, "gp parse: %s\n", error_message (g));
    exit (1);
  }
  free_grammar (g);
}

static void test_standard_read (
#ifdef YAEP
  const char *(*read_terminal) (int *),
#else
  const char *(*read_terminal) (int *, int *, enum gp_assoc *),
#endif
  const char *(*read_rule) (const char ***, const char **, int *, int **) ) {
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
  if (parse (g, test_read_token, test_syntax_error, test_parse_alloc, test_parse_free, &root,
             &ambiguous_p)) {
    fprintf (stderr, "gp parse: %s\n", error_message (g));
    exit (1);
  }
  free_grammar (g);
}

static void test_complex_parse (bool one_parse, bool ambiguous, bool print_cost,
                                bool recovery_match, int argc, char **argv) {
  struct grammar *g;
  struct tree_node *root;
  ambig_type ambiguous_p;

  if ((g = create_grammar ()) == NULL) {
    fprintf (stderr, "create_grammar: No memory\n");
    exit (1);
  }
  set_one_parse_flag (g, one_parse);
  if (print_cost) {
    set_cost_flag (g, 1);
  }
  if (argc > 1)
    set_debug_level (g, atoi (argv[1]));
  else
    set_debug_level (g, 3);
  if (argc > 2) set_error_recovery_flag (g, atoi (argv[2]));
  if (argc > 3) set_one_parse_flag (g, atoi (argv[3]));
  if (recovery_match) {
    set_recovery_match (g, recovery_match);
  }
  if (parse_grammar (g, 1, description) != 0) {
    fprintf (stderr, "%s\n", error_message (g));
    exit (1);
  }
  if (parse (g, test_read_token, test_syntax_error, test_parse_alloc, test_parse_free, &root,
             &ambiguous_p)) {
    fprintf (stderr, "gp parse: %s\n", error_message (g));
    exit (1);
  }
  if (ambiguous != ambiguous_p) {
    fprintf (stderr, "Grammar should be %sambiguous\n", ambiguous ? "" : "un");
    exit (1);
  }
  if (print_cost) {
    fprintf (stderr, "cost = %d\n", root->val.anode.cost);
  }
  free_grammar (g);
}

#endif
