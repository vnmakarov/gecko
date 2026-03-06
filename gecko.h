/* This file is a part of Gecko Parser (GLR parser) project.
   Copyright (C) 2026 Vladimir Makarov <vmakarov.gcc@gmail.com>.
*/

/* This is interface file of general (working on any CFG) syntax parser with minimal error recovery
   and syntax directed translation. The algorithm is described in doc directory.  The
   algorithm is sufficiently fast to be used in serious language processors. */

#ifndef __GECKO__
#define __GECKO__

#include <stdbool.h>
#include <limits.h>

#ifdef __GNUC__
#define GP_UNUSED __attribute__ ((unused))
#else
#define GP_UNUSED
#endif

/* The following is a forward declaration of grammar formed by function gp_read_grammar. */
struct grammar;

/* The following value is reserved to be designation of empty node for translation.  It should be
   positive number which is not intersected with symbol numbers. */
#define GP_NIL_TRANSLATION_NUMBER INT_MAX

/* The following values are Gecko Parser error codes: */
#define GP_NO_MEMORY 1
#define GP_UNDEFINED_OR_BAD_GRAMMAR 2
#define GP_WRONG_ARG 3
#define GP_DESCRIPTION_SYNTAX_ERROR_CODE 4
#define GP_FIXED_NAME_USAGE 5
#define GP_REPEATED_TERM_DECL 6
#define GP_NEGATIVE_TERM_CODE 7
#define GP_TOO_WIDE_TERM_RANGE_CODE 8
#define GP_REPEATED_TERM_CODE 9
#define GP_REPEATED_TERM_ASSOC 10
#define GP_UNDEFINED_TERM_ASSOC 11
#define GP_NO_RULES 12
#define GP_TERM_IN_RULE_LHS 13
#define GP_INCORRECT_TRANSLATION 14
#define GP_INCORRECT_SYMBOL_NUMBER 15
#define GP_REPEATED_SYMBOL_NUMBER 16
#define GP_UNACCESSIBLE_NONTERM 17
#define GP_NONTERM_DERIVATION 18
#define GP_LOOP_NONTERM 19
#define GP_INVALID_TOKEN_CODE 20

enum gp_assoc { GP_NON_ASSOC, GP_LEFT_ASSOC, GP_RIGHT_ASSOC };

enum gp_tree_node_type { /* the parse tree node: */
                         GP_NIL,
                         GP_ERROR,
                         GP_TERM,
                         GP_ANODE,
                         GP_ALT,
                         GP_VISITED = 0x80, /* for internal use only */
};

struct gp_nil { /* The node exists in one exemplar. See comment to read_rule. */
};

/* The following node exists in one example.  It is used as translation of terminal `error': */
struct gp_error { /* The node exists in one exemplar. The translation of terminal `error': */
};

struct gp_term { /* the terminal node: */
  int code;      /* the terminal code */
  void *attr;    /* the terminal attributes */
};

struct gp_anode {   /* the abstract node: */
  int children_num; /* elements in the next array */
  const char *name; /* the abstract node name */
  /* References for nodes for which the abstract node refers.  The array end marker is NULL. */
  struct gp_tree_node **children;
};

struct gp_alt { /* alternative translations: */
  struct gp_tree_node *first, *second;
};

struct gp_tree_node {          /* the generalized node of the parse tree: */
  enum gp_tree_node_type type; /* the type of node */
  unsigned num;                /* node number */
  union {                      /* the node itself */
    struct gp_nil nil;
    struct gp_error error;
    struct gp_term term;
    struct gp_anode anode;
    struct gp_alt alt; /* alternative */
  } val;
};

/* Create undefined grammar.  The function returns NULL if there is no memory.
   The function should be called the first. */
extern struct grammar *gp_create_grammar (void);

/* Return the last occurred error code for given grammar. */
extern int gp_error_code (struct grammar *g);

/* Return message containing error message corresponding to the last occurred error code. */
extern const char *gp_error_message (struct grammar *g);

/* Read terminals/rules into grammar G and checks it depending on STRICT_P.
   Returns zero if it is all ok. Otherwise, return error code occurred (its code
   will be in gp_error_code and message in gp_error_message).

   READ_TERMINAL is function for reading terminals.  This function is called before function
   read_rule.  The function should return the name and the code of the next terminal.  If all
   terminals have been read the function returns NULL.  The return code should be nonnegative.

   READ_RULE is function called to read the next rule.  This function is called after function
   read_terminal.  The function should return the name of LHS rule and array of names of symbols in
   RHS of the rule (the array end marker should be NULL).  If all rules have been read the function
   returns NULL.  All symbol with name which was not provided the previous function are considered
   to be nonterminal. The function also returns translation given by abstract node name and its
   fields which will be translation of symbols (with indexes given in array *TRANSL) in the RHS of
   the rule.  All indexes in TRANSL should be different (so the translation of a symbol can not be
   represented twice).  The end marker of the array should be a negative value.  There is a reserved
   value of the translation symbol number denoting empty node.  It is value defined by macro
   GP_NIL_TRANSLATION_NUMBER.  If *TRANSL is NULL or contains only the end marker, translations of
   the rule will be nil node.  If ABS_NODE is NULL, abstract node is not created.  In this case
   *TRANSL should be NULL or contain at most one element which means that the translation of the
   rule will be nil node or the translation of the symbol in RHS given by the single array element. */
extern int gp_read_grammar (struct grammar *g, bool strict_p,
                            const char *(*read_terminal) (int *code, int *priority, enum gp_assoc *assoc),
                            const char *(*read_rule) (const char ***rhs, const char **abs_node,
                                                      int **transl));

/* Analogous to the previous one but it parses grammar description. */
extern int gp_parse_grammar (struct grammar *g, bool strict_p, const char *description);

/* The following functions set up different parameters which affect parser work.  The functions
   return the previous parameter value.

   * parse_alloc is used to allocate memory for parse tree representation.  By default it is malloc.
     It should be always not NULL.

   * parse_free is used to free memory for parse tree representation.  By default it is free.
     NULL value means no any freeing.

   * syntax error function is used to prints error message about syntax error which occurred on token with
     representation ERR_TOK_REPR and attribute ERR_TOK_ATTR (see type gp_syntax_error_func_t).  The next two
     parameters describes recovery stop token.  If the error recovery is not made (see comments for function
     `gp_set_error_recovery_flag'), the 3rd and 4th parameters will be NULL.  The default function prints only
     the token representations.  You should set up the new function to print positions which can be passed
     through the token attributes.

   * debug_level says what debugging information to output (it works only if we compiled without
     defined macro NO_GP_DEBUG_PRINT):

     * 0 (default value) means print nothing
     * 1 results in printing statistics
     * 2 results in additional print of the result translation
     * 3 results in printing read token, actions (conflicts marked by '!') for dynamically generated
       SLR sets, and high-level erorr recovery info
     * 4 means printing rules, first/follows nonterminal sets, dynamically generated SLR sets,
       and reshapping translation for ambiguous parsing
     * 5 results in printing stacks during parsing and stack merging
     * 6 results in even more detail info about processing stacks during parsing

   * error_recovery_flag means making error recovery if syntax error occurred.  Otherwise, syntax
     error results in finishing parsing (although syntax_error is called once).  The default value
     is true.

   * recovery_match means how much subsequent tokens should be successfully shifted to finish error
     recovery.  The default value is 3.

   * gp_set_attr_merge_func is used during merging stacks.  It gets two attributes of a symbol
     (terminal or nonterminal) from stacks being merged and returns the result attribute.  Null FUNC
     sets up the default function which returns always the first attr.  The default function is set
     up in GP_CREATE_GRAMMAR.  Using the default function results in returning only one translation
     by GP_PARSE. */

typedef void *(*gp_parse_alloc_func_t) (int);
typedef void (*gp_parse_free_func_t) (void *);
extern gp_parse_alloc_func_t gp_set_parse_alloc (struct grammar *g, gp_parse_alloc_func_t fn);
extern gp_parse_free_func_t gp_set_parse_free (struct grammar *g, gp_parse_free_func_t fn);

/* The function  */
typedef void (*gp_syntax_error_func_t) (const char *err_tok_repr, void *err_tok_attr,
                                        const char *stop_tok_repr, void *stop_tok_attr);
extern gp_syntax_error_func_t gp_set_syntax_error (struct grammar *g, gp_syntax_error_func_t fn);

extern int gp_set_debug_level (struct grammar *grammar, int level);
extern bool gp_set_error_recovery_flag (struct grammar *grammar, bool flag);
extern int gp_set_recovery_match (struct grammar *grammar, int n_toks);

typedef void *(*gp_attr_merge_func_t) (void *attr1, void *attr2);
extern gp_attr_merge_func_t gp_set_attr_merge_func (struct grammar *grammar, gp_attr_merge_func_t func);

/* Parse input according the read grammar. Returns the error code (which will be also in
   gp_error_code). If the code is zero, also put parse result into *root (*root will be NULL only if
   syntax error was occurred and error recovery was switched off).  Set up *AMBIGOUS_P if we found
   that the grammar is ambiguous.

   The function READ_TOKEN provides input tokens.  It returns code the next input token and its
   attribute.  If the function returns negative value we've read all tokens. */
extern int gp_parse (struct grammar *grammar, int (*read_token) (void **attr), struct gp_tree_node **root,
                     bool *ambiguous_p);

#ifndef NO_GP_DEBUG_PRINT
/* Print translation of ROOT parsed for GRAMMAR. */
extern void gp_print_translation (struct grammar *grammar, FILE *f, struct gp_tree_node *root);
#endif

/* Free memory allocated for the parse tree. ROOT must be the root of the parse tree as returned by
   gp_parse(). If ROOT is a null pointer, no operation is performed.  If TERMCB is not a null pointer,
   it will be called exactly once for each term node in the parse tree. The TERMCB callback can be used
   by the caller to free the term attributes. The term node itself must not be freed by TERMCB. */
extern void gp_free_tree (struct grammar *grammar, struct gp_tree_node *root,
                          void (*termcb) (struct gp_term *term));

/* Finish work with the grammar.  It should be called the last. */
extern void gp_fin (struct grammar *grammar);

#endif /* #ifndef __GECKO__ */
