/* This is interface file of the code which transforms grammar description given by string into
   representation which can be used by Gecko (GLR parser).  So the code implements functions read_terminal and
   read_rule imported by Gecko (GLR parser).  */

/* This is interface file of the code which transforms grammar description given by string into
   representation which can be used by Gecko (GLR parser).  So the code implements functions read_terminal and
   read_rule imported by Gecko (GLR parser).  */

%{
#include <ctype.h>
#include <assert.h>

/* The following is necessary if we use Gecko (GLR parser) with byacc/bison/msta parser. */
#define yylval gp_yylval
#define yylex gp_yylex
#define yyerror gp_yyerror
#define yyparse gp_yyparse
#define yychar gp_yychar
#define yynerrs gp_yynerrs
#define yydebug gp_yydebug
#define yyerrflag gp_yyerrflag
#define yyssp gp_yyssp
#define yyval gp_yyval
#define yyvsp gp_yyvsp
#define yylhs gp_yylhs
#define yylen gp_yylen
#define yydefred gp_yydefred
#define yydgoto gp_yydgoto
#define yysindex gp_yysindex
#define yyrindex gp_yyrindex
#define yygindex gp_yygindex
#define yytable gp_yytable
#define yycheck gp_yycheck
#define yyss gp_yyss
#define yyvs gp_yyvs

#define YYLEX_PARAM g
#define YYPARSE_PARAM g
#define YYERROR_DECL() yyerror(void *, const char *s)
#define YYERROR_CALL(msg) yyerror(g, msg)

/* The following structure describes syntax grammar terminal. */
  struct sterm {
    char *repr; /* terminal representation. */
    int code;   /* terminal code. */
    int num;    /* order number. */
    int priority;
    enum gp_assoc assoc; /* undefined for prioirty < 0 */
  };

  /* The following structure describes syntax grammar terminal. */
  struct sassoc {
    char *repr; /* terminal representation. */
    enum gp_assoc assoc;
    int priority;
    bool used_p;
  };

  /* The following structure describes syntax grammar rule. */
  struct srule {
    /* The following members are left hand side nonterminal
       representation and abstract node name (if any) for the rule. */
    char *lhs, *anode;
    /* The following is length of right hand side of the rule. */
    int rhs_len;
    /* Terminal/nonterminal representations in RHS of the rule.  The array end marker is NULL. */
    char **rhs;
    /* The translations numbers. */
    int *trans;
  };

  /* Current priority for terminal associativity */
  static int curr_priority;
  
  /* The following vlos contain all syntax terminal, assoc, and rule structures. */
  static vlo_t sterms, assocs, srules;
  static os_t assocs_os; /* container for sassocs */
  
  /* The following contain all right hand sides and translations arrays.
     See members rhs, trans in structure `rule'. */
  static os_t srhs, strans;

  /* This variable is used in yacc action to process alternatives. */
  static char *slhs;

  /* Forward declarations. */
  extern int yyerror (void *arg, const char *str);
  extern int yylex (void *);
  extern int yyparse (void *);

%}

%union {
  void *ref;
  int num;
  enum gp_assoc assoc;
}

%token<ref> IDENT SEM_IDENT CHAR
%token<num> NUMBER
%token TERM LEFT RIGHT NONASSOC
%type<assoc> assocs
%type<ref> trans
%type<num> number

%%

file : file terms opt_sem | file assocs opt_sem | file rule | terms opt_sem | assocs opt_sem | rule;

opt_sem :
        | ';';

terms : terms IDENT number {
          struct sterm term;
          term.repr = (char *) $2;
          term.code = $3;
	  term.priority = -1;
	  term.assoc = GP_NON_ASSOC;
          term.num = VLO_LENGTH (sterms) / sizeof (term);
	  VLO_ADD_MEMORY (sterms, &term, sizeof (term));
        }
     | TERM
     ;

assocs : assocs IDENT {
           $$ = $1;
	   add_assoc ((char *) $2, curr_priority, $1);
         }
       | assocs CHAR {
           $$ = $1;
	   add_assoc ((char *) $2, curr_priority, $1);
         }
       | LEFT {$$ = GP_LEFT_ASSOC; curr_priority++;}
       | RIGHT  {$$ = GP_RIGHT_ASSOC; curr_priority++;}
       | NONASSOC {$$ = GP_NON_ASSOC; curr_priority++;}
       ;

number : { $$ = -1; }
       | '=' NUMBER { $$ = $2; }
       ;

rule : SEM_IDENT { slhs = (char *) $1; } rhs opt_sem;

rhs : rhs '|' alt | alt;

alt : seq trans {
        struct srule rule;
  	int end_marker = -1;

	OS_TOP_ADD_MEMORY (strans, &end_marker, sizeof (int));
	rule.lhs = slhs;
	rule.anode = (char *) $2;
	rule.rhs_len = OS_TOP_LENGTH (srhs) / sizeof (char *);
        OS_TOP_EXPAND (srhs, sizeof (char *));
	rule.rhs = (char **) OS_TOP_BEGIN (srhs);
	rule.rhs[rule.rhs_len] = NULL;
	OS_TOP_FINISH (srhs);
	rule.trans = (int *) OS_TOP_BEGIN (strans);
	OS_TOP_FINISH (strans);
        VLO_ADD_MEMORY (srules, &rule, sizeof (rule));
    }
    ;

seq : seq IDENT {
        char *repr = (char *) $2;
        OS_TOP_ADD_MEMORY (srhs, &repr, sizeof (repr));
      }
    | seq CHAR {
        struct sterm term;
        term.repr = (char *) $2;
        term.code = term.repr[1];
        term.num = VLO_LENGTH (sterms) / sizeof (term);
	term.priority = -1;
	term.assoc = GP_NON_ASSOC;
        VLO_ADD_MEMORY (sterms, &term, sizeof (term));
        OS_TOP_ADD_MEMORY (srhs, &term.repr, sizeof (term.repr));
      }
    |
    ;

trans : { $$ = NULL; }
      | '#' { $$ = NULL; }
      | '#' NUMBER {
          int symb_num = $2;
          $$ = NULL;
          OS_TOP_ADD_MEMORY (strans, &symb_num, sizeof (int));
        }
      | '#' '-' {
          int symb_num = GP_NIL_TRANSLATION_NUMBER;
          $$ = NULL;
         OS_TOP_ADD_MEMORY (strans, &symb_num, sizeof (int));
        }
      | '#' IDENT '(' numbers ')' { $$ = $2; }
      | '#' IDENT { $$ = $2; }
      ;

numbers :
        | numbers NUMBER {
            int symb_num = $2;
            OS_TOP_ADD_MEMORY (strans, &symb_num, sizeof (int));
          }
        | numbers '-' {
            int symb_num = GP_NIL_TRANSLATION_NUMBER;
            OS_TOP_ADD_MEMORY (strans, &symb_num, sizeof (int));
          }
	;

%%
/* The following is current input character of the grammar description. */
static const char *curr_ch;

/* The following is current line number of the grammar description. */
static int ln;

/* The following contains all representation of the syntax tokens. */
static os_t stoks;

/* The following is number of syntax terminal and syntax rules being read. */
static int nsterm, nsrule;

/* The following implements lexical analyzer for yacc code. */
int yylex (void *g) {
  int c;
  int n_errs = 0;

  for (;;) {
    c = *curr_ch++;
    switch (c) {
    case '\0': return 0;
    case '\n': ln++;
    case '\t':
    case ' ': break;
    case '/':
      c = *curr_ch++;
      if (c != '*' && n_errs == 0) {
        n_errs++;
        curr_ch--;
        yyerror (g, "invalid input character /");
      }
      for (;;) {
        c = *curr_ch++;
        if (c == '\0') yyerror (g, "unfinished comment");
        if (c == '\n') ln++;
        if (c == '*') {
          c = *curr_ch++;
          if (c == '/') break;
          curr_ch--;
        }
      }
      break;
    case '=':
    case '#':
    case '|':
    case ';':
    case '-':
    case '(':
    case ')': return c;
    case '\'':
      OS_TOP_ADD_BYTE (stoks, '\'');
      yylval.num = *curr_ch++;
      OS_TOP_ADD_BYTE (stoks, yylval.num);
      if (*curr_ch++ != '\'') yyerror (g, "invalid character");
      OS_TOP_ADD_BYTE (stoks, '\'');
      OS_TOP_ADD_BYTE (stoks, '\0');
      yylval.ref = OS_TOP_BEGIN (stoks);
      OS_TOP_FINISH (stoks);
      return CHAR;
    default:
      if (isalpha (c) || c == '_') {
        OS_TOP_ADD_BYTE (stoks, c);
        while ((c = *curr_ch++) != '\0' && (isalnum (c) || c == '_')) OS_TOP_ADD_BYTE (stoks, c);
        curr_ch--;
        OS_TOP_ADD_BYTE (stoks, '\0');
        yylval.ref = OS_TOP_BEGIN (stoks);
        if (strcmp ((char *) yylval.ref, "TERM") == 0) {
          OS_TOP_NULLIFY (stoks);
          return TERM;
        }
        if (strcmp ((char *) yylval.ref, "LEFT") == 0) {
          OS_TOP_NULLIFY (stoks);
          return LEFT;
        }
        if (strcmp ((char *) yylval.ref, "RIGHT") == 0) {
          OS_TOP_NULLIFY (stoks);
          return RIGHT;
        }
        if (strcmp ((char *) yylval.ref, "NONASSOC") == 0) {
          OS_TOP_NULLIFY (stoks);
          return NONASSOC;
        }
        OS_TOP_FINISH (stoks);
        while ((c = *curr_ch++) != '\0')
          if (c == '\n')
            ln++;
          else if (c != '\t' && c != ' ')
            break;
        if (c != ':') curr_ch--;
        return (c == ':' ? SEM_IDENT : IDENT);
      } else if (isdigit (c)) {
        yylval.num = c - '0';
        while ((c = *curr_ch++) != '\0' && isdigit (c)) yylval.num = yylval.num * 10 + (c - '0');
        curr_ch--;
        return NUMBER;
      } else {
        n_errs++;
        if (n_errs == 1) {
          char str[100];

          if (isprint (c)) {
            sprintf (str, "invalid input character '%c'", c);
            yyerror (g, str);
          } else
            yyerror (g, "invalid input character");
        }
      }
    }
  }
}

/* The following implements syntactic error diagnostic function yacc code. */
int yyerror (void *g, const char *str GP_UNUSED) {
  error (g, GP_DESCRIPTION_SYNTAX_ERROR_CODE, "description syntax error on ln %d", ln);
  return 0;
}

/* The following function is used to sort array of syntax terminals by names. */
static int sterm_name_cmp (const void *t1, const void *t2) {
  return strcmp (((struct sterm *) t1)->repr, ((struct sterm *) t2)->repr);
}

/* The following function is used to sort array of syntax terminals by order number. */
static int sterm_num_cmp (const void *t1, const void *t2) {
  return ((struct sterm *) t1)->num - ((struct sterm *) t2)->num;
}

static void add_assoc (const char *repr, int priority, enum gp_assoc assoc) {
  OS_TOP_EXPAND (assocs_os, sizeof (struct sassoc));
  struct sassoc *sassoc = OS_TOP_BEGIN (assocs_os);
  OS_TOP_FINISH (assocs_os);
  sassoc->repr = (char *) repr;
  sassoc->priority = priority;
  sassoc->assoc = assoc;
  VLO_ADD_MEMORY (assocs, &sassoc, sizeof (sassoc));
}

static hash_table_t assoc_htab;

static uint64_t assoc_hash (hash_table_entry_t s) { /* return hash of assoc */
  const char *str = ((struct sassoc *) s)->repr;
  return hash (str, strlen (str), 42);
}

static bool assoc_eq (hash_table_entry_t s1, hash_table_entry_t s2) { /* Equality of assocs. */
  return strcmp (((struct sassoc *) s1)->repr, ((struct sassoc *) s2)->repr) == 0;
}

static struct sassoc *find_assoc (char *repr) {
  struct sassoc assoc;
  assoc.repr = repr;
  hash_table_entry_t *res = find_hash_table_entry (assoc_htab, &assoc, false);
  return (struct sassoc *) *res;
}

static void insert_assoc (struct sassoc *assoc) {
  hash_table_entry_t *entry = find_hash_table_entry (assoc_htab, assoc, true);
  assert (*entry == NULL);
  *entry = (hash_table_entry_t) assoc;
}

static void free_sgrammar (void);

/* The following is major function which parses the description and transforms it into IR. */
static int set_sgrammar (struct grammar *g, const char *grammar_name) {
  int i, j, num;
  struct sterm *term, *prev, *arr;
  int code = 256;

  ln = 1;
  if ((code = setjmp (g->error_longjump_buff)) != 0) {
    free_sgrammar ();
    return code;
  }
  curr_priority = 0;
  OS_CREATE (stoks, g->alloc, 0);
  VLO_CREATE (sterms, g->alloc, 0);
  VLO_CREATE (assocs, g->alloc, 0);
  OS_CREATE (assocs_os, g->alloc, 0);
  VLO_CREATE (srules, g->alloc, 0);
  OS_CREATE (srhs, g->alloc, 0);
  OS_CREATE (strans, g->alloc, 0);
  assoc_htab = create_hash_table (g->alloc, 80, assoc_hash, assoc_eq);
  curr_ch = grammar_name;
  yyparse (g);
  /* sort array of syntax terminals by names. */
  num = VLO_LENGTH (sterms) / sizeof (struct sterm);
  arr = (struct sterm *) VLO_BEGIN (sterms);
  qsort (arr, num, sizeof (struct sterm), sterm_name_cmp);
  /* Check different codes for the same syntax terminal and remove duplicates. */
  for (i = j = 0, prev = NULL; i < num; i++) {
    term = arr + i;
    if (prev == NULL || strcmp (prev->repr, term->repr) != 0) {
      prev = term;
      arr[j++] = *term;
    } else if (term->code != -1 && prev->code != -1 && prev->code != term->code) {
      char str[GP_MAX_ERROR_MESSAGE_LENGTH / 2];

      strncpy (str, prev->repr, sizeof (str));
      str[sizeof (str) - 1] = '\0';
      error (g, GP_REPEATED_TERM_CODE, "term %s described repeatedly with different code", str);
    } else if (prev->code != -1)
      prev->code = term->code;
  }
  VLO_SHORTEN (sterms, (num - j) * sizeof (struct sterm));
  num = j;
  /* sort array of syntax terminals by order number. */
  qsort (arr, num, sizeof (struct sterm), sterm_num_cmp);
  for (i = 0; i < (int) (VLO_LENGTH (assocs) / sizeof (struct sassoc *)); i++) {
    struct sassoc *assoc = ((struct sassoc **)VLO_BEGIN (assocs))[i];
    assoc->used_p = false;
    if (find_assoc (assoc->repr) != NULL) {
      error (g, GP_REPEATED_TERM_ASSOC, "term %s is repeteadly described in an associtivity clause", assoc->repr);
    } else {
      insert_assoc (assoc);
    }
  }
  /* Assign codes and priories */
  for (i = 0; i < num; i++) {
    term = (struct sterm *) VLO_BEGIN (sterms) + i;
    if (term->code < 0) term->code = code++;
    struct sassoc *assoc = find_assoc (term->repr);
    if (assoc == NULL) continue;
    term->priority = assoc->priority;
    term->assoc = assoc->assoc;
    assoc->used_p = true;
  }
  for (i = 0; i < (int) (VLO_LENGTH (assocs) / sizeof (struct sassoc *)); i++) {
    struct sassoc *assoc = ((struct sassoc **) VLO_BEGIN (assocs))[i];
    if (!assoc->used_p)
      error (g, GP_UNDEFINED_TERM_ASSOC, "term %s described in associtivity clause is not defined", assoc->repr);
  }
  nsterm = nsrule = 0;
  return 0;
}

/* The following frees IR. */
static void free_sgrammar (void) {
  OS_DELETE (strans);
  OS_DELETE (srhs);
  VLO_DELETE (srules);
  VLO_DELETE (assocs);
  OS_DELETE (assocs_os);
  delete_hash_table (assoc_htab);
  VLO_DELETE (sterms);
  OS_DELETE (stoks);
}

/* The following two functions implements functions used by YAEP. */
static const char *sread_terminal (int *code, int *priority, enum gp_assoc *assoc) {
  struct sterm *term;
  const char *name;

  term = &((struct sterm *) VLO_BEGIN (sterms))[nsterm];
  if ((char *) term >= (char *) VLO_BOUND (sterms)) return NULL;
  *code = term->code;
  *priority = term->priority;
  *assoc = term->assoc;
  name = term->repr;
  nsterm++;
  return name;
}

static const char *sread_rule (const char ***rhs, const char **abs_node, int **transl) {
  struct srule *rule;
  const char *lhs;

  rule = &((struct srule *) VLO_BEGIN (srules))[nsrule];
  if ((char *) rule >= (char *) VLO_BOUND (srules)) return NULL;
  lhs = rule->lhs;
  *rhs = (const char **) rule->rhs;
  *abs_node = rule->anode;
  *transl = rule->trans;
  nsrule++;
  return lhs;
}

/* The following function parses grammar desrciption. */
int gp_parse_grammar (struct grammar *g, bool strict_p, const char *description)
{
  int code;

  assert (g != NULL);
  if ((code = set_sgrammar (g, description)) != 0) return code;
  code = gp_read_grammar (g, strict_p, sread_terminal, sread_rule);
  free_sgrammar ();
  return code;
}
