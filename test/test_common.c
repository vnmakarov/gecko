/* A part of Gecko Parser (GLR Parser) project.
   Copyright (c) 2026  Vladimir Makarov <vmakarov@gcc.gnu.org>
*/

#define GP_TEST_ANSIC_H_ /* FIXME: avoid inclusion of ansic.h via ansic.c */

struct lex {
  short code;
  short column;
  int line;
  const char *id;
  struct lex *next;
};

static os_t lexs;
static struct lex *list;
static struct lex *curr = NULL;

static int column = 0;
static int line = 1;

static hash_table_t table;

static uint64_t hash (hash_table_entry_t el) {
  const char *id = (char *) el;
  uint64_t result, i;

  for (result = i = 0; *id++ != '\0'; i++) result += ((unsigned char) *id << (i % CHAR_BIT));
  return result;
}

static bool eq (hash_table_entry_t el1, hash_table_entry_t el2) {
  return strcmp ((char *) el1, (char *) el2) == 0;
}

static void initiate_typedefs (gp_allocator_t *alloc) {
  table = create_hash_table (alloc, 50000, hash, eq);
}

static void add_typedef (const char *id, int level) { /* Now we ignore level */
  hash_table_entry_t *entry_ptr;
  assert (level == 0);
  entry_ptr = find_hash_table_entry (table, id, 1);
  if (*entry_ptr == NULL)
    *entry_ptr = (hash_table_entry_t) id;
  else
    assert (strcmp (id, (const char *) *entry_ptr) == 0);
#ifdef DEBUG
  fprintf (stderr, "add typedef %s\n", id);
#endif
}

static inline int find_typedef (const char *id, int level) {
  hash_table_entry_t *entry_ptr;
  entry_ptr = find_hash_table_entry (table, id, 0);
#ifdef DEBUG
  if (*entry_ptr != NULL) fprintf (stderr, "found typedef %s\n", id);
#endif
  return *entry_ptr != NULL;
}
