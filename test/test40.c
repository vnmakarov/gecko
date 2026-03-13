/* This file is a part of Gecko (a GLR parser) project.
   Copyright (C) 2026 Vladimir Makarov <vmakarov.gcc@gmail.com>.
*/

#include <limits.h>
#include <stdio.h>
#include <stddef.h>
#include <ctype.h>
#include <string.h>

#define input input_unused
#include "common.h"
#undef input

#include "allocate.h"
#include "objstack.h"
#include "hashtab.h"
#include "ticker.h"

#include "ansic.h"
#include "gecko.h"

static os_t lexs;
static struct lex *list;
static struct lex *curr = NULL;

int column = 0;
int line = 1;

static hash_table_t table;

static uint64_t hash (hash_table_entry_t el) {
  const char *id = (char *) el;
  unsigned result, i;

  for (result = i = 0; *id++ != '\0'; i++) result += ((unsigned char) *id << (i % CHAR_BIT));
  return result;
}

static bool eq (hash_table_entry_t el1, hash_table_entry_t el2) {
  return strcmp ((char *) el1, (char *) el2) == 0;
}

static void initiate_typedefs (gp_allocator_t *alloc) { table = create_hash_table (alloc, 50000, hash, eq); }

/* Now we ignore level */
static void add_typedef (const char *id, int level) {
  hash_table_entry_t *entry_ptr;

  assert (level == 0);
  entry_ptr = find_hash_table_entry (table, id, 1);
  if (*entry_ptr == NULL)
    *entry_ptr = (hash_table_entry_t) id;
  else
    assert (strcmp (id, *entry_ptr) == 0);
#ifdef DEBUG
  fprintf (stderr, "add typedef %s\n", id);
#endif
}

#ifdef __GNUC__
inline
#endif
  static int find_typedef (const char *id, int level) {
  hash_table_entry_t *entry_ptr;

  entry_ptr = find_hash_table_entry (table, id, 0);
#ifdef DEBUG
  if (*entry_ptr != NULL) fprintf (stderr, "found typedef %s\n", id);
#endif
  return *entry_ptr != NULL;
}

int get_lex (void) {
  if (curr == NULL)
    curr = list;
  else
    curr = curr->next;
  if (curr == NULL) return 0;
  line = curr->line;
  column = curr->column;
  if (curr->code == IDENTIFIER)
    return IDENTIFIER;
  else
    return curr->code;
}

#define yylex yylex1

#include "ansic.c"

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
    if (code == IDENTIFIER) {
      OS_TOP_ADD_MEMORY (lexs, yytext, strlen (yytext) + 1);
      lex.id = OS_TOP_BEGIN (lexs);
      OS_TOP_FINISH (lexs);
    } else
      lex.id = NULL;
    lex.code = code;
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

/* The following function imported by Gecko (see comments in the interface file). */
static int test_read_token_from_lex (void **attr) {
  int code;

  *attr = (void *) (ptrdiff_t) line;
  code = get_lex ();
  if (code <= 0) return -1;
  return code;
}

static const char *description
  = "TERM\n"
    "IDENTIFIER = 300\n"
    "SIGNED = 301\n"
    "CONST = 302\n"
    "INLINE = 303\n"
    "AUTO = 304\n"
    "BREAK = 305\n"
    "CASE = 306\n"
    "CHAR = 307\n"
    "CONTINUE = 308\n"
    "DEFAULT = 310\n"
    "DO = 311\n"
    "DOUBLE = 312\n"
    "ELSE = 313\n"
    "ENUM = 314\n"
    "EXTERN = 315\n"
    "FLOAT = 316\n"
    "FOR = 317\n"
    "GOTO = 318\n"
    "IF = 320\n"
    "INT = 321\n"
    "LONG = 322\n"
    "REGISTER = 323\n"
    "RETURN = 324\n"
    "SHORT = 325\n"
    "SIZEOF = 326\n"
    "STATIC = 328\n"
    "STRUCT = 330\n"
    "SWITCH = 331\n"
    "TYPEDEF = 332\n"
    "UNION = 333\n"
    "UNSIGNED = 334\n"
    "VOID = 335\n"
    "VOLATILE = 336\n"
    "WHILE = 337\n"
    "CONSTANT = 338\n"
    "STRING_LITERAL = 340\n"
    "RIGHT_ASSIGN = 341\n"
    "LEFT_ASSIGN = 342\n"
    "ADD_ASSIGN = 343\n"
    "SUB_ASSIGN = 344\n"
    "MUL_ASSIGN = 345\n"
    "DIV_ASSIGN = 346\n"
    "MOD_ASSIGN = 347\n"
    "AND_ASSIGN = 348\n"
    "XOR_ASSIGN = 350\n"
    "OR_ASSIGN = 351\n"
    "RIGHT_OP = 352\n"
    "LEFT_OP = 353\n"
    "INC_OP = 354\n"
    "DEC_OP = 355\n"
    "PTR_OP = 356\n"
    "AND_OP = 357\n"
    "OR_OP = 358\n"
    "LE_OP = 360\n"
    "GE_OP = 361\n"
    "EQ_OP = 362\n"
    "NE_OP = 363\n"
    "ELIPSIS = 364\n"
    "RESTRICT = 365\n"
    "_BOOL = 366\n"
    "_COMPLEX = 367\n"
    "_IMAGINARY = 368\n"
    "\n"
    "/* Additional rules: */\n"
    "\n"
    "start : translation_unit\n"
    "      ;\n"
    "\n"
    "identifier : IDENTIFIER\n"
    "           ;\n"
    "\n"
    "constant : CONSTANT\n"
    "         ;\n"
    "\n"
    "string_literal : STRING_LITERAL\n"
    "               ;\n"
    "\n"
    "/* A.2  Phrase structure grammar: */\n"
    "/* A.2.1  Expressions: */\n"
    "/* (6.5.1): */\n"
    "primary_expression : identifier\n"
    "                   | constant\n"
    "                   | string_literal\n"
    "                   | '(' expression ')'\n"
    "                   ;\n"
    "/* (6.5.2): */\n"
    "/* postfix_expression : primary_expression\n"
    "                   | postfix_expression '[' expression ']'\n"
    "                   | postfix_expression '(' [argument_expression_list] ')'\n"
    "                   | postfix_expression '.' identifier\n"
    "                   | postfix_expression PTR_OP identifier\n"
    "                   | postfix_expression INC_OP\n"
    "                   | postfix_expression DEC_OP\n"
    "                   | '(' type_name ')' '{' initializer_list '}'\n"
    "                   | '(' type_name ')' '{' initializer_list ',' '}' */\n"
    "\n"
    "postfix_expression : primary_expression\n"
    "                   | postfix_expression '[' expression ']'\n"
    "                   | postfix_expression '(' argument_expression_list_opt ')'\n"
    "                   | postfix_expression '.' identifier\n"
    "                   | postfix_expression PTR_OP identifier\n"
    "                   | postfix_expression INC_OP\n"
    "                   | postfix_expression DEC_OP\n"
    "                   | '(' type_name ')' '{' initializer_list '}'\n"
    "                   | '(' type_name ')' '{' initializer_list ',' '}'\n"
    "                   ;\n"
    "\n"
    "argument_expression_list_opt :\n"
    "                             | argument_expression_list\n"
    "                             ;\n"
    "/* (6.5.2): */\n"
    "argument_expression_list : assignment_expression\n"
    "                         | argument_expression_list ',' assignment_expression\n"
    "                         ;\n"
    "\n"
    "/* (6.5.3): */\n"
    "unary_expression : postfix_expression\n"
    "                 | INC_OP unary_expression\n"
    "                 | DEC_OP unary_expression\n"
    "                 | unary_operator  cast_expression\n"
    "                 | SIZEOF unary_expression\n"
    "                 | SIZEOF '(' type_name ')'\n"
    "                 ;\n"
    "\n"
    "/* (6.5.3): */\n"
    "unary_operator : '&'\n"
    "               | '*'\n"
    "               | '+'\n"
    "               | '-'\n"
    "               | '~'\n"
    "               | '!'\n"
    "               ;\n"
    "\n"
    "/* (6.5.4): */\n"
    "cast_expression : unary_expression\n"
    "                | '(' type_name ')' cast_expression\n"
    "                ;\n"
    "\n"
    "/* (6.5.5): */\n"
    "multiplicative_expression : cast_expression\n"
    "                          | multiplicative_expression '*' cast_expression\n"
    "                          | multiplicative_expression '/' cast_expression\n"
    "                          | multiplicative_expression '%' cast_expression\n"
    "                          ;\n"
    "\n"
    "/* (6.5.6): */\n"
    "additive_expression : multiplicative_expression\n"
    "                    | additive_expression '+' multiplicative_expression\n"
    "                    | additive_expression '-' multiplicative_expression\n"
    "                    ;\n"
    "\n"
    "/* (6.5.7): */\n"
    "shift_expression : additive_expression\n"
    "                 | shift_expression LEFT_OP additive_expression\n"
    "                 | shift_expression RIGHT_OP additive_expression\n"
    "                 ;\n"
    "\n"
    "/* (6.5.8): */\n"
    "relational_expression : shift_expression\n"
    "                      | relational_expression '<' shift_expression\n"
    "                      | relational_expression '>' shift_expression\n"
    "                      | relational_expression LE_OP shift_expression\n"
    "                      | relational_expression GE_OP shift_expression\n"
    "                      ;\n"
    "\n"
    "/* (6.5.9): */\n"
    "equality_expression : relational_expression\n"
    "                    | equality_expression EQ_OP relational_expression\n"
    "                    | equality_expression NE_OP relational_expression\n"
    "                    ;\n"
    "\n"
    "/* (6.5.10): */\n"
    "AND_expression : equality_expression\n"
    "               | AND_expression '&' equality_expression\n"
    "               ;\n"
    "\n"
    "/* (6.5.11): */\n"
    "exclusive_OR_expression : AND_expression\n"
    "                        | exclusive_OR_expression '^' AND_expression\n"
    "                        ;\n"
    "\n"
    "/* (6.5.12): */\n"
    "inclusive_OR_expression : exclusive_OR_expression\n"
    "                        | inclusive_OR_expression '|' exclusive_OR_expression\n"
    "                        ;\n"
    "\n"
    "/* (6.5.13): */\n"
    "logical_AND_expression : inclusive_OR_expression\n"
    "                       | logical_AND_expression AND_OP inclusive_OR_expression\n"
    "                       ;\n"
    "\n"
    "/* (6.5.14): */\n"
    "logical_OR_expression : logical_AND_expression\n"
    "                      | logical_OR_expression OR_OP logical_AND_expression\n"
    "                      ;\n"
    "\n"
    "/* (6.5.15): */\n"
    "conditional_expression : logical_OR_expression\n"
    "                       | logical_OR_expression '?' expression ':' conditional_expression\n"
    "                       ;\n"
    "\n"
    "/* (6.5.16): */\n"
    "assignment_expression : conditional_expression\n"
    "                      | unary_expression  assignment_operator  assignment_expression\n"
    "                      ;\n"
    "\n"
    "/* (6.5.16): */\n"
    "assignment_operator :  '='\n"
    "                    |  MUL_ASSIGN\n"
    "                    |  DIV_ASSIGN\n"
    "                    |  MOD_ASSIGN\n"
    "                    |  ADD_ASSIGN\n"
    "                    |  SUB_ASSIGN\n"
    "                    |  LEFT_ASSIGN\n"
    "                    |  RIGHT_ASSIGN\n"
    "                    |  AND_ASSIGN\n"
    "                    |  XOR_ASSIGN\n"
    "                    |  OR_ASSIGN\n"
    "                    ;\n"
    "\n"
    "/* (6.5.17): */\n"
    "expression : assignment_expression\n"
    "           | expression ',' assignment_expression\n"
    "           ;\n"
    "\n"
    "/* (6.6): */\n"
    "constant_expression : conditional_expression\n"
    "                    ;\n"
    "\n"
    "/* A.2.2  Declarations: */\n"
    "/* (6.7): */\n"
    "/* declaration : declaration_specifiers [init_declarator_list] ';' */\n"
    "               \n"
    "declaration : declaration_specifiers init_declarator_list_opt ';'\n"
    "            ;\n"
    "\n"
    "init_declarator_list_opt :\n"
    "                         | init_declarator_list\n"
    "                         ;\n"
    "\n"
    "/* (6.7): */\n"
    "/* declaration_specifiers : storage_class_specifier  [declaration_specifiers]\n"
    "   	               | type_specifier  [declaration_specifiers]\n"
    "                       | type_qualifier  [declaration_specifiers]\n"
    "                       | function_specifier  [declaration_specifiers] */\n"
    "\n"
    "declaration_specifiers : storage_class_specifier  declaration_specifiers_opt\n"
    "   	               | type_specifier  declaration_specifiers_opt\n"
    "                       | type_qualifier  declaration_specifiers_opt\n"
    "                       | function_specifier  declaration_specifiers_opt\n"
    "                       ;\n"
    "\n"
    "declaration_specifiers_opt :\n"
    "                           | declaration_specifiers\n"
    "                           ;\n"
    "\n"
    "/* (6.7): */\n"
    "init_declarator_list : init_declarator\n"
    "                     | init_declarator_list ',' init_declarator\n"
    "                     ;\n"
    "\n"
    "/* (6.7): */\n"
    "init_declarator : declarator\n"
    "                | declarator '=' initializer\n"
    "                ;\n"
    "/* (6.7.1): */\n"
    "storage_class_specifier : TYPEDEF\n"
    "                        | EXTERN\n"
    "                        | STATIC\n"
    "                        | AUTO\n"
    "	                | REGISTER\n"
    "                        ;\n"
    "\n"
    "/* (6.7.2): */\n"
    "type_specifier : VOID\n"
    "               | CHAR\n"
    "               | SHORT\n"
    "               | INT\n"
    "               | LONG\n"
    "               | FLOAT\n"
    "               | DOUBLE\n"
    "               | SIGNED\n"
    "               | UNSIGNED\n"
    "               | _BOOL\n"
    "               | _COMPLEX\n"
    "               | _IMAGINARY\n"
    "               | struct_or_union_specifier\n"
    "               | enum_specifier\n"
    "               | typedef_name\n"
    "               ;\n"
    "\n"
    "/* (6.7.2.1): */\n"
    "/* struct_or_union_specifier : struct_or_union  [identifier]\n"
    "                                 '{' struct_declaration_list '}'\n"
    "                          | struct_or_union  identifier */\n"
    "\n"
    "struct_or_union_specifier : struct_or_union  identifier_opt\n"
    "                                 '{' struct_declaration_list '}'\n"
    "                          | struct_or_union  identifier\n"
    "                          ;\n"
    "\n"
    "identifier_opt :\n"
    "               | identifier\n"
    "               ;\n"
    "\n"
    "/* (6.7.2.1): */\n"
    "struct_or_union : STRUCT\n"
    "                | UNION\n"
    "                ;\n"
    "\n"
    "/* (6.7.2.1): */\n"
    "struct_declaration_list : struct_declaration\n"
    "                        | struct_declaration_list  struct_declaration\n"
    "                        ;\n"
    "\n"
    "/* (6.7.2.1): */\n"
    "struct_declaration : specifier_qualifier_list  struct_declarator_list ';'\n"
    "                   ;\n"
    "\n"
    "/* (6.7.2.1): */\n"
    "/* specifier_qualifier_list : type_specifier  [specifier_qualifier_list]\n"
    "                         | type_qualifier  [specifier_qualifier_list] */\n"
    "\n"
    "specifier_qualifier_list : type_specifier  specifier_qualifier_list_opt\n"
    "                         | type_qualifier  specifier_qualifier_list_opt\n"
    "                         ;\n"
    "\n"
    "specifier_qualifier_list_opt : \n"
    "                             | specifier_qualifier_list\n"
    "                             ;\n"
    "\n"
    "/* (6.7.2.1): */\n"
    "struct_declarator_list : struct_declarator\n"
    "                       | struct_declarator_list ',' struct_declarator\n"
    "                       ;\n"
    "\n"
    "/* (6.7.2.1): */\n"
    "/* struct_declarator : declarator\n"
    "                  | [declarator] ':' constant_expression */\n"
    "\n"
    "struct_declarator : declarator\n"
    "                  | declarator_opt ':' constant_expression\n"
    "                  ;\n"
    "\n"
    "declarator_opt :\n"
    "               | declarator\n"
    "               ;\n"
    "\n"
    "/* (6.7.2.2): */\n"
    "enum_specifier : ENUM identifier_opt '{' enumerator_list '}'\n"
    "               | ENUM identifier_opt '{' enumerator_list ',' '}'\n"
    "               | ENUM identifier\n"
    "               ;\n"
    "\n"
    "/* (6.7.2.2): */\n"
    "enumerator_list : enumerator\n"
    "                | enumerator_list ',' enumerator\n"
    "                ;\n"
    "\n"
    "/* (6.7.2.2): */\n"
    "enumerator : enumeration_constant\n"
    "           | enumeration_constant '=' constant_expression\n"
    "           ;\n"
    "\n"
    "/* (6.7.3): */\n"
    "type_qualifier : CONST\n"
    "               | RESTRICT\n"
    "               | VOLATILE\n"
    "               ;\n"
    "\n"
    "/* (6.7.4): */\n"
    "function_specifier : INLINE\n"
    "                   ;\n"
    "\n"
    "/* (6.7.5): */\n"
    "/* declarator : [pointer] direct_declarator */\n"
    "\n"
    "declarator : pointer_opt direct_declarator\n"
    "           ;\n"
    "\n"
    "pointer_opt :\n"
    "            | pointer\n"
    "            ;\n"
    "/* (6.7.5): */\n"
    "/* direct_declarator : identifier\n"
    "                  | '(' declarator ')'\n"
    "                  | direct_declarator '[' [type_qualifier_list] [assignment_expression] ']'\n"
    "                  | direct_declarator '[' STATIC [type_qualifier_list] assignment_expression "
    "']'\n"
    "                  | direct_declarator '[' type_qualifier_list STATIC assignment_expression "
    "']'\n"
    "                  | direct_declarator '[' [type_qualifier_list] '*' ']'\n"
    "                  | direct_declarator '(' parameter_type_list ')'\n"
    "                  | direct_declarator '(' [identifier_list] ')' */\n"
    "\n"
    "direct_declarator : identifier\n"
    "                  | '(' declarator ')'\n"
    "                  | direct_declarator '[' type_qualifier_list_opt assignment_expression_opt "
    "']'\n"
    "                  | direct_declarator '[' STATIC type_qualifier_list_opt "
    "assignment_expression ']'\n"
    "                  | direct_declarator '[' type_qualifier_list STATIC assignment_expression "
    "']'\n"
    "                  | direct_declarator '[' type_qualifier_list_opt '*' ']'\n"
    "                  | direct_declarator '(' parameter_type_list ')'\n"
    "                  | direct_declarator '(' identifier_list_opt ')'\n"
    "                  ;\n"
    "\n"
    "type_qualifier_list_opt :\n"
    "                        | type_qualifier_list\n"
    "                        ;\n"
    "\n"
    "identifier_list_opt :\n"
    "                    | identifier_list\n"
    "                    ;\n"
    "\n"
    "/* (6.7.5): */\n"
    "pointer : '*' type_qualifier_list_opt\n"
    "        | '*' type_qualifier_list_opt pointer\n"
    "        ;\n"
    "\n"
    "/* (6.7.5): */\n"
    "type_qualifier_list : type_qualifier\n"
    "                    | type_qualifier_list  type_qualifier\n"
    "                    ;\n"
    "\n"
    "/* (6.7.5): */\n"
    "parameter_type_list : parameter_list\n"
    "                    | parameter_list ',' ELIPSIS\n"
    "                    ;\n"
    "\n"
    "/* (6.7.5): */\n"
    "parameter_list : parameter_declaration\n"
    "               | parameter_list ',' parameter_declaration\n"
    "               ;\n"
    "\n"
    "/* (6.7.5): */\n"
    "/* parameter_declaration : declaration_specifiers declarator\n"
    "                      | declaration_specifiers [abstract_declarator] */\n"
    "\n"
    "parameter_declaration : declaration_specifiers declarator\n"
    "                      | declaration_specifiers abstract_declarator_opt\n"
    "                      ;\n"
    "\n"
    "abstract_declarator_opt :\n"
    "                        | abstract_declarator\n"
    "                        ;\n"
    "\n"
    "/* (6.7.5): */\n"
    "identifier_list : identifier\n"
    "                | identifier_list ',' identifier\n"
    "                ;\n"
    "\n"
    "/* (6.7.6): */\n"
    "type_name: specifier_qualifier_list  abstract_declarator_opt\n"
    "         ;\n"
    "\n"
    "/* (6.7.6): */\n"
    "abstract_declarator : pointer\n"
    "                    | pointer_opt direct_abstract_declarator\n"
    "                     ;\n"
    "\n"
    "/* (6.7.6): */\n"
    "/* direct_abstract_declarator : '(' abstract_declarator ')'\n"
    "                           | [direct_abstract_declarator] '[' [assignment_expression] ']'\n"
    "                           | [direct_abstract_declarator] '[' '*' ']'\n"
    "                           | [direct_abstract_declarator] '(' [parameter_type_list] ')' */\n"
    "\n"
    "direct_abstract_declarator : '(' abstract_declarator ')'\n"
    "                           | direct_abstract_declarator_opt '[' assignment_expression_opt "
    "']'\n"
    "                           | direct_abstract_declarator_opt '[' '*' ']'\n"
    "                           | direct_abstract_declarator_opt '(' parameter_type_list_opt ')'\n"
    "                           ;\n"
    "\n"
    "direct_abstract_declarator_opt :\n"
    "                               | direct_abstract_declarator\n"
    "                               ;\n"
    "\n"
    "assignment_expression_opt :\n"
    "                          | assignment_expression\n"
    "                          ;\n"
    "\n"
    "parameter_type_list_opt :\n"
    "                        | parameter_type_list\n"
    "                        ;\n"
    "\n"
    "/* (6.7.7): */\n"
    "typedef_name : identifier\n"
    "             ;\n"
    "\n"
    "/* (6.7.8): */\n"
    "initializer : assignment_expression\n"
    "            | '{' initializer_list '}'\n"
    "            | '{' initializer_list ',' '}'\n"
    "            ;\n"
    "\n"
    "/* (6.7.8): */\n"
    "/* initializer_list : [designation] initializer\n"
    "                 | initializer_list ',' [designation] initializer */\n"
    "\n"
    "initializer_list : designation_opt initializer\n"
    "                 | initializer_list ',' designation_opt initializer\n"
    "                 ;\n"
    "\n"
    "designation_opt :\n"
    "                | designation\n"
    "                ;\n"
    "\n"
    "/* (6.7.8): */\n"
    "designation : designator_list '='\n"
    "            ;\n"
    "\n"
    "/* (6.7.8): */\n"
    "designator_list : designator\n"
    "                | designator_list  designator\n"
    "                ;\n"
    "\n"
    "/* (6.7.8): */\n"
    "designator : '[' constant_expression ']'\n"
    "           | '.' identifier\n"
    "           ;\n"
    "\n"
    "/* A.2.3  Statements: */\n"
    "/* (6.8): */\n"
    "statement : labeled_statement\n"
    "          | compound_statement\n"
    "          | expression_statement\n"
    "          | selection_statement\n"
    "          | iteration_statement\n"
    "          | jump_statement\n"
    "          ;\n"
    "\n"
    "/* (6.8.1): */\n"
    "labeled_statement : identifier ':' statement\n"
    "                  | CASE constant_expression ':' statement\n"
    "                  | DEFAULT ':' statement\n"
    "                  ;\n"
    "\n"
    "/* (6.8.2): */\n"
    "/* compound_statement : '{' [block_item_list] '}' */\n"
    "\n"
    "compound_statement : '{' block_item_list_opt '}'\n"
    "                   ;\n"
    "\n"
    "block_item_list_opt :\n"
    "                    | block_item_list\n"
    "                    ;\n"
    "\n"
    "/* (6.8.2): */\n"
    "block_item_list : block_item\n"
    "                | block_item_list  block_item\n"
    "                ;\n"
    "\n"
    "/* (6.8.2): */\n"
    "block_item : declaration\n"
    "           | statement\n"
    "           ;\n"
    "\n"
    "/* (6.8.3): */\n"
    "/* expression_statement : [expression] ';' */\n"
    "\n"
    "expression_statement : expression_opt ';'\n"
    "                     ;\n"
    "expression_opt :\n"
    "               | expression\n"
    "               ;\n"
    "\n"
    "/* (6.8.4): */\n"
    "selection_statement : IF '(' expression ')' statement\n"
    "                    | IF '(' expression ')' statement ELSE statement\n"
    "                    | SWITCH '(' expression ')' statement\n"
    "                    ;\n"
    "\n"
    "/* (6.8.5): */\n"
    "iteration_statement : WHILE '(' expression ')' statement\n"
    "                    | DO statement WHILE '(' expression ')' ';'\n"
    "                    | FOR '(' expression_opt ';' expression_opt ';' expression_opt ')' "
    "statement\n"
    "                    | FOR '(' declaration  expression_opt ';' expression_opt ')' statement\n"
    "                    ;\n"
    "\n"
    "/* (6.8.6): */\n"
    "jump_statement : GOTO identifier ';'\n"
    "               | CONTINUE ';'\n"
    "               | BREAK ';'\n"
    "               | RETURN expression_opt ';'\n"
    "               ;\n"
    "\n"
    "/* A.2.4  External definitions: */\n"
    "/* (6.9): */\n"
    "translation_unit : external_declaration\n"
    "                 | translation_unit external_declaration\n"
    "                 ;\n"
    "\n"
    "/* (6.9): */\n"
    "external_declaration : function_definition\n"
    "                     | declaration\n"
    "                     ;\n"
    "\n"
    "/* (6.9.1): */\n"
    "/* function_definition : declaration_specifiers declarator  [declaration_list] "
    "compound_statement */\n"
    "\n"
    "function_definition : declaration_specifiers declarator  declaration_list_opt "
    "compound_statement\n"
    "                   ;\n"
    "\n"
    "declaration_list_opt :\n"
    "                     | declaration_list\n"
    "                     ;\n"
    "\n"
    "/* (6.9.1): */\n"
    "declaration_list : declaration\n"
    "                 | declaration_list  declaration\n"
    "                 ;\n"
    "\n"
    "/* A.1.5  Constants: */\n"
    "/* (6.4.4.3): */\n"
    "enumeration_constant : identifier\n"
    "                     ;\n";

#ifdef linux
#include <unistd.h>
#endif

int main (int argc, char **argv) {
  ticker_t t;
  int code;
  int ambiguity;
  struct gp_tree_node *root;
  struct grammar *g;
#ifdef linux
  char *start = sbrk (0);
#endif

  gp_allocator_t *alloc = gp_alloc_new (NULL, NULL, NULL, NULL);
  if (alloc == NULL) {
    exit (1);
  }
  store_lexs (alloc);
  initiate_typedefs (alloc);
  curr = NULL;
  t = create_ticker ();
  if ((g = gp_create_grammar ()) == NULL) {
    fprintf (stderr, "gp_create_grammar: No memory\n");
    exit (1);
  }
  if (argc > 1)
    gp_set_debug_level (g, atoi (argv[1]));
  else
    gp_set_debug_level (g, 3);

  if (gp_parse_grammar (g, 1, description) != 0) {
    fprintf (stderr, "%s\n", gp_error_message (g));
    exit (1);
  }
  gp_set_parse_alloc (g, test_parse_alloc);
  gp_set_parse_free (g, test_parse_free);
  gp_set_syntax_error (g, test_syntax_error);
  if (gp_parse (g, test_read_token_from_lex, &root, &ambiguity)) {
    fprintf (stderr, "gp_parse: %s\n", gp_error_message (g));
    exit (1);
  }
  gp_fin (g);
#ifdef linux
  printf ("all time %.2f, memory=%.1fkB\n", active_time (t), ((char *) sbrk (0) - start) / 1024.);
#else
  printf ("all time %.2f\n", active_time (t));
#endif
  gp_alloc_del (alloc);
  exit (0);
}
