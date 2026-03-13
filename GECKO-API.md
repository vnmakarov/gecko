# Gecko Parser API Reference

Gecko is a general-purpose GLR (Generalized LR) syntax parser that works on any context-free grammar, with automatic minimal error
recovery and syntax-directed translation.

## Constants

### Translation

| Constant                    | Value     | Description                                             |
|-----------------------------|-----------|---------------------------------------------------------|
| `GP_NIL_TRANSLATION_NUMBER` | `INT_MAX` | Reserved value designating an empty node in translation.|
|                             |           | Must not intersect with symbol numbers.                 |

### Error Codes

| Constant                           | Value | Description                          |
|------------------------------------|-------|--------------------------------------|
| `GP_NO_MEMORY`                     | 1     | Out of memory                        |
| `GP_UNDEFINED_OR_BAD_GRAMMAR`      | 2     | Undefined or malformed grammar       |
| `GP_WRONG_ARG`                     | 3     | Wrong argument                       |
| `GP_DESCRIPTION_SYNTAX_ERROR_CODE` | 4     | Syntax error in grammar description  |
| `GP_FIXED_NAME_USAGE`              | 5     | Usage of a fixed/reserved name       |
| `GP_REPEATED_TERM_DECL`            | 6     | Repeated terminal declaration        |
| `GP_NEGATIVE_TERM_CODE`            | 7     | Negative terminal code               |
| `GP_TOO_WIDE_TERM_RANGE_CODE`      | 8     | Terminal code range too wide         |
| `GP_REPEATED_TERM_CODE`            | 9     | Repeated terminal code               |
| `GP_REPEATED_TERM_ASSOC`           | 10    | Repeated terminal associativity      |
| `GP_UNDEFINED_TERM_ASSOC`          | 11    | Undefined terminal associativity     |
| `GP_WRONG_TERM_ASSOC`              | 12    | Wrong terminal associativity         |
| `GP_NO_RULES`                      | 13    | No rules defined                     |
| `GP_TERM_IN_RULE_LHS`              | 14    | Terminal used in rule left-hand side |
| `GP_INCORRECT_TRANSLATION`         | 15    | Incorrect translation specification  |
| `GP_INCORRECT_SYMBOL_NUMBER`       | 16    | Incorrect symbol number              |
| `GP_REPEATED_SYMBOL_NUMBER`        | 17    | Repeated symbol number               |
| `GP_UNACCESSIBLE_NONTERM`          | 18    | Unaccessible nonterminal             |
| `GP_NONTERM_DERIVATION`            | 19    | Nonterminal derivation issue         |
| `GP_LOOP_NONTERM`                  | 20    | Looping nonterminal                  |
| `GP_INVALID_TOKEN_CODE`            | 21    | Invalid token code                   |

## Enumerations

### `enum gp_assoc`

Terminal associativity for conflict resolution (as in YACC).

| Value            | Description                              |
|------------------|------------------------------------------|
| `GP_NON_ASSOC`   | Non-associative (no conflict resolution) |
| `GP_LEFT_ASSOC`  | Left-associative                         |
| `GP_RIGHT_ASSOC` | Right-associative                        |

### `enum gp_tree_node_type`

Type of a parse tree node.

| Value      | Description                                |
|------------|--------------------------------------------|
| `GP_NIL`   | Empty/nil node                             |
| `GP_TERM`  | Terminal node                              |
| `GP_ANODE` | Abstract node                              |
| `GP_ALT`   | Alternative translations (ambiguous parse) |
| `GP_OPT`   | Context-dependent alternative (option node) |

## Parse Tree Data Structures

### `struct gp_tree_node`

The generalized node of the parse tree. All parse tree nodes use this structure with a tagged union.

```c
struct gp_tree_node {
  enum gp_tree_node_type type; /* the type of node */
  unsigned num;                /* node number */
  union {
    struct gp_nil nil;
    struct gp_term term;
    struct gp_anode anode;
    struct gp_alt alt;
  } val;
};
```

### `struct gp_nil`

An empty node. Exists in one instance. Used as translation when a rule has no meaningful translation.

### `struct gp_term`

A terminal node in the parse tree.

| Field  | Type     | Description                            |
|--------|----------|----------------------------------------|
| `code` | `int`    | The terminal code                      |
| `attr` | `void *` | The terminal attributes (user-defined) |

### `struct gp_anode`

An abstract node representing a grammar rule application.

| Field          | Type                     | Description                            |
|----------------|--------------------------|----------------------------------------|
| `children_num` | `int`                    | Number of children                     |
| `name`         | `const char *`           | The abstract node name                 |
| `children`     | `struct gp_tree_node **` | Array of child nodes (NULL-terminated) |

### `struct gp_alt`

An alternative node representing ambiguous parses.

| Field    | Type                    | Description                    |
|----------|-------------------------|--------------------------------|
| `first`  | `struct gp_tree_node *` | First alternative translation  |
| `second` | `struct gp_tree_node *` | Second alternative translation |

## Functions

### Grammar Creation and Finalization

#### `gp_create_grammar`

```c
struct grammar *gp_create_grammar(void);
```

Create an undefined grammar. This function must be called first. Returns `NULL` if there is no memory.

#### `gp_fin`

```c
void gp_fin(struct grammar *grammar);
```

Finish work with the grammar and free all associated resources. This function should be called last.

### Error Reporting

#### `gp_error_code`

```c
int gp_error_code(struct grammar *g);
```

Return the last occurred error code for the given grammar or zero otherwise.

#### `gp_error_message`

```c
const char *gp_error_message(struct grammar *g);
```

Return a message corresponding to the last occurred error code or empty string otherwise

### Grammar Definition

#### `gp_read_grammar`

```c
int gp_read_grammar(struct grammar *g, bool strict_p,
                     const char *(*read_terminal)(int *code, int *priority,
                                                  enum gp_assoc *assoc),
                     const char *(*read_rule)(const char ***rhs,
                                              const char **abs_node,
                                              int **transl));
```

Read terminals and rules into grammar `g` and check it depending on `strict_p`.  Returns zero on success,
or an error code on failure (also available via `gp_error_code` and `gp_error_message`).

**Parameters:**

- `g` -- the grammar to populate
- `strict_p` -- enable strict grammar checking which means reporting about terminals unreachable from axiom,
   unability to produce any terminal string and other non-critical grammar errors.
- `read_terminal` -- callback to read the next terminal. Called before `read_rule`. Returns the terminal name and
   sets `*code` (must be non-negative), `*priority`, and `*assoc`. Return `NULL` when all terminals have been read.
   Use `GP_NON_ASSOC` if you don't want conflict resolution.
- `read_rule` -- callback to read the next rule. Called after all terminals are read. Returns
   the LHS nonterminal name and sets `*rhs` to a NULL-terminated array of RHS symbol names. Any symbol
   not declared as a terminal is treated as a nonterminal. Also sets `*abs_node` (abstract node name) and `*transl`
   (array of RHS symbol indices for translation children, terminated by a negative value). Return `NULL` when all rules have been read.

**Translation rules:**

- All indices in `*transl` must be distinct (a symbol's translation cannot appear twice)
- If `*transl` is `NULL` or contains only the end marker, the rule's translation is a nil node
- If `*abs_node` is `NULL`, no abstract node is created. In this case `*transl` should be `NULL`
  or contain at most one element -- the translation is either a nil node or the translation of the single referenced RHS symbol
- Use `GP_NIL_TRANSLATION_NUMBER` as an index to denote an empty node in the translation

#### `gp_parse_grammar`

```c
int gp_parse_grammar(struct grammar *g, bool strict_p, const char *description);
```

Analogous to `gp_read_grammar` but parses a textual grammar description string.  See the
[Grammar Description Format](#grammar-description-format) section below.

### Grammar Description Format

The grammar description is a text string consisting of three kinds of declarations: **terminal declarations**,
**associativity declarations**, and **rules**.  Declarations can be separated by optional semicolons.
C-style comments (`/* ... */`) are allowed anywhere whitespace is permitted.

#### Terminal Declarations

Terminal declarations begin with the keyword `TERM` followed by zero or more terminal names with optional
code assignments:

```
TERM name1 name2 = 300 name3 ;
```

- Each terminal name is an identifier (letters, digits, underscores; starting with a letter or underscore)
- An optional `= NUMBER` after a name assigns a specific integer code to that terminal
- Terminals without an explicit code are automatically assigned codes starting from 256
- Character literals like `'a'` used in rules are implicitly declared as terminals with codes equal to their ASCII values
- A bare `TERM;` with no names is valid (useful when all terminals are character literals)

#### Associativity Declarations

Associativity declarations specify operator precedence and associativity for conflict resolution in
ambiguous grammars (as in YACC):

```
LEFT '+' ;
LEFT '*' ;
RIGHT ASSIGN ;
NONASSOC EQ NEQ ;
```

- `LEFT` -- left-associative
- `RIGHT` -- right-associative
- `NONASSOC` -- non-associative

Each declaration applies to one or more terminal names or character literals listed after the keyword.
Declarations appearing later have higher precedence.  A terminal must not appear in more than one
associativity declaration.

#### Rules

Rules define the grammar productions.  A rule starts with a nonterminal name followed by a colon, then
one or more alternatives separated by `|`:

```
nonterminal : symbol1 symbol2 ... # translation
            | symbol3 symbol4 ... # translation
            ;
```

- The **left-hand side** (LHS) is an identifier followed by `:`.  Any identifier used in the LHS that was
  not declared as a terminal is treated as a nonterminal.
- The **right-hand side** (RHS) is a sequence of zero or more symbols (identifiers or character literals).
  An empty sequence denotes an empty (epsilon) production.
- The first rule's LHS nonterminal is the grammar's axiom (start symbol).
- The trailing semicolon is optional.

#### Translation Specifications

Each alternative can have an optional **translation** specification after `#`:

| Form                 | Meaning                                                                                                      |
|----------------------|--------------------------------------------------------------------------------------------------------------|
| *(nothing)*          | No translation (nil node)                                                                                    |
| `#`                  | No translation (nil node)                                                                                    |
| `# N`                | Translation is the parse tree of RHS symbol at index N (0-based)                                             |
| `# -`                | Translation is a nil node (explicit)                                                                         |
| `# name`             | Create an abstract node named `name` with no children                                                        |
| `# name (N1 N2 ...)` | Create an abstract node named `name` with children being translations of RHS symbols at indices N1, N2, etc. |

Within the parenthesized list, `-` can be used in place of a number to denote a nil node child.

#### Complete Example

```
TERM
IDENTIFIER = 300
NUMBER = 301 ;

LEFT '+' '-' ;
LEFT '*' '/' ;

expr : term                    # 0
     | expr '+' expr           # plus (0 2)
     | expr '-' expr           # minus (0 2)
     | expr '*' expr           # mult (0 2)
     | expr '/' expr           # div (0 2)
     | IDENTIFIER              # 0
     | NUMBER                  # 0
     | '(' expr ')'            # 1
     ;
```

### Parser Configuration

All configuration functions return the previous parameter value.

#### `gp_set_parse_alloc`

```c
gp_parse_alloc_func_t gp_set_parse_alloc(struct grammar *g,
                                         gp_parse_alloc_func_t fn);
```

Set the memory allocation function for parse tree nodes. Default is `malloc`. Must not be `NULL`.

**Type:** `typedef void *(*gp_parse_alloc_func_t)(int);`

#### `gp_set_parse_free`

```c
gp_parse_free_func_t gp_set_parse_free(struct grammar *g,
                                       gp_parse_free_func_t fn);
```

Set the memory deallocation function for parse tree nodes. Default is `free`. `NULL` means no freeing.

**Type:** `typedef void (*gp_parse_free_func_t)(void *);`

#### `gp_set_syntax_error`

```c
gp_syntax_error_func_t gp_set_syntax_error(struct grammar *g,
                                           gp_syntax_error_func_t fn);
```

Set the syntax error reporting function. Called when a syntax error occurs during parsing.

**Type:**
```c
typedef void (*gp_syntax_error_func_t)(const char *err_tok_repr, void *err_tok_attr,
                                       const char *stop_tok_repr, void *stop_tok_attr);
```

**Callback parameters:**

- `err_tok_repr`, `err_tok_attr` -- representation and attribute of the token where the error occurred
- `stop_tok_repr`, `stop_tok_attr` -- representation and attribute of the token where recovery stopped.

The default function prints token representations to stderr. Set a custom function to print source
positions (which can be passed through token attributes).

#### `gp_set_debug_level`

```c
int gp_set_debug_level(struct grammar *grammar, int level);
```

Set the debug output level (only effective if compiled without `NO_GP_DEBUG_PRINT`).

| Level | Output                                                                                           |
|-------|--------------------------------------------------------------------------------------------------|
| 0     | Nothing (default)                                                                                |
| 1     | Statistics                                                                                       |
| 2     | Additionally, the result translation                                                             |
| 3     | Additionally, read tokens, actions (conflicts marked by `!`) for dynamically generated SLR sets, |
|       |   and high-level error recovery info                                                             |
| 4     | Additionally, rules, first/follow nonterminal sets, dynamically generated SLR sets               |
| 5     | Additionally, stacks during parsing and stack merging                                            |
| 6     | Even more detail about processing stacks during parsing                                          |

#### `gp_set_recovery_match`

```c
int gp_set_recovery_match(struct grammar *grammar, int n_toks);
```

Set how many subsequent tokens must be successfully shifted to confirm that error recovery is complete. Default is 3.

#### `gp_set_node_merge_func`

```c
gp_node_merge_func_t gp_set_node_merge_func(struct grammar *grammar,
                                            gp_node_merge_func_t func);
```

Set the parse node merge function used when merging stacks during GLR parsing. The function receives the grammar,
two parse tree nodes of a symbol from stacks being merged, and a flag `opt_p` indicating whether the merge should
produce a context-dependent alternative (option) node rather than a regular alternative node (see `gp_parse` for
details). The function returns the result node. `NULL` sets the default function, which always returns the first
node. Using the default function causes `gp_parse` to return only one translation.

**Type:**
```c
typedef void *(*gp_node_merge_func_t)(struct grammar *grammar,
                                      struct gp_tree_node *node1,
                                      struct gp_tree_node *node2,
                                      bool opt_p);
```

### Parsing

#### `gp_parse`

```c
int gp_parse(struct grammar *grammar, int (*read_token)(void **attr),
             struct gp_tree_node **root, int *ambiguity);
```

Parse input according to the grammar. Returns an error code (also available via `gp_error_code`).
On success (code zero), the parse tree root is stored in `*root`.

**Parameters:**

- `grammar` -- the grammar (previously set up via `gp_read_grammar` or `gp_parse_grammar`)
- `read_token` -- callback providing input tokens. Returns the token code and sets `*attr` to the token attribute.
  A negative return value signals end of input.
- `root` -- output: the root of the parse tree. It never returns NULL.
- `ambiguity` -- output: ambiguity status of the parse:
  - `0` -- no ambiguity found
  - `1` -- the grammar is ambiguous on the input
  - `2` -- the final stack was produced by merging stacks where two or more terminal attributes or abstract nodes
    were different

**Ambiguity level 2 and option nodes:**

Consider merging two stacks with two different translations for two stack elements:
`...a...b...` and `...c...d...`.  Using the result stack `...alt(a,b)...alt(c,d)...` with regular
alternative nodes would imply 4 possible choices (ac, ad, bc, bd) instead of the two correct ones (ac, bd).
Therefore for such cases the parser uses option (context-dependent alternative, `GP_OPT`) nodes for the merged
stack: `...opt(a,b)...opt(c,d)...`.  The node merge function receives `opt_p = true` for these cases.
It is possible to transform the parse tree to contain only alternative nodes, but this is a non-trivial task.
Fortunately, this is a very rare case for programming language grammars.

#### `gp_get_alt_node`

```c
struct gp_tree_node *gp_get_alt_node(struct grammar *g, struct gp_tree_node *first, struct gp_tree_node *second);
```

Return a `GP_ALT` node with given `first` and `second`.  The function can be used in the node merge function
if you want to keep all possible alternatives during stack merging.

#### `gp_get_opt_node`

```c
struct gp_tree_node *gp_get_opt_node(struct grammar *g, struct gp_tree_node *first, struct gp_tree_node *second);
```

Analogous to `gp_get_alt_node` but returns a `GP_OPT` (context-dependent alternative) node.  Use this in the node
merge function when `opt_p` is true to preserve correct alternative semantics (see `gp_parse` for details).

### Parse Tree Operations

#### `gp_print_translation`

```c
void gp_print_translation(struct grammar *grammar, FILE *f,
                           struct gp_tree_node *root);
```

Print the translation of the parse tree rooted at `root` to file `f`. Only available if compiled without `NO_GP_DEBUG_PRINT`.

#### `gp_free_tree`

```c
void gp_free_tree(struct grammar *grammar, struct gp_tree_node *root);
```

Free memory allocated for the parse tree. `root` must be the root returned by `gp_parse`. If `root` is `NULL`, no operation is performed.
