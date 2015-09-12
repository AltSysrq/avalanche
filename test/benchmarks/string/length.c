/* This is just a microbenchmark for finding the length of an ASCII9 string. */
extern unsigned long ava_string_length(unsigned long long s);

int main(void) {
  unsigned long long s = 0xFFFFFFFF00000001ULL;
  unsigned i;

  for (i = 0; i < 1024*1024*128; ++i)
    ava_string_length(s);

  return 0;
}