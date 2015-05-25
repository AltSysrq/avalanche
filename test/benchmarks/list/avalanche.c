#include <stdlib.h>

#include <runtime/avalanche.h>

static ava_list_value generate_list(size_t n) {
  ava_list_value list = ava_empty_list();
  size_t i;

  for (i = 0; i < n; ++i)
    list = ava_list_append(list, ava_value_of_integer(i));

  return list;
}

static ava_integer do_sum_list(ava_list_value list) {
  ava_integer sum = 0;
  size_t i, n = ava_list_length(list);

  for (i = 0; i < n; ++i)
    sum += ava_integer_of_value(ava_list_index(list, i), 0);

  return sum;
}

int main(int argc, char** argv) {
  ava_list_value list;
  unsigned list_sz = atoi(argv[1]);
  ava_bool sum_list = atoi(argv[2]);

  ava_init();

  list = generate_list(list_sz);

  if (sum_list) {
    return 0 == do_sum_list(list);
  } else {
    return 0 == ava_list_length(list);
  }
}
