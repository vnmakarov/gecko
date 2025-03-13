/* This file is a part of Gecko Parser (GLR Parser) project.
   Copyright (C) 2025 Vladimir Makarov <vmakarov.gcc@gmail.com>.
*/

#ifndef GP_TEST_ANSIC_H_
#define GP_TEST_ANSIC_H_

#define IDENTIFIER 300
#define SIGNED 301
#define CONST 302
#define INLINE 303
#define AUTO 304
#define BREAK 305
#define CASE 306
#define CHAR 307
#define CONTINUE 308
#define DEFAULT 310
#define DO 311
#define DOUBLE 312
#define ELSE 313
#define ENUM 314
#define EXTERN 315
#define FLOAT 316
#define FOR 317
#define GOTO 318
#define IF 320
#define INT 321
#define LONG 322
#define REGISTER 323
#define RETURN 324
#define SHORT 325
#define SIZEOF 326
#define STATIC 328
#define STRUCT 330
#define SWITCH 331
#define TYPEDEF 332
#define UNION 333
#define UNSIGNED 334
#define VOID 335
#define VOLATILE 336
#define WHILE 337
#define CONSTANT 338
#define STRING_LITERAL 340
#define RIGHT_ASSIGN 341
#define LEFT_ASSIGN 342
#define ADD_ASSIGN 343
#define SUB_ASSIGN 344
#define MUL_ASSIGN 345
#define DIV_ASSIGN 346
#define MOD_ASSIGN 347
#define AND_ASSIGN 348
#define XOR_ASSIGN 350
#define OR_ASSIGN 351
#define RIGHT_OP 352
#define LEFT_OP 353
#define INC_OP 354
#define DEC_OP 355
#define PTR_OP 356
#define AND_OP 357
#define OR_OP 358
#define LE_OP 360
#define GE_OP 361
#define EQ_OP 362
#define NE_OP 363
#define ELIPSIS 364
#define RESTRICT 365
#define _BOOL 366
#define _COMPLEX 367
#define _IMAGINARY 368

struct lex {
  short code;
  short column;
  int line;
  const char *id;
  struct lex *next;
};

extern int column;
extern int line;

extern int yylex (void);
extern char *get_yytext (void);

#endif
