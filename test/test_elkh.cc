#include <limits.h>
#include <stdio.h>
#include <stddef.h>
#include <ctype.h>
#include <string.h>
#include "objstack.h"
#include "hashtab.h"
#include "ticker.h"

#define TOK_EOF 0
#define LPAR 1
#define RPAR 2
#define LBR 3
#define RBR 4
#define LFBR 5
#define RFBR 6
#define DOT 7
#define COMMA 8
#define AND 9
#define MUL 10
#define PLUS 11
#define MINUS 12
#define TILDA 13
#define EXCLAM 14
#define DIV 15
#define MOD 16
#define LT 17
#define GT 18
#define XOR 19
#define OR 20
#define QUEST 21
#define COL 22
#define EQ 23
#define SEMICOL 24

#define BASE 300

#define IDENTIFIER (BASE + 25)
#define SIGNED (BASE + 26)
#define CONST (BASE + 27)
#define INLINE (BASE + 28)
#define AUTO (BASE + 29)
#define BREAK (BASE + 30)
#define CASE (BASE + 31)
#define CHAR (BASE + 32)
#define CONTINUE (BASE + 33)
#define DEFAULT (BASE + 34)
#define DO (BASE + 35)
#define DOUBLE (BASE + 36)
#define ELSE (BASE + 37)
#define ENUM (BASE + 38)
#define EXTERN (BASE + 39)
#define FLOAT (BASE + 40)
#define FOR (BASE + 41)
#define GOTO (BASE + 42)
#define IF (BASE + 43)
#define INT (BASE + 44)
#define LONG (BASE + 45)
#define REGISTER (BASE + 46)
#define RETURN (BASE + 47)
#define SHORT (BASE + 48)
#define SIZEOF (BASE + 49)
#define STATIC (BASE + 50)
#define STRUCT (BASE + 51)
#define SWITCH (BASE + 52)
#define TYPEDEF (BASE + 53)
#define UNION (BASE + 54)
#define UNSIGNED (BASE + 55)
#define VOID (BASE + 56)
#define VOLATILE (BASE + 57)
#define WHILE (BASE + 58)
#define CONSTANT (BASE + 59)
#define STRING_LITERAL (BASE + 60)
#define RIGHT_ASSIGN (BASE + 61)
#define LEFT_ASSIGN (BASE + 62)
#define ADD_ASSIGN (BASE + 63)
#define SUB_ASSIGN (BASE + 64)
#define MUL_ASSIGN (BASE + 65)
#define DIV_ASSIGN (BASE + 66)
#define MOD_ASSIGN (BASE + 67)
#define AND_ASSIGN (BASE + 68)
#define XOR_ASSIGN (BASE + 69)
#define OR_ASSIGN (BASE + 70)
#define RIGHT_OP (BASE + 71)
#define LEFT_OP (BASE + 72)
#define INC_OP (BASE + 73)
#define DEC_OP (BASE + 74)
#define PTR_OP (BASE + 75)
#define AND_OP (BASE + 76)
#define OR_OP (BASE + 77)
#define LE_OP (BASE + 78)
#define GE_OP (BASE + 79)
#define EQ_OP (BASE + 80)
#define NE_OP (BASE + 81)
#define ELIPSIS (BASE + 82)
#define RESTRICT (BASE + 83)
#define _BOOL (BASE + 84)
#define _COMPLEX (BASE + 85)
#define _IMAGINARY (BASE + 86)

extern "C" {
#include "test_common.c"

int get_lex (void) {
  if (curr == NULL)
    curr = list;
  else
    curr = curr->next;
  if (curr == NULL) return TOK_EOF;
  line = curr->line;
  column = curr->column;
  if (curr->code == IDENTIFIER)
    return IDENTIFIER;
  else
    return curr->code;
}

#define yylex yylex1

#include "lex.yy.c"

static void store_lexs (gp_allocator_t *alloc) {
  struct lex lex, *prev;
  int code;
#ifdef DEBUG
  int nt = 0;
#endif

  OS_CREATE (lexs, alloc, 0);
  list = NULL;
  prev = NULL;
  while ((code = yylex ()) > 0) {
#ifdef DEBUG
    nt++;
#endif
    int eh_code;
    switch (code) {
    case '(': eh_code = LPAR; break;
    case ')': eh_code = RPAR; break;
    case '[': eh_code = LBR; break;
    case ']': eh_code = RBR; break;
    case '{': eh_code = LFBR; break;
    case '}': eh_code = RFBR; break;
    case '.': eh_code = DOT; break;
    case ',': eh_code = COMMA; break;
    case '&': eh_code = AND; break;
    case '*': eh_code = MUL; break;
    case '+': eh_code = PLUS; break;
    case '-': eh_code = MINUS; break;
    case '~': eh_code = TILDA; break;
    case '!': eh_code = EXCLAM; break;
    case '/': eh_code = DIV; break;
    case '%': eh_code = MOD; break;
    case '<': eh_code = LT; break;
    case '>': eh_code = GT; break;
    case '^': eh_code = XOR; break;
    case '|': eh_code = OR; break;
    case '?': eh_code = QUEST; break;
    case ':': eh_code = COL; break;
    case '=': eh_code = EQ; break;
    case ';': eh_code = SEMICOL; break;
    default: eh_code = code - BASE; break;
    }
    if (code == IDENTIFIER) {
      OS_TOP_ADD_MEMORY (lexs, yytext, strlen (yytext) + 1);
      lex.id = (const char *) OS_TOP_BEGIN (lexs);
      OS_TOP_FINISH (lexs);
    } else
      lex.id = NULL;
    lex.code = eh_code;
    lex.line = line;
    lex.column = column;
    lex.next = NULL;
    OS_TOP_ADD_MEMORY (lexs, &lex, sizeof (lex));
    if (prev == NULL)
      prev = list = (struct lex *) OS_TOP_BEGIN (lexs);
    else {
      prev = prev->next = (struct lex *) OS_TOP_BEGIN (lexs);
    }
    OS_TOP_FINISH (lexs);
  }
#ifdef DEBUG
  fprintf (stderr, "%d tokens\n", nt);
#endif
}
}

#include "lexerint.h"
#include "elkh-c.h"
#include "glr.h"
#include "elkh-c.cc"

class Lexer : public LexerInterface {
 public:
  static void nextToken (LexerInterface *lex);
  virtual NextTokenFunc getTokenFunc () const { return &Lexer::nextToken; }

  string tokenDesc () const;
  string tokenKindDesc (int kind) const;
};

void Lexer::nextToken (LexerInterface *lex) {
  lex->type = get_lex ();
  lex->sval = (SemanticValue) (void *) (ptrdiff_t) line;
  return;
}

string Lexer::tokenDesc () const { return tokenKindDesc (type); }

string Lexer::tokenKindDesc (int kind) const {
  switch (kind) {
  case TOK_EOF: return "EOF";
  default: return termNames[kind];
  }
}

/* All parse_alloc memory is contained here. */
static os_t mem_os;

int main (void) {
  ticker_t t = create_ticker ();
  int code;
  struct grammar *g;

  gp_allocator_t *alloc = gp_alloc_new (NULL, NULL, NULL, NULL);
  if (alloc == NULL) {
    exit (1);
  }
  OS_CREATE (mem_os, alloc, 0);
  t = create_ticker ();
  store_lexs (alloc);
#ifdef linux
  printf ("scanner time %.6f, memory=%.1fkB\n", active_time (t),
          get_peak_heap_size () / 1024.);
#else
  printf ("scanner time %.6f\n", active_time (t));
#endif
  initiate_typedefs (alloc);
  curr = NULL;

  Lexer lexer;
  lexer.nextToken (&lexer);
  Context cont;
  GLR glr (&cont, cont.makeTables ());

  glr.noisyFailedParse = false;

  SemanticValue result;
  int parser_res = 0;
  if (!glr.glrParse (lexer, result)) {
    printf ("parse error\n");
    parser_res = 1;
  }
  if (parser_res) {
    OS_DELETE (mem_os);
    exit (1);
  }
#ifdef linux
  printf ("parse time %.6f, memory=%.1fkB\n", active_time (t), get_peak_heap_size () / 1024.);
#else
  printf ("parse time %.6f\n", active_time (t));
#endif
  return 0;
}
