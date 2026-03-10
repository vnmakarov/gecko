![Gecko](gecko.png)

# Gecko: A standalone GLR parser library
  * Gecko is licensed under the MIT license

# Features

* Parse inputs described by **unambigous and ambigous grammar**
* Small and simple
  * x86-64 code size is only 80KB
  * 3.5K C SLOC
* Fast
  * Close to YACC/Bison speed on unambigous grammars (about 5-10% slower)
  * Process 2M C lines per second on AMD9900X
  * 2 times faster than YEAP (the fastest Early parser) on unambigous and moderately ambigous grammars
  * 2.5 times faster than ElkHound (a famous GLR parser) on umambigous and moderately ambigous grammars
  * 50 times faster than ElkHound on higly ambigous grammars
* Automatic syntax error recovery and error reporting
  * Simple interface for custom support of syntax error reporting
* Operator grammar support
  * Descriptions of operator priority and associativity analaogous to YACC/Bison
* Simple syntax direct translation
  * Generate **abstract syntax tree** as an output
* Library
  * Can be embedded into other programs
  * A grammar for Gecko can be constructed through function calls or using
    a YACC-like description syntax
  * Very **fast startup** after reading grammar from file or string

# Differences between Gecko and [YAEP](https://github.com/vnmakarov/yaep) (the fastest Earley Parser implementation I know):
  * Gecko is faster on unambigous and moderately ambigous grammars
  * Gecko speed practically does not depend on grammar size
  * Gecko requires less memory for parsing
  * Gecko memory consumption does not depends on input length (or slightly depends for ambigous grammars)
  * Gecko does not require to modify grammar for syntax error recovery and reporting
  * Gecko permits shorter grammars by supporting operator grammars (operator associativity
    and precedence) analogously to YACC/Bison
  * YAEP can permit to describe rules with costs and find minimal cost translation for ambigous gramamrs
  * YAEP is faster on higly ambigous grammars when all possible translations or minimal cost translation
    are required.  It also generates much smaller DAG for all possible translations for highly ambigous grammars

# Usage example:

* The following is a small example of how to use Gecko to parse expressions.  We have omitted the functions
  `read_token`, `syntax_error_func`, and `parse_alloc_func` which are needed to provide tokens, print syntax
  error messages, and allocate memory for the parser.

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
  int ambiguous_p;

  if ((g = gp_create_grammar ()) == NULL) {
      fprintf (stderr, "gp_create_grammar: No memory\n");
      exit (1);
  }
  if (gp_parse_grammar (g, TRUE, description) != 0) {
      fprintf (stderr, "%s\n", gp_error_message (g));
      exit (1);
    }
  if (gp_parse (g, read_token_func, &root, &ambiguous_p))
    fprintf (stderr, "gp_parse: %s\n", gp_error_message (g));
  gp_fin (g);
}
```

# Installing:
  * ``cd <srcdir>``
  * ``make``
  * ``make test`` (optional) 
  * ``make bench`` (optional) 
  * ``make install``

# Speed comparison with YACC, ElkHound, YAEP, and GCC/Clang parsers:

* Tested parsers:
  * Berkley YACC 1.9
  * GCC-15.2.
  * Clang-21.1.8
  * YAEP as of Oct. 2015.
  * [ElkHound](https://github.com/WeiDUorg/elkhound) as of 2019-02-17 (a GLR parser)
* Bison is claimed to be a GLR parser but I did not managed to use **ambigous**
  C grammar (see below) for it.  Therefore and because its results on **unambigous**
  C grammar are close to YACC one I excluded it from the tests.
* Grammar:
  * The base test grammar is the **ANSI C** grammar which is mostly
    a left recursion grammar.
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
  * The first test is 10000K sieve functions so resulting file size was 1.5M C lines.
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

# BenchMark Results

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


  * YAEP has very good results as it uses a dynamic programming to speedup Earley's parser
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

  * GCC and Clang have a bit different test as other parsers can not take
    C extensions in the original test.

* Highly ambigous grammar (E=E+E|a) with abstract tree generation
  * Input is `a(+a){200}`.  In other words, 200 operators `+` are used:
  
|                      | Time            |Memory (parse only) MB|
|----------------------|----------------:|---------------------:|
|ElkHound              |  9.50           |  9.6                 |
|YAEP                  |  5.53           |  154                 |
|Gecko                 |  1.10           |  119                 |


## ------------------------------------

## Gecko internals

GC of parse tree nodes.

### Syntax error reporting
