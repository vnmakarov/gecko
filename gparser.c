#include <stdio.h>
#include <stdarg.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

#include "allocate.h"
#include "hashtab.h"
#include "vlobject.h"
#include "objstack.h"
#include "gparser.h"

#ifndef CHAR_BIT
#define CHAR_BIT 8
#endif

#ifndef GP_MAX_ERROR_MESSAGE_LENGTH
#define GP_MAX_ERROR_MESSAGE_LENGTH 200
#endif

/* Prime number (79087987342985798987987) mod 32 used for hash calculations: */
static const unsigned jauquet_prime_mod32 = 2053222611;
static const unsigned hash_shift = 611; /* shift used for hash calculations */

struct symbs {             /* information about grammar vocabulary: */
  int n_terms, n_nonterms; /* number of all symbols and terminals */
  os_t symbs_os;           /* all symbols are placed in this object */
  /* References to the symbols, terminals, nonterminals are stored in the following vlos. The
     indexes in the arrays are the same as corresponding symbol, terminal, and nonterminal numbers.
   */
  vlo_t symbs_vlo, terms_vlo, nonterms_vlo;
  hash_table_t repr_to_symb_tab;      /* table to find symbol by its representation */
  hash_table_t code_to_term_tab;      /* table to find term symbol by its code */
  struct symb **term_code_trans_vect; /* terminal code to terminal symbol vector */
  int term_code_trans_vect_start, term_code_trans_vect_end;
};

struct grammar {    /* major structure which stores information about grammar: */
  bool undefined_p; /* true for undefined or erroneous grammar */
  int error_code;   /* the last occurred error code for given grammar */
  char error_message[GP_MAX_ERROR_MESSAGE_LENGTH
                     + 1];    /* string always containing the last error message */
  struct symb *axiom;         /* grammar axiom (there is only one rule with axiom in lhs) */
  struct symb *end_marker;    /* auxiliary symbol denoting EOF */
  struct symb *term_error;    /* auxiliary symbol used for describing error recovery */
  int term_error_num;         /* and its internal number */
  int recovery_token_matches; /* number of subsequent tokens should be successfuly shifted to finish
                                 error recovery */
  int debug_level;            /* ??? */
  bool one_parse_p;           /* true if we need only one parse */
  bool cost_p;                /* true if we need parse(s) with minimal costs */
  bool error_recovery_p;      /* true if we need to make error recovery. */
  struct symbs *symbs;        /* vocabulary used for this grammar */
  struct rules *rules;        /* rules used for this grammar */
  struct term_sets *term_sets; /* terminal sets used for this grammar */
  gp_allocator_t *alloc;       /* Allocator */
};

static struct grammar *grammar; /* the reference for the current grammar structure */

/* The following is set up the parser and used globally. */
static int (*read_token) (void **attr);
static void (*syntax_error) (int err_tok_num, void *err_tok_attr, int start_ignored_tok_num,
                             void *start_ignored_tok_attr, int start_recovered_tok_num,
                             void *start_recovered_tok_attr);
static void *(*parse_alloc) (int nmemb);
static void (*parse_free) (void *mem);

/* Forward decrlarations: */
static void gp_error (int code, const char *format, ...);

/* The default number of tokens sucessfully matched to stop error recovery alternative (state). */
#define DEFAULT_RECOVERY_TOKEN_MATCHES 3

/* This page is abstract data `grammar symbols'. */

typedef long int term_set_el_t; /* type of element of array representing set of terminals */

struct rule;

struct symb {       /* symbol of the grammar: */
  const char *repr; /* external representation of the symbol */
  union {
    struct {
      int code;     /* code of the terminal symbol */
      int term_num; /* order number of the terminal */
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

static int all_searches, all_collisions;

/* delete_hash_table plus accumulate all_searches and all_collisions. */
static inline void delete_htab_update_statistics (hash_table_t htab) {
  all_searches += get_searches (htab);
  all_collisions += get_collisions (htab);
  delete_hash_table (htab);
}

static unsigned symb_repr_hash (hash_table_entry_t s) { /* return hash of symbol representation */
  unsigned result = jauquet_prime_mod32;
  const char *str = ((struct symb *) s)->repr;
  for (int i = 0; str[i] != '\0'; i++) result = result * hash_shift + (unsigned) str[i];
  return result;
}

/* Equality of symbol representations. */
static bool symb_repr_eq (hash_table_entry_t s1, hash_table_entry_t s2) {
  return strcmp (((struct symb *) s1)->repr, ((struct symb *) s2)->repr) == 0;
}

/* Hash of terminal code. */
static unsigned symb_code_hash (hash_table_entry_t s) {
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
static struct symbs *symb_init (void) {
  void *mem = gp_malloc (grammar->alloc, sizeof (struct symbs));
  struct symbs *result = (struct symbs *) mem;
  OS_CREATE (result->symbs_os, grammar->alloc, 0);
  VLO_CREATE (result->symbs_vlo, grammar->alloc, 1024);
  VLO_CREATE (result->terms_vlo, grammar->alloc, 512);
  VLO_CREATE (result->nonterms_vlo, grammar->alloc, 512);
  result->repr_to_symb_tab = create_hash_table (grammar->alloc, 300, symb_repr_hash, symb_repr_eq);
  result->code_to_term_tab = create_hash_table (grammar->alloc, 200, symb_code_hash, symb_code_eq);
  result->term_code_trans_vect = NULL;
  result->n_nonterms = result->n_terms = 0;
  return result;
}

/* Return symbol (or NULL if it does not exist) whose representation is REPR. */
static struct symb *symb_find_by_repr (const char *repr) {
  struct symb symb;
  symb.repr = repr;
  return (struct symb *) *find_hash_table_entry (grammar->symbs->repr_to_symb_tab, &symb, false);
}

/* Return symbol (or NULL if it does not exist) which is terminal with CODE. */
static inline struct symb *term_find_by_code (int code) {
  struct symbs *symbs = grammar->symbs;
  if (symbs->term_code_trans_vect != NULL) {
    if (code < symbs->term_code_trans_vect_start || code >= symbs->term_code_trans_vect_end)
      return NULL;
    return symbs->term_code_trans_vect[code - symbs->term_code_trans_vect_start];
  }
  struct symb symb;
  symb.term_p = true;
  symb.u.term.code = code;
  return (struct symb *) *find_hash_table_entry (symbs->code_to_term_tab, &symb, false);
}

/* Create new terminal symbol and return reference for it. The symbol should be not in the tables.
   The function should create own copy of name for the new symbol. */
static struct symb *symb_add_term (const char *name, int code) {
  struct symb symb, *result;
  hash_table_entry_t *repr_entry, *code_entry;

  symb.repr = name;
  symb.term_p = true;
  symb.num = grammar->symbs->n_nonterms + grammar->symbs->n_terms;
  symb.u.term.code = code;
  symb.u.term.term_num = grammar->symbs->n_terms++;
  symb.empty_p = false;
  repr_entry = find_hash_table_entry (grammar->symbs->repr_to_symb_tab, &symb, true);
  assert (*repr_entry == NULL);
  code_entry = find_hash_table_entry (grammar->symbs->code_to_term_tab, &symb, true);
  assert (*code_entry == NULL);
  OS_TOP_ADD_STRING (grammar->symbs->symbs_os, name);
  symb.repr = (char *) OS_TOP_BEGIN (grammar->symbs->symbs_os);
  OS_TOP_FINISH (grammar->symbs->symbs_os);
  OS_TOP_ADD_MEMORY (grammar->symbs->symbs_os, &symb, sizeof (struct symb));
  result = (struct symb *) OS_TOP_BEGIN (grammar->symbs->symbs_os);
  OS_TOP_FINISH (grammar->symbs->symbs_os);
  *repr_entry = (hash_table_entry_t) result;
  *code_entry = (hash_table_entry_t) result;
  VLO_ADD_MEMORY (grammar->symbs->symbs_vlo, &result, sizeof (struct symb *));
  VLO_ADD_MEMORY (grammar->symbs->terms_vlo, &result, sizeof (struct symb *));
  return result;
}

/* Create new nonterminal symbol and return reference for it. The symbol should be
   not in the table. The function should create own copy of name for the new symbol. */
static struct symb *symb_add_nonterm (const char *name) {
  struct symb symb;
  symb.repr = name;
  symb.term_p = false;
  symb.num = grammar->symbs->n_nonterms + grammar->symbs->n_terms;
  symb.u.nonterm.rules = NULL;
  symb.u.nonterm.loop_p = 0;
  symb.u.nonterm.nonterm_num = grammar->symbs->n_nonterms++;
  hash_table_entry_t *entry = find_hash_table_entry (grammar->symbs->repr_to_symb_tab, &symb, true);
  assert (*entry == NULL);
  OS_TOP_ADD_STRING (grammar->symbs->symbs_os, name);
  symb.repr = (char *) OS_TOP_BEGIN (grammar->symbs->symbs_os);
  OS_TOP_FINISH (grammar->symbs->symbs_os);
  OS_TOP_ADD_MEMORY (grammar->symbs->symbs_os, &symb, sizeof (struct symb));
  struct symb *result = (struct symb *) OS_TOP_BEGIN (grammar->symbs->symbs_os);
  OS_TOP_FINISH (grammar->symbs->symbs_os);
  *entry = (hash_table_entry_t) result;
  VLO_ADD_MEMORY (grammar->symbs->symbs_vlo, &result, sizeof (struct symb *));
  VLO_ADD_MEMORY (grammar->symbs->nonterms_vlo, &result, sizeof (struct symb *));
  return result;
}

static struct symb *symb_get (int n) { /* return N-th symbol (if any) or NULL otherwise */
  if (n < 0 || (VLO_LENGTH (grammar->symbs->symbs_vlo) / sizeof (struct symb *) <= (size_t) n))
    return NULL;
  struct symb *symb = ((struct symb **) VLO_BEGIN (grammar->symbs->symbs_vlo))[n];
  assert (symb->num == n);
  return symb;
}

static struct symb *term_get (int n) { /* return N-th symbol (if any) or NULL otherwise */
  if (n < 0 || (VLO_LENGTH (grammar->symbs->terms_vlo) / sizeof (struct symb *) <= (size_t) n))
    return NULL;
  struct symb *symb = ((struct symb **) VLO_BEGIN (grammar->symbs->terms_vlo))[n];
  assert (symb->term_p && symb->u.term.term_num == n);
  return symb;
}

static struct symb *nonterm_get (int n) { /* return N-th symbol (if any) or NULL otherwise */
  if (n < 0 || (VLO_LENGTH (grammar->symbs->nonterms_vlo) / sizeof (struct symb *) <= (size_t) n))
    return NULL;
  struct symb *symb = ((struct symb **) VLO_BEGIN (grammar->symbs->nonterms_vlo))[n];
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

static void symb_finish_adding_terms (void) {
  int i, max_code, min_code;
  struct symb *symb;
  for (min_code = max_code = i = 0; (symb = term_get (i)) != NULL; i++) {
    if (i == 0 || min_code > symb->u.term.code) min_code = symb->u.term.code;
    if (i == 0 || max_code < symb->u.term.code) max_code = symb->u.term.code;
  }
  assert (i != 0);
  if (max_code - min_code >= TERM_CODE_TRANS_VECT_SIZE) {
    gp_error (GP_TOO_WIDE_TERM_RANGE_CODE, "term code range is more %d", TERM_CODE_TRANS_VECT_SIZE);
  } else {
    grammar->symbs->term_code_trans_vect_start = min_code;
    grammar->symbs->term_code_trans_vect_end = max_code + 1;
    void *mem = gp_malloc (grammar->alloc, sizeof (struct symb *) * (max_code - min_code + 1));
    grammar->symbs->term_code_trans_vect = (struct symb **) mem;
    for (i = 0; (symb = term_get (i)) != NULL; i++)
      grammar->symbs->term_code_trans_vect[symb->u.term.code - min_code] = symb;
  }
}

static void symb_empty (struct symbs *symbs) { /* free memory for symbols */
  if (symbs == NULL) return;
  if (grammar->symbs->term_code_trans_vect != NULL) {
    gp_free (grammar->alloc, grammar->symbs->term_code_trans_vect);
    grammar->symbs->term_code_trans_vect = NULL;
  }
  empty_hash_table (symbs->repr_to_symb_tab);
  empty_hash_table (symbs->code_to_term_tab);
  VLO_NULLIFY (symbs->nonterms_vlo);
  VLO_NULLIFY (symbs->terms_vlo);
  VLO_NULLIFY (symbs->symbs_vlo);
  OS_EMPTY (symbs->symbs_os);
  symbs->n_nonterms = symbs->n_terms = 0;
}

/* Finalize work with symbols. */
static void symb_fin (struct symbs *symbs) {
  if (symbs == NULL) return;
  if (grammar->symbs->term_code_trans_vect != NULL)
    gp_free (grammar->alloc, grammar->symbs->term_code_trans_vect);
  delete_hash_table (grammar->symbs->repr_to_symb_tab);
  delete_hash_table (grammar->symbs->code_to_term_tab);
  VLO_DELETE (grammar->symbs->nonterms_vlo);
  VLO_DELETE (grammar->symbs->terms_vlo);
  VLO_DELETE (grammar->symbs->symbs_vlo);
  OS_DELETE (grammar->symbs->symbs_os);
  gp_free (grammar->alloc, symbs);
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
static struct term_sets *term_set_init (void) {
  void *mem = gp_malloc (grammar->alloc, sizeof (struct term_sets));
  struct term_sets *result = (struct term_sets *) mem;
  OS_CREATE (result->term_set_os, grammar->alloc, 0);
  VLO_CREATE (result->tab_term_set_vlo, grammar->alloc, 4096);
  result->n_term_sets = result->n_term_sets_size = 0;
  return result;
}

/* Return new terminal SET. Its value is undefined. */
static term_set_el_t *term_set_create (void) {
  assert (sizeof (term_set_el_t) <= 8);
  /* Make it 64 bit multiple to have the same statistics for 64 bit machines. */
  int size = ((grammar->symbs->n_terms + CHAR_BIT * 8 - 1) / (CHAR_BIT * 8)) * 8;
  OS_TOP_EXPAND (grammar->term_sets->term_set_os, size);
  term_set_el_t *result = (term_set_el_t *) OS_TOP_BEGIN (grammar->term_sets->term_set_os);
  OS_TOP_FINISH (grammar->term_sets->term_set_os);
  grammar->term_sets->n_term_sets++;
  grammar->term_sets->n_term_sets_size += size;
  return result;
}

static inline void term_set_clear (term_set_el_t *set) { /* make terminal SET empty: */
  int size = (grammar->symbs->n_terms + TERM_SET_EL_BITS - 1) / (TERM_SET_EL_BITS);
  term_set_el_t *bound = set + size;
  while (set < bound) *set++ = 0;
}

static inline void term_set_copy (term_set_el_t *dest,
                                  term_set_el_t *src) { /* copy SRC into DEST */
  int size = (grammar->symbs->n_terms + TERM_SET_EL_BITS - 1) / (TERM_SET_EL_BITS);
  term_set_el_t *bound = dest + size;
  while (dest < bound) *dest++ = *src++;
}

/* Add all terminals from set OP with to SET. Return true if SET has been changed. */
static inline bool term_set_or (term_set_el_t *set, term_set_el_t *op) {
  term_set_el_t *bound;
  int size = (grammar->symbs->n_terms + TERM_SET_EL_BITS - 1) / (TERM_SET_EL_BITS);
  bound = set + size;
  bool changed_p = false;
  while (set < bound) {
    if ((*set | *op) != *set) changed_p = true;
    *set++ |= *op++;
  }
  return changed_p;
}

/* Add terminal with number NUM to SET. Return true if SET has been changed. */
static inline bool term_set_up (term_set_el_t *set, int num) {
  assert (num < grammar->symbs->n_terms);
  int ind = num / TERM_SET_EL_BITS;
  term_set_el_t bit = ((term_set_el_t) 1) << (num % TERM_SET_EL_BITS);
  bool changed_p = (set[ind] & bit) == 0;
  set[ind] |= bit;
  return changed_p;
}

/* Return true if terminal with number NUM is in SET. */
static inline int term_set_test (term_set_el_t *set, int num) {
  assert (num >= 0 && num < grammar->symbs->n_terms);
  int ind = num / TERM_SET_EL_BITS;
  term_set_el_t bit = ((term_set_el_t) 1) << (num % TERM_SET_EL_BITS);
  return (set[ind] & bit) != 0;
}

/* Return set which is in the table with number NUM. */
static inline term_set_el_t *term_set_from_table (int num) {
  assert (num < (int) (VLO_LENGTH (grammar->term_sets->tab_term_set_vlo)
                       / sizeof (struct tab_term_set *)));
  return ((struct tab_term_set **) VLO_BEGIN (grammar->term_sets->tab_term_set_vlo))[num]->set;
}

static void term_set_print (FILE *f, term_set_el_t *set) { /* print terminal SET into file F */
  for (int i = 0; i < grammar->symbs->n_terms; i++)
    if (term_set_test (set, i)) {
      fprintf (f, " ");
      symb_print (f, term_get (i), false);
    }
}

static void term_set_empty (struct term_sets *term_sets) { /* free memory for terminal sets */
  if (term_sets == NULL) return;
  VLO_NULLIFY (term_sets->tab_term_set_vlo);
  OS_EMPTY (term_sets->term_set_os);
  term_sets->n_term_sets = term_sets->n_term_sets_size = 0;
}

static void term_set_fin (struct term_sets *term_sets) { /* finalize work with terminal sets */
  if (term_sets == NULL) return;
  VLO_DELETE (term_sets->tab_term_set_vlo);
  OS_DELETE (term_sets->term_set_os);
  gp_free (grammar->alloc, term_sets);
  term_sets = NULL;
}

/* This page is abstract data `grammar rules'. */

struct rule {            /* rule of the grammar: */
  int num;               /* order number of rule */
  int rhs_len;           /* length of rhs */
  struct rule *next;     /* the next grammar rule */
  struct rule *lhs_next; /* the next grammar rule with the same nonterminal in the rule lhs */
  struct symb *lhs;      /* nonterminal in the left hand side of the rule */
  struct symb **rhs;     /* symbols in the right hand side of the rule */
  /* The following three members define rule translation: */
  const char *anode; /* abstract node name if any */
  int anode_cost;    /* the cost of the abstract node if any, otherwise 0 */
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
  os_t rules_os;           /* all rules are placed in this object: */
};

/* Initialize work with rules and returns pointer to rules storage. */
static struct rules *rule_init (void) {
  void *mem;
  struct rules *result;

  mem = gp_malloc (grammar->alloc, sizeof (struct rules));
  result = (struct rules *) mem;
  OS_CREATE (result->rules_os, grammar->alloc, 0);
  result->first_rule = result->curr_rule = NULL;
  result->n_rules = result->n_rhs_lens = 0;
  return result;
}

/* Create new rule with LHS empty rhs. */
static struct rule *rule_new_start (struct symb *lhs, const char *anode, int anode_cost) {
  struct rule *rule;
  struct symb *empty;

  assert (!lhs->term_p);
  OS_TOP_EXPAND (grammar->rules->rules_os, sizeof (struct rule));
  rule = (struct rule *) OS_TOP_BEGIN (grammar->rules->rules_os);
  OS_TOP_FINISH (grammar->rules->rules_os);
  rule->lhs = lhs;
  if (anode == NULL) {
    rule->anode = NULL;
    rule->anode_cost = 0;
  } else {
    OS_TOP_ADD_STRING (grammar->rules->rules_os, anode);
    rule->anode = (char *) OS_TOP_BEGIN (grammar->rules->rules_os);
    OS_TOP_FINISH (grammar->rules->rules_os);
    rule->anode_cost = anode_cost;
  }
  rule->trans_len = 0;
  rule->order = NULL;
  rule->next = NULL;
  if (grammar->rules->curr_rule != NULL) grammar->rules->curr_rule->next = rule;
  rule->lhs_next = lhs->u.nonterm.rules;
  lhs->u.nonterm.rules = rule;
  rule->rhs_len = 0;
  empty = NULL;
  OS_TOP_ADD_MEMORY (grammar->rules->rules_os, &empty, sizeof (struct symb *));
  rule->rhs = (struct symb **) OS_TOP_BEGIN (grammar->rules->rules_os);
  grammar->rules->curr_rule = rule;
  if (grammar->rules->first_rule == NULL) grammar->rules->first_rule = rule;
  rule->rule_start_offset = grammar->rules->n_rhs_lens + grammar->rules->n_rules;
  rule->num = grammar->rules->n_rules++;
  return rule;
}

/* Add SYMB at the end of current rule rhs. */
static void rule_new_symb_add (struct symb *symb) {
  struct symb *empty;

  empty = NULL;
  OS_TOP_ADD_MEMORY (grammar->rules->rules_os, &empty, sizeof (struct symb *));
  grammar->rules->curr_rule->rhs = (struct symb **) OS_TOP_BEGIN (grammar->rules->rules_os);
  grammar->rules->curr_rule->rhs[grammar->rules->curr_rule->rhs_len] = symb;
  grammar->rules->curr_rule->rhs_len++;
  grammar->rules->n_rhs_lens++;
}

/* Create and initialize situation cache (it should be called at end of forming each rule).  */
static void rule_new_stop (void) {
  int i;

  OS_TOP_FINISH (grammar->rules->rules_os);
  OS_TOP_EXPAND (grammar->rules->rules_os, grammar->rules->curr_rule->rhs_len * sizeof (int));
  grammar->rules->curr_rule->order = (int *) OS_TOP_BEGIN (grammar->rules->rules_os);
  OS_TOP_FINISH (grammar->rules->rules_os);
  for (i = 0; i < grammar->rules->curr_rule->rhs_len; i++) grammar->rules->curr_rule->order[i] = -1;
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
    if (rule->anode != NULL) fprintf (f, "%s (", rule->anode);
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

/* Free memory for rules. */
static void rule_empty (struct rules *rules) {
  if (rules == NULL) return;
  OS_EMPTY (rules->rules_os);
  rules->first_rule = rules->curr_rule = NULL;
  rules->n_rules = rules->n_rhs_lens = 0;
}

/* Finalize work with rules. */
static void rule_fin (struct rules *rules) {
  if (rules == NULL) return;
  OS_DELETE (rules->rules_os);
  gp_free (grammar->alloc, rules);
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

/* vlo is array which is indexed by situation number (sit->rule->rule_start_offset + sit->pos): */
static vlo_t sit_table_vlo;

static struct sit **sit_table; /* the above vlo as array: */
static os_t sits_os;           /* all situations are placed in the object */
static int n_all_sits;         /* current number of unique situations */

static void sit_init (void) { /* Initialize work with situations: */
  n_all_sits = 0;
  OS_CREATE (sits_os, grammar->alloc, 0);
  VLO_CREATE (sit_table_vlo, grammar->alloc, 4096);
  sit_table = (struct sit **) VLO_BEGIN (sit_table_vlo);
}

/* Set up lookahead of situation SIT. Returns true if the situation tail may derive empty string. */
static bool sit_set_lookahead (struct sit *sit) {
  struct symb *symb, **symb_ptr;

  sit->lookahead = term_set_create ();
  term_set_clear (sit->lookahead);
  symb_ptr = &sit->rule->rhs[sit->pos];
  while ((symb = *symb_ptr) != NULL) {
    if (symb->term_p)
      term_set_up (sit->lookahead, symb->u.term.term_num);
    else
      term_set_or (sit->lookahead, symb->u.nonterm.first);
    if (!symb->empty_p) break;
    symb_ptr++;
  }
  if (symb != NULL) return false;
  term_set_or (sit->lookahead, sit->rule->lhs->u.nonterm.follow);
  return true;
}

/* Return situations with given characteristics. Remember that situations are stored in one
 * exemplar. */
static inline struct sit *sit_create (struct rule *rule, int pos) {
  struct sit *sit;
  int diff;

  if ((diff
       = (char *) (sit_table + rule->rule_start_offset + pos) - (char *) VLO_BOUND (sit_table_vlo))
      >= 0) {
    diff += sizeof (struct sit *);
    VLO_EXPAND (sit_table_vlo, diff);
    sit_table = (struct sit **) VLO_BEGIN (sit_table_vlo);
    struct sit **bound = (struct sit **) VLO_BOUND (sit_table_vlo);
    for (struct sit **ptr = bound - diff / sizeof (struct sit *); ptr < bound; ptr++) *ptr = NULL;
  }
  if ((sit = sit_table[rule->rule_start_offset + pos]) != NULL) return sit;
  OS_TOP_EXPAND (sits_os, sizeof (struct sit));
  sit = (struct sit *) OS_TOP_BEGIN (sits_os);
  OS_TOP_FINISH (sits_os);
  n_all_sits++;
  sit->rule = rule;
  sit->pos = pos;
  sit->sit_number = n_all_sits;
  sit->empty_tail_p = sit_set_lookahead (sit);
  sit_table[rule->rule_start_offset + pos] = sit;
  return sit;
}

#ifndef NO_GP_DEBUG_PRINT

static void sit_print (FILE *f, struct sit *sit) { /* print situation SIT to file F: */
  fprintf (f, "%3d ", sit->sit_number);
  rule_dot_print (f, sit->rule, sit->pos);
}

#endif /* #ifndef NO_GP_DEBUG_PRINT */

/* Return hash of sequence of N_SITS situations in array SITS. */
static unsigned sits_hash (int n_sits, struct sit **sits) {
  int n, i;
  unsigned result;

  result = jauquet_prime_mod32;
  for (i = 0; i < n_sits; i++) {
    n = sits[i]->sit_number;
    result = result * hash_shift + n;
  }
  return result;
}

/* Finalize work with situations. */
static void sit_fin (void) {
  VLO_DELETE (sit_table_vlo);
  OS_DELETE (sits_os);
}

struct action_desc {
  unsigned short actions_num;   /* number of actions for given nonterm */
  unsigned short actions_start; /* index of first term action, defined for actions_num != 0 */
};

struct action {
  bool shift_p : 1;
  int term_num : 31;
  union {
    struct set *set;   /* shift set */
    struct rule *rule; /* reduce */
  } u;
};

/* This page is abstract data `sets'. */

typedef unsigned short trans_el_t;
struct set {                      /* the grammar state: */
  int num;                        /* unique number of the state */
  struct symb *symb;              /* symb shifting which resulted into this state */
  int n_start_sits, n_sits;       /* numbers of (start) situations in the following array */
  struct sit **sits;              /* array of situation */
  struct set **goto_map;          /* map nonterm -> goto set */
  struct action_desc *action_map; /* map term -> actiotn desc */
  struct action *actions;         /* action number -> action */
};

/* The set being created. It is defined only when new_set_ready_p is true. */
static struct set *new_set;

/* The following says that new_set its members are defined. Before this the access
   to data of the set being formed are possible only through the following variables. */
static bool new_set_ready_p;

/* To optimize code we use the following variables to access to data of new set. They are always
   defined and correspondingly situations and the current number of start situations of
   the set being formed. */
static struct sit **new_sits;
static int new_n_start_sits;

static int n_sets, n_sets_start_sits;         /* # of unique sets and their start situations */
static int n_goto_vects, n_goto_vect_len;     /* goto vects and their length */
static int n_actions;                         /* actions number*/
static int n_action_vects, n_action_vect_len; /* action vects and their length */
static os_t set_sits_os;                      /* container of situations of being formed sets */
static vlo_t sets_vlo;                        /* map: set num -> set */
static os_t sets_os;                          /* container of sets */

static hash_table_t set_tab; /* set table: key is only start situations */

/* Hash of set. */
static unsigned set_hash (hash_table_entry_t s) {
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

static void set_init (void) { /* initialize work with sets: */
  OS_CREATE (set_sits_os, grammar->alloc, 2048);
  VLO_CREATE (sets_vlo, grammar->alloc, 0);
  OS_CREATE (sets_os, grammar->alloc, 0);
  set_tab = create_hash_table (grammar->alloc, 8192, set_hash, set_eq);
  n_sets = n_sets_start_sits = 0;
  n_goto_vects = n_goto_vect_len = n_actions = n_action_vects = n_action_vect_len = 0;
}

static struct action *set_get_actions (struct set *set, int term, int *actions_num) {
  assert (term >= 0 && term < grammar->symbs->n_terms);
  *actions_num = 0;
  if (set->actions == NULL) return NULL;
  *actions_num = set->action_map[term].actions_num;
  return &set->actions[set->action_map[term].actions_start];
}

static inline void set_new_set_start (void) { /* start forming of new set: */
  new_set = NULL;
  new_set_ready_p = false;
  new_n_start_sits = 0;
  new_sits = NULL;
}

/* Add start SIT at the end of the situation array of the set being formed: */
static inline void set_new_add_start_sit (struct sit *sit) {
  assert (!new_set_ready_p);
  OS_TOP_EXPAND (set_sits_os, sizeof (struct sit *));
  new_sits = (struct sit **) OS_TOP_BEGIN (set_sits_os);
  new_sits[new_n_start_sits] = sit;
  new_n_start_sits++;
}

/* Add nonstart SIT (if it is not there yet) at the end of array of the new situations. */
static inline void set_new_add_nonstart_sit (struct sit *sit) {
  assert (new_set_ready_p);
  /* When we add non-start situations we need to have situations w/o duplicates. */
  for (int i = new_n_start_sits; i < new_set->n_sits; i++)
    if (new_sits[i] == sit) return;
  OS_TOP_EXPAND (set_sits_os, sizeof (struct sit *));
  new_sits = new_set->sits = (struct sit **) OS_TOP_BEGIN (set_sits_os);
  new_sits[new_set->n_sits++] = sit;
}

/* The new set should contain only start situations.  Insert set into the set table new_set will be
   set to the table set. If the function returns true then there was no such table set yet. */
static bool set_insert (void) {
  OS_TOP_EXPAND (sets_os, sizeof (struct set));
  new_set = (struct set *) OS_TOP_BEGIN (sets_os);
  new_set->n_start_sits = new_n_start_sits;
  new_set->sits = new_sits;
  new_set_ready_p = true;
  /* Insert set into table: */
  hash_table_entry_t *entry = find_hash_table_entry (set_tab, new_set, true);
  if (*entry != NULL) {
    OS_TOP_NULLIFY (sets_os);
    new_set = (struct set *) *entry;
    new_sits = new_set->sits;
    OS_TOP_NULLIFY (set_sits_os);
    return false;
  }
  OS_TOP_FINISH (sets_os);
  VLO_ADD_MEMORY (sets_vlo, &new_set, sizeof (new_set));
  new_set->num = n_sets++;
  assert (n_sets == (int) (VLO_LENGTH (sets_vlo) / sizeof (struct set *)));
  new_set->goto_map = NULL;
  new_set->action_map = NULL;
  new_set->actions = NULL;
  new_set->n_sits = new_n_start_sits;
  *entry = (hash_table_entry_t) new_set;
  n_sets_start_sits += new_n_start_sits;
  return true;
}

static inline void set_new_set_stop (void) { /* finish work with set being formed: */
  OS_TOP_FINISH (set_sits_os);
}

static void *set_calloc (size_t size) { /* allocate and data data in set os */
  OS_TOP_EXPAND (sets_os, size);
  void *res = (struct set *) OS_TOP_BEGIN (sets_os);
  OS_TOP_FINISH (sets_os);
  memset (res, 0, size);
  return res;
}

#ifndef NO_GP_DEBUG_PRINT

/* Print SET to file F. If NONSTART_P is true then print all situations. The situations are printed
   with the lookahead set if LOOKAHEAD_P. */
static void set_print (FILE *f, struct set *set, bool nonstart_p) {
  int num, n_start_sits, n_sits;
  struct sit **sits;

  if (set == NULL && !new_set_ready_p) {
    /* The following is necessary if we call the function from a debugger. In this case new_set,
       and their members may be not set up yet. */
    num = -1;
    n_start_sits = n_sits = new_n_start_sits;
    sits = new_sits;
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

static void set_fin (void) { /* finalize work with sets: */
  delete_htab_update_statistics (set_tab);
  OS_DELETE (sets_os);
  VLO_DELETE (sets_vlo);
  OS_DELETE (set_sits_os);
}

/* Jump buffer for processing errors. */
static jmp_buf error_longjump_buff;

/* Store error CODE and message. The function makes long jump after that. */
static void gp_error (int code, const char *format, ...) {
  va_list arguments;

  grammar->error_code = code;
  va_start (arguments, format);
  vsprintf (grammar->error_message, format, arguments);
  va_end (arguments);
  assert (strlen (grammar->error_message) < GP_MAX_ERROR_MESSAGE_LENGTH);
  longjmp (error_longjump_buff, code);
}

/* Process allocation errors. */
static void error_func_for_allocate (void *ignored) {
  (void) ignored;
  gp_error (GP_NO_MEMORY, "no memory");
}

/* Allocate memory for new grammar. */
struct grammar *gp_create_grammar (void) {
  gp_allocator_t *allocator;

  allocator = gp_alloc_new (NULL, NULL, NULL, NULL);
  if (allocator == NULL) {
    return NULL;
  }
  grammar = NULL;
  grammar = (struct grammar *) gp_malloc (allocator, sizeof (*grammar));
  if (grammar == NULL) {
    gp_alloc_del (allocator);
    return NULL;
  }
  grammar->alloc = allocator;
  gp_alloc_seterr (allocator, error_func_for_allocate, gp_alloc_getuserptr (allocator));
  if (setjmp (error_longjump_buff) != 0) {
    gp_free_grammar (grammar);
    return NULL;
  }
  grammar->undefined_p = true;
  grammar->error_code = 0;
  *grammar->error_message = '\0';
  grammar->debug_level = 0;
  grammar->one_parse_p = 1;
  grammar->cost_p = 0;
  grammar->error_recovery_p = 1;
  grammar->recovery_token_matches = DEFAULT_RECOVERY_TOKEN_MATCHES;
  grammar->symbs = NULL;
  grammar->term_sets = NULL;
  grammar->rules = NULL;
  grammar->symbs = symb_init ();
  grammar->term_sets = term_set_init ();
  grammar->rules = rule_init ();
  return grammar;
}

/* Make grammar empty. */
static void gp_empty_grammar (void) {
  if (grammar != NULL) {
    rule_empty (grammar->rules);
    term_set_empty (grammar->term_sets);
    symb_empty (grammar->symbs);
  }
}

/* Return the last occurred error code for given grammar. */
int gp_error_code (struct grammar *g) {
  assert (g != NULL);
  return g->error_code;
}

/* Return message are always contains error message corresponding to the last occurred error code.
 */
const char *gp_error_message (struct grammar *g) {
  assert (g != NULL);
  return g->error_message;
}

/* Create sets FIRST and FOLLOW for all grammar nonterminals. */
static void create_first_follow_sets (void) {
  struct symb *symb;
  for (int i = 0; (symb = nonterm_get (i)) != NULL; i++) {
    symb->u.nonterm.first = term_set_create ();
    term_set_clear (symb->u.nonterm.first);
    symb->u.nonterm.follow = term_set_create ();
    term_set_clear (symb->u.nonterm.follow);
  }
  bool changed_p;
  do {
    changed_p = false;
    for (int i = 0; (symb = nonterm_get (i)) != NULL; i++)
      for (struct rule *rule = symb->u.nonterm.rules; rule != NULL; rule = rule->lhs_next) {
        bool first_continue_p = true;
        struct symb **rhs = rule->rhs;
        int rhs_len = rule->rhs_len;
        for (int j = 0; j < rhs_len; j++) {
          struct symb *rhs_symb = rhs[j];
          if (rhs_symb->term_p) {
            if (first_continue_p && term_set_up (symb->u.nonterm.first, rhs_symb->u.term.term_num))
              changed_p = true;
          } else {
            if (first_continue_p && term_set_or (symb->u.nonterm.first, rhs_symb->u.nonterm.first))
              changed_p = true;
            int k;
            for (k = j + 1; k < rhs_len; k++) {
              struct symb *next_rhs_symb = rhs[k];
              if (next_rhs_symb->term_p
                  && term_set_up (rhs_symb->u.nonterm.follow, next_rhs_symb->u.term.term_num))
                changed_p = true;
              else if (!next_rhs_symb->term_p
                       && term_set_or (rhs_symb->u.nonterm.follow, next_rhs_symb->u.nonterm.first))
                changed_p = true;
              if (!next_rhs_symb->empty_p) break;
            }
            if (k == rhs_len && term_set_or (rhs_symb->u.nonterm.follow, symb->u.nonterm.follow))
              changed_p = true;
          }
          if (!rhs_symb->empty_p) first_continue_p = false;
        }
      }
  } while (changed_p);
}

/* Set up flags empty_p, access_p and derivation_p for all grammar symbols. */
static void set_empty_access_derives (void) {
  struct symb *symb;
  for (int i = 0; (symb = symb_get (i)) != NULL; i++) {
    symb->empty_p = false;
    symb->derivation_p = symb->term_p;
    symb->access_p = false;
  }
  grammar->axiom->access_p = true;
  bool empty_changed_p, derivation_changed_p, accessibility_change_p;
  do {
    empty_changed_p = derivation_changed_p = accessibility_change_p = false;
    for (int i = 0; (symb = nonterm_get (i)) != NULL; i++)
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

static void set_loop_p (void) { /* set up flags loop_p for nonterminals: */
  struct symb *symb;
  /* Initialize accoding to minimal criteria: There is a rule in which the nonterminal stands and
     all the rest symbols can derive empty strings. */
  for (struct rule *rule = grammar->rules->first_rule; rule != NULL; rule = rule->next)
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
    for (int i = 0; (lhs = nonterm_get (i)) != NULL; i++)
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
static void check_grammar (int strict_p) {
  struct symb *symb;
  set_empty_access_derives ();
  set_loop_p ();
  if (strict_p) {
    for (int i = 0; (symb = nonterm_get (i)) != NULL; i++) {
      if (!symb->derivation_p)
        gp_error (GP_NONTERM_DERIVATION, "nonterm `%s' does not derive any term string",
                  symb->repr);
      else if (!symb->access_p)
        gp_error (GP_UNACCESSIBLE_NONTERM, "nonterm `%s' is not accessible from axiom", symb->repr);
    }
  } else if (!grammar->axiom->derivation_p)
    gp_error (GP_NONTERM_DERIVATION, "nonterm `%s' does not derive any term string",
              grammar->axiom->repr);
  for (int i = 0; (symb = nonterm_get (i)) != NULL; i++)
    if (symb->u.nonterm.loop_p)
      gp_error (GP_LOOP_NONTERM, "nonterm `%s' can derive only itself (grammar with loops)",
                symb->repr);
  /* We should have correct flags empty_p here. */
  create_first_follow_sets ();
}

/* Names of additional symbols. Don't use them in grammars. */
#define AXIOM_NAME "$S"
#define END_MARKER_NAME "$eof"
#define TERM_ERROR_NAME "error"

/* They should be negative. */
#define END_MARKER_CODE -1
#define TERM_ERROR_CODE -2

/* Read terminals/rules. Return error code or 0. Return pointer in G to the grammar. */
int gp_read_grammar (struct grammar *g, bool strict_p, const char *(*read_terminal) (int *code),
                     const char *(*read_rule) (const char ***rhs, const char **abs_node,
                                               int *anode_cost, int **transl)) {
  struct symb *symb;
  assert (g != NULL);
  grammar = g;
  int code;
  if ((code = setjmp (error_longjump_buff)) != 0) return code;
  if (!grammar->undefined_p) gp_empty_grammar ();
  const char *name;
  while ((name = (*read_terminal) (&code)) != NULL) {
    if (code < 0) gp_error (GP_NEGATIVE_TERM_CODE, "term `%s' has negative code", name);
    symb = symb_find_by_repr (name);
    if (symb != NULL) gp_error (GP_REPEATED_TERM_DECL, "repeated declaration of term `%s'", name);
    if (term_find_by_code (code) != NULL)
      gp_error (GP_REPEATED_TERM_CODE, "repeated code %d in term `%s'", code, name);
    symb_add_term (name, code);
  }
  /* Adding error symbol. */
  if (symb_find_by_repr (TERM_ERROR_NAME) != NULL)
    gp_error (GP_FIXED_NAME_USAGE, "do not use fixed name `%s'", TERM_ERROR_NAME);
  if (term_find_by_code (TERM_ERROR_CODE) != NULL) abort ();
  grammar->term_error = symb_add_term (TERM_ERROR_NAME, TERM_ERROR_CODE);
  grammar->term_error_num = grammar->term_error->u.term.term_num;
  grammar->axiom = grammar->end_marker = NULL;
  const char *lhs, **rhs, *anode;
  int anode_cost, *transl;
  struct rule *rule;
  struct symb *start;
  while ((lhs = (*read_rule) (&rhs, &anode, &anode_cost, &transl)) != NULL) {
    symb = symb_find_by_repr (lhs);
    if (symb == NULL)
      symb = symb_add_nonterm (lhs);
    else if (symb->term_p)
      gp_error (GP_TERM_IN_RULE_LHS, "term `%s' in the left hand side of rule", lhs);
    if (anode == NULL && transl != NULL && *transl >= 0 && transl[1] >= 0)
      gp_error (GP_INCORRECT_TRANSLATION, "rule for `%s' has incorrect translation", lhs);
    if (anode != NULL && anode_cost < 0)
      gp_error (GP_NEGATIVE_COST, "translation for `%s' has negative cost", lhs);
    if (grammar->axiom == NULL) {
      /* We made this here becuase we want that the start rule has number 0. */
      /* Add axiom and end marker. */
      start = symb;
      grammar->axiom = symb_find_by_repr (AXIOM_NAME);
      if (grammar->axiom != NULL)
        gp_error (GP_FIXED_NAME_USAGE, "do not use fixed name `%s'", AXIOM_NAME);
      grammar->axiom = symb_add_nonterm (AXIOM_NAME);
      grammar->end_marker = symb_find_by_repr (END_MARKER_NAME);
      if (grammar->end_marker != NULL)
        gp_error (GP_FIXED_NAME_USAGE, "do not use fixed name `%s'", END_MARKER_NAME);
      if (term_find_by_code (END_MARKER_CODE) != NULL) abort ();
      grammar->end_marker = symb_add_term (END_MARKER_NAME, END_MARKER_CODE);
      /* Add rules for start */
      rule = rule_new_start (grammar->axiom, NULL, 0);
      rule_new_symb_add (symb);
      rule_new_symb_add (grammar->end_marker);
      rule_new_stop ();
      rule->order[0] = 0;
      rule->trans_len = 1;
    }
    rule = rule_new_start (symb, anode, (anode != NULL ? anode_cost : 0));
    while (*rhs != NULL) {
      symb = symb_find_by_repr (*rhs);
      if (symb == NULL) symb = symb_add_nonterm (*rhs);
      rule_new_symb_add (symb);
      rhs++;
    }
    rule_new_stop ();
    if (transl != NULL) {
      int i, el;
      for (i = 0; (el = transl[i]) >= 0; i++)
        if (el >= rule->rhs_len) {
          if (el != GP_NIL_TRANSLATION_NUMBER)
            gp_error (GP_INCORRECT_SYMBOL_NUMBER,
                      "translation symbol number %d in rule for `%s' is out of range", el, lhs);
          else
            rule->trans_len++;
        } else if (rule->order[el] >= 0)
          gp_error (GP_REPEATED_SYMBOL_NUMBER,
                    "repeated translation symbol number %d in rule for `%s'", el, lhs);
        else {
          rule->order[el] = i;
          rule->trans_len++;
        }
      assert (i < rule->rhs_len || transl[i] < 0);
    }
  }
  if (grammar->axiom == NULL) gp_error (GP_NO_RULES, "grammar does not contains rules");
  assert (start != NULL);
  /* Adding axiom : error $eof if it is neccessary. */
  for (rule = start->u.nonterm.rules; rule != NULL; rule = rule->lhs_next)
    if (rule->rhs[0] == grammar->term_error) break;
  if (rule == NULL) {
    rule = rule_new_start (grammar->axiom, NULL, 0);
    rule_new_symb_add (grammar->term_error);
    rule_new_symb_add (grammar->end_marker);
    rule_new_stop ();
    rule->trans_len = 0;
  }
  check_grammar (strict_p);
  symb_finish_adding_terms ();
#ifndef NO_GP_DEBUG_PRINT
  if (grammar->debug_level > 2) {
    /* Print rules. */
    fprintf (stderr, "Rules:\n");
    for (rule = grammar->rules->first_rule; rule != NULL; rule = rule->next) {
      fprintf (stderr, "  ");
      rule_print (stderr, rule, true, true);
    }
    fprintf (stderr, "\n");
    /* Print symbol sets. */
    for (int i = 0; (symb = nonterm_get (i)) != NULL; i++) {
      fprintf (stderr, "Nonterm %s:  Empty=%s , Access=%s, Derive=%s\n", symb->repr,
               (symb->empty_p ? "Yes" : "No"), (symb->access_p ? "Yes" : "No"),
               (symb->derivation_p ? "Yes" : "No"));
      if (grammar->debug_level > 3) {
        fprintf (stderr, "  First: ");
        term_set_print (stderr, symb->u.nonterm.first);
        fprintf (stderr, "\n  Follow: ");
        term_set_print (stderr, symb->u.nonterm.follow);
        fprintf (stderr, "\n\n");
      }
    }
  }
#endif
  grammar->undefined_p = false;
  return 0;
}

#include "sgramm.c"

int gp_set_debug_level (struct grammar *g, int level) {
  assert (g != NULL);
  int old = g->debug_level;
  g->debug_level = level;
  return old;
}

bool gp_set_one_parse_flag (struct grammar *g, bool flag) {
  assert (g != NULL);
  bool old = g->one_parse_p;
  g->one_parse_p = flag;
  return old;
}

bool gp_set_cost_flag (struct grammar *g, bool flag) {
  assert (g != NULL);
  bool old = g->cost_p;
  g->cost_p = flag;
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

static void gp_parse_init (void) { /* initialize all internal data for parser: */
  sit_init ();
  set_init ();
  for (struct rule *rule = grammar->rules->first_rule; rule != NULL; rule = rule->next)
    rule->caller_anode = NULL;
}

/* The function should be called the last (it frees all allocated data for parser). */
static void gp_parse_fin (void) {
  set_fin ();
  sit_fin ();
}

/* Add the rest (non-start) situations to the new set. */
static inline void expand_new_start_set (void) {
  for (int i = 0; i < new_set->n_sits; i++) {
    struct sit *sit = new_sits[i];
    if (sit->pos >= sit->rule->rhs_len) continue;
    /* There is a symbol after dot in the situation. */
    struct symb *symb = sit->rule->rhs[sit->pos];
    if (!symb->term_p)
      for (struct rule *rule = symb->u.nonterm.rules; rule != NULL; rule = rule->lhs_next)
        set_new_add_nonstart_sit (sit_create (rule, 0));
  }
  set_new_set_stop ();
#ifndef NO_GP_DEBUG_PRINT
  if (grammar->debug_level > 2) set_print (stderr, new_set, grammar->debug_level > 3);
#endif
}

struct symb_sit {
  struct symb *symb;
  int sit_num;
};

static vlo_t symb_sits;

static vlo_t actions_vlo;

static int symb_sit_cmp (const void *el1, const void *el2) {
  const struct symb_sit *e1 = (const struct symb_sit *) el1, *e2 = (const struct symb_sit *) el2;
  if (e1->symb == e2->symb) return 0;
  return e1->symb->num - e2->symb->num;
}

static int action_cmp (const void *el1, const void *el2) {
  const struct action *e1 = (const struct action *) el1, *e2 = (const struct action *) el2;
  int diff = e1->term_num - e2->term_num;
  if (diff != 0) return diff;
  if (e1->shift_p) return -1;
  if (e2->shift_p) return 1;
  return e1->u.rule->num - e2->u.rule->num;
}

#ifndef NO_GP_DEBUG_PRINT
static void print_action (FILE *f, struct action *a) {
  struct symb *term = ((struct symb **) VLO_BEGIN (grammar->symbs->terms_vlo))[a->term_num];
  fprintf (f, "%s : ", term->repr);
  if (a->shift_p) {
    fprintf (f, "shift to S%d", a->u.set->num);
  } else {
    fprintf (f, "reduce \"");
    rule_print (f, a->u.rule, false, false);
    fprintf (f, "\"");
  }
}
#endif

static void build_goto_map_and_actions (struct set *set) {
  VLO_NULLIFY (symb_sits);
  VLO_NULLIFY (actions_vlo);
  for (int i = 0; i < set->n_sits; i++) {
    struct sit *sit = set->sits[i];
    if (sit->pos >= sit->rule->rhs_len) {
      for (int j = 0; j < grammar->symbs->n_terms; j++)
        if (term_set_test (sit->lookahead, j)) {
          struct action action;
          action.shift_p = false;
          action.term_num = j;
          action.u.rule = sit->rule;
          VLO_ADD_MEMORY (actions_vlo, &action, sizeof (action));
        }
      continue;
    }
    /* There is a symbol after dot in the situation. */
    struct symb *symb = sit->rule->rhs[sit->pos];
    struct symb_sit symb_sit = {symb, i};
    VLO_ADD_MEMORY (symb_sits, &symb_sit, sizeof (symb_sit));
  }
  int n = VLO_LENGTH (symb_sits) / sizeof (struct symb_sit);
  struct symb_sit *symb_sit_addr = (struct symb_sit *) VLO_BEGIN (symb_sits);
  qsort (symb_sit_addr, n, sizeof (struct symb_sit), symb_sit_cmp);
  set_new_set_start ();
  for (int i = 0; i < n; i++) { /* build derived sets, goto map, and collect actions: */
    struct symb_sit *symb_sit = &symb_sit_addr[i];
    struct sit *sit = set->sits[symb_sit->sit_num];
    assert (sit->pos < sit->rule->rhs_len);
    struct symb_sit *next_symb_sit = i + 1 >= n ? NULL : &symb_sit_addr[i + 1];
    set_new_add_start_sit (sit_create (sit->rule, sit->pos + 1));
    if (next_symb_sit == NULL || symb_sit->symb != next_symb_sit->symb) { /* the last symb sit: */
      if (set_insert ()) {
        new_set->symb = symb_sit->symb;
        expand_new_start_set ();
      }
      struct set *trans_set = new_set;
      if (!symb_sit->symb->term_p) { /* goto */
        if (set->goto_map == NULL) {
          set->goto_map = set_calloc (sizeof (struct set *) * grammar->symbs->n_nonterms);
          n_goto_vects++;
          n_goto_vect_len += grammar->symbs->n_nonterms;
        }
        set->goto_map[symb_sit->symb->u.nonterm.nonterm_num] = trans_set;
      } else { /* shift */
        struct action action = {true, symb_sit->symb->u.term.term_num, {.set = trans_set}};
        VLO_ADD_MEMORY (actions_vlo, &action, sizeof (action));
      }
      set_new_set_start ();
    }
  }
  set_new_set_stop ();
  int nta = VLO_LENGTH (actions_vlo) / sizeof (struct action);
  if (nta == 0) return;
  /* build action descs: */
  struct action *action_addr = (struct action *) VLO_BEGIN (actions_vlo);
  assert (set->action_map == NULL);
  set->action_map = set_calloc (sizeof (struct action_desc) * grammar->symbs->n_terms);
  n_action_vects++;
  n_action_vect_len += grammar->symbs->n_terms;
  set->actions = set_calloc (sizeof (struct action) * nta);
  n_actions += nta;
  qsort (action_addr, nta, sizeof (struct action), action_cmp);
  int actions_num = 0;
  for (int i = 0; i < nta; i++) { /* build derived sets, goto map, and collect actions: */
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
  if (grammar->debug_level > 2) {
    fprintf (stderr, "  Actions for");
    set_print (stderr, set, false);
    for (int i = 0; i < nta; i++) {
      fprintf (stderr, "            ");
      print_action (stderr, &set->actions[i]);
      fprintf (stderr, "\n");
    }
  }
#endif
}

static struct set *get_start_set (void) { /* Form the 1st set: */
  set_new_set_start ();
  for (struct rule *rule = grammar->axiom->u.nonterm.rules; rule != NULL; rule = rule->lhs_next) {
    struct sit *sit = sit_create (rule, 0);
    set_new_add_start_sit (sit);
  }
  if (!set_insert ()) assert (false);
  new_set->symb = grammar->axiom;
  expand_new_start_set ();
  struct set *start_set = new_set;
  build_goto_map_and_actions (start_set);
  return start_set;
}

typedef unsigned short stack_el_t;

struct stack {
  vlo_t els;
};

static vlo_t free_stacks;

#ifndef NO_GP_DEBUG_PRINT
static int n_stacks, n_stack_els, n_curr_stack_els;
#endif

static void stack_init (void) {
  VLO_CREATE (free_stacks, grammar->alloc, 16);
#ifndef NO_GP_DEBUG_PRINT
  n_stacks = n_stack_els = n_curr_stack_els = 0;
#endif
}

static void stack_vlo_free (vlo_t *stack_vlo) {
  VLO_ADD_MEMORY (free_stacks, VLO_BEGIN (*stack_vlo), VLO_LENGTH (*stack_vlo));
}

static void stack_finish (void) {
  for (int i = 0; i < (int) (VLO_LENGTH (free_stacks) / sizeof (struct stack *)); i++) {
    struct stack *stack = ((struct stack **) VLO_BEGIN (free_stacks))[i];
    VLO_DELETE (stack->els);
    gp_free (grammar->alloc, stack);
  }
  VLO_DELETE (free_stacks);
}

static struct stack *stack_create (struct stack *base) {
  struct stack *stack;
  if (VLO_LENGTH (free_stacks) == 0) {
#ifndef NO_GP_DEBUG_PRINT
    n_stacks++;
#endif
    stack = gp_malloc (grammar->alloc, sizeof (struct stack));
    VLO_CREATE (stack->els, grammar->alloc,
                (base == NULL ? 0 : (int) VLO_LENGTH (base->els)) + 4 * sizeof (stack_el_t));
  } else {
    stack = ((struct stack **) VLO_BOUND (free_stacks))[-1];
    VLO_SHORTEN (free_stacks, sizeof (struct stack *));
    VLO_NULLIFY (stack->els);
  }
#ifndef NO_GP_DEBUG_PRINT
  if (base != NULL) {
    n_curr_stack_els += VLO_LENGTH (base->els) / sizeof (stack_el_t);
    if (n_stack_els < n_curr_stack_els) n_stack_els = n_curr_stack_els;
  }
#endif
  if (base != NULL) VLO_ADD_MEMORY (stack->els, VLO_BEGIN (base->els), VLO_LENGTH (base->els));
  return stack;
}

static void stack_free (struct stack *stack) {
  VLO_ADD_MEMORY (free_stacks, &stack, sizeof (stack));
#ifndef NO_GP_DEBUG_PRINT
  n_curr_stack_els -= VLO_LENGTH (stack->els) / sizeof (stack_el_t);
#endif
}

static struct set *stack_get_top_set (struct stack *stack) {
  assert (VLO_LENGTH (stack->els) != 0);
  int num = ((stack_el_t *) VLO_BOUND (stack->els))[-1];
  return ((struct set **) VLO_BEGIN (sets_vlo))[num];
}

static void stack_shift (struct stack *stack, struct set *set) {
  stack_el_t num = set->num;
  assert (num == set->num);
  VLO_ADD_MEMORY (stack->els, &num, sizeof (num));
#ifndef NO_GP_DEBUG_PRINT
  n_curr_stack_els++;
  if (n_stack_els < n_curr_stack_els) n_stack_els = n_curr_stack_els;
#endif
}

static void stack_reduce (struct stack *stack, struct rule *rule) {
  int len = VLO_LENGTH (stack->els) / sizeof (stack_el_t);
  assert (rule->rhs_len < len);
  int num = ((stack_el_t *) VLO_BEGIN (stack->els))[len - 1 - rule->rhs_len];
  struct set *set = ((struct set **) VLO_BEGIN (sets_vlo))[num];
  VLO_SHORTEN (stack->els, sizeof (stack_el_t) * rule->rhs_len);
  struct set *goto_set = set->goto_map[rule->lhs->u.nonterm.nonterm_num];
  stack_el_t goto_num = goto_set->num;
  VLO_ADD_MEMORY (stack->els, &goto_num, sizeof (stack_el_t));
#ifndef NO_GP_DEBUG_PRINT
  n_curr_stack_els += (1 - rule->rhs_len);
  if (n_stack_els < n_curr_stack_els) n_stack_els = n_curr_stack_els;
#endif
}

static bool merge_stacks (vlo_t *stacks) {
  bool merge_p = false;
  int last = 0;
  for (int i = 0; i < (int) (VLO_LENGTH (*stacks) / sizeof (struct stack *)); i++) {
    struct stack *curr = ((struct stack **) VLO_BEGIN (*stacks))[i];
    if (curr == NULL) continue;
    ((struct stack **) VLO_BEGIN (*stacks))[last++] = curr;
    for (int j = i + 1; j < (int) (VLO_LENGTH (*stacks) / sizeof (struct stack *)); j++) {
      struct stack *curr2 = ((struct stack **) VLO_BEGIN (*stacks))[j];
      if (curr2 == NULL || VLO_LENGTH (curr->els) != VLO_LENGTH (curr2->els)
          || memcmp (VLO_BEGIN (curr->els), VLO_BEGIN (curr2->els), VLO_LENGTH (curr->els)) != 0)
        continue;
      ((struct stack **) VLO_BEGIN (*stacks))[j] = NULL;
      merge_p = true;
      stack_free (curr2);
    }
  }
  VLO_SHORTEN (*stacks, VLO_LENGTH (*stacks) - last * sizeof (struct stack *));
  return merge_p;
}

#ifndef NO_GP_DEBUG_PRINT
static void print_stack (FILE *f, struct stack *stack) {
  fprintf (f, "          ");
  for (int i = 0; i < (int) (VLO_LENGTH (stack->els) / sizeof (stack_el_t)); i++)
    fprintf (f, " s%d", ((stack_el_t *) VLO_BEGIN (stack->els))[i]);
  fprintf (f, "\n");
}

static void print_stacks (FILE *f, vlo_t *stacks) {
  for (int i = 0; i < (int) (VLO_LENGTH (*stacks) / sizeof (struct stack *)); i++)
    print_stack (f, ((struct stack **) VLO_BEGIN (*stacks))[i]);
}

static void stack_action_print (FILE *f, struct action *a, vlo_t *stacks, bool new_p) {
  fprintf (f, "  Parsing %sstacks after action [", new_p ? "new " : "");
  print_action (f, a);
  fprintf (f, "]:\n");
  print_stacks (f, stacks);
}

static void stack_merge_print (FILE *f, vlo_t *stacks) {
  fprintf (f, "  Parsing stacks after pnode merging\n");
  print_stacks (f, stacks);
}
#endif

#define SWAP(a, b, t) \
  do {                \
    t = a;            \
    a = b;            \
    b = t;            \
  } while (false)

static int toks_num, n_parse_term_nodes, n_parse_abstract_nodes, n_parse_alt_nodes;

/* Major function to make parsing. Return true if we parsed successfully. */
static bool parse (void) {
  VLO_CREATE (symb_sits, grammar->alloc, 16);
  VLO_CREATE (actions_vlo, grammar->alloc, 16);
  stack_init ();
  struct stack *single_stack = stack_create (NULL);
  stack_shift (single_stack, get_start_set ());
  vlo_t curr_stacks, new_stacks, temp_vlo;
  VLO_CREATE (curr_stacks, grammar->alloc, 2 * sizeof (vlo_t));
  VLO_CREATE (new_stacks, grammar->alloc, 2 * sizeof (vlo_t));
  toks_num = n_parse_term_nodes = n_parse_abstract_nodes = n_parse_alt_nodes = 0;
  void *attr;
  int code = read_token (&attr);
  struct symb *term_symb = term_find_by_code (code);
  int la_term = term_symb->u.term.term_num;
#ifndef NO_GP_DEBUG_PRINT
  if (grammar->debug_level > 1) {
    fprintf (stderr, "  Read %s\n", term_symb->repr);
    toks_num++;
  }
#endif
  for (;;) {
    if (single_stack != NULL) {
      for (;;) {
        struct set *set = stack_get_top_set (single_stack);
        int actions_num;
        struct action *actions = set_get_actions (set, la_term, &actions_num);
        if (actions_num != 1) {
          VLO_ADD_MEMORY (curr_stacks, &single_stack, sizeof (single_stack));
          single_stack = NULL;
          goto multi_stack;
        }
        if (actions[0].shift_p) { /* shift */
          struct set *shifted_set = actions[0].u.set;
          assert (shifted_set != NULL);
          stack_shift (single_stack, shifted_set);
          goto next_token;
        } else {
          struct rule *r = actions[0].u.rule;
          stack_reduce (single_stack, r);
        }
      }
    }
  multi_stack:
    assert (VLO_LENGTH (new_stacks) == 0);
    while (VLO_LENGTH (curr_stacks) != 0) {
      struct stack *curr_stack = ((struct stack **) VLO_BOUND (curr_stacks))[-1];
      VLO_SHORTEN (curr_stacks, sizeof (struct stack *));
      struct set *set = stack_get_top_set (curr_stack);
      if (set->goto_map == NULL && set->action_map == NULL) build_goto_map_and_actions (set);
      int actions_num;
      struct action *actions = set_get_actions (set, la_term, &actions_num);
      if (actions_num == 0) {
        stack_free (curr_stack);
        continue;
      }
      for (int i = 0; i < actions_num; i++) {
        struct action *action = &actions[i];
        struct stack *stack = i == actions_num - 1 ? curr_stack : stack_create (curr_stack);
        if (action->shift_p) { /* shift */
          struct set *shifted_set = action->u.set;
          assert (shifted_set != NULL);
          stack_shift (stack, shifted_set);
          VLO_ADD_MEMORY (new_stacks, &stack, sizeof (stack));
        } else {
          struct rule *r = action->u.rule;
          stack_reduce (stack, r);
          VLO_ADD_MEMORY (curr_stacks, &stack, sizeof (curr_stack));
        }
#ifndef NO_GP_DEBUG_PRINT
        if (grammar->debug_level > 2)
          stack_action_print (stderr, action, action->shift_p ? &new_stacks : &curr_stacks,
                              action->shift_p);
#endif
      }
    }
    if (VLO_LENGTH (new_stacks) == 0) break;
    SWAP (curr_stacks, new_stacks, temp_vlo);
    if (merge_stacks (&curr_stacks)) {
#ifndef NO_GP_DEBUG_PRINT
      if (grammar->debug_level > 2) stack_merge_print (stderr, &curr_stacks);
#endif
      if (VLO_LENGTH (curr_stacks) == sizeof (struct stack *)) {
        single_stack = ((struct stack **) VLO_BEGIN (curr_stacks))[0];
        VLO_NULLIFY (curr_stacks);
      }
    }
  next_token:
    if (code == END_MARKER_CODE) {
      if (single_stack != NULL) VLO_ADD_MEMORY (curr_stacks, single_stack, sizeof (single_stack));
      break;
    }
    code = read_token (&attr);
    term_symb = term_find_by_code (code);
    la_term = term_symb->u.term.term_num;
#ifndef NO_GP_DEBUG_PRINT
    if (grammar->debug_level > 1) {
      fprintf (stderr, "  Read %s\n", term_symb->repr);
      toks_num++;
    }
#endif
  }
  bool res = false;
  if (VLO_LENGTH (curr_stacks) == sizeof (struct stack *)) {
    struct stack *stack = ((struct stack **) VLO_BEGIN (curr_stacks))[0];
    struct set *set = stack_get_top_set (stack);
    struct symb *symb = set->symb;
    if (strcmp (symb->repr, END_MARKER_NAME) == 0) {
      res = true;
    }
  }
  stack_vlo_free (&curr_stacks);
  VLO_DELETE (new_stacks);
  VLO_DELETE (curr_stacks);
  VLO_DELETE (symb_sits);
  VLO_DELETE (actions_vlo);
  stack_finish ();
  return res;
}

static void *parse_alloc_default (int nmemb) {
  assert (nmemb > 0);
  void *result = malloc (nmemb);
  if (result == NULL) exit (1);
  return result;
}

static void parse_free_default (void *mem) { free (mem); }

/* Parse input according read grammar. ONE_PARSE_FLAG means build only one parse tree. For
   unambiguous grammar the flag does not affect the result. LA_LEVEL means usage of static (if 1)
   or dynamic (2) lookahead to decrease size of sets. Static lookaheads gives the best results
   with the point of space and speed, dynamic ones does sligthly worse, and no usage of lookaheds
   does the worst. D_LEVEL says what debugging information to output (it works only if we compiled
   without defined macro NO_GP_DEBUG_PRINT). The function returns the error code (which will be
   also in error_code). The function sets up *AMBIGUOUS_P if we found that the grammer is ambigous
   (it works even we asked only one parse tree without alternatives). */
int gp_parse (struct grammar *g, int (*read) (void **attr),
              void (*error) (int err_tok_num, void *err_tok_attr, int start_ignored_tok_num,
                             void *start_ignored_tok_attr, int start_recovered_tok_num,
                             void *start_recovered_tok_attr),
              void *(*alloc) (int nmemb), void (*free) (void *mem), struct gp_tree_node **root,
              bool *ambiguous_p) {
  /* Set up parse allocation */
  if (alloc == NULL) {
    if (free != NULL) return GP_NO_MEMORY;
    /* Set up defaults */
    alloc = parse_alloc_default;
    free = parse_free_default;
  }

  all_searches = all_collisions = 0;
  grammar = g;
  assert (grammar != NULL);
  read_token = read;
  syntax_error = error;
  parse_alloc = alloc;
  parse_free = free;
  *root = NULL;
  *ambiguous_p = false;
  int code;
  bool parse_init_p = false;
  if ((code = setjmp (error_longjump_buff)) != 0) {
    if (parse_init_p) gp_parse_fin ();
    return code;
  }
  if (grammar->undefined_p) gp_error (GP_UNDEFINED_OR_BAD_GRAMMAR, "undefined or bad grammar");
  gp_parse_init ();
  parse_init_p = true;
  bool ok_p GP_UNUSED = parse ();
#ifndef NO_GP_DEBUG_PRINT
  if (grammar->debug_level > 0) {
    fprintf (stderr, "%s\n", ok_p ? "SUCCESS!" : "FAIL!");
    fprintf (stderr, "%sGrammar: #terms = %d, #nonterms = %d, ", *ambiguous_p ? "AMBIGUOUS " : "",
             grammar->symbs->n_terms, grammar->symbs->n_nonterms);
    fprintf (stderr, "#rules = %d, rules size = %d\n", grammar->rules->n_rules,
             grammar->rules->n_rhs_lens + grammar->rules->n_rules);
    fprintf (stderr, "Input: #tokens = %d, #all situations = %d\n", toks_num, n_all_sits);
    fprintf (stderr, "       #terminal sets = %d, their size = %d\n",
             grammar->term_sets->n_term_sets, grammar->term_sets->n_term_sets_size);
    fprintf (stderr, "       #sets = %d, #their start situations = %d\n", n_sets,
             n_sets_start_sits);
    fprintf (stderr, "       #goto vectors = %d, their length = %d\n", n_goto_vects,
             n_goto_vect_len);
    fprintf (stderr, "       #actions = %d, #action vectors = %d, their length = %d\n", n_actions,
             n_action_vects, n_action_vect_len);
    fprintf (stderr, "       max #stacks = %d, max #stack els = %d\n", n_stacks, n_stack_els);
    fprintf (stderr, "       #term nodes = %d, #abstract nodes = %d\n", n_parse_term_nodes,
             n_parse_abstract_nodes);
    fprintf (stderr, "       #alternative nodes = %d, #all nodes = %d\n", n_parse_alt_nodes,
             n_parse_term_nodes + n_parse_abstract_nodes + n_parse_alt_nodes);
  }
#endif
  gp_parse_fin ();
#ifndef NO_GP_DEBUG_PRINT
  if (grammar->debug_level > 0) { /* do it after deleting hash tables */
    if (all_searches == 0) all_searches++;
    fprintf (stderr, "       #table collisions = %.2g%% (%d out of %d)\n",
             all_collisions * 100.0 / all_searches, all_collisions, all_searches);
  }
#endif
  return 0;
}

/* Free memory allocated for the grammar. */
void gp_free_grammar (struct grammar *g) {
  if (g != NULL) {
    gp_allocator_t *allocator = g->alloc;
    rule_fin (g->rules);
    term_set_fin (g->term_sets);
    symb_fin (g->symbs);
    gp_free (allocator, g);
    gp_alloc_del (allocator);
  }
  grammar = NULL;
}

static void free_tree_reduce (struct gp_tree_node *node) {
  struct gp_tree_node **childp;
  size_t numChildren, pos, freePos;

  assert (node != NULL);
  assert ((node->type & _gp_VISITED) == 0);
  enum gp_tree_node_type type = node->type;
  node->type = (enum gp_tree_node_type) (node->type | _gp_VISITED);
  switch (type) {
  case GP_NIL:
  case GP_ERROR:
  case GP_TERM: break;
  case GP_ANODE:
    if (node->val.anode.name[0] == '\0') { /* We have already seen the node name */
      node->val.anode.name = NULL;
    } else { /* Mark the node name as seen */
      node->val._anode_name.name[0] = '\0';
    }
    for (numChildren = 0, childp = node->val.anode.children; *childp != NULL;
         ++numChildren, ++childp) {
      if ((*childp)->type & _gp_VISITED) {
        *childp = NULL;
      } else {
        free_tree_reduce (*childp);
      }
    }
    /* Compactify children array */
    for (freePos = 0, pos = 0; pos != numChildren; ++pos) {
      if (node->val.anode.children[pos] != NULL) {
        if (freePos < pos) {
          node->val.anode.children[freePos] = node->val.anode.children[pos];
          node->val.anode.children[pos] = NULL;
        }
        ++freePos;
      }
    }
    break;
  case GP_ALT:
    if (node->val.alt.node->type & _gp_VISITED) {
      node->val.alt.node = NULL;
    } else {
      free_tree_reduce (node->val.alt.node);
    }
    while ((node->val.alt.next != NULL) && (node->val.alt.next->type & _gp_VISITED)) {
      assert (node->val.alt.next->type == (GP_ALT | _gp_VISITED));
      node->val.alt.next = node->val.alt.next->val.alt.next;
    }
    if (node->val.alt.next != NULL) {
      assert ((node->val.alt.next->type & _gp_VISITED) == 0);
      free_tree_reduce (node->val.alt.next);
    }
    break;
  default: assert ("This should not happen" == NULL);
  }
}

static void free_tree_sweep (struct gp_tree_node *node, void (*parse_free_fn) (void *),
                             void (*termcb) (struct gp_term *)) {
  struct gp_tree_node **childp;

  if (node == NULL) return;
  assert (node->type & _gp_VISITED);
  enum gp_tree_node_type type = (enum gp_tree_node_type) (node->type & ~_gp_VISITED);
  switch (type) {
  case GP_NIL:
  case GP_ERROR: break;
  case GP_TERM:
    if (termcb != NULL) termcb (&node->val.term);
    break;
  case GP_ANODE:
    parse_free_fn (node->val._anode_name.name);
    for (childp = node->val.anode.children; *childp != NULL; ++childp)
      free_tree_sweep (*childp, parse_free_fn, termcb);
    break;
  case GP_ALT:
    free_tree_sweep (node->val.alt.node, parse_free_fn, termcb);
    struct gp_tree_node *next = node->val.alt.next;
    parse_free_fn (node);
    free_tree_sweep (next, parse_free_fn, termcb);
    return; /* Tail recursion */
  default: assert ("This should not happen" == NULL);
  }
  parse_free_fn (node);
}

void gp_free_tree (struct gp_tree_node *root, void (*parse_free_fn) (void *),
                   void (*termcb) (struct gp_term *)) {
  if (root == NULL) return;
  if (parse_free_fn == NULL) parse_free_fn = parse_free_default;
  /* Since the parse tree is actually a DAG, we must carefully avoid double free errors.
     Therefore, we walk the parse tree twice. On the first walk, we reduce the DAG to an actual
     tree. On the second walk, we recursively free the tree nodes. */
  free_tree_reduce (root);
  free_tree_sweep (root, parse_free_fn, termcb);
}

/* This page contains a test code for GPARSER. To use it, define macro GP_TEST during compilation.
 */

#ifdef GP_TEST

/* All parse_alloc memory is contained here. */
static os_t mem_os;

static void *test_parse_alloc (int size) {
  OS_TOP_EXPAND (mem_os, size);
  void *result = OS_TOP_BEGIN (mem_os);
  OS_TOP_FINISH (mem_os);
  return result;
}

static int nterm; /* the current number of next input grammar terminal */

/* The function imported by GPARSER (see comments in the interface file). */
const char *read_terminal (int *code) {
  nterm++;
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

/* The function imported by GPARSER (see comments in the interface file). */
const char *read_rule (const char ***rhs, const char **anode, int *anode_cost, int **transl) {
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
    *anode_cost = 0;
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
    *anode_cost = 0;
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
    *anode_cost = 0;
    *transl = tr_5;
    return "F";
  case 6:
    *rhs = rhs_6;
    *anode = NULL;
    *anode_cost = 0;
    *transl = tr_6;
    return "F";
  default: return NULL;
  }
}

static int ntok; /* the current number of next input token */

/* The function imported by GPARSER (see comments in the interface file). */
static int test_read_token (void **attr) {
  const char input[] = "a+a*(a*a+a)";
  ntok++;
  *attr = NULL;
  if ((size_t) ntok < sizeof (input)) return input[ntok - 1];
  return -1;
}

/* Printing syntax error. */
static void test_syntax_error (int err_tok_num, void *err_tok_attr GP_UNUSED,
                               int start_ignored_tok_num, void *start_ignored_tok_attr GP_UNUSED,
                               int start_recovered_tok_num,
                               void *start_recovered_tok_attr GP_UNUSED) {
  if (start_ignored_tok_num < 0)
    fprintf (stderr, "Syntax error on token %d\n", err_tok_num);
  else
    fprintf (stderr, "Syntax error on token %d:ignore %d tokens starting with token = %d\n",
             err_tok_num, start_recovered_tok_num - start_ignored_tok_num, start_ignored_tok_num);
}

/* The following two functions calls gparser with two different ways of forming grammars. */
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
  OS_CREATE (mem_os, grammar->alloc, 0);
  gp_set_one_parse_flag (g, false);
  if (argc > 1)
    gp_set_debug_level (g, atoi (argv[2]));
  else
    gp_set_debug_level (g, 3);
  if (argc > 3) gp_set_error_recovery_flag (g, atoi (argv[3]));
  if (argc > 4) gp_set_one_parse_flag (g, atoi (argv[4]));
  if (gp_read_grammar (g, true, read_terminal, read_rule) != 0) {
    fprintf (stderr, "%s\n", gp_error_message (g));
    OS_DELETE (mem_os);
    exit (1);
  }
  ntok = 0;
  if (gp_parse (g, test_read_token, test_syntax_error, test_parse_alloc, NULL, &root, &ambiguous_p))
    fprintf (stderr, "gp_parse: %s\n", gp_error_message (g));
  OS_DELETE (mem_os);
  gp_free_grammar (g);
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
  OS_CREATE (mem_os, grammar->alloc, 0);
  gp_set_one_parse_flag (g, false);
  if (argc > 2)
    gp_set_debug_level (g, atoi (argv[2]));
  else
    gp_set_debug_level (g, 3);
  if (argc > 3) gp_set_error_recovery_flag (g, atoi (argv[3]));
  if (argc > 4) gp_set_one_parse_flag (g, atoi (argv[4]));
  if (gp_parse_grammar (g, true, description) != 0) {
    fprintf (stderr, "%s\n", gp_error_message (g));
    OS_DELETE (mem_os);
    exit (1);
  }
  if (gp_parse (g, test_read_token, test_syntax_error, test_parse_alloc, NULL, &root, &ambiguous_p))
    fprintf (stderr, "gp_parse: %s\n", gp_error_message (g));
  OS_DELETE (mem_os);
  gp_free_grammar (g);
}

int main (int argc, char **argv) {
  if (argc <= 1 || atoi (argv[1]))
    use_description (argc, argv);
  else
    use_functions (argc, argv);
  exit (0);
}

#endif /* #ifdef GP_TEST */
