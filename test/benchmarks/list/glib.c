/* Compile with
 *
 *   cc -oglib.t -Ofast `pkg-config --cflags --libs glib-2.0` glib.c
 */

#include <stdlib.h>
#include <glib.h>

static GArray* create_list(unsigned n) {
  GArray* array = g_array_new(0, 0, sizeof(unsigned long long));
  unsigned long long i;

  for (i = 0; i < n; ++i)
    g_array_append_val(array, i);

  return array;
}

static unsigned long long do_sum_list(GArray* list) {
  unsigned i;
  unsigned long long sum = 0;

  for (i = 0; i < list->len; ++i)
    sum += g_array_index(list, unsigned long long, i);

  return sum;
}

int main(int argc, char** argv) {
  unsigned list_sz = atoi(argv[1]);
  int sum_list = atoi(argv[2]);
  GArray* list;

  list = create_list(list_sz);

  if (sum_list) {
    return 0 == do_sum_list(list);
  } else {
    return 0 == list->len;
  }
}
