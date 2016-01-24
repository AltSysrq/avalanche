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
#include "test.c"

#include "runtime/avalanche/defs.h"
#include "runtime/avalanche/string.h"
#include "runtime/avalanche/name-mangle.h"

defsuite(name_mangle);

static void assert_mangles_to(const char* expected,
                              ava_name_mangling_scheme scheme,
                              const char* orig) {
  ava_demangled_name in = { scheme, ava_string_of_cstring(orig) };
  ava_string out = ava_name_mangle(in);

  ck_assert_str_eq(expected, ava_string_to_cstring(out));

  /* Ensure that it also round-trips */
  ava_demangled_name result = ava_name_demangle(out);
  ck_assert_int_eq(scheme, result.scheme);
  ck_assert_str_eq(orig, ava_string_to_cstring(result.name));
}

static void assert_demangles_to(ava_name_mangling_scheme scheme,
                                const char* expected,
                                const char* orig) {
  ava_demangled_name out = ava_name_demangle(ava_string_of_cstring(orig));

  ck_assert_int_eq(scheme, out.scheme);
  ck_assert_str_eq(expected, ava_string_to_cstring(out.name));

  /* Ensure that it also round-trips */
  ck_assert_str_eq(orig, ava_string_to_cstring(ava_name_mangle(out)));
}

deftest(simple_noop_mangling) {
  assert_mangles_to("foobar", ava_nms_none, "foobar");
}

deftest(simple_noop_demangling) {
  assert_demangles_to(ava_nms_none, "foobar", "foobar");
}

deftest(doc_example_mangling) {
  assert_mangles_to("a$avast__ava_lang__org___prelude__$2B",
                    ava_nms_ava,
                    "avast.ava-lang.org:prelude.+");
}

deftest(ava_mangling_consecutive_specials) {
  assert_mangles_to("a$_$2D_x__$2E__x___$3A___",
                    ava_nms_ava, "---x...x:::");
  assert_mangles_to("a$_$3A", ava_nms_ava, "-:");
}

deftest(demangle_ava_like_zero_length) {
  assert_demangles_to(ava_nms_none, "a$", "a$");
}

deftest(demangle_ava_like_isolated_dollar) {
  assert_demangles_to(ava_nms_none, "a$$", "a$$");
}

deftest(demangle_ava_like_truncated_dollar) {
  assert_demangles_to(ava_nms_none, "a$$0", "a$$0");
}

deftest(demangle_ava_like_invalid_dollar) {
  assert_demangles_to(ava_nms_none, "a$$0X", "a$$0X");
  assert_demangles_to(ava_nms_none, "a$$X0", "a$$X0");
}

deftest(demangle_ava_like_lowercase_dollar) {
  assert_demangles_to(ava_nms_none, "a$$0a", "a$$0a");
  assert_demangles_to(ava_nms_none, "a$$a0", "a$$a0");
}
