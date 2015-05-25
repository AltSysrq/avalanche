#include <stdlib.h>
#include <runtime/avalanche.h>

static ava_map_value build_map(unsigned n) {
  ava_map_value map = ava_empty_map();
  unsigned i;

  for (i = 0; i < n; ++i)
    map = ava_map_add(map, ava_value_of_integer(i),
                      ava_value_of_integer(i+1));

  return map;
}

static ava_integer do_sum_map(ava_map_value map, unsigned n) {
  ava_integer sum = 0;
  unsigned i;

  for (i = 0; i < n; ++i)
    sum += ava_integer_of_value(
      ava_map_get(map,
                  ava_map_find(map, ava_value_of_integer(i))), 0);

  return sum;
}

int main(int argc, char** argv) {
  ava_map_value map;
  unsigned map_sz = atoi(argv[1]);
  ava_bool sum_map = atoi(argv[2]);

  ava_init();

  map = build_map(map_sz);

  if (sum_map) {
    return 0 == do_sum_map(map, map_sz);
  } else {
    return 0 == ava_map_npairs(map);
  }
}
