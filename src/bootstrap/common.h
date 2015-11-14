/*-
 * Copyright (c) 2015, Jason Lingle
 *
 * Permission to  use, copy,  modify, and/or distribute  this software  for any
 * purpose  with or  without fee  is hereby  granted, provided  that the  above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE  IS PROVIDED "AS  IS" AND  THE AUTHOR DISCLAIMS  ALL WARRANTIES
 * WITH  REGARD   TO  THIS  SOFTWARE   INCLUDING  ALL  IMPLIED   WARRANTIES  OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT  SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL,  DIRECT,   INDIRECT,  OR  CONSEQUENTIAL  DAMAGES   OR  ANY  DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF  CONTRACT, NEGLIGENCE  OR OTHER  TORTIOUS ACTION,  ARISING OUT  OF OR  IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

typedef struct {
  unsigned argc;
  const char*const* argv;
} main_data;

static ava_value main_impl(void* arg);
static void spit(ava_string outfile, const ava_pcode_global_list* pcode)
AVA_UNUSED;

static ava_string slurp_file(ava_string infile) AVA_UNUSED;
static ava_pcode_global_list* slurp(ava_string infile) AVA_UNUSED;

int main(int argc, const char*const* argv) {
  main_data md = { (unsigned)argc, argv };

  ava_init();
  ava_invoke_in_context(main_impl, &md);

  return 0;
}

static void spit(ava_string outfile, const ava_pcode_global_list* pcode) {
  FILE* out;
  ava_string pcode_string;
  size_t len;

  pcode_string = ava_pcode_global_list_to_string(pcode, 0);
  len = ava_string_length(pcode_string);

  out = fopen(ava_string_to_cstring(outfile), "w");
  if (!out)
    err(EX_CANTCREAT, "Failed to open %s for writing",
        ava_string_to_cstring(outfile));

  if (len != fwrite(ava_string_to_cstring(pcode_string), 1, len, out))
    err(EX_IOERR, "Error writing %s", ava_string_to_cstring(outfile));

  fclose(out);
}

static ava_string slurp_file(ava_string infile) {
  ava_string accum;
  char buff[4096];
  size_t nread;
  FILE* in;

  in = fopen(ava_string_to_cstring(infile), "r");
  if (!in)
    err(EX_NOINPUT, "Failed to open %s for reading",
        ava_string_to_cstring(infile));

  accum = AVA_EMPTY_STRING;
  do {
    nread = fread(buff, 1, sizeof(buff), in);
    accum = ava_strcat(accum, ava_string_of_bytes(buff, nread));
  } while (!feof(in) && !ferror(in));

  if (ferror(in))
    err(EX_IOERR, "Error reading %s", ava_string_to_cstring(infile));

  fclose(in);

  return accum;
}

static ava_pcode_global_list* slurp(ava_string infile) {
  return ava_pcode_global_list_of_string(slurp_file(infile));
}
