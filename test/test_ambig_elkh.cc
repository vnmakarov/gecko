#include <limits.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include "ticker.h"

#define TOK_EOF 0
#define PLUS 1
#define A 2

#define S10 "+a+a+a+a+a+a+a+a+a+a"
#define S100 S10 S10 S10 S10 S10 S10 S10 S10 S10 S10
static const char *input = "a" S100 S100;

#include "lexerint.h"
#include "elkh-ambig.h"
#include "glr.h"
#include "elkh-ambig.cc"

class Lexer : public LexerInterface {
 public:
  static void nextToken (LexerInterface *lex);
  virtual NextTokenFunc getTokenFunc () const { return &Lexer::nextToken; }

  string tokenDesc () const;
  string tokenKindDesc (int kind) const;
};

static int curr;

void Lexer::nextToken (LexerInterface *lex) {
  switch (input[curr]) {
  case '+': lex->type = PLUS; break;
  case 'a': lex->type = A; break;
  default: lex->type = TOK_EOF; break;
  }
  curr++;
  lex->sval = (SemanticValue) NULL;
  return;
}

string Lexer::tokenDesc () const { return tokenKindDesc (type); }

string Lexer::tokenKindDesc (int kind) const {
  switch (kind) {
  case TOK_EOF: return "EOF";
  default: return termNames[kind];
  }
}

int main (void) {
  ticker_t t;
#ifdef linux
  char *start = (char *) sbrk (0);
#endif
  curr = 0;
  Lexer lexer;
  lexer.nextToken (&lexer);
  Context cont;
  GLR glr (&cont, cont.makeTables ());

  glr.noisyFailedParse = false;

  SemanticValue result;
  int parser_res = 0;
  if (!glr.glrParse (lexer, result)) {
    printf ("parse error\n");
    exit (1);
  }
#ifdef linux
  printf ("parse time %.2f, memory=%.1fkB\n", active_time (t), ((char *) sbrk (0) - start) / 1024.);
#else
  printf ("parse time %.2f\n", active_time (t));
#endif
  return 0;
}
