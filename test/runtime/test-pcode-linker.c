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
#include "runtime/avalanche/map.h"
#include "runtime/avalanche/pcode.h"
#include "runtime/avalanche/pcode-validation.h"
#include "runtime/avalanche/pcode-linker.h"

defsuite(pcode_linker);

static ava_pcode_global_list* parse_pcode(const char* str) {
  ava_pcode_global_list* pcode;
  ava_compile_error_list errors;

  pcode = ava_pcode_global_list_of_string(ava_string_of_cstring(str));

  TAILQ_INIT(&errors);
  (void)ava_xcode_from_pcode(pcode, &errors, ava_empty_map());

  ck_assert_msg(TAILQ_EMPTY(&errors),
                "Test source P-Code was invalid.\n%s",
                ava_error_list_to_string(&errors, 50, ava_false));

  return pcode;
}

static void assert_pcode_valid(const ava_pcode_global_list* pcode) {
  ava_compile_error_list errors;

  TAILQ_INIT(&errors);
  (void)ava_xcode_from_pcode(pcode, &errors, ava_empty_map());

  ck_assert_msg(TAILQ_EMPTY(&errors),
                "Output P-Code was invalid.\n%s\nP-Code:\n%s",
                ava_string_to_cstring(
                  ava_error_list_to_string(&errors, 50, ava_false)),
                ava_string_to_cstring(
                  ava_pcode_global_list_to_string(pcode, 1)));
}

static void assert_pcode_equals(const char* expected_denorm,
                                const ava_pcode_global_list* actual_pc) {
  const char* expected = ava_string_to_cstring(
    ava_pcode_global_list_to_string(
      parse_pcode(expected_denorm), 1));
  const char* actual = ava_string_to_cstring(
    ava_pcode_global_list_to_string(actual_pc, 1));

  ck_assert_msg(0 == strcmp(expected, actual),
                "Output P-Code does not match expected.\n"
                "Expected:\n%s\n"
                "Actual:\n%s", expected, actual);
}

static ava_pcode_global_list* to_interface(const char* source) {
  ava_pcode_global_list* pcode;

  pcode = parse_pcode(source);
  pcode = ava_pcode_to_interface(pcode);
  assert_pcode_valid(pcode);
  return pcode;
}

static void to_interface_like(const char* expected, const char* input) {
  assert_pcode_equals(expected, to_interface(input));
}

deftest(empty_pcode_to_interface) {
  ava_pcode_global_list* res;

  res = to_interface("");
  ck_assert(TAILQ_EMPTY(res));
}

deftest(pcode_with_no_exports_becomes_empty_interface) {
  ava_pcode_global_list* res;

  res = to_interface(
    "[src-pos source.ava 0 1 1 1 1]\n"
    "[init 2]\n"
    "[fun false [ava module:init] [ava pos] [\"\"] [\n"
    "  [ret v0]\n"
    "]]\n"
    "[var true [ava SOME-CONST]]\n"
    "[ext-var [ava some-ext-var]]\n"
    "[ext-fun [ava some-ext-fun] [ava pos pos]]\n");
  ck_assert(TAILQ_EMPTY(res));
}

deftest(interface_preserves_simple_exports) {
  to_interface_like(
    "[src-pos source.ava 0 1 1 1 1]\n"
    "[export 2 true foo]\n"
    "[ext-var [ava foo]]\n"
    "[export 4 false bar]\n"
    "[ext-fun [ava bar] [ava pos pos]]\n",

    "[src-pos source.ava 0 1 1 1 1]\n"
    "[export 2 true foo]\n"
    "[ext-var [ava foo]]\n"
    "[export 4 false bar]\n"
    "[ext-fun [ava bar] [ava pos pos]]\n");
}

deftest(interface_changes_vars_to_ext_var) {
  to_interface_like(
    "[src-pos source.ava 0 1 1 1 1]\n"
    "[export 2 true foo]\n"
    "[ext-var [ava foo]]\n",

    "[src-pos source.ava 0 1 1 1 1]\n"
    "[export 2 true foo]\n"
    "[var true [ava foo]]\n");
}

deftest(interface_changes_funs_to_ext_fun) {
  to_interface_like(
    "[src-pos source.ava 0 1 1 1 1]\n"
    "[export 2 true bar]\n"
    "[ext-fun [ava bar] [ava pos pos]]\n",

    "[src-pos source.ava 0 1 1 1 1]\n"
    "[export 2 true bar]\n"
    "[fun true [ava bar] [ava pos pos] [x y] [\n"
    "  [ret v0]\n"
    "]]\n");
}

deftest(interface_keeps_macros) {
  to_interface_like(
    "[macro true foo [die]]\n"
    "[macro false foo [die]]\n",

    "[macro true foo [die]]\n"
    "[macro false foo [die]]\n");
}

deftest(interface_relinks_globals) {
  to_interface_like(
    "[export 1 true bar]\n"
    "[ext-var [ava bar]]\n",

    "[var false [ava private]]\n"
    "[export 2 true bar]\n"
    "[var true [ava bar]]\n");
}

deftest(interface_deletes_redundant_src_pos) {
  to_interface_like(
    "[src-pos source.ava 1 2 2 2 2]\n"
    "[macro true foo [die]]\n",

    "[src-pos source.ava 0 1 1 1 1]\n"
    "[var false [ava private]]\n"
    "[src-pos source.ava 1 2 2 2 2]\n"
    "[macro true foo [die]]\n");
}

deftest(interface_deletes_src_pos_at_eof) {
  to_interface_like(
    "[macro true foo [die]]\n",

    "[macro true foo [die]]\n"
    "[src-pos source.ava 0 1 1 1 1]\n"
    "[var false [ava private]]\n");
}
