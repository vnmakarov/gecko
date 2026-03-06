/* This file is a major part of Gecko Parser (GLR parser) project.
   Copyright (C) 2026 Vladimir Makarov <vmakarov.gcc@gmail.com>.
*/

#include <stdio.h>
#include <stdarg.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "allocate.h"
#include "hash.h"
#include "hashtab.h"
#include "vlobject.h"
#include "objstack.h"
#include "bitmap.h"
#include "gecko.h"

#ifdef __GNUC__
#define FORCE_INLINE inline __attribute__ ((always_inline))
#define NO_INLINE __attribute__ ((noinline))
#define LIKELY(c) __builtin_expect (c, 1)
#define UNLIKELY(c) __builtin_expect (c, 0)
#else
#define FORCE_INLINE inline
#define NO_INLINE
#define LIKELY(c) c
#define UNLIKELY(c) c
#endif

#ifndef GP_MAX_ERROR_MESSAGE_LENGTH
#define GP_MAX_ERROR_MESSAGE_LENGTH 200
#endif

struct symbs {             /* information about grammar vocabulary: */
  int n_terms, n_nonterms; /* number of all terminals and non-terminals */
  os_t symbs_os;           /* all symbols are placed in this object */
  /* References to the symbols, terminals, nonterminals are stored in the following vlos. The
     indexes in the arrays are the same as corresponding symbol, terminal, and nonterm numbers. */
  vlo_t symbs_vlo, terms_vlo, nonterms_vlo;
  hash_table_t repr_to_symb_tab;      /* table to find symbol by its representation */
  hash_table_t code_to_term_tab;      /* table to find term symbol by its code */
  struct symb **term_code_trans_vect; /* terminal code to terminal symbol vector */
  int term_code_trans_vect_start, term_code_trans_vect_end;
};

struct sit;
struct set;
struct action_desc;

struct grammar {    /* major structure which stores information about grammar: */
  bool undefined_p; /* true for undefined or erroneous grammar */
  int error_code;   /* the last occurred error code for given grammar */
  char error_message[GP_MAX_ERROR_MESSAGE_LENGTH + 1]; /* the last error message */
  struct symb *axiom;         /* grammar axiom (there is only one rule with axiom in lhs) */
  struct symb *end_marker;    /* auxiliary symbol denoting EOF */
  int recovery_token_matches; /* number of subsequent tokens should be successfuly shifted to finish
                                 error recovery */
  int debug_level;
  bool error_recovery_p;       /* true if we need to make error recovery. */
  struct symbs *symbs;         /* vocabulary used for this grammar */
  struct rules *rules;         /* rules used for this grammar */
  struct term_sets *term_sets; /* terminal sets used for this grammar */
  gp_allocator_t *alloc;       /* allocator of internal parser data (grammar, stacks, etc) */

  jmp_buf error_longjump_buff; /* jump buffer for processing errors */

  gp_attr_merge_func_t attr_merge; /* function for merging stack elements attributes */

  int (*read_token) (void **attr);     /* function for reading tokens */
  gp_parse_alloc_func_t parse_alloc;   /* function to allocate parse tree nodes */
  gp_parse_free_func_t parse_free;     /* function to free parse tree nodes */
  gp_syntax_error_func_t syntax_error; /* function to print syntax error */

  vlo_t caller_anode_names; /* Pointers to allocated names of anodes */

  /* statistic numbers for hash tables updated at the end of gp_parse and gp_free_tree: */
  int all_searches, all_collisions;

  vlo_t temp_vlo;

  /* vlo is array which is indexed by situation number (sit->rule->rule_start_offset + sit->pos): */
  vlo_t sit_table_vlo;
  struct sit **sit_table; /* the above vlo as array: */
  os_t sits_os;           /* all situations are placed in the object */
  int n_all_sits;         /* current number of unique situations */

  vlo_t sets_vlo; /* use to build sets */

  /* The set being created. It is defined only when new_set_ready_p is true. */
  struct set *new_set;
  /* The following says that new_set its members are defined. Before this the access
     to data of the set being formed are possible only through the following variables. */
  bool new_set_ready_p;
  /* To optimize code we use the following variables to access to data of new set. They are always defined
     and correspondingly situations and the current number of start situations of the set being formed. */
  struct sit **new_sits;
  int new_n_start_sits;
  int n_sets, n_sets_start_sits;         /* # of unique sets and their start situations */
  int n_goto_vects, n_goto_vect_len;     /* goto vects and their length */
  int n_actions;                         /* actions number*/
  int n_action_vects, n_action_vect_len; /* action vects and their length */
  os_t set_sits_os;                      /* container of situations of being formed sets */
  os_t sets_os;                          /* container of sets */
  hash_table_t set_tab;                  /* set table: key is only start situations */

  vlo_t symb_sits;   /* container for symb_sits */
  vlo_t actions_vlo; /* container for set actions  */

  struct action_desc *empty_action_map; /* no actions for each terminal */

  vlo_t all_nodes;       /* all parse tree nodes */
  bitmap_t marked_nodes; /* it is used in GC to find nodes reached from curr stacks */

  hash_table_t nodes_htab;       /* internal htab for parse nodes to minimize number of allocated nodes */
  struct gp_tree_node temp_node; /* used for insertion of node into the table */

  /* Statistic numbers: tokens, all parse tree nodes, terminal nodes, abstract and alternative nodes: */
  int toks_num, n_parse_nodes, n_parse_term_nodes;
  int n_parse_abstract_nodes, n_parse_alt_nodes;
  /* Parse tree nodes representing empty and error nodes.  They exist in one examplar. */
  struct gp_tree_node *empty_node, *error_node;

  /* internal data used for error recovery */
  os_t recovery_infos;                       /* container for recovery_info structures */
  struct recovery_info *free_recovery_infos; /* infos already allocated and can be reused */

  vlo_t free_stacks;    /* pointers to stack structs already allocated and can be reused */
  vlo_t temp_nodes_vlo; /* temp vlo containing pointers to parse tree nodes and used for translation */

  int n_stacks; /* all allocated stacks */
#ifndef NO_GP_DEBUG_PRINT
  int n_peak_stack_els, n_curr_stack_els; /* peak allocated and currently allocated stacks */
#endif

  /* Token buffers used mostly during error recovery: */
  int curr_buff_token_ind; /* current position in the token buffer */
  vlo_t token_buff;        /* container of token_buff_el structs */

  /* The current stacks and the stacks which will be current after reading and processing current
     terminal, in order word stacks containing situation with position after the terminal.  */
  vlo_t curr_stacks, new_stacks;
  /* Stacks used during error recovery.  They are ordered by decreasing cost.
     Delayed stacks are produced by removing symbol from the original stack.  */
  vlo_t delayed_stacks;
  /* Current stacks which had no actions on the current term.  They and delayed stacks
     from them are start stacks for error recovery.  Error recovery starts when there
     are no actions on all current stacks.  They are also used during error recovery.  */
  vlo_t failed_stacks;
#ifndef NO_GP_DEBUG_PRINT
  int n_single_stack_actions, n_multi_stack_actions; /* actions taken for single and multiple stacks */
  bool *visits_p;                                    /* temporary array used for printing parse trees */
#endif
};

/* Forward decrlarations: */
static void gp_error (struct grammar *g, int code, const char *format, ...);

/* The default number of tokens sucessfully matched to stop error recovery alternative (state). */
#define DEFAULT_RECOVERY_TOKEN_MATCHES 3

/* This page is abstract data `grammar symbols'. */

typedef long int term_set_el_t; /* type of element of array representing set of terminals */

struct rule;

struct symb {       /* symbol of the grammar: */
  const char *repr; /* external representation of the symbol */
  union {
    struct {
      int code;            /* code of the terminal symbol */
      int term_num;        /* order number of the terminal */
      int priority;        /* < 0 no priority */
      enum gp_assoc assoc; /* undefined for priority < 0 */
    } term;
    struct {
      struct rule *rules;            /* all rules with the nonterminal in the rule LHS */
      int nonterm_num;               /* order number of the nonterminal */
      bool loop_p;                   /* flag that nonterminal may derivate itself */
      term_set_el_t *first, *follow; /* FIRST and FOLLOW sets of the nonterminal */
    } nonterm;
  } u;
  bool term_p;       /* true if it is nonterminal */
  bool access_p;     /* true if the symbol is accessible (derivated) from the axiom */
  bool derivation_p; /* true if it is a term or it is a nonterm which derivates a term string */
  bool empty_p;      /* true if it is nonterminal which may derivate empty string */
  int num;           /* order number of the symbol */
};

/* delete_hash_table plus accumulate all_searches and all_collisions. */
static inline void delete_htab_update_statistics (struct grammar *g, hash_table_t htab) {
  g->all_searches += get_searches (htab);
  g->all_collisions += get_collisions (htab);
  delete_hash_table (htab);
}

static uint64_t symb_repr_hash (hash_table_entry_t s) { /* return hash of symbol representation */
  const char *str = ((struct symb *) s)->repr;
  return hash (str, strlen (str), 42);
}

/* Equality of symbol representations. */
static bool symb_repr_eq (hash_table_entry_t s1, hash_table_entry_t s2) {
  return strcmp (((struct symb *) s1)->repr, ((struct symb *) s2)->repr) == 0;
}

static uint64_t symb_code_hash (hash_table_entry_t s) { /* Hash of terminal code. */
  struct symb *symb = ((struct symb *) s);
  assert (symb->term_p);
  return symb->u.term.code;
}

/* Equality of terminal codes. */
static bool symb_code_eq (hash_table_entry_t s1, hash_table_entry_t s2) {
  struct symb *symb1 = ((struct symb *) s1);
  struct symb *symb2 = ((struct symb *) s2);
  assert (symb1->term_p && symb2->term_p);
  return symb1->u.term.code == symb2->u.term.code;
}

/* Initialize work with symbols and returns storage for the symbols. */
static struct symbs *symb_init (struct grammar *g) {
  void *mem = gp_malloc (g->alloc, sizeof (struct symbs));
  struct symbs *result = (struct symbs *) mem;
  OS_CREATE (result->symbs_os, g->alloc, 0);
  VLO_CREATE (result->symbs_vlo, g->alloc, 1024);
  VLO_CREATE (result->terms_vlo, g->alloc, 512);
  VLO_CREATE (result->nonterms_vlo, g->alloc, 512);
  result->repr_to_symb_tab = create_hash_table (g->alloc, 300, symb_repr_hash, symb_repr_eq);
  result->code_to_term_tab = create_hash_table (g->alloc, 200, symb_code_hash, symb_code_eq);
  result->term_code_trans_vect = NULL;
  result->n_nonterms = result->n_terms = 0;
  return result;
}

/* Return symbol (or NULL if it does not exist) whose representation is REPR. */
static struct symb *symb_find_by_repr (struct grammar *g, const char *repr) {
  struct symb symb;
  symb.repr = repr;
  return (struct symb *) *find_hash_table_entry (g->symbs->repr_to_symb_tab, &symb, false);
}

/* Return symbol (or NULL if it does not exist) which is terminal with CODE. */
static struct symb *term_tab_find_by_code (struct grammar *g, int code) {
  struct symbs *symbs = g->symbs;
  struct symb symb;
  symb.term_p = true;
  symb.u.term.code = code;
  return (struct symb *) *find_hash_table_entry (symbs->code_to_term_tab, &symb, false);
}

/* Return symbol (or NULL if it does not exist) which is terminal with CODE. */
static FORCE_INLINE struct symb *term_find_by_code (struct grammar *g, int code) {
  struct symbs *symbs = g->symbs;
  struct symb *symb;
  assert (symbs->term_code_trans_vect != NULL);
  if (code < symbs->term_code_trans_vect_start || code >= symbs->term_code_trans_vect_end
      || (symb = symbs->term_code_trans_vect[code - symbs->term_code_trans_vect_start]) == NULL) {
    gp_error (g, GP_INVALID_TOKEN_CODE, "invalid token code %d", code);
    return NULL;
  }
  return symb;
}

/* Create new terminal symbol and return reference for it. The symbol should be not in the tables.
   The function should create own copy of name for the new symbol. */
static struct symb *symb_add_term (struct grammar *g, const char *name, int code, int priority,
                                   enum gp_assoc assoc) {
  struct symb symb, *result;
  hash_table_entry_t *repr_entry, *code_entry;

  symb.repr = name;
  symb.term_p = true;
  symb.num = g->symbs->n_nonterms + g->symbs->n_terms;
  symb.u.term.code = code;
  symb.u.term.term_num = g->symbs->n_terms++;
  symb.u.term.priority = priority;
  symb.u.term.assoc = assoc;
  symb.empty_p = false;
  repr_entry = find_hash_table_entry (g->symbs->repr_to_symb_tab, &symb, true);
  assert (*repr_entry == NULL);
  code_entry = find_hash_table_entry (g->symbs->code_to_term_tab, &symb, true);
  assert (*code_entry == NULL);
  OS_TOP_ADD_STRING (g->symbs->symbs_os, name);
  symb.repr = (char *) OS_TOP_BEGIN (g->symbs->symbs_os);
  OS_TOP_FINISH (g->symbs->symbs_os);
  OS_TOP_ADD_MEMORY (g->symbs->symbs_os, &symb, sizeof (struct symb));
  result = (struct symb *) OS_TOP_BEGIN (g->symbs->symbs_os);
  OS_TOP_FINISH (g->symbs->symbs_os);
  *repr_entry = (hash_table_entry_t) result;
  *code_entry = (hash_table_entry_t) result;
  VLO_ADD_MEMORY (g->symbs->symbs_vlo, &result, sizeof (struct symb *));
  VLO_ADD_MEMORY (g->symbs->terms_vlo, &result, sizeof (struct symb *));
  return result;
}

/* Create new nonterminal symbol and return reference for it. The symbol should be
   not in the table. The function should create own copy of name for the new symbol. */
static struct symb *symb_add_nonterm (struct grammar *g, const char *name) {
  struct symb symb;
  symb.repr = name;
  symb.term_p = false;
  symb.num = g->symbs->n_nonterms + g->symbs->n_terms;
  symb.u.nonterm.rules = NULL;
  symb.u.nonterm.loop_p = 0;
  symb.u.nonterm.nonterm_num = g->symbs->n_nonterms++;
  hash_table_entry_t *entry = find_hash_table_entry (g->symbs->repr_to_symb_tab, &symb, true);
  assert (*entry == NULL);
  OS_TOP_ADD_STRING (g->symbs->symbs_os, name);
  symb.repr = (char *) OS_TOP_BEGIN (g->symbs->symbs_os);
  OS_TOP_FINISH (g->symbs->symbs_os);
  OS_TOP_ADD_MEMORY (g->symbs->symbs_os, &symb, sizeof (struct symb));
  struct symb *result = (struct symb *) OS_TOP_BEGIN (g->symbs->symbs_os);
  OS_TOP_FINISH (g->symbs->symbs_os);
  *entry = (hash_table_entry_t) result;
  VLO_ADD_MEMORY (g->symbs->symbs_vlo, &result, sizeof (struct symb *));
  VLO_ADD_MEMORY (g->symbs->nonterms_vlo, &result, sizeof (struct symb *));
  return result;
}

static struct symb *symb_get (struct grammar *g, int n) { /* return N-th symbol (if any) or NULL otherwise */
  if (n < 0 || (VLO_LENGTH (g->symbs->symbs_vlo) / sizeof (struct symb *) <= (size_t) n)) return NULL;
  struct symb *symb = ((struct symb **) VLO_BEGIN (g->symbs->symbs_vlo))[n];
  assert (symb->num == n);
  return symb;
}

static struct symb *term_get (struct grammar *g, int n) { /* return N-th term (if any) or NULL otherwise */
  if (n < 0 || (VLO_LENGTH (g->symbs->terms_vlo) / sizeof (struct symb *) <= (size_t) n)) return NULL;
  struct symb *symb = ((struct symb **) VLO_BEGIN (g->symbs->terms_vlo))[n];
  assert (symb->term_p && symb->u.term.term_num == n);
  return symb;
}

static struct symb *nonterm_get (struct grammar *g,
                                 int n) { /* return N-th symbol (if any) or NULL otherwise */
  if (n < 0 || (VLO_LENGTH (g->symbs->nonterms_vlo) / sizeof (struct symb *) <= (size_t) n)) return NULL;
  struct symb *symb = ((struct symb **) VLO_BEGIN (g->symbs->nonterms_vlo))[n];
  assert (!symb->term_p && symb->u.nonterm.nonterm_num == n);
  return symb;
}

#ifndef NO_GP_DEBUG_PRINT

/* Print symbol SYMB to file F. Terminal is printed with its code if CODE_P. */
static void symb_print (FILE *f, struct symb *symb, int code_p) {
  fprintf (f, "%s", symb->repr);
  if (code_p && symb->term_p) fprintf (f, "(%d)", symb->u.term.code);
}

#endif /* #ifndef NO_GP_DEBUG_PRINT */

#define TERM_CODE_TRANS_VECT_SIZE 10000

static void symb_finish_adding_terms (struct grammar *g) {
  int i, max_code, min_code;
  struct symb *symb;
  for (min_code = max_code = i = 0; (symb = term_get (g, i)) != NULL; i++) {
    if (i == 0 || min_code > symb->u.term.code) min_code = symb->u.term.code;
    if (i == 0 || max_code < symb->u.term.code) max_code = symb->u.term.code;
  }
  assert (i != 0);
  if (max_code - min_code >= TERM_CODE_TRANS_VECT_SIZE) {
    gp_error (g, GP_TOO_WIDE_TERM_RANGE_CODE, "term code range is more %d", TERM_CODE_TRANS_VECT_SIZE);
  } else {
    g->symbs->term_code_trans_vect_start = min_code;
    g->symbs->term_code_trans_vect_end = max_code + 1;
    void *mem = gp_calloc (g->alloc, (max_code - min_code + 1), sizeof (struct symb *));
    g->symbs->term_code_trans_vect = (struct symb **) mem;
    for (i = 0; (symb = term_get (g, i)) != NULL; i++)
      g->symbs->term_code_trans_vect[symb->u.term.code - min_code] = symb;
  }
}

static void symb_empty (struct grammar *g, struct symbs *symbs) { /* free memory for symbols */
  if (symbs == NULL) return;
  if (g->symbs->term_code_trans_vect != NULL) {
    gp_free (g->alloc, g->symbs->term_code_trans_vect);
    g->symbs->term_code_trans_vect = NULL;
  }
  empty_hash_table (symbs->repr_to_symb_tab);
  empty_hash_table (symbs->code_to_term_tab);
  VLO_NULLIFY (symbs->nonterms_vlo);
  VLO_NULLIFY (symbs->terms_vlo);
  VLO_NULLIFY (symbs->symbs_vlo);
  OS_EMPTY (symbs->symbs_os);
  symbs->n_nonterms = symbs->n_terms = 0;
}

static void symb_fin (struct grammar *g, struct symbs *symbs) { /* Finalize work with symbols. */
  if (symbs == NULL) return;
  if (g->symbs->term_code_trans_vect != NULL) gp_free (g->alloc, g->symbs->term_code_trans_vect);
  delete_hash_table (g->symbs->repr_to_symb_tab);
  delete_hash_table (g->symbs->code_to_term_tab);
  VLO_DELETE (g->symbs->nonterms_vlo);
  VLO_DELETE (g->symbs->terms_vlo);
  VLO_DELETE (g->symbs->symbs_vlo);
  OS_DELETE (g->symbs->symbs_os);
  gp_free (g->alloc, symbs);
  symbs = NULL;
}

/* This page contains abstract data set of terminals. */

typedef long int term_set_el_t; /* type of element of array representing set of terminals */
#define TERM_SET_EL_BITS (CHAR_BIT * sizeof (term_set_el_t))

struct tab_term_set { /* element of term set hash table: */
  int num;            /* number of set in the table */
  term_set_el_t *set; /* terminal set itself */
};

struct term_sets {                   /* container for the abstract data: */
  os_t term_set_os;                  /* all terminal sets are stored in the os */
  int n_term_sets, n_term_sets_size; /* number of terminal sets and their overall size */
  vlo_t tab_term_set_vlo;            /* refs to all struct tab_term_set are stored in the vlo */
};

/* Initialize work with terminal sets and returns storage for terminal sets. */
static struct term_sets *term_set_init (struct grammar *g) {
  void *mem = gp_malloc (g->alloc, sizeof (struct term_sets));
  struct term_sets *result = (struct term_sets *) mem;
  OS_CREATE (result->term_set_os, g->alloc, 0);
  VLO_CREATE (result->tab_term_set_vlo, g->alloc, 4096);
  result->n_term_sets = result->n_term_sets_size = 0;
  return result;
}

/* Return new terminal SET. Its value is undefined. */
static term_set_el_t *term_set_create (struct grammar *g) {
  assert (sizeof (term_set_el_t) <= 8);
  /* Make it 64 bit multiple to have the same statistics for 64 bit machines. */
  int size = ((g->symbs->n_terms + CHAR_BIT * 8 - 1) / (CHAR_BIT * 8)) * 8;
  OS_TOP_EXPAND (g->term_sets->term_set_os, size);
  term_set_el_t *result = (term_set_el_t *) OS_TOP_BEGIN (g->term_sets->term_set_os);
  OS_TOP_FINISH (g->term_sets->term_set_os);
  g->term_sets->n_term_sets++;
  g->term_sets->n_term_sets_size += size;
  return result;
}

static inline void term_set_clear (struct grammar *g, term_set_el_t *set) { /* make terminal SET empty: */
  int size = (g->symbs->n_terms + TERM_SET_EL_BITS - 1) / (TERM_SET_EL_BITS);
  term_set_el_t *bound = set + size;
  while (set < bound) *set++ = 0;
}

/* Copy SRC into DEST */
static inline void term_set_copy (struct grammar *g, term_set_el_t *dest, term_set_el_t *src) {
  int size = (g->symbs->n_terms + TERM_SET_EL_BITS - 1) / (TERM_SET_EL_BITS);
  term_set_el_t *bound = dest + size;
  while (dest < bound) *dest++ = *src++;
}

/* Add all terminals from set OP with to SET. Return true if SET has been changed. */
static inline bool term_set_or (struct grammar *g, term_set_el_t *set, term_set_el_t *op) {
  term_set_el_t *bound;
  int size = (g->symbs->n_terms + TERM_SET_EL_BITS - 1) / (TERM_SET_EL_BITS);
  bound = set + size;
  bool changed_p = false;
  while (set < bound) {
    if ((*set | *op) != *set) changed_p = true;
    *set++ |= *op++;
  }
  return changed_p;
}

/* Add terminal with number NUM to SET. Return true if SET has been changed. */
static inline bool term_set_up (struct grammar *g, term_set_el_t *set, int num) {
  assert (num < g->symbs->n_terms);
  int ind = num / TERM_SET_EL_BITS;
  term_set_el_t bit = ((term_set_el_t) 1) << (num % TERM_SET_EL_BITS);
  bool changed_p = (set[ind] & bit) == 0;
  set[ind] |= bit;
  return changed_p;
}

/* Return true if terminal with number NUM is in SET. */
static inline int term_set_test (struct grammar *g, term_set_el_t *set, int num) {
  assert (num >= 0 && num < g->symbs->n_terms);
  int ind = num / TERM_SET_EL_BITS;
  term_set_el_t bit = ((term_set_el_t) 1) << (num % TERM_SET_EL_BITS);
  return (set[ind] & bit) != 0;
}

/* Return set which is in the table with number NUM. */
static inline term_set_el_t *term_set_from_table (struct grammar *g, int num) {
  assert (num < (int) (VLO_LENGTH (g->term_sets->tab_term_set_vlo) / sizeof (struct tab_term_set *)));
  return ((struct tab_term_set **) VLO_BEGIN (g->term_sets->tab_term_set_vlo))[num]->set;
}

#ifndef NO_GP_DEBUG_PRINT
static void term_set_print (struct grammar *g, FILE *f,
                            term_set_el_t *set) { /* print terminal SET into file F */
  for (int i = 0; i < g->symbs->n_terms; i++)
    if (term_set_test (g, set, i)) {
      fprintf (f, " ");
      symb_print (f, term_get (g, i), false);
    }
}
#endif

static void term_set_empty (struct term_sets *term_sets) { /* free memory for terminal sets */
  if (term_sets == NULL) return;
  VLO_NULLIFY (term_sets->tab_term_set_vlo);
  OS_EMPTY (term_sets->term_set_os);
  term_sets->n_term_sets = term_sets->n_term_sets_size = 0;
}

static void term_set_fin (struct grammar *g,
                          struct term_sets *term_sets) { /* finalize work with terminal sets */
  if (term_sets == NULL) return;
  VLO_DELETE (term_sets->tab_term_set_vlo);
  OS_DELETE (term_sets->term_set_os);
  gp_free (g->alloc, term_sets);
  term_sets = NULL;
}

/* This page is abstract data `grammar rules'. */

struct rule {                 /* rule of the grammar: */
  int num;                    /* order number of rule */
  int rhs_len;                /* length of rhs */
  int lhs_nonterm_num;        /* = lhs->u.nonterm.nonterm_num */
  struct rule *next;          /* the next grammar rule */
  struct rule *lhs_next;      /* the next grammar rule with the same nonterminal in the rule lhs */
  struct symb *lhs;           /* nonterminal in the left hand side of the rule */
  struct symb **rhs;          /* symbols in the right hand side of the rule */
  struct symb *priority_term; /* last priority terminal, NULL otherwise */
  /* The following three members define rule translation: */
  const char *anode; /* abstract node name if any */
  int trans_len;     /* number of symbol translations in the rule translation */
  /* ???Array elements correspond to element of rhs with the same index. The element value is order
     number of the corresponding symbol translation in the rule translation. If the symbol
     translation is rejected, the corresponding element value is negative. */
  int *order;
  /* ???Size of all previous rule lengths + number of the previous rules. Imagine that all left hand
     symbol and right hand size symbols of the rules are stored in array. Then the following member
     is index of the rule lhs in the array. */
  int rule_start_offset;
  char *caller_anode; /* the same string as anode but memory allocated in parse_alloc */
};

struct rules {             /* container for the abstract data */
  int n_rules, n_rhs_lens; /* number of all rules and their summary rhs length */
  struct rule *first_rule; /* the first rule */
  struct rule *curr_rule;  /* rule being formed */
  vlo_t rules_vlo;         /* all refereneces to rules are placed in this object: */
  os_t rules_os;           /* all rules are placed in this object: */
};

/* Initialize work with rules and returns pointer to rules storage. */
static struct rules *rule_init (struct grammar *g) {
  void *mem;
  struct rules *result;

  mem = gp_malloc (g->alloc, sizeof (struct rules));
  result = (struct rules *) mem;
  VLO_CREATE (result->rules_vlo, g->alloc, 0);
  OS_CREATE (result->rules_os, g->alloc, 0);
  result->first_rule = result->curr_rule = NULL;
  result->n_rules = result->n_rhs_lens = 0;
  return result;
}

/* Create new rule with LHS empty rhs. */
static struct rule *rule_new_start (struct grammar *g, struct symb *lhs, const char *anode) {
  struct rule *rule;
  struct symb *empty;

  assert (!lhs->term_p);
  OS_TOP_EXPAND (g->rules->rules_os, sizeof (struct rule));
  rule = (struct rule *) OS_TOP_BEGIN (g->rules->rules_os);
  OS_TOP_FINISH (g->rules->rules_os);
  rule->lhs = lhs;
  rule->lhs_nonterm_num = lhs->u.nonterm.nonterm_num;
  rule->priority_term = NULL;
  if (anode == NULL) {
    rule->anode = NULL;
  } else {
    OS_TOP_ADD_STRING (g->rules->rules_os, anode);
    rule->anode = (char *) OS_TOP_BEGIN (g->rules->rules_os);
    OS_TOP_FINISH (g->rules->rules_os);
  }
  rule->trans_len = 0;
  rule->order = NULL;
  rule->next = NULL;
  if (g->rules->curr_rule != NULL) g->rules->curr_rule->next = rule;
  rule->lhs_next = lhs->u.nonterm.rules;
  lhs->u.nonterm.rules = rule;
  rule->rhs_len = 0;
  empty = NULL;
  OS_TOP_ADD_MEMORY (g->rules->rules_os, &empty, sizeof (struct symb *));
  rule->rhs = (struct symb **) OS_TOP_BEGIN (g->rules->rules_os);
  g->rules->curr_rule = rule;
  if (g->rules->first_rule == NULL) g->rules->first_rule = rule;
  rule->rule_start_offset = g->rules->n_rhs_lens + g->rules->n_rules;
  rule->num = g->rules->n_rules++;
  assert (VLO_LENGTH (g->rules->rules_vlo) / sizeof (struct rule *) == (size_t) rule->num);
  VLO_ADD_MEMORY (g->rules->rules_vlo, &rule, sizeof (rule));
  return rule;
}

/* Add SYMB at the end of current rule rhs. */
static void rule_new_symb_add (struct grammar *g, struct symb *symb) {
  struct symb *empty;

  empty = NULL;
  OS_TOP_ADD_MEMORY (g->rules->rules_os, &empty, sizeof (struct symb *));
  g->rules->curr_rule->rhs = (struct symb **) OS_TOP_BEGIN (g->rules->rules_os);
  g->rules->curr_rule->rhs[g->rules->curr_rule->rhs_len] = symb;
  if (symb->term_p && symb->u.term.priority >= 0) g->rules->curr_rule->priority_term = symb;
  g->rules->curr_rule->rhs_len++;
  g->rules->n_rhs_lens++;
}

/* Create and initialize situation cache (it should be called at end of forming each rule).  */
static void rule_new_stop (struct grammar *g) {
  int i;

  OS_TOP_FINISH (g->rules->rules_os);
  OS_TOP_EXPAND (g->rules->rules_os, g->rules->curr_rule->rhs_len * sizeof (int));
  g->rules->curr_rule->order = (int *) OS_TOP_BEGIN (g->rules->rules_os);
  OS_TOP_FINISH (g->rules->rules_os);
  for (i = 0; i < g->rules->curr_rule->rhs_len; i++) g->rules->curr_rule->order[i] = -1;
}

static struct rule *rule_get (struct grammar *g, int num) {
  assert ((int) VLO_LENGTH (g->rules->rules_vlo) > (int) (sizeof (struct rule *) * num));
  return ((struct rule **) VLO_BEGIN (g->rules->rules_vlo))[num];
}

#ifndef NO_GP_DEBUG_PRINT

/* Print RULE with its translation (if TRANS_P) to file F. */
static void rule_print (FILE *f, struct rule *rule, bool trans_p, bool newln_p) {
  int i, j;

  symb_print (f, rule->lhs, false);
  fprintf (f, " :");
  for (i = 0; i < rule->rhs_len; i++) {
    fprintf (f, " ");
    symb_print (f, rule->rhs[i], false);
  }
  if (trans_p) {
    fprintf (f, " # ");
    if (rule->anode != NULL) {
      fprintf (f, "%s (", rule->anode);
    }
    for (i = 0; i < rule->trans_len; i++) {
      for (j = 0; j < rule->rhs_len; j++)
        if (rule->order[j] == i) {
          fprintf (f, " %d:", j);
          symb_print (f, rule->rhs[j], false);
          break;
        }
      if (j >= rule->rhs_len) fprintf (f, " nil");
    }
    if (rule->anode != NULL) fprintf (f, " )");
  }
  if (newln_p) fprintf (f, "\n");
}

/* Print RULE to file F with dot in position POS. */
static void rule_dot_print (FILE *f, struct rule *rule, int pos) {
  assert (pos >= 0 && pos <= rule->rhs_len);
  symb_print (f, rule->lhs, false);
  fprintf (f, " :");
  for (int i = 0; i < rule->rhs_len; i++) {
    fprintf (f, i == pos ? " ." : " ");
    symb_print (f, rule->rhs[i], false);
  }
  if (rule->rhs_len == pos) fprintf (f, ".");
}

#endif /* #ifndef NO_GP_DEBUG_PRINT */

static void rule_empty (struct rules *rules) { /* Free memory for rules. */
  if (rules == NULL) return;
  VLO_NULLIFY (rules->rules_vlo);
  OS_EMPTY (rules->rules_os);
  rules->first_rule = rules->curr_rule = NULL;
  rules->n_rules = rules->n_rhs_lens = 0;
}

static void rule_fin (struct grammar *g, struct rules *rules) { /* Finalize work with rules. */
  if (rules == NULL) return;
  VLO_DELETE (rules->rules_vlo);
  OS_DELETE (rules->rules_os);
  gp_free (g->alloc, rules);
  rules = NULL;
}

/* This page is abstract data `situations'. */

struct sit {         /* the situation: */
  struct rule *rule; /* the situation rule */
  short pos;         /* position of dot in rhs of the situation rule */
  bool empty_tail_p; /* true if the tail can derive empty string */
  int sit_number;    /* unique situation number */
  /* the situation lookahead = FIRST (the situation tail & FOLLOW (lhs)) */
  term_set_el_t *lookahead;
};

static void sit_init (struct grammar *g) { /* Initialize work with situations: */
  g->n_all_sits = 0;
  OS_CREATE (g->sits_os, g->alloc, 0);
  VLO_CREATE (g->sit_table_vlo, g->alloc, 4096);
  g->sit_table = (struct sit **) VLO_BEGIN (g->sit_table_vlo);
}

/* Set up lookahead of situation SIT. Returns true if the situation tail may derive empty string. */
static bool sit_set_lookahead (struct grammar *g, struct sit *sit) {
  struct symb *symb, **symb_ptr;

  sit->lookahead = term_set_create (g);
  term_set_clear (g, sit->lookahead);
  symb_ptr = &sit->rule->rhs[sit->pos];
  while ((symb = *symb_ptr) != NULL) {
    if (symb->term_p)
      term_set_up (g, sit->lookahead, symb->u.term.term_num);
    else
      term_set_or (g, sit->lookahead, symb->u.nonterm.first);
    if (!symb->empty_p) break;
    symb_ptr++;
  }
  if (symb != NULL) return false;
  term_set_or (g, sit->lookahead, sit->rule->lhs->u.nonterm.follow);
  return true;
}

/* Return situations with given characteristics. Remember that sits are stored in one exemplar. */
static inline struct sit *sit_create (struct grammar *g, struct rule *rule, int pos) {
  struct sit *sit;
  int diff = (char *) (g->sit_table + rule->rule_start_offset + pos) - (char *) VLO_BOUND (g->sit_table_vlo);

  if (diff >= 0) {
    diff += sizeof (struct sit *);
    VLO_EXPAND (g->sit_table_vlo, diff);
    g->sit_table = (struct sit **) VLO_BEGIN (g->sit_table_vlo);
    struct sit **bound = (struct sit **) VLO_BOUND (g->sit_table_vlo);
    for (struct sit **ptr = bound - diff / sizeof (struct sit *); ptr < bound; ptr++) *ptr = NULL;
  }
  if ((sit = g->sit_table[rule->rule_start_offset + pos]) != NULL) return sit;
  OS_TOP_EXPAND (g->sits_os, sizeof (struct sit));
  sit = (struct sit *) OS_TOP_BEGIN (g->sits_os);
  OS_TOP_FINISH (g->sits_os);
  g->n_all_sits++;
  sit->rule = rule;
  sit->pos = pos;
  sit->sit_number = g->n_all_sits;
  sit->empty_tail_p = sit_set_lookahead (g, sit);
  g->sit_table[rule->rule_start_offset + pos] = sit;
  return sit;
}

#ifndef NO_GP_DEBUG_PRINT

static void sit_print (FILE *f, struct sit *sit) { /* print situation SIT to file F: */
  fprintf (f, "%3d ", sit->sit_number);
  rule_dot_print (f, sit->rule, sit->pos);
}

#endif /* #ifndef NO_GP_DEBUG_PRINT */

/* Return hash of sequence of N_SITS situations in array SITS. */
static uint64_t sits_hash (int n_sits, struct sit **sits) {
  uint64_t result = hash_init (24);
  for (int i = 0; i < n_sits; i++) result = hash_step (result, sits[i]->sit_number);
  return hash_finish (result);
}

static void sit_fin (struct grammar *g) { /* Finalize work with situations. */
  VLO_DELETE (g->sit_table_vlo);
  OS_DELETE (g->sits_os);
}

struct action_desc {
  unsigned short actions_num;   /* number of actions for given nonterm */
  unsigned short actions_start; /* index of first term action, defined for actions_num != 0 */
};

struct action {
  bool shift_p;
  int term_num; /* action on given term */
  union {
    struct set *set; /* shift set */
    struct rule *rule;
  } u;
};

/* This page is abstract data `sets'. */

typedef unsigned short trans_el_t;
struct set {                      /* the grammar state: */
  int num;                        /* unique number of the state */
  int n_start_sits, n_sits;       /* numbers of (start) situations in the following array */
  int n_actions;                  /* len of array actions */
  struct symb *symb;              /* symb shifting which resulted into this state */
  struct sit **sits;              /* array of situation */
  struct set **goto_map;          /* map nonterm -> goto set */
  struct action_desc *action_map; /* map term -> action desc */
  struct action *actions;         /* action number -> action */
};

/* Hash of set. */
static uint64_t set_hash (hash_table_entry_t s) {
  struct set *set = (struct set *) s;
  return sits_hash (set->n_start_sits, set->sits);
}

static bool set_eq (hash_table_entry_t s1, hash_table_entry_t s2) { /* equality of sets: */
  struct set *set1 = (struct set *) s1;
  struct set *set2 = (struct set *) s2;
  struct sit **sit_ptr1, **sit_ptr2, **sit_bound1;
  if (set1->n_start_sits != set2->n_start_sits) return false;
  sit_ptr1 = set1->sits;
  sit_bound1 = sit_ptr1 + set1->n_start_sits;
  sit_ptr2 = set2->sits;
  while (sit_ptr1 < sit_bound1)
    if (*sit_ptr1++ != *sit_ptr2++) return false;
  return true;
}

static void set_init (struct grammar *g) { /* initialize work with sets: */
  OS_CREATE (g->set_sits_os, g->alloc, 2048);
  OS_CREATE (g->sets_os, g->alloc, 0);
  g->set_tab = create_hash_table (g->alloc, 8192, set_hash, set_eq);
  g->n_sets = g->n_sets_start_sits = 0;
  g->n_goto_vects = g->n_goto_vect_len = 0;
  g->n_actions = g->n_action_vects = g->n_action_vect_len = 0;
}

static FORCE_INLINE struct action *set_get_actions (struct grammar *g, struct set *set, int term,
                                                    int *actions_num) {
  assert (term >= 0 && term < g->symbs->n_terms);
  *actions_num = 0;
  *actions_num = set->action_map[term].actions_num;
  assert (actions_num == 0 || set->actions != NULL);
  return &set->actions[set->action_map[term].actions_start];
}

static inline void set_new_set_start (struct grammar *g) { /* start forming of new set: */
  g->new_set = NULL;
  g->new_set_ready_p = false;
  g->new_n_start_sits = 0;
  g->new_sits = NULL;
}

/* Add start SIT at the end of the situation array of the set being formed: */
static inline void set_new_add_start_sit (struct grammar *g, struct sit *sit) {
  assert (!g->new_set_ready_p);
  OS_TOP_EXPAND (g->set_sits_os, sizeof (struct sit *));
  g->new_sits = (struct sit **) OS_TOP_BEGIN (g->set_sits_os);
  g->new_sits[g->new_n_start_sits] = sit;
  g->new_n_start_sits++;
}

/* Add nonstart SIT (if it is not there yet) at the end of array of the new situations. */
static inline void set_new_add_nonstart_sit (struct grammar *g, struct sit *sit) {
  assert (g->new_set_ready_p);
  /* When we add non-start situations we need to have situations w/o duplicates. */
  for (int i = g->new_n_start_sits; i < g->new_set->n_sits; i++)
    if (g->new_sits[i] == sit) return;
  OS_TOP_EXPAND (g->set_sits_os, sizeof (struct sit *));
  g->new_sits = g->new_set->sits = (struct sit **) OS_TOP_BEGIN (g->set_sits_os);
  g->new_sits[g->new_set->n_sits++] = sit;
}

/* The new set should contain only start situations.  Insert set into the set table new_set will be
   set to the table set. If the function returns true then there was no such table set yet. */
static bool set_insert (struct grammar *g) {
  OS_TOP_EXPAND (g->sets_os, sizeof (struct set));
  g->new_set = (struct set *) OS_TOP_BEGIN (g->sets_os);
  g->new_set->n_start_sits = g->new_n_start_sits;
  g->new_set->sits = g->new_sits;
  g->new_set_ready_p = true;
  /* Insert set into table: */
  hash_table_entry_t *entry = find_hash_table_entry (g->set_tab, g->new_set, true);
  if (*entry != NULL) {
    OS_TOP_NULLIFY (g->sets_os);
    g->new_set = (struct set *) *entry;
    g->new_sits = g->new_set->sits;
    OS_TOP_NULLIFY (g->set_sits_os);
    return false;
  }
  OS_TOP_FINISH (g->sets_os);
  g->new_set->num = g->n_sets++;
  g->new_set->goto_map = NULL;
  g->new_set->action_map = NULL;
  g->new_set->actions = NULL;
  g->new_set->n_actions = 0;
  g->new_set->n_sits = g->new_n_start_sits;
  *entry = (hash_table_entry_t) g->new_set;
  g->n_sets_start_sits += g->new_n_start_sits;
  return true;
}

static inline void set_new_set_stop (struct grammar *g) { /* finish work with set being formed: */
  OS_TOP_FINISH (g->set_sits_os);
}

static void *set_calloc (struct grammar *g, size_t size) { /* allocate and data data in set os */
  OS_TOP_EXPAND (g->sets_os, size);
  void *res = (struct set *) OS_TOP_BEGIN (g->sets_os);
  OS_TOP_FINISH (g->sets_os);
  memset (res, 0, size);
  return res;
}

#ifndef NO_GP_DEBUG_PRINT

/* Print SET to file F. If NONSTART_P is true then print all situations. The situations are printed
   with the lookahead set if LOOKAHEAD_P. */
static void set_print (struct grammar *g, FILE *f, struct set *set, bool nonstart_p) {
  int num, n_start_sits, n_sits;
  struct sit **sits;

  if (set == NULL && !g->new_set_ready_p) {
    /* The following is necessary if we call the function from a debugger. In this case new_set,
       and their members may be not set up yet. */
    num = -1;
    n_start_sits = n_sits = g->new_n_start_sits;
    sits = g->new_sits;
  } else {
    num = set->num;
    n_sits = set->n_sits;
    sits = set->sits;
    n_start_sits = set->n_start_sits;
  }
  fprintf (f, "  Set = %d\n", num);
  for (int i = 0; i < n_sits; i++) {
    fprintf (f, "    ");
    sit_print (f, sits[i]);
    if (i == n_start_sits - 1) {
      if (!nonstart_p) break;
      fprintf (f, "\n    -----------\n");
    }
  }
  fprintf (f, "\n");
}

#endif /* #ifndef NO_GP_DEBUG_PRINT */

static void set_fin (struct grammar *g) { /* finalize work with sets: */
  delete_htab_update_statistics (g, g->set_tab);
  OS_DELETE (g->sets_os);
  OS_DELETE (g->set_sits_os);
}

/* Store error CODE and message. The function makes long jump after that. */
static void gp_error (struct grammar *g, int code, const char *format, ...) {
  va_list arguments;

  g->error_code = code;
  va_start (arguments, format);
  vsprintf (g->error_message, format, arguments);
  va_end (arguments);
  assert (strlen (g->error_message) < GP_MAX_ERROR_MESSAGE_LENGTH);
  longjmp (g->error_longjump_buff, code);
}

static void error_func_for_allocate (void *g) { /* Process allocation errors. */
  gp_error (g, GP_NO_MEMORY, "no memory");
}

static void *default_attr_merge (void *attr1, void *attr2 GP_UNUSED) { return attr1; }

static void *parse_alloc_default (int nmemb) {
  assert (nmemb > 0);
  void *result = malloc (nmemb);
  if (result == NULL) exit (1);
  return result;
}

static void parse_free_default (void *mem) { free (mem); }

static void syntax_error_default (const char *err_tok_repr, void *err_tok_attr GP_UNUSED,
                                  const char *stop_tok_repr, void *stop_tok_attr GP_UNUSED) {
  if (stop_tok_repr == NULL)
    fprintf (stderr, "Syntax error on token %s\n", err_tok_repr);
  else
    fprintf (stderr, "Syntax error on token %s and stopping on token %s\n", err_tok_repr, stop_tok_repr);
}

struct grammar *gp_create_grammar (void) { /* Allocate memory for new grammar. */
  gp_allocator_t *allocator;

  allocator = gp_alloc_new (NULL, NULL, NULL, NULL);
  if (allocator == NULL) {
    return NULL;
  }
  struct grammar *g = (struct grammar *) gp_malloc (allocator, sizeof (struct grammar));
  if (g == NULL) {
    gp_alloc_del (allocator);
    return NULL;
  }
  g->alloc = allocator;
  gp_alloc_seterr (allocator, error_func_for_allocate, g);
  if (setjmp (g->error_longjump_buff) != 0) {
    gp_fin (g);
    return NULL;
  }
  g->undefined_p = true;
  g->error_code = 0;
  *g->error_message = '\0';
  g->parse_alloc = parse_alloc_default;
  g->parse_free = parse_free_default;
  g->syntax_error = syntax_error_default;
  g->debug_level = 0;
  g->error_recovery_p = true;
  g->recovery_token_matches = DEFAULT_RECOVERY_TOKEN_MATCHES;
  g->symbs = NULL;
  g->term_sets = NULL;
  g->rules = NULL;
  g->symbs = symb_init (g);
  g->term_sets = term_set_init (g);
  g->rules = rule_init (g);
  g->attr_merge = default_attr_merge;
  VLO_CREATE (g->caller_anode_names, g->alloc, 0);
  VLO_CREATE (g->temp_vlo, g->alloc, 0);
  VLO_CREATE (g->all_nodes, g->alloc, 0);
  bitmap_create (&g->marked_nodes, g->alloc);
  return g;
}

static void gp_empty_grammar (struct grammar *g) { /* Make grammar empty. */
  if (g != NULL) {
    rule_empty (g->rules);
    term_set_empty (g->term_sets);
    symb_empty (g, g->symbs);
  }
}

int gp_error_code (struct grammar *g) { /* Return the last occurred error code for given grammar. */
  assert (g != NULL);
  return g->error_code;
}

/* Return message containing error message corresponding to the last occurred error code.  */
const char *gp_error_message (struct grammar *g) {
  assert (g != NULL);
  return g->error_message;
}

/* Create sets FIRST and FOLLOW for all grammar nonterminals. */
static void create_first_follow_sets (struct grammar *g) {
  struct symb *symb;
  for (int i = 0; (symb = nonterm_get (g, i)) != NULL; i++) {
    symb->u.nonterm.first = term_set_create (g);
    term_set_clear (g, symb->u.nonterm.first);
    symb->u.nonterm.follow = term_set_create (g);
    term_set_clear (g, symb->u.nonterm.follow);
  }
  bool changed_p;
  do {
    changed_p = false;
    for (int i = 0; (symb = nonterm_get (g, i)) != NULL; i++)
      for (struct rule *rule = symb->u.nonterm.rules; rule != NULL; rule = rule->lhs_next) {
        bool first_continue_p = true;
        struct symb **rhs = rule->rhs;
        int rhs_len = rule->rhs_len;
        for (int j = 0; j < rhs_len; j++) {
          struct symb *rhs_symb = rhs[j];
          if (rhs_symb->term_p) {
            if (first_continue_p && term_set_up (g, symb->u.nonterm.first, rhs_symb->u.term.term_num))
              changed_p = true;
          } else {
            if (first_continue_p && term_set_or (g, symb->u.nonterm.first, rhs_symb->u.nonterm.first))
              changed_p = true;
            int k;
            for (k = j + 1; k < rhs_len; k++) {
              struct symb *next_rhs_symb = rhs[k];
              if (next_rhs_symb->term_p
                  && term_set_up (g, rhs_symb->u.nonterm.follow, next_rhs_symb->u.term.term_num))
                changed_p = true;
              else if (!next_rhs_symb->term_p
                       && term_set_or (g, rhs_symb->u.nonterm.follow, next_rhs_symb->u.nonterm.first))
                changed_p = true;
              if (!next_rhs_symb->empty_p) break;
            }
            if (k == rhs_len && term_set_or (g, rhs_symb->u.nonterm.follow, symb->u.nonterm.follow))
              changed_p = true;
          }
          if (!rhs_symb->empty_p) first_continue_p = false;
        }
      }
  } while (changed_p);
}

/* Set up flags empty_p, access_p and derivation_p for all grammar symbols. */
static void set_empty_access_derives (struct grammar *g) {
  struct symb *symb;
  for (int i = 0; (symb = symb_get (g, i)) != NULL; i++) {
    symb->empty_p = false;
    symb->derivation_p = symb->term_p;
    symb->access_p = false;
  }
  g->axiom->access_p = true;
  bool empty_changed_p, derivation_changed_p, accessibility_change_p;
  do {
    empty_changed_p = derivation_changed_p = accessibility_change_p = false;
    for (int i = 0; (symb = nonterm_get (g, i)) != NULL; i++)
      for (struct rule *rule = symb->u.nonterm.rules; rule != NULL; rule = rule->lhs_next) {
        bool empty_p = true, derivation_p = true;
        for (int j = 0; j < rule->rhs_len; j++) {
          struct symb *rhs_symb = rule->rhs[j];
          if (symb->access_p) {
            accessibility_change_p |= rhs_symb->access_p ^ true;
            rhs_symb->access_p = 1;
          }
          empty_p &= rhs_symb->empty_p;
          derivation_p &= rhs_symb->derivation_p;
        }
        if (empty_p) {
          empty_changed_p |= symb->empty_p ^ empty_p;
          symb->empty_p = empty_p;
        }
        if (derivation_p) {
          derivation_changed_p |= symb->derivation_p ^ derivation_p;
          symb->derivation_p = derivation_p;
        }
      }
  } while (empty_changed_p || derivation_changed_p || accessibility_change_p);
}

static void set_loop_p (struct grammar *g) { /* set up flags loop_p for nonterminals: */
  struct symb *symb;
  /* Initialize accoding to minimal criteria: There is a rule in which the nonterminal stands and
     all the rest symbols can derive empty strings. */
  for (struct rule *rule = g->rules->first_rule; rule != NULL; rule = rule->next)
    for (int i = 0; i < rule->rhs_len; i++)
      if (!(symb = rule->rhs[i])->term_p) {
        int j;
        for (j = 0; j < rule->rhs_len; j++)
          if (i == j)
            continue;
          else if (!rule->rhs[j]->empty_p)
            break;
        if (j >= rule->rhs_len) symb->u.nonterm.loop_p = 1;
      }
  /* Major cycle: Check looped nonterminal that there is a rule with the nonterminal in lhs with a
     looped nonterminal in rhs and all the rest rhs symbols deriving empty string. */
  bool changed_p;
  do {
    struct symb *lhs;
    changed_p = false;
    for (int i = 0; (lhs = nonterm_get (g, i)) != NULL; i++)
      if (lhs->u.nonterm.loop_p) {
        bool loop_p = false;
        for (struct rule *rule = lhs->u.nonterm.rules; rule != NULL; rule = rule->lhs_next)
          for (int j = 0; j < rule->rhs_len; j++)
            if (!(symb = rule->rhs[j])->term_p && symb->u.nonterm.loop_p) {
              int k;
              for (k = 0; k < rule->rhs_len; k++)
                if (j == k)
                  continue;
                else if (!rule->rhs[k]->empty_p)
                  break;
              if (k >= rule->rhs_len) loop_p = true;
            }
        if (!loop_p) changed_p = true;
        lhs->u.nonterm.loop_p = loop_p;
      }
  } while (changed_p);
}

/* Evaluate different sets and flags for grammar and checks the grammar on correctness. */
static void check_grammar (struct grammar *g, int strict_p) {
  struct symb *symb;
  set_empty_access_derives (g);
  set_loop_p (g);
  if (strict_p) {
    for (int i = 0; (symb = nonterm_get (g, i)) != NULL; i++) {
      if (!symb->derivation_p)
        gp_error (g, GP_NONTERM_DERIVATION, "nonterm `%s' does not derive any term string", symb->repr);
      else if (!symb->access_p)
        gp_error (g, GP_UNACCESSIBLE_NONTERM, "nonterm `%s' is not accessible from axiom", symb->repr);
    }
  } else if (!g->axiom->derivation_p)
    gp_error (g, GP_NONTERM_DERIVATION, "nonterm `%s' does not derive any term string", g->axiom->repr);
  for (int i = 0; (symb = nonterm_get (g, i)) != NULL; i++)
    if (symb->u.nonterm.loop_p)
      gp_error (g, GP_LOOP_NONTERM, "nonterm `%s' can derive only itself (grammar with loops)", symb->repr);
  /* We should have correct flags empty_p here. */
  create_first_follow_sets (g);
}

/* Names of additional symbols. Don't use them in grammars. */
#define AXIOM_NAME "$S"
#define END_MARKER_NAME "$eof"

#define END_MARKER_CODE (-1) /* Should be negative. */

/* Read terminals/rules. Return error code or 0. Return pointer in G to the grammar. */
int gp_read_grammar (struct grammar *g, bool strict_p,
                     const char *(*read_terminal) (int *code, int *priority, enum gp_assoc *assoc),
                     const char *(*read_rule) (const char ***rhs, const char **abs_node, int **transl)) {
  struct symb *symb;
  assert (g != NULL);
  int code;
  if ((code = setjmp (g->error_longjump_buff)) != 0) return code;
  if (!g->undefined_p) gp_empty_grammar (g);
  const char *name;
  int priority;
  enum gp_assoc assoc;
  while ((name = (*read_terminal) (&code, &priority, &assoc)) != NULL) {
    if (code < 0) gp_error (g, GP_NEGATIVE_TERM_CODE, "term `%s' has negative code", name);
    symb = symb_find_by_repr (g, name);
    if (symb != NULL) gp_error (g, GP_REPEATED_TERM_DECL, "repeated declaration of term `%s'", name);
    if (term_tab_find_by_code (g, code) != NULL)
      gp_error (g, GP_REPEATED_TERM_CODE, "repeated code %d in term `%s'", code, name);
    symb_add_term (g, name, code, priority, assoc);
  }
  g->axiom = g->end_marker = NULL;
  const char *lhs, **rhs, *anode;
  int *transl;
  struct rule *rule;
  struct symb *start;
  while ((lhs = (*read_rule) (&rhs, &anode, &transl)) != NULL) {
    symb = symb_find_by_repr (g, lhs);
    if (symb == NULL)
      symb = symb_add_nonterm (g, lhs);
    else if (symb->term_p)
      gp_error (g, GP_TERM_IN_RULE_LHS, "term `%s' in the left hand side of rule", lhs);
    if (anode == NULL && transl != NULL && *transl >= 0 && transl[1] >= 0)
      gp_error (g, GP_INCORRECT_TRANSLATION, "rule for `%s' has incorrect translation", lhs);
    if (g->axiom == NULL) {
      /* We made this here becuase we want that the start rule has number 0. */
      /* Add axiom and end marker. */
      start = symb;
      g->axiom = symb_find_by_repr (g, AXIOM_NAME);
      if (g->axiom != NULL) gp_error (g, GP_FIXED_NAME_USAGE, "do not use fixed name `%s'", AXIOM_NAME);
      g->axiom = symb_add_nonterm (g, AXIOM_NAME);
      g->end_marker = symb_find_by_repr (g, END_MARKER_NAME);
      if (g->end_marker != NULL)
        gp_error (g, GP_FIXED_NAME_USAGE, "do not use fixed name `%s'", END_MARKER_NAME);
      if (term_tab_find_by_code (g, END_MARKER_CODE) != NULL) abort ();
      g->end_marker = symb_add_term (g, END_MARKER_NAME, END_MARKER_CODE, -1, GP_NON_ASSOC);
      /* Add rules for start */
      rule = rule_new_start (g, g->axiom, NULL);
      rule_new_symb_add (g, symb);
      rule_new_symb_add (g, g->end_marker);
      rule_new_stop (g);
      rule->order[0] = 0;
      rule->trans_len = 1;
    }
    rule = rule_new_start (g, symb, anode);
    while (*rhs != NULL) {
      symb = symb_find_by_repr (g, *rhs);
      if (symb == NULL) symb = symb_add_nonterm (g, *rhs);
      rule_new_symb_add (g, symb);
      rhs++;
    }
    rule_new_stop (g);
    if (transl != NULL) {
      int i, el;
      for (i = 0; (el = transl[i]) >= 0; i++)
        if (el >= rule->rhs_len) {
          if (el != GP_NIL_TRANSLATION_NUMBER)
            gp_error (g, GP_INCORRECT_SYMBOL_NUMBER,
                      "translation symbol number %d in rule for `%s' is out of range", el, lhs);
          else
            rule->trans_len++;
        } else if (rule->order[el] >= 0)
          gp_error (g, GP_REPEATED_SYMBOL_NUMBER, "repeated translation symbol number %d in rule for `%s'",
                    el, lhs);
        else {
          rule->order[el] = i;
          rule->trans_len++;
        }
      assert (i < rule->rhs_len || transl[i] < 0);
    }
  }
  if (g->axiom == NULL) gp_error (g, GP_NO_RULES, "grammar does not contains rules");
  assert (start != NULL);
  check_grammar (g, strict_p);
  symb_finish_adding_terms (g);
#ifndef NO_GP_DEBUG_PRINT
  if (g->debug_level > 3) {
    /* Print rules. */
    fprintf (stderr, "Rules:\n");
    for (rule = g->rules->first_rule; rule != NULL; rule = rule->next) {
      fprintf (stderr, "  ");
      rule_print (stderr, rule, true, true);
    }
    fprintf (stderr, "\n");
    /* Print symbol sets. */
    for (int i = 0; (symb = nonterm_get (g, i)) != NULL; i++) {
      fprintf (stderr, "Nonterm %s:  Empty=%s , Access=%s, Derive=%s\n", symb->repr,
               (symb->empty_p ? "Yes" : "No"), (symb->access_p ? "Yes" : "No"),
               (symb->derivation_p ? "Yes" : "No"));
      if (g->debug_level > 3) {
        fprintf (stderr, "  First: ");
        term_set_print (g, stderr, symb->u.nonterm.first);
        fprintf (stderr, "\n  Follow: ");
        term_set_print (g, stderr, symb->u.nonterm.follow);
        fprintf (stderr, "\n\n");
      }
    }
  }
#endif
  g->undefined_p = false;
  return 0;
}

#include "sgramm.c"

gp_parse_alloc_func_t gp_set_parse_alloc (struct grammar *g, gp_parse_alloc_func_t fn) {
  assert (g != NULL);
  if (fn == NULL) gp_error (g, GP_WRONG_ARG, "null parse_alloc func");
  gp_parse_alloc_func_t old = g->parse_alloc;
  g->parse_alloc = fn;
  return old;
}

gp_parse_free_func_t gp_set_parse_free (struct grammar *g, gp_parse_free_func_t fn) {
  assert (g != NULL);
  gp_parse_free_func_t old = g->parse_free;
  g->parse_free = fn;
  return old;
}

gp_syntax_error_func_t gp_set_syntax_error (struct grammar *g, gp_syntax_error_func_t fn) {
  assert (g != NULL);
  gp_syntax_error_func_t old = g->syntax_error;
  g->syntax_error = fn;
  return old;
}

int gp_set_debug_level (struct grammar *g, int level) {
  assert (g != NULL);
  int old = g->debug_level;
  g->debug_level = level;
  return old;
}

bool gp_set_error_recovery_flag (struct grammar *g, bool flag) {
  assert (g != NULL);
  bool old = g->error_recovery_p;
  g->error_recovery_p = flag;
  return old;
}

int gp_set_recovery_match (struct grammar *g, int n_toks) {
  assert (g != NULL);
  int old = g->recovery_token_matches;
  g->recovery_token_matches = n_toks;
  return old;
}

static void gp_parse_init (struct grammar *g) { /* initialize all internal data for parser: */
  sit_init (g);
  set_init (g);
  for (struct rule *rule = g->rules->first_rule; rule != NULL; rule = rule->next) rule->caller_anode = NULL;
}

gp_attr_merge_func_t gp_set_attr_merge_func (struct grammar *g, gp_attr_merge_func_t func) {
  gp_attr_merge_func_t res = g->attr_merge;
  g->attr_merge = func == NULL ? default_attr_merge : func;
  return res;
}

/* The function should be called the last (it frees all allocated data for parser). */
static void gp_parse_fin (struct grammar *g) {
  set_fin (g);
  sit_fin (g);
}

/* Add the rest (non-start) situations to the new set. */
static inline void expand_new_start_set (struct grammar *g) {
  for (int i = 0; i < g->new_set->n_sits; i++) {
    struct sit *sit = g->new_sits[i];
    if (sit->pos >= sit->rule->rhs_len) continue;
    /* There is a symbol after dot in the situation. */
    struct symb *symb = sit->rule->rhs[sit->pos];
    if (!symb->term_p)
      for (struct rule *rule = symb->u.nonterm.rules; rule != NULL; rule = rule->lhs_next)
        set_new_add_nonstart_sit (g, sit_create (g, rule, 0));
  }
  set_new_set_stop (g);
#ifndef NO_GP_DEBUG_PRINT
  if (g->debug_level > 3) set_print (g, stderr, g->new_set, g->debug_level > 3);
#endif
}

struct symb_sit {
  struct symb *symb;
  int sit_num;
};

static int symb_sit_cmp (const void *el1, const void *el2) {
  const struct symb_sit *e1 = (const struct symb_sit *) el1, *e2 = (const struct symb_sit *) el2;
  if (e1->symb == e2->symb) return 0;
  return e1->symb->num - e2->symb->num;
}

static int action_cmp (const void *el1, const void *el2) {
  const struct action *e1 = (const struct action *) el1, *e2 = (const struct action *) el2;
  int diff = e1->term_num - e2->term_num;
  if (diff != 0) return diff;
  if (e1->shift_p) return -1; /* put shift first */
  if (e2->shift_p) return 1;
  return e1->u.rule->num - e2->u.rule->num;
}

#ifndef NO_GP_DEBUG_PRINT
static void print_action (struct grammar *g, FILE *f, struct action *a) {
  struct symb *term = ((struct symb **) VLO_BEGIN (g->symbs->terms_vlo))[a->term_num];
  fprintf (f, "%s : ", term->repr);
  if (a->shift_p) {
    fprintf (f, "shift to S%d", a->u.set->num);
  } else {
    struct rule *rule = rule_get (g, a->u.rule->num);
    fprintf (f, "reduce \"");
    rule_print (f, rule, false, false);
    fprintf (f, "\"");
  }
}
#endif

static void remove_priority_conflict_actions (struct grammar *g, struct action *actions, int *actions_num) {
  struct symb *term = term_get (g, actions[0].term_num);  // ???
  assert (term->u.term.term_num == actions[0].term_num);
  int num = *actions_num, new_num = 1;
  if (term->u.term.priority < 0 || num <= 1 || !actions[0].shift_p) return;
  bool remove_shift_p = false;
  for (int i = 1; i < num; i++) {
    struct action *action = &actions[i];
    assert (!action->shift_p);
    struct symb *reduce_term = action->u.rule->priority_term;
    if (reduce_term == NULL) continue;
    if (term->u.term.priority < reduce_term->u.term.priority) { /* remove shift */
      remove_shift_p = true;
      actions[new_num++] = *action;
    } else if (term->u.term.priority > reduce_term->u.term.priority) { /* remove reduce */
    } else if (term->u.term.assoc == GP_LEFT_ASSOC) {                  /* remove shift */
      remove_shift_p = true;
      actions[new_num++] = *action;
    } else if (term->u.term.assoc == GP_RIGHT_ASSOC) { /* remove reduce */
    } else {                                           /* nonassoc: remove both */
      remove_shift_p = true;
    }
  }
  if (remove_shift_p) {
    for (int i = 1; i < new_num; i++) actions[i - 1] = actions[i];
    new_num--;
  }
  *actions_num = new_num;
}

static void build_goto_map_and_actions (struct grammar *g, struct set *set) {
  VLO_NULLIFY (g->symb_sits);
  VLO_NULLIFY (g->actions_vlo);
  for (int i = 0; i < set->n_sits; i++) {
    struct sit *sit = set->sits[i];
    if (sit->pos >= sit->rule->rhs_len) {
      for (int j = 0; j < g->symbs->n_terms; j++)
        if (term_set_test (g, sit->lookahead, j)) {
          struct action action;
          action.shift_p = false;
          action.term_num = j;
          action.u.rule = sit->rule;
          VLO_ADD_MEMORY (g->actions_vlo, &action, sizeof (action));
        }
      continue;
    }
    /* There is a symbol after dot in the situation. */
    struct symb *symb = sit->rule->rhs[sit->pos];
    struct symb_sit symb_sit = {symb, i};
    VLO_ADD_MEMORY (g->symb_sits, &symb_sit, sizeof (symb_sit));
  }
  int n = VLO_LENGTH (g->symb_sits) / sizeof (struct symb_sit);
  struct symb_sit *symb_sit_addr = (struct symb_sit *) VLO_BEGIN (g->symb_sits);
  qsort (symb_sit_addr, n, sizeof (struct symb_sit), symb_sit_cmp);
  set_new_set_start (g);
  for (int i = 0; i < n; i++) { /* build derived sets, goto map, and collect actions: */
    struct symb_sit *symb_sit = &symb_sit_addr[i];
    struct sit *sit = set->sits[symb_sit->sit_num];
    assert (sit->pos < sit->rule->rhs_len);
    struct symb_sit *next_symb_sit = i + 1 >= n ? NULL : &symb_sit_addr[i + 1];
    set_new_add_start_sit (g, sit_create (g, sit->rule, sit->pos + 1));
    if (next_symb_sit == NULL || symb_sit->symb != next_symb_sit->symb) { /* the last symb sit: */
      if (set_insert (g)) {
        g->new_set->symb = symb_sit->symb;
        expand_new_start_set (g);
        VLO_ADD_MEMORY (g->sets_vlo, &g->new_set, sizeof (struct set *));
      }
      struct set *trans_set = g->new_set;
      if (!symb_sit->symb->term_p) { /* goto */
        if (set->goto_map == NULL) {
          set->goto_map = set_calloc (g, sizeof (struct set *) * g->symbs->n_nonterms);
          g->n_goto_vects++;
          g->n_goto_vect_len += g->symbs->n_nonterms;
        }
        set->goto_map[symb_sit->symb->u.nonterm.nonterm_num] = trans_set;
      } else { /* shift */
        struct action action = {true, symb_sit->symb->u.term.term_num, {.set = trans_set}};
        VLO_ADD_MEMORY (g->actions_vlo, &action, sizeof (action));
      }
      set_new_set_start (g);
    }
  }
  set_new_set_stop (g);
  set->action_map = g->empty_action_map;
  int nta = VLO_LENGTH (g->actions_vlo) / sizeof (struct action);
  if (nta == 0) return;
  /* build action descs: */
  struct action *action_addr = (struct action *) VLO_BEGIN (g->actions_vlo);
  qsort (action_addr, nta, sizeof (struct action), action_cmp);
  int new_nta = 0;
  for (int i = 0, term_actions_num = 0, start = 0; i < nta; i++) { /* apply priorities: */
    struct action *action = &action_addr[i];
    struct action *prev_action = i == 0 ? NULL : &action_addr[i - 1];
    struct action *next_action = i + 1 >= nta ? NULL : &action_addr[i + 1];
    if (prev_action != NULL && action->term_num == prev_action->term_num) {
      term_actions_num++;
    } else {
      start = new_nta;
      term_actions_num = 1;
    }
    action_addr[new_nta++] = *action;
    if (next_action == NULL || action->term_num != next_action->term_num) {
      remove_priority_conflict_actions (g, &action_addr[start], &term_actions_num);
      new_nta = start + term_actions_num;
    }
  }
  nta = new_nta;
  assert (set->action_map == g->empty_action_map);
  set->action_map = set_calloc (g, sizeof (struct action_desc) * g->symbs->n_terms);
  g->n_action_vects++;
  g->n_action_vect_len += g->symbs->n_terms;
  set->actions = set_calloc (g, sizeof (struct action) * nta);
  set->n_actions = nta;
  g->n_actions += nta;
  int actions_num = 0;
  for (int i = 0; i < nta; i++) { /* set up set actions, actions_start, and actions_num: */
    struct action *action = &action_addr[i];
    struct action *prev_action = i == 0 ? NULL : &action_addr[i - 1];
    struct action *next_action = i + 1 >= nta ? NULL : &action_addr[i + 1];
    set->actions[i] = *action;
    if (prev_action != NULL && action->term_num == prev_action->term_num) {
      actions_num++;
    } else {
      set->action_map[action->term_num].actions_start = i;
      actions_num = 1;
    }
    if (next_action == NULL || action->term_num != next_action->term_num)
      set->action_map[action->term_num].actions_num = actions_num;
  }
#ifndef NO_GP_DEBUG_PRINT
  if (g->debug_level > 2) {
    fprintf (stderr, "  Actions for");
    set_print (g, stderr, set, false);
    bool conflict_p = false;
    for (int i = 0; i < nta; i++) {
      if (i == 0 || set->actions[i - 1].term_num != set->actions[i].term_num)
        conflict_p = i + 1 < nta && set->actions[i].term_num == set->actions[i + 1].term_num;
      fprintf (stderr, "           %c ", conflict_p ? '!' : ' ');
      print_action (g, stderr, &set->actions[i]);
      fprintf (stderr, "\n");
    }
  }
#endif
}

static void build_empty_action_map (struct grammar *g) {
  g->empty_action_map = set_calloc (g, sizeof (struct action_desc) * g->symbs->n_terms);
  for (int i = 0; i < g->symbs->n_terms; i++) {
    g->empty_action_map[i].actions_num = 0;
    g->empty_action_map[i].actions_start = 0;
  }
  g->n_action_vects++;
  g->n_action_vect_len += g->symbs->n_terms;
}

static struct set *build_sets (struct grammar *g) { /* Return the 1st set: */
  build_empty_action_map (g);
  VLO_CREATE (g->sets_vlo, g->alloc, 0);
  set_new_set_start (g);
  for (struct rule *rule = g->axiom->u.nonterm.rules; rule != NULL; rule = rule->lhs_next) {
    struct sit *sit = sit_create (g, rule, 0);
    set_new_add_start_sit (g, sit);
  }
  if (!set_insert (g)) assert (false);
  g->new_set->symb = g->axiom;
  expand_new_start_set (g);
  struct set *start_set = g->new_set;
  VLO_ADD_MEMORY (g->sets_vlo, &start_set, sizeof (struct set *));
  while (VLO_LENGTH (g->sets_vlo) != 0) {
    struct set *set = ((struct set **) VLO_BOUND (g->sets_vlo))[-1];
    VLO_SHORTEN (g->sets_vlo, sizeof (struct set *));
    build_goto_map_and_actions (g, set);
  }
  VLO_DELETE (g->sets_vlo);
  return start_set;
}

static uint64_t node_hash (hash_table_entry_t n) {
  struct gp_tree_node *node = (struct gp_tree_node *) n;
  uint64_t h;
  switch (node->type) {
  case GP_TERM:
    h = hash64 (node->val.term.code, 2);
    h = hash_step (h, (uint64_t) node->val.term.attr);
    break;
  case GP_ANODE:
    h = hash (node->val.anode.children, sizeof (struct gp_tree_node *) * node->val.anode.children_num, 3);
    /* name exists in one exemplar */
    h = hash_step (h, (uint64_t) node->val.anode.name);
    break;
  case GP_ALT:
    h = hash64 ((uint64_t) node->val.alt.first, 4);
    h = hash64 ((uint64_t) node->val.alt.second, h);
    break;
  default: assert (false); return 0; /* nil and error node exist in one exemplar */
  }
  return hash_finish (h);
}

static bool node_eq_p (hash_table_entry_t n1, hash_table_entry_t n2) {
  struct gp_tree_node *node1 = (struct gp_tree_node *) n1, *node2 = (struct gp_tree_node *) n2;
  if (node1->type != node2->type) return false;
  switch (node1->type) {
  case GP_TERM:
    return (node1->val.term.code == node2->val.term.code && node1->val.term.attr == node2->val.term.attr);
  case GP_ANODE:
    if (node1->val.anode.children_num != node2->val.anode.children_num) return false;
    if (node1->val.anode.name != node2->val.anode.name) return false; /* name exists in one examplar */
    return (memcmp (node1->val.anode.children, node2->val.anode.children,
                    sizeof (struct gp_tree_node *) * node1->val.anode.children_num)
            == 0);
  case GP_ALT:
    return (node1->val.alt.first == node2->val.alt.first && node1->val.alt.second == node2->val.alt.second);
  default: assert (false); /* nil and error node exist in one exemplar */
  }
}

static void tree_nodes_init (struct grammar *g) {
  g->nodes_htab = create_hash_table (g->alloc, 300, node_hash, node_eq_p);
}

static void tree_nodes_finish (struct grammar *g) { delete_htab_update_statistics (g, g->nodes_htab); }

#define SWAP(a, b, t) \
  do {                \
    t = a;            \
    a = b;            \
    b = t;            \
  } while (false)

struct recovery_info {
  union {
    struct {
      int n_matched_toks; /* number of last matched toks */
      int buff_token_ind;
      int cost;
    } token_info;
    struct recovery_info *next; /* used for freed infos */
  } u;
};

static void recovery_info_init (struct grammar *g) {
  OS_CREATE (g->recovery_infos, g->alloc, 0);
  g->free_recovery_infos = NULL;
}

static struct recovery_info *recovery_info_get_free (struct grammar *g) {
  struct recovery_info *info = g->free_recovery_infos;
  if (info != NULL) {
    g->free_recovery_infos = info->u.next;
    return info;
  }
  OS_TOP_EXPAND (g->recovery_infos, sizeof (struct recovery_info));
  info = (struct recovery_info *) OS_TOP_BEGIN (g->recovery_infos);
  OS_TOP_FINISH (g->recovery_infos);
  return info;
}

static void recovery_info_free (struct grammar *g, struct recovery_info *info) {
  info->u.next = g->free_recovery_infos;
  g->free_recovery_infos = info;
}

static struct recovery_info *recovery_info_copy (struct grammar *g, struct recovery_info *info) {
  struct recovery_info *res = recovery_info_get_free (g);
  *res = *info;
  return res;
}

static void recovery_info_finish (struct grammar *g) { OS_DELETE (g->recovery_infos); }

typedef struct stack_el {
  bool attr_p;
  int ntoks; /* read tokens before the state: ??? setup */
  struct set *set;
  void *anode_attr; /* abstract node or term attr if attr_p */
} stack_el_t;

struct stack {
  bool ambigous_p;
  int num;
  struct recovery_info *recovery;
  vlo_t els;
};

static void stack_init (struct grammar *g) {
  VLO_CREATE (g->free_stacks, g->alloc, 16);
  g->n_stacks = 0;
#ifndef NO_GP_DEBUG_PRINT
  g->n_peak_stack_els = g->n_curr_stack_els = 0;
#endif
  recovery_info_init (g);
}

static void stack_vlo_free (struct grammar *g, vlo_t *stack_vlo) {
  VLO_ADD_MEMORY (g->free_stacks, VLO_BEGIN (*stack_vlo), VLO_LENGTH (*stack_vlo));
}

static void stack_finish (struct grammar *g) {
  for (int i = 0; i < (int) (VLO_LENGTH (g->free_stacks) / sizeof (struct stack *)); i++) {
    struct stack *stack = ((struct stack **) VLO_BEGIN (g->free_stacks))[i];
    VLO_DELETE (stack->els);
    gp_free (g->alloc, stack);
  }
  VLO_DELETE (g->free_stacks);
  recovery_info_finish (g);
}

static struct stack *stack_create (struct grammar *g, struct stack *base) {
  struct stack *stack;
  if (VLO_LENGTH (g->free_stacks) == 0) {
    stack = gp_malloc (g->alloc, sizeof (struct stack));
    stack->num = g->n_stacks++;
    VLO_CREATE (stack->els, g->alloc,
                (base == NULL ? 0 : (int) VLO_LENGTH (base->els)) + 4 * sizeof (stack_el_t));
  } else {
    stack = ((struct stack **) VLO_BOUND (g->free_stacks))[-1];
    VLO_SHORTEN (g->free_stacks, sizeof (struct stack *));
    VLO_NULLIFY (stack->els);
  }
#ifndef NO_GP_DEBUG_PRINT
  if (base != NULL) {
    g->n_curr_stack_els += VLO_LENGTH (base->els) / sizeof (stack_el_t);
    if (g->n_peak_stack_els < g->n_curr_stack_els) g->n_peak_stack_els = g->n_curr_stack_els;
  }
#endif
  stack->ambigous_p = false;
  if (base != NULL) {
    stack->ambigous_p = base->ambigous_p;
    VLO_ADD_MEMORY (stack->els, VLO_BEGIN (base->els), VLO_LENGTH (base->els));
  }
  if (base == NULL || base->recovery == NULL) {
    stack->recovery = NULL;
  } else {
    stack->recovery = recovery_info_copy (g, base->recovery);
  }
  return stack;
}

/* Max free stacks to speedup stack allocation */
#define MAX_FREE_STACKS 1000

static void stack_free (struct grammar *g, struct stack *stack) {
  if (stack->recovery != NULL) recovery_info_free (g, stack->recovery);
#ifndef NO_GP_DEBUG_PRINT
  g->n_curr_stack_els -= VLO_LENGTH (stack->els) / sizeof (stack_el_t);
#endif
  if (VLO_LENGTH (g->free_stacks) < MAX_FREE_STACKS) { /* keep free stack in cache */
    VLO_ADD_MEMORY (g->free_stacks, &stack, sizeof (stack));
  } else {
    VLO_DELETE (stack->els);
    gp_free (g->alloc, stack);
  }
}

static void stack_init_recovery (struct grammar *g, struct stack *stack, int buff_token_ind) {
  assert (stack->recovery == NULL);
  stack->recovery = recovery_info_get_free (g);
  stack->recovery->u.token_info.buff_token_ind = buff_token_ind;
  stack->recovery->u.token_info.n_matched_toks = stack->recovery->u.token_info.cost = 0;
}

static void stack_free_recovery (struct grammar *g, struct stack *stack) {
  assert (stack->recovery != NULL);
  recovery_info_free (g, stack->recovery);
  stack->recovery = NULL;
}

struct token_buff_el {
  int code;   /* token code */
  void *attr; /* token attribute */
};

/* Add token to token buffer. */
static void token_buff_add (struct grammar *g, int code, void *attr) {
  VLO_EXPAND (g->token_buff, sizeof (struct token_buff_el));
  struct token_buff_el *el = &((struct token_buff_el *) VLO_BOUND (g->token_buff))[-1];
  el->code = code;
  el->attr = attr;
}

/* Read a token and save it int the buffer. */
static int token_buff_read (struct grammar *g, void **attr) {
  int code = g->read_token (attr);
  token_buff_add (g, code, *attr);
  return code;
}

static int token_buff_len (struct grammar *g) {
  return VLO_LENGTH (g->token_buff) / sizeof (struct token_buff_el);
}

/* Return token (given by index in the buffer) from the buffer. */
static int token_buff_get (struct grammar *g, int ind, void **attr) {
  assert (ind >= 0 && ind * sizeof (struct token_buff_el *) < (size_t) VLO_LENGTH (g->token_buff));
  struct token_buff_el *el = &((struct token_buff_el *) VLO_BEGIN (g->token_buff))[ind];
  *attr = el->attr;
  return el->code;
}

/* Return the next token.  Take it from the buffer if the buffer is not fully read. */
static int token_read (struct grammar *g, void **attr) {
  size_t size = g->curr_buff_token_ind * sizeof (struct token_buff_el);
  if (size <= (size_t) VLO_LENGTH (g->token_buff)) {
    if (size == (size_t) VLO_LENGTH (g->token_buff)) { /* buffer was read fully: nullify it */
      g->curr_buff_token_ind = 0;
      VLO_NULLIFY (g->token_buff);
    } else {
      struct token_buff_el *el
        = &((struct token_buff_el *) VLO_BEGIN (g->token_buff))[g->curr_buff_token_ind++];
      *attr = el->attr;
      return el->code;
    }
  }
  return g->read_token (attr);
}

#ifndef NO_GP_DEBUG_PRINT
static void token_buff_print (struct grammar *g, FILE *f) {
  for (int i = 0; i < (int) (VLO_LENGTH (g->token_buff) / sizeof (struct token_buff_el)); i++) {
    struct token_buff_el *el = &((struct token_buff_el *) VLO_BEGIN (g->token_buff))[i];
    struct symb *term = term_find_by_code (g, el->code);
    fprintf (f, " %d:%s", i, term->repr);
  }
}
#endif

static void token_buff_init (struct grammar *g) {
  g->curr_buff_token_ind = 0;
  VLO_CREATE (g->token_buff, g->alloc, 0);
}

static void token_buff_finish (struct grammar *g) { VLO_DELETE (g->token_buff); }

static void push_init_set (struct grammar *g, struct stack *stack, struct set *set) {
  VLO_EXPAND (stack->els, sizeof (stack_el_t));
  stack_el_t *el = &((stack_el_t *) VLO_BOUND (stack->els))[-1];
  el->set = set;
  el->attr_p = false;
  el->ntoks = 0;
  el->anode_attr = NULL;
#ifndef NO_GP_DEBUG_PRINT
  g->n_curr_stack_els++;
  if (g->n_peak_stack_els < g->n_curr_stack_els) g->n_peak_stack_els = g->n_curr_stack_els;
#endif
}

static FORCE_INLINE struct set *stack_get_top_set (struct stack *stack) {
  assert (VLO_LENGTH (stack->els) != 0);
  return ((stack_el_t *) VLO_BOUND (stack->els))[-1].set;
}

static FORCE_INLINE struct set *stack_shift (struct grammar *g, struct stack *stack, struct set *set,
                                             void *attr, int ntoks) {
  assert (set->symb->term_p);
  VLO_EXPAND (stack->els, sizeof (stack_el_t));
  stack_el_t *el = &((stack_el_t *) VLO_BOUND (stack->els))[-1];
  el->set = set;
  el->attr_p = true;
  el->ntoks = ntoks;
  el->anode_attr = attr;
#ifndef NO_GP_DEBUG_PRINT
  g->n_curr_stack_els++;
  if (g->n_peak_stack_els < g->n_curr_stack_els) g->n_peak_stack_els = g->n_curr_stack_els;
#endif
  return set;
}

static FORCE_INLINE struct gp_tree_node *get_term_node (struct grammar *g, int code, void *attr) {
  struct gp_tree_node *node = &g->temp_node;
  node->type = GP_TERM;
  node->val.term.code = code;
  node->val.term.attr = attr;
  hash_table_entry_t *entry = find_hash_table_entry (g->nodes_htab, node, true);
  struct gp_tree_node *term_node = (struct gp_tree_node *) *entry;
  if (term_node != NULL) return term_node;
#if !defined(NO_GP_DEBUG_PRINT)
  g->n_parse_term_nodes++;
#endif
  term_node = (*g->parse_alloc) (sizeof (struct gp_tree_node));
  *term_node = *node;
  *entry = term_node;
  term_node->num = g->n_parse_nodes++;
  VLO_ADD_MEMORY (g->all_nodes, &term_node, sizeof (struct parse_tree_node *));
  return term_node;
}

static FORCE_INLINE struct gp_tree_node *get_stack_term_node (struct grammar *g, stack_el_t *el) {
  assert (el->attr_p && el->set->symb->term_p);
  return get_term_node (g, el->set->symb->u.term.code, el->anode_attr);
}

static struct gp_tree_node *get_anode (struct grammar *g, const char *name, int children_num,
                                       struct gp_tree_node **children) {
  struct gp_tree_node *node = &g->temp_node;
  node->type = GP_ANODE;
  node->val.anode.name = name;
  node->val.anode.children_num = children_num;
  node->val.anode.children = children;
  hash_table_entry_t *entry = find_hash_table_entry (g->nodes_htab, node, true);
  struct gp_tree_node *anode = (struct gp_tree_node *) *entry;
  if (anode != NULL) return anode;
#if !defined(NO_GP_DEBUG_PRINT)
  g->n_parse_abstract_nodes++;
#endif
  anode = (*g->parse_alloc) (sizeof (struct gp_tree_node));
  *anode = *node;
  anode->num = g->n_parse_nodes++;
  anode->val.anode.children = g->parse_alloc (children_num * sizeof (struct gp_tree_node *));
  memcpy (anode->val.anode.children, children, children_num * sizeof (struct gp_tree_node *));
  *entry = anode;
  VLO_ADD_MEMORY (g->all_nodes, &anode, sizeof (struct parse_tree_node *));
  return anode;
}

static struct gp_tree_node *get_alt_node (struct grammar *g, struct gp_tree_node *first,
                                          struct gp_tree_node *second) {
  struct gp_tree_node *node = &g->temp_node;
  node->type = GP_ALT;
  node->val.alt.first = first;
  node->val.alt.second = second;
  hash_table_entry_t *entry = find_hash_table_entry (g->nodes_htab, node, true);
  struct gp_tree_node *alt_node = (struct gp_tree_node *) *entry;
  if (alt_node != NULL) return alt_node;
#ifndef NO_GP_DEBUG_PRINT
  g->n_parse_alt_nodes++;
#endif
  alt_node = (*g->parse_alloc) (sizeof (struct gp_tree_node));
  *alt_node = *node;
  alt_node->num = g->n_parse_nodes++;
  *entry = alt_node;
  VLO_ADD_MEMORY (g->all_nodes, &alt_node, sizeof (struct parse_tree_node *));
  return alt_node;
}

static NO_INLINE void *get_transl (struct grammar *g, stack_el_t *stack_addr, int stack_len,
                                   struct rule *rule) {
  int rhs_len = rule->rhs_len;
  if (rule->anode == NULL) {
    assert (rule->trans_len == 1);
    for (int i = 0, start = stack_len - rhs_len; i < rhs_len; i++) {
      int disp = rule->order[i];
      if (disp < 0) continue;
      stack_el_t *el = &stack_addr[start + i];
      if (el->attr_p) return get_stack_term_node (g, el);
      return el->anode_attr;
    }
  }
  assert (rule->anode != NULL);
  if (rule->caller_anode == NULL) {
    rule->caller_anode = ((char *) (*g->parse_alloc) (strlen (rule->anode) + 1));
    VLO_ADD_MEMORY (g->caller_anode_names, &rule->caller_anode, sizeof (rule->caller_anode));
    strcpy (rule->caller_anode, rule->anode);
  }
  VLO_NULLIFY (g->temp_nodes_vlo);
  VLO_EXPAND (g->temp_nodes_vlo, sizeof (struct gp_tree_node *) * rule->trans_len);
  struct gp_tree_node **children = VLO_BEGIN (g->temp_nodes_vlo);
  for (int i = 0; i < rule->trans_len; i++) children[i] = g->empty_node;
  bool err_p = true;
  for (int i = 0, start = stack_len - rhs_len; i < rhs_len; i++) {
    int disp = rule->order[i];
    if (disp < 0) continue;
    stack_el_t *el = &stack_addr[start + i];
    struct gp_tree_node *anode = (struct gp_tree_node *) el->anode_attr;
    if (el->attr_p) {
      anode = get_stack_term_node (g, el);
    }
    children[disp] = anode;
    if (anode != g->error_node) err_p = false;
  }
  if (err_p) return g->error_node; /* all children are error node */
  return get_anode (g, rule->caller_anode, rule->trans_len, children);
}

static FORCE_INLINE struct set *stack_reduce (struct grammar *g, struct stack *stack, struct rule *rule) {
  int rhs_len = rule->rhs_len;
  int nonterm_num = rule->lhs_nonterm_num;
  int stack_len = VLO_LENGTH (stack->els) / sizeof (stack_el_t);
  assert (rhs_len < stack_len);
  stack_el_t *stack_addr = (stack_el_t *) VLO_BEGIN (stack->els);
  struct set *set = stack_addr[stack_len - 1 - rhs_len].set;
  int ntoks = stack_addr[stack_len - 1].ntoks;
  VLO_SHORTEN (stack->els, sizeof (stack_el_t) * rhs_len);
  void *anode_attr = g->empty_node;
  if (rule->anode != NULL || rule->trans_len != 0) anode_attr = get_transl (g, stack_addr, stack_len, rule);
  struct set *goto_set = set->goto_map[nonterm_num];
  VLO_EXPAND (stack->els, sizeof (stack_el_t));
  stack_el_t *el = &((stack_el_t *) VLO_BOUND (stack->els))[-1];
  el->set = goto_set;
  el->attr_p = false;
  el->ntoks = ntoks; /* ??? */
  el->anode_attr = anode_attr;
#ifndef NO_GP_DEBUG_PRINT
  g->n_curr_stack_els += (1 - rhs_len);
  if (g->n_peak_stack_els < g->n_curr_stack_els) g->n_peak_stack_els = g->n_curr_stack_els;
#endif
  return goto_set;
}

static bool stack_eq_p (struct stack *stack1, struct stack *stack2, bool *diff_attr_p) {
  if (VLO_LENGTH (stack1->els) != VLO_LENGTH (stack2->els)) return false;
  assert (stack1->recovery == NULL && stack2->recovery == NULL);
  stack_el_t *stack_addr1 = (stack_el_t *) VLO_BEGIN (stack1->els);
  stack_el_t *stack_addr2 = (stack_el_t *) VLO_BEGIN (stack2->els);
  *diff_attr_p = false;
  for (int i = (int) (VLO_LENGTH (stack1->els) / sizeof (stack_el_t)) - 1; i >= 0; i--) {
    stack_el_t *el1 = &stack_addr1[i], *el2 = &stack_addr2[i];
    if (el1->set != el2->set) return false;
    if (el1->anode_attr != el2->anode_attr) *diff_attr_p = true;
  }
  return true;
}

static FORCE_INLINE bool merge_stacks (struct grammar *g, vlo_t *stacks) {
  bool merge_p = false;
  int last = 0;
  for (int i = 0; i < (int) (VLO_LENGTH (*stacks) / sizeof (struct stack *)); i++) {
    struct stack *curr = ((struct stack **) VLO_BEGIN (*stacks))[i];
    if (curr == NULL) continue;
    ((struct stack **) VLO_BEGIN (*stacks))[last++] = curr;
    for (int j = i + 1; j < (int) (VLO_LENGTH (*stacks) / sizeof (struct stack *)); j++) {
      struct stack *curr2 = ((struct stack **) VLO_BEGIN (*stacks))[j];
      if (curr2 == NULL) continue;
      bool diff_attr_p;
      if (!stack_eq_p (curr, curr2, &diff_attr_p)) continue;
      curr->ambigous_p = merge_p = true;
      ((struct stack **) VLO_BEGIN (*stacks))[j] = NULL;
      if (diff_attr_p) {
        stack_el_t *stack_addr1 = (stack_el_t *) VLO_BEGIN (curr->els);
        stack_el_t *stack_addr2 = (stack_el_t *) VLO_BEGIN (curr2->els);
        for (int k = (int) (VLO_LENGTH (curr->els) / sizeof (stack_el_t)) - 1; k >= 0; k--) {
          stack_el_t *el1 = &stack_addr1[k], *el2 = &stack_addr2[k];
          el1->anode_attr = g->attr_merge (el1->anode_attr, el2->anode_attr);
        }
      }
      stack_free (g, curr2);
    }
  }
  VLO_SHORTEN (*stacks, VLO_LENGTH (*stacks) - last * sizeof (struct stack *));
  return merge_p;
}

#ifndef NO_GP_DEBUG_PRINT

static void print_stack_els (FILE *f, struct stack *stack) {
  for (int i = 0; i < (int) (VLO_LENGTH (stack->els) / sizeof (stack_el_t)); i++) {
    stack_el_t *el = &((stack_el_t *) VLO_BEGIN (stack->els))[i];
    struct symb *symb = el->set->symb;
    fprintf (f, " [%d]s%d:%s", el->ntoks, el->set->num, symb->repr);
    if (el->anode_attr == NULL) continue;
    if (el->attr_p) {
      assert (symb->term_p);
      fprintf (f, ":a%llx", (long long) el->anode_attr);
    } else {
      struct gp_tree_node *anode = (struct gp_tree_node *) el->anode_attr;
      fprintf (f, ":n%u", anode->num);
    }
  }
  fprintf (f, "\n");
}

static void print_stack (FILE *f, struct stack *stack) {
  fprintf (f, "      {#%d}", stack->num);
  if (stack->recovery != NULL)
    fprintf (f, "token #%d (matched=%d, cost=%d):", stack->recovery->u.token_info.buff_token_ind,
             stack->recovery->u.token_info.n_matched_toks, stack->recovery->u.token_info.cost);
  print_stack_els (stderr, stack);
}

static void print_single_stack (struct grammar *g, FILE *f, struct stack *stack, struct action *action) {
  assert (stack->recovery == NULL);
  fprintf (f, "  Single stack after [");
  print_action (g, f, action);
  fprintf (f, "]: {#%d}", stack->num);
  print_stack_els (f, stack);
}

static void print_stacks (FILE *f, const char *title, vlo_t *stacks, int start) {
  fprintf (f, "%s:\n", title);
  for (int i = start; i < (int) (VLO_LENGTH (*stacks) / sizeof (struct stack *)); i++)
    print_stack (f, ((struct stack **) VLO_BEGIN (*stacks))[i]);
}
#endif

static bool process_term_for_stack (struct grammar *g, struct stack *start_stack, int term, void *attr) {
  bool shift_p = false;
  int len = VLO_LENGTH (g->curr_stacks);
  int new_els_num = VLO_LENGTH (g->new_stacks) / sizeof (struct stack *);
  VLO_ADD_MEMORY (g->curr_stacks, &start_stack, sizeof (start_stack));
#ifndef NO_GP_DEBUG_PRINT
  if (UNLIKELY (g->debug_level > 5)) {
    struct symb *symb = ((struct symb **) VLO_BEGIN (g->symbs->terms_vlo))[term];
    fprintf (stderr, "    Processing stack on term %s:", symb->repr);
    print_stack (stderr, start_stack);
  }
#endif
  while (VLO_LENGTH (g->curr_stacks) > len) {
    struct stack *curr_stack = ((struct stack **) VLO_BOUND (g->curr_stacks))[-1];
    VLO_SHORTEN (g->curr_stacks, sizeof (struct stack *));
    stack_el_t *el = &((stack_el_t *) VLO_BOUND (curr_stack->els))[-1];
    struct set *set = el->set;
    int ntoks = el->ntoks;
    int actions_num;
    struct action *actions = set_get_actions (g, set, term, &actions_num);
    if (actions_num == 0) {
      VLO_ADD_MEMORY (g->failed_stacks, &curr_stack, sizeof (curr_stack));
      continue;
    }
#ifndef NO_GP_DEBUG_PRINT
    g->n_multi_stack_actions++;
#endif
    for (int i = 0; i < actions_num; i++) {
      struct action *action = &actions[i];
      struct stack *stack = i == actions_num - 1 ? curr_stack : stack_create (g, curr_stack);
#ifndef NO_GP_DEBUG_PRINT
      if (UNLIKELY (g->debug_level > 5)) {
        fprintf (stderr, "      Apply action ");
        print_action (g, stderr, action);
      }
#endif
      if (LIKELY (!action->shift_p)) { /* reduce */
        set = stack_reduce (g, stack, action->u.rule);
        VLO_ADD_MEMORY (g->curr_stacks, &stack, sizeof (stack));
      } else { /* shift */
        struct set *shifted_set = action->u.set;
        assert (shifted_set != NULL);
        stack_shift (g, stack, shifted_set, attr, ntoks + 1);
        VLO_ADD_MEMORY (g->new_stacks, &stack, sizeof (stack));
        shift_p = true;
        if (stack->recovery != NULL) {
          stack->recovery->u.token_info.n_matched_toks++;
          stack->recovery->u.token_info.buff_token_ind++;
        }
      }
    }
#ifndef NO_GP_DEBUG_PRINT
    if (UNLIKELY (g->debug_level > 5)) fprintf (stderr, "\n");
#endif
  }
#ifndef NO_GP_DEBUG_PRINT
  if (UNLIKELY (g->debug_level > 5))
    print_stacks (stderr, "    Result stacks after processing the stack", &g->new_stacks, new_els_num);
#endif
  return shift_p;
}

/* Stack can not be matched: add derived stack to delayed_stacks.  Delayed stacks is oredered by
   their decreasing cost.  Added delayed stack will be the first between one with the same cost.  */
static void add_delayed_recovery_stack (struct grammar *g, struct stack *stack) {
  assert (VLO_LENGTH (stack->els) != 0);
  if ((size_t) VLO_LENGTH (stack->els) <= sizeof (struct stack_el)) return;
  struct stack_el *el = &((struct stack_el *) VLO_BOUND (stack->els))[-1];
  struct stack *delayed_stack = stack_create (g, stack);
  /* stack: S0...SN-1, SN; delayed stack: SO...SN-1 */
  VLO_SHORTEN (delayed_stack->els, sizeof (struct stack_el));
  struct stack_el *prev_el = &((struct stack_el *) VLO_BOUND (delayed_stack->els))[-1];
  delayed_stack->recovery->u.token_info.cost += el->ntoks - prev_el->ntoks;
  VLO_EXPAND (g->delayed_stacks, sizeof (struct stack *));
  int i = (int) (VLO_LENGTH (g->delayed_stacks) / sizeof (struct stack *)) - 2;
  for (; i >= 0; i--) {
    struct stack *s = ((struct stack **) VLO_BEGIN (g->delayed_stacks))[i];
    if (s->recovery->u.token_info.cost > delayed_stack->recovery->u.token_info.cost) break;
    /* delayed_stack is inserted deeper than existing stacks with the same cost */
    ((struct stack **) VLO_BEGIN (g->delayed_stacks))[i + 1] = s;
  }
  ((struct stack **) VLO_BEGIN (g->delayed_stacks))[i + 1] = delayed_stack;
}

/* Finish error recovery modifying new_stacks and returning final single_stack. */
static struct stack *recovery_stop (struct grammar *g, bool one_stack_p, struct symb *error_term,
                                    void *error_attr) {
  for (int i = 0; i < (int) (VLO_LENGTH (g->delayed_stacks) / sizeof (struct stack *)); i++) {
    struct stack *stack = ((struct stack **) VLO_BEGIN (g->delayed_stacks))[i];
    stack_free (g, stack);
  }
  VLO_NULLIFY (g->delayed_stacks);
  int min_cost = -1, buff_token_ind = -1;
  struct stack *eof_stack = NULL;
  for (int i = 0; i < (int) (VLO_LENGTH (g->new_stacks) / sizeof (struct stack *)); i++) {
    struct stack *stack = ((struct stack **) VLO_BEGIN (g->new_stacks))[i];
    struct set *set = stack_get_top_set (stack);
    struct symb *symb = set->symb;
    if (symb == g->end_marker) {
      eof_stack = stack;
      buff_token_ind = stack->recovery->u.token_info.buff_token_ind;
      break;
    }
    if (stack->recovery->u.token_info.n_matched_toks >= g->recovery_token_matches
        && (min_cost < 0 || stack->recovery->u.token_info.cost < min_cost)) {
      min_cost = stack->recovery->u.token_info.cost;
      buff_token_ind = stack->recovery->u.token_info.buff_token_ind;
    }
  }
#ifndef NO_GP_DEBUG_PRINT
  if (UNLIKELY (g->debug_level > 3))
    print_stacks (stderr, "    Stacks before error recovery stop", &g->new_stacks, 0);
#endif
  int n = 0;
  struct stack *single_stack = NULL;
  for (int i = 0; i < (int) (VLO_LENGTH (g->new_stacks) / sizeof (struct stack *)); i++) {
    struct stack *stack = ((struct stack **) VLO_BEGIN (g->new_stacks))[i];
    if (stack == eof_stack) {
      stack_free_recovery (g, stack);
      ((struct stack **) VLO_BEGIN (g->new_stacks))[n++] = stack;
    } else if (eof_stack != NULL || stack->recovery->u.token_info.n_matched_toks < g->recovery_token_matches
               || stack->recovery->u.token_info.buff_token_ind != buff_token_ind
               || stack->recovery->u.token_info.cost > min_cost) {
      stack_free (g, stack);
    } else if (one_stack_p && single_stack == NULL) {
      single_stack = stack;
      stack_free_recovery (g, stack);
      ((struct stack **) VLO_BEGIN (g->new_stacks))[n++] = stack;
    } else if (!one_stack_p) {
      stack_free_recovery (g, stack);
      ((struct stack **) VLO_BEGIN (g->new_stacks))[n++] = stack;
    } else {
      stack_free (g, stack);
    }
  }
  VLO_SHORTEN (g->new_stacks, VLO_LENGTH (g->new_stacks) - n * sizeof (struct stack *));
  g->curr_buff_token_ind = buff_token_ind;
#ifndef NO_GP_DEBUG_PRINT
  if (UNLIKELY (g->debug_level > 2)) {
    fprintf (stderr, "<<<%s error recovery stop (buff token ind =%d)>>>\n",
             one_stack_p ? "Single stack" : "Multi-stack", buff_token_ind);
    if (g->debug_level > 3)
      print_stacks (stderr, "    Result stacks after error recovery", &g->new_stacks, 0);
  }
#endif
  buff_token_ind -= g->recovery_token_matches;
  if (buff_token_ind < 0) buff_token_ind = 0; /* we can finish at the end marker */
  void *stop_token_attr;
  int stop_code = token_buff_get (g, buff_token_ind, &stop_token_attr);
  struct symb *stop_token_term = term_find_by_code (g, stop_code);
  g->syntax_error (error_term->repr, error_attr, stop_token_term->repr, stop_token_attr);
  return single_stack;
}

#ifndef NO_GP_DEBUG_PRINT
static void print_read (struct grammar *g, FILE *f, struct symb *term, int stacks_num) {
  fprintf (f, "  Read %s (%d, #stacks: %d, #nodes: all=%d)", term->repr, g->toks_num, stacks_num,
           g->n_parse_nodes);
  g->toks_num++;
  if (g->debug_level < 3) {
    fprintf (f, "\n");
  } else {
    fprintf (f, ": buff (%d) =", g->curr_buff_token_ind);
    token_buff_print (g, f);
    fprintf (f, "\n");
  }
}
#endif

/* Make error recovery and set up final new_stacks and return final single_stack. */
static struct stack *recovery (struct grammar *g, int code, void *attr, bool one_stack_p) {
  assert (VLO_LENGTH (g->failed_stacks) != 0 && VLO_LENGTH (g->new_stacks) == 0
          && VLO_LENGTH (g->curr_stacks) == 0);
#ifndef NO_GP_DEBUG_PRINT
  if (UNLIKELY (g->debug_level > 2))
    fprintf (stderr, "<<<%s error recovery start>>>\n", one_stack_p ? "Single stack" : "Multi-stack");
#endif
  struct symb *error_term = term_find_by_code (g, code);
  void *error_attr = attr;
  token_buff_add (g, code, attr);
  assert (VLO_LENGTH (g->delayed_stacks) == 0);
  for (int i = 0; i < (int) (VLO_LENGTH (g->failed_stacks) / sizeof (struct stack *)); i++) {
    struct stack *stack = ((struct stack **) VLO_BEGIN (g->failed_stacks))[i];
    stack_init_recovery (g, stack, g->curr_buff_token_ind);
    add_delayed_recovery_stack (g, stack);
    VLO_ADD_MEMORY (g->new_stacks, &stack, sizeof (stack));
  }
  VLO_NULLIFY (g->failed_stacks);
  int skipped = 0;
  bool stop_p = false;
  for (;;) {
#ifndef NO_GP_DEBUG_PRINT
    if (UNLIKELY (g->debug_level > 3))
      print_stacks (stderr, "   Delayed recovery stacks", &g->delayed_stacks, 0);
#endif
    int n;
    for (n = 0;; n++) {
      int ind = VLO_LENGTH (g->delayed_stacks) / sizeof (struct stack *) - n - 1;
      if (ind < 0) break;
      struct stack *stack = ((struct stack **) VLO_BEGIN (g->delayed_stacks))[ind];
      if (stack->recovery->u.token_info.cost > skipped) break;
      add_delayed_recovery_stack (g, stack);
      VLO_ADD_MEMORY (g->new_stacks, &stack, sizeof (stack));
#ifndef NO_GP_DEBUG_PRINT
      if (UNLIKELY (g->debug_level > 3)) {
        fprintf (stderr, " Move delayed stack:");
        print_stack (stderr, stack);
      }
#endif
    }
    VLO_SHORTEN (g->delayed_stacks, n * sizeof (struct stack *));
    vlo_t temp_vlo;
    VLO_NULLIFY (g->curr_stacks);
    SWAP (g->curr_stacks, g->new_stacks, temp_vlo);
    int max_buff_ind = 0;
    for (int i = 0; i < (int) (VLO_LENGTH (g->curr_stacks) / sizeof (struct stack *)); i++) {
      struct stack *stack = ((struct stack **) VLO_BEGIN (g->curr_stacks))[i];
      struct set *set = stack_get_top_set (stack);
      int curr_code = token_buff_get (g, stack->recovery->u.token_info.buff_token_ind, &attr);
      struct symb *term = term_find_by_code (g, curr_code);
      int actions_num;
      set_get_actions (g, set, term->u.term.term_num, &actions_num);
      bool shift_p = true;
      if (actions_num != 0) {
        shift_p = process_term_for_stack (g, stack, term->u.term.term_num, attr);
      } else { /* skip */
        stack->recovery->u.token_info.buff_token_ind++;
        stack->recovery->u.token_info.cost++;
        stack->recovery->u.token_info.n_matched_toks = 0;
        VLO_ADD_MEMORY (g->new_stacks, &stack, sizeof (stack));
      }
      if (shift_p && max_buff_ind < stack->recovery->u.token_info.buff_token_ind)
        max_buff_ind = stack->recovery->u.token_info.buff_token_ind;
    }
    VLO_NULLIFY (g->curr_stacks);
    for (int i = 0; i < (int) (VLO_LENGTH (g->failed_stacks) / sizeof (struct stack *)); i++) {
      struct stack *stack = ((struct stack **) VLO_BEGIN (g->failed_stacks))[i];
      assert (stack->recovery != NULL);
      stack->recovery->u.token_info.buff_token_ind++;
      stack->recovery->u.token_info.cost++;
      stack->recovery->u.token_info.n_matched_toks = 0;
      VLO_ADD_MEMORY (g->new_stacks, &stack, sizeof (stack));
      if (max_buff_ind < stack->recovery->u.token_info.buff_token_ind)
        max_buff_ind = stack->recovery->u.token_info.buff_token_ind;
    }
    VLO_NULLIFY (g->failed_stacks);
#ifndef NO_GP_DEBUG_PRINT
    if (UNLIKELY (g->debug_level > 3)) print_stacks (stderr, "   New recovery stacks", &g->new_stacks, 0);
#endif
    for (int i = 0; i < (int) (VLO_LENGTH (g->new_stacks) / sizeof (struct stack *)); i++) {
      struct stack *stack = ((struct stack **) VLO_BEGIN (g->new_stacks))[i];
      struct stack_el *el = &((struct stack_el *) VLO_BOUND (stack->els))[-1];
      if (el->set->symb == g->end_marker
          || stack->recovery->u.token_info.n_matched_toks >= g->recovery_token_matches)
        stop_p = true;
    }
    if (stop_p) break;
    if (token_buff_len (g) <= max_buff_ind) {
      int new_code = token_buff_read (g, &attr);
#ifndef NO_GP_DEBUG_PRINT
      if (UNLIKELY (g->debug_level > 2))
        print_read (g, stderr, term_find_by_code (g, new_code),
                    VLO_LENGTH (g->new_stacks) / sizeof (struct stack *));
#endif
      skipped++;
    }
  }
  return recovery_stop (g, one_stack_p, error_term, error_attr);
}

static FORCE_INLINE void gc_mark_anode (struct grammar *g, struct gp_tree_node *anode) {
  if (!bitmap_set_bit_p (&g->marked_nodes, anode->num) || (anode->type != GP_ANODE && anode->type != GP_ALT))
    return;
  VLO_ADD_MEMORY (g->temp_vlo, &anode, sizeof (struct gp_tree_node *));
}

static void gc_mark_parse_tree (struct grammar *g, struct gp_tree_node *anode) {
  if (!bitmap_set_bit_p (&g->marked_nodes, anode->num) || (anode->type != GP_ANODE && anode->type != GP_ALT))
    return;
  VLO_ADD_MEMORY (g->temp_vlo, &anode, sizeof (struct gp_tree_node *));
  while (VLO_LENGTH (g->temp_vlo) != 0) {
    anode = ((struct gp_tree_node **) VLO_BOUND (g->temp_vlo))[-1];
    VLO_SHORTEN (g->temp_vlo, sizeof (struct gp_tree_node *));
    if (anode->type == GP_ANODE) {
      for (int i = anode->val.anode.children_num - 1; i >= 0; i--)
        gc_mark_anode (g, anode->val.anode.children[i]);
    } else {
      assert (anode->type == GP_ALT);
      gc_mark_anode (g, anode->val.alt.first);
      gc_mark_anode (g, anode->val.alt.second);
    }
  }
}

static void gc_mark_stack (struct grammar *g, struct stack *stack) {
  for (int i = 0; i < (int) (VLO_LENGTH (stack->els) / sizeof (stack_el_t)); i++) {
    stack_el_t *el = &((stack_el_t *) VLO_BEGIN (stack->els))[i];
    if (!el->attr_p && el->anode_attr != NULL) gc_mark_parse_tree (g, el->anode_attr);
  }
}

static void gc (struct grammar *g, vlo_t *stacks) {
  assert (g->parse_free != NULL);
#ifndef NO_GP_DEBUG_PRINT
  if (g->debug_level > 2) fprintf (stderr, "++++GG start\n");
#endif
  /* Mark: */
  bitmap_clear (&g->marked_nodes);
  for (int i = 0; i < (int) (VLO_LENGTH (*stacks) / sizeof (struct stack *)); i++) {
    struct stack *stack = ((struct stack **) VLO_BEGIN (*stacks))[i];
    gc_mark_stack (g, stack);
  }
  /* Sweep */
  int last = 0;
#ifndef NO_GP_DEBUG_PRINT
  int n = 0, removed = 0;
  if (g->debug_level > 3) fprintf (stderr, "GC: Removed nodes:\n");
#endif
  int nodes_num = (int) (VLO_LENGTH (g->all_nodes) / sizeof (struct gp_tree_node *));
  for (int i = 0; i < nodes_num; i++) {
    struct gp_tree_node *node = ((struct gp_tree_node **) VLO_BEGIN (g->all_nodes))[i];
    if (bitmap_bit_p (&g->marked_nodes, node->num)) {
      ((struct gp_tree_node **) VLO_BEGIN (g->all_nodes))[last++] = node;
    } else {
#ifndef NO_GP_DEBUG_PRINT
      removed++;
      if (g->debug_level > 3) {
        fprintf (stderr, " n%u", node->num);
        n++;
        if (n >= 16) {
          fprintf (stderr, "\n");
          n = 0;
        }
      }
#endif
      remove_element_from_hash_table_entry (g->nodes_htab, node);
      g->parse_free (node);
    }
  }
  VLO_SHORTEN (g->all_nodes, VLO_LENGTH (g->all_nodes) - last * sizeof (struct gp_parse_node *));
  if (hash_table_size (g->nodes_htab) > 4 * (size_t) last) { /* decrease nodes htab size */
#ifndef NO_GP_DEBUG_PRINT
    if (g->debug_level > 2)
      fprintf (stderr, "++++GC node htab rebuilding: before size=%u, nodes %d\n",
               (unsigned) hash_table_size (g->nodes_htab), last);
#endif
    delete_htab_update_statistics (g, g->nodes_htab);
    g->nodes_htab = create_hash_table (g->alloc, 3 * last / 2, node_hash, node_eq_p);
    for (int i = 0; i < last; i++) {
      struct gp_tree_node *node = ((struct gp_tree_node **) VLO_BEGIN (g->all_nodes))[i];
      hash_table_entry_t *entry = find_hash_table_entry (g->nodes_htab, node, true);
      assert (*entry == NULL);
      *entry = node;
    }
  }
#ifndef NO_GP_DEBUG_PRINT
  if (g->debug_level > 3 && n != 0) fprintf (stderr, "\n");
  if (g->debug_level > 2)
    fprintf (stderr, "++++GC finish: before nodes %d, kept %d, removed %d; node htab size = %u\n", nodes_num,
             last, removed, (unsigned) hash_table_size (g->nodes_htab));
#endif
}

/* Start threshold to initiate parse tree nodes GC */
#define GC_START_NODES_THRESHOLD 1000

/* Major function to make parsing. Return true if we parsed successfully. */
static bool parse (struct grammar *g, bool *ambiguous_p, struct gp_tree_node **transl) {
  g->n_parse_nodes = 0;
  g->empty_node = (struct gp_tree_node *) g->parse_alloc (sizeof (struct gp_tree_node));
  g->empty_node->type = GP_NIL;
  g->empty_node->num = g->n_parse_nodes++;
  g->error_node = (struct gp_tree_node *) g->parse_alloc (sizeof (struct gp_tree_node));
  g->error_node->type = GP_ERROR;
  g->error_node->num = g->n_parse_nodes++;
  VLO_CREATE (g->symb_sits, g->alloc, 16);
  VLO_CREATE (g->actions_vlo, g->alloc, 16);
  VLO_CREATE (g->temp_nodes_vlo, g->alloc, 16);
  stack_init (g);
  token_buff_init (g);
  tree_nodes_init (g);
  struct stack *single_stack = stack_create (g, NULL);
  struct set *start_set = build_sets (g);
  push_init_set (g, single_stack, start_set);
  VLO_CREATE (g->curr_stacks, g->alloc, 2 * sizeof (vlo_t));
  VLO_CREATE (g->new_stacks, g->alloc, 2 * sizeof (vlo_t));
  g->toks_num = 0;
  g->n_parse_term_nodes = g->n_parse_abstract_nodes = g->n_parse_alt_nodes = 0;
#ifndef NO_GP_DEBUG_PRINT
  g->n_single_stack_actions = g->n_multi_stack_actions = 0;
#endif
  void *attr;
  int code = g->read_token (&attr);
  struct symb *term_symb = term_find_by_code (g, code);
  int term = term_symb->u.term.term_num;
#ifndef NO_GP_DEBUG_PRINT
  if (UNLIKELY (g->debug_level > 2)) print_read (g, stderr, term_symb, 1);
#endif
  bool one_stack_p;
  size_t gc_nodes_threshold = GC_START_NODES_THRESHOLD;
  VLO_CREATE (g->failed_stacks, g->alloc, 0);
  VLO_CREATE (g->delayed_stacks, g->alloc, 0);
  for (;;) {
    if (single_stack != NULL) {
      stack_el_t *el = &((stack_el_t *) VLO_BOUND (single_stack->els))[-1];
      struct set *set = el->set;
      int ntoks = el->ntoks;
      for (;;) {
        int actions_num;
        struct action *actions = set_get_actions (g, set, term, &actions_num);
        if (actions_num != 1) {
          VLO_ADD_MEMORY (g->curr_stacks, &single_stack, sizeof (single_stack));
          one_stack_p = actions_num == 0;
          single_stack = NULL;
          goto multi_stack;
        }
#ifndef NO_GP_DEBUG_PRINT
        g->n_single_stack_actions++;
#endif
        if (LIKELY (!actions[0].shift_p)) { /* reduce */
          set = stack_reduce (g, single_stack, actions[0].u.rule);
#ifndef NO_GP_DEBUG_PRINT
          if (UNLIKELY (g->debug_level > 4)) print_single_stack (g, stderr, single_stack, &actions[0]);
#endif
        } else { /* shift */
          struct set *shifted_set = actions[0].u.set;
          assert (shifted_set != NULL);
          set = stack_shift (g, single_stack, shifted_set, attr, ntoks + 1);
#ifndef NO_GP_DEBUG_PRINT
          if (UNLIKELY (g->debug_level > 4)) print_single_stack (g, stderr, single_stack, &actions[0]);
#endif
          if (code == END_MARKER_CODE) {
            VLO_ADD_MEMORY (g->curr_stacks, &single_stack, sizeof (single_stack));
            goto finish;
          }
          code = token_read (g, &attr);
          term_symb = term_find_by_code (g, code);
          term = term_symb->u.term.term_num;
#ifndef NO_GP_DEBUG_PRINT
          if (UNLIKELY (g->debug_level > 2)) print_read (g, stderr, term_symb, 1);
#endif
        }
      }
    }
    one_stack_p = false;
  multi_stack: /* error recovery start through this too */
    assert (VLO_LENGTH (g->new_stacks) == 0);
    while (VLO_LENGTH (g->failed_stacks) != 0) {
      struct stack *failed_stack = ((struct stack **) VLO_BOUND (g->failed_stacks))[-1];
      VLO_SHORTEN (g->failed_stacks, sizeof (struct stack *));
      stack_free (g, failed_stack);
    }
    bool shift_p = false;
    while (VLO_LENGTH (g->curr_stacks) != 0) {
      struct stack *curr_stack = ((struct stack **) VLO_BOUND (g->curr_stacks))[-1];
      VLO_SHORTEN (g->curr_stacks, sizeof (struct stack *));
      if (process_term_for_stack (g, curr_stack, term, attr)) shift_p = true;
    }
    if (!shift_p) { /* error: */
      single_stack = recovery (g, code, attr, one_stack_p);
      code = token_buff_get (g, g->curr_buff_token_ind - 1, &attr); /* last read token */
    }
    if (VLO_LENGTH (g->new_stacks) == 0) break;
    vlo_t temp_vlo;
    SWAP (g->curr_stacks, g->new_stacks, temp_vlo);
    if (merge_stacks (g, &g->curr_stacks)) {
#ifndef NO_GP_DEBUG_PRINT
      if (UNLIKELY (g->debug_level > 4))
        print_stacks (stderr, "  Parsing stacks after node merging", &g->curr_stacks, 0);
#endif
    } else {
#ifndef NO_GP_DEBUG_PRINT
      if (UNLIKELY (g->debug_level > 4)) print_stacks (stderr, "  New parsing stacks", &g->curr_stacks, 0);
#endif
    }
    if (VLO_LENGTH (g->curr_stacks) == sizeof (struct stack *)) {
      if (code == END_MARKER_CODE) break;
      single_stack = ((struct stack **) VLO_BEGIN (g->curr_stacks))[0];
      VLO_NULLIFY (g->curr_stacks);
    }
    if (g->parse_free != NULL && VLO_LENGTH (g->all_nodes) >= gc_nodes_threshold) {
      gc (g, &g->curr_stacks);
      gc_nodes_threshold = 2 * VLO_LENGTH (g->all_nodes);
    }
    code = token_read (g, &attr);
    term_symb = term_find_by_code (g, code);
    term = term_symb->u.term.term_num;
#ifndef NO_GP_DEBUG_PRINT
    if (UNLIKELY (g->debug_level > 2))
      print_read (g, stderr, term_symb, VLO_LENGTH (g->curr_stacks) / sizeof (struct stack *));
#endif
  }
finish:
  bool res = false;
  if (VLO_LENGTH (g->curr_stacks) == sizeof (struct stack *)) {
    struct stack *stack = ((struct stack **) VLO_BEGIN (g->curr_stacks))[0];
    struct set *set = stack_get_top_set (stack);
    struct symb *symb = set->symb;
    if (strcmp (symb->repr, END_MARKER_NAME) == 0) {
      assert (VLO_LENGTH (stack->els) == 3 * sizeof (stack_el_t));
      stack_el_t *el = &((stack_el_t *) VLO_BEGIN (stack->els))[1];
      assert (!el->attr_p);
      *transl = (struct gp_tree_node *) el->anode_attr;
      res = true;
      *ambiguous_p = stack->ambigous_p;
    }
  }
  VLO_DELETE (g->delayed_stacks);
  VLO_DELETE (g->failed_stacks);
  stack_vlo_free (g, &g->curr_stacks);
  VLO_DELETE (g->new_stacks);
  VLO_DELETE (g->curr_stacks);
  VLO_DELETE (g->symb_sits);
  VLO_DELETE (g->actions_vlo);
  VLO_DELETE (g->temp_nodes_vlo);
  stack_finish (g);
  token_buff_finish (g);
  tree_nodes_finish (g);
  return res;
}

#ifndef NO_GP_DEBUG_PRINT

/* Prints NODE into file F and prints all its children. */
static void print_node (struct grammar *g, FILE *f, struct gp_tree_node *node) {
  int i;

  assert (node != NULL);
  if (g->visits_p[node->num]) return;
  g->visits_p[node->num] = true;
  fprintf (f, "%7d: ", node->num);
  switch (node->type) {
  case GP_NIL: fprintf (f, "EMPTY\n"); break;
  case GP_ERROR: fprintf (f, "ERROR\n"); break;
  case GP_TERM:
    fprintf (f, "TERMINAL: code=%d, repr=%s\n", node->val.term.code,
             term_find_by_code (g, node->val.term.code)->repr);
    break;
  case GP_ANODE:
    fprintf (f, "ABSTRACT: %s (", node->val.anode.name);
    for (i = 0; i < node->val.anode.children_num; i++) fprintf (f, " %d", node->val.anode.children[i]->num);
    fprintf (f, " )\n");
    for (i = 0; i < node->val.anode.children_num; i++) print_node (g, f, node->val.anode.children[i]);
    break;
  case GP_ALT:
    fprintf (f, "ALTERNATIVE:");
    fprintf (f, "%d %d\n", node->val.alt.first->num, node->val.alt.second->num);
    print_node (g, f, node->val.alt.first);
    print_node (g, f, node->val.alt.second);
    break;
  default: assert (false);
  }
}

/* Print parse tree with ROOT. */
static void print_parse (struct grammar *g, FILE *f, struct gp_tree_node *root) {
  g->visits_p = (bool *) gp_malloc (g->alloc, g->n_parse_nodes * sizeof (bool));
  memset (g->visits_p, 0, g->n_parse_nodes * sizeof (bool));
  print_node (g, f, root);
  gp_free (g->alloc, g->visits_p);
  fprintf (f, "\n");
}

void gp_print_translation (struct grammar *g, FILE *f, struct gp_tree_node *root) {
  print_parse (g, f, root);
}

#endif

/* Parse input according read grammar. For unambiguous grammar the flag does not affect the result.
   D_LEVEL says what debugging information to output (it works only if we compiled without defined
   macro NO_GP_DEBUG_PRINT). The function returns the error code (which will be also in error_code).
   The function sets up *AMBIGUOUS_P if we found that the grammar is ambigous (it works even we
   asked only one parse tree without alternatives). */
int gp_parse (struct grammar *g, int (*read) (void **attr), struct gp_tree_node **root, bool *ambiguous_p) {
  g->all_searches = g->all_collisions = 0;
  assert (g != NULL);
  g->read_token = read;
  *root = NULL;
  *ambiguous_p = false;
  int code;
  bool parse_init_p = false;
  if ((code = setjmp (g->error_longjump_buff)) != 0) {
    if (parse_init_p) gp_parse_fin (g);
    return code;
  }
  if (g->undefined_p) gp_error (g, GP_UNDEFINED_OR_BAD_GRAMMAR, "undefined or bad grammar");
  gp_parse_init (g);
  parse_init_p = true;
  struct gp_tree_node *result;
  bool ok_p = parse (g, ambiguous_p, &result);
  if (ok_p) *root = result;
#ifndef NO_GP_DEBUG_PRINT
  if (g->debug_level > 0) {
    if (ok_p && g->debug_level > 1) {
      fprintf (stderr, "Translation:\n");
      print_parse (g, stderr, result);
    }
    fprintf (stderr, "%sGrammar: #terms = %d, #nonterms = %d, ", *ambiguous_p ? "AMBIGUOUS " : "",
             g->symbs->n_terms, g->symbs->n_nonterms);
    fprintf (stderr, "#rules = %d, rules size = %d\n", g->rules->n_rules,
             g->rules->n_rhs_lens + g->rules->n_rules);
    fprintf (stderr, "Input: #tokens = %d, #all situations = %d\n", g->toks_num, g->n_all_sits);
    fprintf (stderr, "       #terminal sets = %d, their size = %d\n", g->term_sets->n_term_sets,
             g->term_sets->n_term_sets_size);
    fprintf (stderr, "       #sets = %d, #their start situations = %d\n", g->n_sets, g->n_sets_start_sits);
    fprintf (stderr, "       #goto vectors = %d, their length = %d\n", g->n_goto_vects, g->n_goto_vect_len);
    fprintf (stderr, "       #actions = %d, #action vectors = %d, their length = %d\n", g->n_actions,
             g->n_action_vects, g->n_action_vect_len);
    fprintf (stderr, "       max #stacks = %d, max #stack els = %d\n", g->n_stacks, g->n_peak_stack_els);
    fprintf (stderr, "       #single stack actions = %d, #multi stack actions = %d\n",
             g->n_single_stack_actions, g->n_multi_stack_actions);
    fprintf (stderr, "       #term nodes = %d, #abstract nodes = %d\n", g->n_parse_term_nodes,
             g->n_parse_abstract_nodes);
    fprintf (stderr, "       #alternative nodes = %d, #all nodes = %d\n", g->n_parse_alt_nodes,
             g->n_parse_term_nodes + g->n_parse_abstract_nodes + g->n_parse_alt_nodes);
  }
#endif
  gp_parse_fin (g);
#ifndef NO_GP_DEBUG_PRINT
  if (g->debug_level > 0) { /* do it after deleting hash tables */
    if (g->all_searches == 0) g->all_searches++;
    fprintf (stderr, "       #table collisions = %.2g%% (%d out of %d)\n",
             g->all_collisions * 100.0 / g->all_searches, g->all_collisions, g->all_searches);
  }
#endif
  return ok_p ? 0 : 1; /* !!! change in the future */
}

void gp_fin (struct grammar *g) { /* Free memory allocated for the grammar. */
  if (g != NULL) {
    g->parse_free (g->empty_node);
    g->parse_free (g->error_node);
    gp_allocator_t *allocator = g->alloc;
    bitmap_destroy (&g->marked_nodes);
    VLO_DELETE (g->all_nodes);
    VLO_DELETE (g->temp_vlo);
    for (int i = 0; i < (int) (VLO_LENGTH (g->caller_anode_names) / sizeof (char *)); i++) {
      char *name = ((char **) VLO_BEGIN (g->caller_anode_names))[i];
      gp_free (allocator, name);
    }
    VLO_DELETE (g->caller_anode_names);
    rule_fin (g, g->rules);
    term_set_fin (g, g->term_sets);
    symb_fin (g, g->symbs);
    gp_free (allocator, g);
    gp_alloc_del (allocator);
  }
}

static void free_tree_reduce (struct gp_tree_node *node) {
  assert (node != NULL);
  assert ((node->type & GP_VISITED) == 0);
  enum gp_tree_node_type type = node->type;
  node->type = (enum gp_tree_node_type) (node->type | GP_VISITED);
  switch (type) {
  case GP_NIL:
  case GP_ERROR:
  case GP_TERM: break;
  case GP_ANODE:
    if (node->val.anode.name[0] == '\0') /* We have already seen the node name */
      node->val.anode.name = NULL;
    for (int i = 0; i < node->val.anode.children_num; i++)
      if (node->val.anode.children[i]->type & GP_VISITED) {
        node->val.anode.children[i] = NULL;
      } else {
        free_tree_reduce (node->val.anode.children[i]);
      }
    break;
  case GP_ALT:
    if (node->val.alt.first->type & GP_VISITED)
      node->val.alt.first = NULL;
    else
      free_tree_reduce (node->val.alt.first);
    if (node->val.alt.second->type & GP_VISITED)
      node->val.alt.second = NULL;
    else
      free_tree_reduce (node->val.alt.second);
    break;
  default: assert ("This should not happen" == NULL);
  }
}

static void free_tree_sweep (struct grammar *g, struct gp_tree_node *node,
                             void (*termcb) (struct gp_term *)) {
  if (node == NULL) return;
  assert (node->type & GP_VISITED);
  enum gp_tree_node_type type = (enum gp_tree_node_type) (node->type & ~GP_VISITED);
  switch (type) {
  case GP_NIL:
  case GP_ERROR: break; /* we don't free empty and error node yet */
  case GP_TERM:
    if (termcb != NULL) termcb (&node->val.term);
    break;
  case GP_ANODE:
    for (int i = 0; i < node->val.anode.children_num; i++)
      if (node->val.anode.children[i] != NULL) free_tree_sweep (g, node->val.anode.children[i], termcb);
    g->parse_free (node->val.anode.children);
    break;
  case GP_ALT:
    free_tree_sweep (g, node->val.alt.first, termcb);
    free_tree_sweep (g, node->val.alt.second, termcb);
    break;
  default: assert ("This should not happen" == NULL);
  }
  g->parse_free (node);
}

void gp_free_tree (struct grammar *g, struct gp_tree_node *root, void (*termcb) (struct gp_term *)) {
  if (root == NULL || g->parse_free == NULL) return;
  /* Since the parse tree is actually a DAG, we must carefully avoid double free errors.
     Therefore, we walk the parse tree twice. On the first walk, we reduce the DAG to an actual
     tree. On the second walk, we recursively free the tree nodes. */
  free_tree_reduce (root);
  free_tree_sweep (g, root, termcb);
}

/* This page contains a test code for Gecko. To use it, define macro GP_TEST during compilation. */

#ifdef GP_TEST

static int nterm; /* the current number of next input grammar terminal */

/* The function imported by Gecko (see comments in the interface file). */
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
  default: return NULL;
  }
}

static int nrule; /* the current number of next rule grammar terminal */

/* The function imported by Gecko (see comments in the interface file). */
const char *read_rule (const char ***rhs, const char **anode, int **transl) {
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

static int ntok; /* the current number of next input token */

/* The function imported by Gecko (see comments in the interface file). */
static int test_read_token (void **attr) {
  const char input[] = "a+a*(a*a+a)";
  // const char input[] = "a+a**(a*a+a)";
  //  const char input[] = "a+a*";
  ntok++;
  *attr = NULL;
  if ((size_t) ntok < sizeof (input)) return input[ntok - 1];
  return -1;
}

/* The following two functions calls Gecko with two different ways of forming grammars. */
static void use_functions (int argc, char **argv) {
  struct grammar *g;
  struct gp_tree_node *root;
  bool ambiguous_p;

  nterm = nrule = 0;
  fprintf (stderr, "Use functions\n");
  if ((g = gp_create_grammar ()) == NULL) {
    fprintf (stderr, "No memory\n");
    exit (1);
  }
  if (argc > 1)
    gp_set_debug_level (g, atoi (argv[2]));
  else
    gp_set_debug_level (g, 3);
  if (argc > 3) gp_set_error_recovery_flag (g, atoi (argv[3]));
  if (gp_read_grammar (g, true, read_terminal, read_rule) != 0) {
    fprintf (stderr, "%s\n", gp_error_message (g));
    exit (1);
  }
  ntok = 0;
  if (gp_parse (g, test_read_token, &root, &ambiguous_p))
    fprintf (stderr, "gecko: %s\n", gp_error_message (g));
  gp_free_tree (g, root, NULL);
  gp_fin (g);
}

static const char *description
  = "\n"
    "TERM;\n"
    "E : T         # 0\n"
    "  | E '+' T   # plus (0 2)\n"
    "  ;\n"
    "T : F         # 0\n"
    "  | T '*' F   # mult (0 2)\n"
    "  ;\n"
    "F : 'a'       # 0\n"
    "  | '(' E ')' # 1\n"
    "  ;\n";

static void use_description (int argc, char **argv) {
  struct grammar *g;
  struct gp_tree_node *root;
  bool ambiguous_p;

  fprintf (stderr, "Use description\n");
  if ((g = gp_create_grammar ()) == NULL) {
    fprintf (stderr, "gp_create_grammar: No memory\n");
    exit (1);
  }
  if (argc > 2)
    gp_set_debug_level (g, atoi (argv[2]));
  else
    gp_set_debug_level (g, 3);
  if (argc > 3) gp_set_error_recovery_flag (g, atoi (argv[3]));
  if (gp_parse_grammar (g, true, description) != 0) {
    fprintf (stderr, "%s\n", gp_error_message (g));
    exit (1);
  }
  if (gp_parse (g, test_read_token, &root, &ambiguous_p))
    fprintf (stderr, "gecko: %s\n", gp_error_message (g));
  gp_free_tree (g, root, NULL);
  gp_fin (g);
}

int main (int argc, char **argv) {
  if (argc <= 1 || atoi (argv[1]))
    use_description (argc, argv);
  else
    use_functions (argc, argv);
  exit (0);
}

#endif /* #ifdef GP_TEST */
