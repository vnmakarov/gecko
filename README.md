![Gecko](gecko.png)

# Gecko: A standalone GLR parser library
  * Gecko is licensed under the MIT license

## Features

* Parse inputs described by **unambiguous and ambiguous grammars**
* Small and simple
  * x86-64 code size is only 80KB
  * 3.5K C SLOC
* Strong emphasis on good automatic syntax error recovery and error reporting, as this is a complicated feature in other compiler-compilers
  * Requires no grammar modifications
  * Simple interface for custom support of syntax error reporting
* Fast
  * Close to YACC/Bison speed on unambiguous grammars (about 5-10% slower)
  * Processes 2M C lines per second on AMD9900X
  * 2 times faster than [YAEP](https://github.com/vnmakarov/yaep) (the fastest Earley parser I know)
    on unambiguous and moderately ambiguous grammars
  * 2.5 times faster than [ElkHound](https://github.com/WeiDUorg/elkhound) (a famous GLR parser)
    on unambiguous and moderately ambiguous grammars
  * about 10 times faster than ElkHound on highly ambiguous grammars
* Operator grammar support
  * Descriptions of operator priority and associativity analogous to YACC/Bison
* Simple syntax-directed translation
  * Generates **abstract syntax tree** as an output
* Library
  * Can be embedded into other programs
  * A grammar for Gecko can be constructed through function calls or using
    a YACC-like description syntax
  * Very **fast startup** after reading grammar from file or string

## Comparison of major features of compiler-compilers

|                                         | YACC    | Bison   | ElkHound | YAEP     | Gecko    |
|-----------------------------------------|---------|---------|----------|----------|----------|
|Library                                  | No      | No      | No       | Yes      | Yes      |
|CFG grammar                              | LALR(1) | LALR(1)*| ambiguous| ambiguous| ambiguous|
|Operator grammar (priority/associativity)| Yes     | Yes     | Yes      | No       | Yes      |
|Speed independence on grammar size       | Yes     | Yes     | Yes      | No       | Yes      |
|Syntax error recovery                    | Yes     | Yes     | No       | Yes      | Yes      |
|Automatic error recovery                 | No      | No      | -        | No       | Yes      |
|Actions                                  | Yes     | Yes     | Yes      | No       | No       |
|Simple syntax-directed translation       | No      | No      | No       | Yes      | Yes      |
|Generation of all translations           | -       | -       | Yes      | Yes**    | Yes**    |
|Generation of minimal cost translation   | -       | -       | No       | Yes      | No       |

\* Bison is claimed to be a GLR parser but I did not manage to use **ambiguous** C grammar (see speed comparison) for it

\** All alternatives can be generated through the corresponding ElkHound/Gecko merge functions.
   But to generate abstract node trees for alternatives analogous to YAEP, additional non-trivial work needs to be done.

* Additional differences between Gecko and YAEP:
  * Gecko is faster on unambiguous and moderately ambiguous grammars
  * Gecko requires less memory for parsing
  * Gecko memory consumption does not depend on input length (or slightly depends for ambiguous grammars)
  * Gecko permits shorter grammars by supporting operator grammars (operator associativity
    and precedence) analogously to YACC/Bison
  * YAEP is faster on highly ambiguous grammars when all possible translations or minimal cost translation
    are required.  It also generates much smaller DAG for all possible translations for highly ambiguous grammars

# Gecko usage example:

* The following is a small example of how to use Gecko to parse expressions.  We have omitted the functions
  `read_token`, `syntax_error_func`, `parse_alloc_func`, and `parse_free_func` which are needed to provide tokens,
  print syntax error messages, and allocate memory for the parser.

```
static const char *description =
"\n"
"TERM NUMBER;\n"
"LEFT '+';\n"
"LEFT '*';\n"
"E : E         # 0\n"
"  | E '+' E   # plus (0 2)\n"
"  | E '*' E   # mult (0 2)\n"
"  | '(' E ')' # 1\n"
"  | NUMBER    # 0\n"
"  ;\n"
  ;

static void parse (void)
{
  struct grammar *g;
  struct gp_tree_node *root;
  bool ambiguous_p;

  if ((g = gp_create_grammar ()) == NULL) {
      fprintf (stderr, "gp_create_grammar: No memory\n");
      exit (1);
  }
  if (gp_parse_grammar (g, true, description) != 0) {
      fprintf (stderr, "%s\n", gp_error_message (g));
      exit (1);
    }
  if (gp_parse (g, read_token_func, &root, &ambiguous_p))
    fprintf (stderr, "gp_parse: %s\n", gp_error_message (g));
  gp_fin (g);
}
```

## Installing
  * ``cd <srcdir>``
  * ``make``
  * ``make test`` (optional) 
  * ``make bench`` (optional) 
  * ``make install``
    * `gecko.h` && `libgecko.a` will be installed in `/usr/local/include` and `/usr/local/lib`
    * You can change installation path `/usr/local` by using makefile arg `PREFIX`, e.g. `make PREFIX=/usr install`
    
## Speed comparison with YACC, ElkHound, YAEP, and GCC/Clang parsers

* Tested parsers:
  * Berkeley YACC 1.9
  * GCC-15.2
  * Clang-21.1.8
  * YAEP as of Oct. 2015.
  * [ElkHound](https://github.com/WeiDUorg/elkhound) as of 2019-02-17 (a GLR parser)
* Bison is claimed to be a GLR parser but I did not manage to use **ambiguous**
  C grammar (see below) for it.  Therefore and because its results on **unambiguous**
  C grammar are close to YACC's, I excluded it from the tests.
* Grammar:
  * The base test grammar is the **ANSI C** grammar which is mostly
    a left-recursive grammar.
  * For YAEP, ElkHound, and Gecko, the grammar is slightly **ambiguous** as typenames
    are represented with the same kind of token as identifiers.
  * For the YACC description, typename is a separate token type distinct from
    other identifiers.  The YACC description does not contain any actions except
    for a small number needed to give feedback to the scanner on how to treat
    the next identifier (as a typename or regular identifier).
* Scanning test files:
  * We prepare all tokens beforehand in order to exclude scanning time from our benchmark.
  * For YACC, at the scanning stage we do not yet distinguish identifiers and typenames. 
* Tests:
  * The first test is 100K sieve functions, so the resulting file size was 1.5M C lines.
  * The second test is a pre-release version of gcc-4.0 for i686 with all the source
    code combined into one file
    ([source](http://people.csail.mit.edu/smcc/projects/single-file-programs/)).
    The file size was 635K C lines.
  * The C pre-processor was applied to the files.
  * Additional preparations were made for YACC, YAEP, and Gecko:
    * GCC extensions (mostly attributes and asm) were removed from the
      pre-processed files.  The removed code is a tiny and insignificant
      fraction of the entire code.
    * A very small number of identifiers were renamed for the 2nd file to avoid confusing
      the simple YACC actions to distinguish typenames and identifiers.  So the resulting code
      is not correct as C code but it is correct from the syntactic point of view.
* Measurements:
  * The result times are elapsed (wall) times.
  * Memory is peak allocated memory.
  * For GCC and Clang, memory was instead measured as max resident memory reported by ``/usr/bin/time``.
* How to reproduce: please use `make bench`.
* Test machine is AMD Ryzen 9900X with 64GB memory under Fedora Core 43.

## Benchmark Results

* C grammar
  * First file (**1500K** lines) consisting of 100K sieve functions:

|                      | Time (s)        | Max Memory MB        |
|----------------------|----------------:|---------------------:|
|gcc -fsyntax-only     |   2.93          |   1144               |
|gcc -O0               |  43.66          |   6200               |
|clang -fsyntax-only   |   1.95          |    529               |
|clang -O0             |   6.45          |   2176               |
|YACC                  |   0.68          |    339               |
|ElkHound              |   1.27          |    127               |
|YAEP                  |   0.62          |   1152               |
|Gecko                 |   0.71          |    340               |


  * YAEP has very good results as it uses dynamic programming to speedup Earley's parser
    and therefore it works very fast on files consisting of repeating parts.
  * The file below is more realistic for speed comparison.
      
  * Second file (~**500K** lines) -- a whole old gcc:

|                      |  Time (s)       | Max Memory MB        |
|----------------------|----------------:|---------------------:|
|gcc -fsyntax-only     |   0.73          |    283               |
|gcc -O0               |   8.52          |    881               |
|clang -fsyntax-only   |   0.69          |    223               |
|clang -O0             |   2.08          |    470               |
|YACC                  |   0.26          |    123               |
|ElkHound              |   0.73          |    127               |
|YAEP                  |   0.66          |    471               |
|Gecko                 |   0.29          |    124               |

  * GCC and Clang have a slightly different test as other parsers can not take
    C extensions in the original test.

* Highly ambiguous grammar (E=E+E|a) with abstract tree generation (only for Gecko and YAEP)
  * Input is `a(+a){200}`.  In other words, 200 operators `+` are used:
  
|                      | Time            |Memory (parse only) MB|
|----------------------|----------------:|---------------------:|
|ElkHound              |  9.50           |  9.6                 |
|YAEP                  |  5.53           |  154                 |
|Gecko                 |  1.10           |  119                 |


## Gecko internals overview

* Gecko uses custom allocators, hash tables, variable-length objects, and object stacks from accompanying header-only libraries
* Grammar analysis computes symbol properties (empty, accessible, derives terminals), detects loops, and builds FIRST/FOLLOW
  sets via fixed-point iteration
* Before starting parsing, Gecko constructs SLR(1) sets of the grammar
  * LR items (situations) are memoized -- each unique (rule, position) pair exists once
  * Each SLR set has a goto map (nonterminal → set) and action map (terminal → shift/reduce actions)
  * Priority/associativity conflict resolution is applied the same way for the action map as in YACC
* Parsing
  * **Single-stack fast path**: when one stack has exactly one action, a tight inner loop
    runs without extra allocations -- this makes unambiguous grammars nearly as fast as YACC
  * **Multi-stack path**: multiple stacks are maintained when ambiguity or conflicts arise;
    each stack independently processes shifts and reduces
  * **Stack merging**: stacks with identical sets sequences are merged, combining parse nodes
    via a user callback -- this prevents exponential blowup on ambiguous grammars
  * Parse tree nodes are **hash-consed** (structurally identical nodes are shared)
  * **Garbage collection** of unreachable parse tree nodes runs periodically during parsing (mark-sweep using a bitmap,
    with adaptive threshold)

## Syntax error recovery

* Gecko implements automatic **minimal-cost error recovery** that requires
  no grammar modifications — unlike YACC/Bison/YAEP, you don't need to add
  `error` rules to your grammar

* As Gecko can deal with numerous parsing stacks, this permits implementing a high quality syntax recovery algorithm

* The syntax error recovery guarantees that Gecko always produces parse trees corresponding to
  syntactically correct inputs -- simply, some tokens before and after the error token are ignored

* Error recovery algorithm in brief:                                                                                                    
  * We keep a pool of the current stacks and stacks (called *delayed stacks*) derived from the current stacks
    by popping the top element (LR-state)
  * We repeatedly move more and more expensive delayed stacks to the pool of the current stacks
  * For each failed stack, we add the delayed stack derived from the stack and
    keep the failed stack which skips the current stack token and advances the stack input to the next input token
  * We stop error recovery when we have a minimal cost stack that successfully consumed given number (defined by
    `gp_set_recovery_match`) of tokens without a gap or when we have a final stack that consumed `EOF`
  * The minimal cost stacks (or one minimal cost stack if we have one stack before the error recovery)
    become the start stacks after the recovery

