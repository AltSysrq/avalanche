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

#include <stdlib.h>
#include <string.h>

#define AVA__INTERNAL_INCLUDE 1
#include "runtime/avalanche/defs.h"
#include "runtime/avalanche/string.h"
#include "runtime/avalanche/value.h"
#include "runtime/avalanche/pcode.h"

defsuite(pcode);

static void test_to_string(const char* expected,
                           const ava_pcode_global_list* pcode) {
  ck_assert_str_eq(expected,
                   ava_string_to_cstring(
                     ava_pcode_global_list_to_string(pcode, 0)));
}

static ava_pcode_global_list* from_string(const char* str) {
  return ava_pcode_global_list_of_string(ava_string_of_cstring(str));
}

static void test_round_trip(const char* str) {
  test_to_string(str, from_string(str));
}

/* Smoke tests for the auto-generated code */
deftest(build_empty_pcode) {
  ava_pcg_builder* builder = ava_pcg_builder_new();
  const ava_pcode_global_list* pcode = ava_pcg_builder_get(builder);

  test_to_string("", pcode);
}

deftest(parse_empty_pcode) {
  test_round_trip("");
}

deftest(build_single_statement) {
  const ava_pcode_global_list* pcode;
  ava_pcg_builder* builder = ava_pcg_builder_new();

  ava_pcgb_src_pos(builder, AVA_ASCII9_STRING("foo"),
                   42, 1, 2, 10, 20);
  pcode = ava_pcg_builder_get(builder);

  test_to_string("\\{src-pos foo 42 1 2 10 20\\}\n", pcode);
}

deftest(to_string_correct_escaping) {
  test_round_trip("\\{src-pos \"foo bar\" 42 1 2 10 20\\}\n");
  test_round_trip("\\{src-pos \\{\"\\} 42 1 2 10 20\\}\n");
}

deftest(function_definition) {
  test_round_trip("\\{fun true \"ava fum\" \"ava pos\" arg \\{\n"
                  "\t\\{push d 42\\}\n"
                  "\t\\{ld-imm-vd d0 \"\"\\}\n"
                  "\t\\{ret d0\\}\n"
                  "\\}\\}\n");
}
