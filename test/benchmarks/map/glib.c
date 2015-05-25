/* Compile with
 *
 *   cc -oglib.t -Ofast `pkg-config --cflags --libs glib-2.0` glib.c
 */

#include <stdlib.h>
#include <glib.h>

static unsigned long long* onto_heap(unsigned long long i) {
  /* This maybe makes the comparison less fair */
  unsigned long long* dst = malloc(sizeof(unsigned long long));
  *dst = i;
  return dst;
}

static GHashTable* create_map(unsigned n) {
  GHashTable* ht = g_hash_table_new(g_int64_hash, g_int64_equal);
  unsigned long long i;

  for (i = 0; i < n; ++i)
    g_hash_table_insert(ht, onto_heap(i), onto_heap(i+1));

  return ht;
}

static unsigned long long do_sum_map(GHashTable* map, unsigned n) {
  unsigned long long i;
  unsigned long long sum = 0;

  for (i = 0; i < n; ++i)
    sum += *(const unsigned long long*)
      g_hash_table_lookup(map, &i);

  return sum;
}

int main(int argc, char** argv) {
  unsigned map_sz = atoi(argv[1]);
  int sum_map = atoi(argv[2]);
  GHashTable* map;

  map = create_map(map_sz);

  if (sum_map) {
    return 0 == do_sum_map(map, map_sz);
  } else {
    return 42 == (unsigned long long)map;
  }
}
