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

#include <stdarg.h>

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

static void link_modules_like_(
  const char* expected, ava_bool link_as_packages,
  ...
) {
  ava_compile_error_list errors;
  ava_pcode_linker* linker;
  ava_pcode_global_list* output;
  const char* module_name, * module_source;
  va_list args;

  linker = ava_pcode_linker_new();
  TAILQ_INIT(&errors);

  va_start(args, link_as_packages);
  while ((module_name = va_arg(args, const char*))) {
    module_source = va_arg(args, const char*);
    (link_as_packages?
     ava_pcode_linker_add_package :
     ava_pcode_linker_add_module)(
       linker, ava_string_of_cstring(module_name),
       parse_pcode(module_source));
  }
  va_end(args);

  output = ava_pcode_linker_link(linker, &errors);
  ck_assert_msg(TAILQ_EMPTY(&errors),
                "Link failed.\n%s",
                ava_string_to_cstring(
                  ava_error_list_to_string(&errors, 50, ava_false)));
  assert_pcode_valid(output);
  assert_pcode_equals(expected, output);
}

#define link_modules_like(...) link_modules_like_(__VA_ARGS__, NULL)

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
    "[macro true foo 5 0 [die]]\n"
    "[macro false foo 5 0 [die]]\n",

    "[macro true foo 5 0 [die]]\n"
    "[macro false foo 5 0 [die]]\n");
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
    "[macro true foo 5 0 [die]]\n",

    "[src-pos source.ava 0 1 1 1 1]\n"
    "[var false [ava private]]\n"
    "[src-pos source.ava 1 2 2 2 2]\n"
    "[macro true foo 5 0 [die]]\n");
}

deftest(interface_deletes_src_pos_at_eof) {
  to_interface_like(
    "[macro true foo 5 0 [die]]\n",

    "[macro true foo 5 0 [die]]\n"
    "[src-pos source.ava 0 1 1 1 1]\n"
    "[var false [ava private]]\n");
}

deftest(interface_deletes_unreference_struct) {
  to_interface_like(
    "",

    "[decl-sxt true [[struct foo]]]\n");
}

deftest(interface_keeps_exported_struct) {
  to_interface_like(
    "[decl-sxt true [[struct foo]]]\n"
    "[export 0 true foo]\n",

    "[decl-sxt true [[struct foo]]]\n"
    "[export 0 true foo]\n");
}

deftest(linker_emits_error_on_module_conflict) {
  ava_compile_error_list errors;
  ava_pcode_linker* linker = ava_pcode_linker_new();
  const ava_pcode_global_list* empty = parse_pcode("");

  TAILQ_INIT(&errors);
  ava_pcode_linker_add_module(linker, AVA_ASCII9_STRING("foo"), empty);
  ava_pcode_linker_add_module(linker, AVA_ASCII9_STRING("foo"), empty);
  (void)ava_pcode_linker_link(linker, &errors);

  ck_assert(!TAILQ_EMPTY(&errors));
}

deftest(linker_emits_error_on_package_conflict) {
  ava_compile_error_list errors;
  ava_pcode_linker* linker = ava_pcode_linker_new();
  const ava_pcode_global_list* empty = parse_pcode("");

  TAILQ_INIT(&errors);
  ava_pcode_linker_add_package(linker, AVA_ASCII9_STRING("foo"), empty);
  ava_pcode_linker_add_package(linker, AVA_ASCII9_STRING("foo"), empty);
  (void)ava_pcode_linker_link(linker, &errors);

  ck_assert(!TAILQ_EMPTY(&errors));
}

deftest(linker_considers_packages_and_modules_separate_namespaces) {
  ava_compile_error_list errors;
  ava_pcode_linker* linker = ava_pcode_linker_new();
  const ava_pcode_global_list* empty = parse_pcode("");

  TAILQ_INIT(&errors);
  ava_pcode_linker_add_module(linker, AVA_ASCII9_STRING("foo"), empty);
  ava_pcode_linker_add_package(linker, AVA_ASCII9_STRING("foo"), empty);
  (void)ava_pcode_linker_link(linker, &errors);

  ck_assert(TAILQ_EMPTY(&errors));
}

deftest(empty_link) {
  link_modules_like("", ava_false);
}

deftest(reexported_exports_kept) {
  link_modules_like(
    "[ext-var [ava bar]]\n"
    "[export 0 true bar]\n"
    "[macro true foo 5 0 [die]]\n",
    ava_false,

    "module",
    "[ext-var [ava bar]]\n"
    "[export 0 true bar]\n"
    "[macro true foo 5 0 [die]]\n");
}

deftest(nonreexported_exports_deleted) {
  link_modules_like(
    "[ext-var [ava bar]]\n",
    ava_false,

    "module",
    "[ext-var [ava bar]]\n"
    "[export 0 false bar]\n"
    "[macro false foo 5 0 [die]]\n");
}

deftest(global_refs_relinked_after_export_deletions) {
  link_modules_like(
    "[ext-var [ava private]]\n"
    "[ext-var [ava public]]\n"
    "[export 1 true public]\n"
    "[fun false [ava init] [ava pos] [\"\"] [\n"
    "  [set-glob 0 v0]\n"
    "  [set-glob 1 v0]\n"
    "]]\n"
    "[init 3]\n",
    ava_false,

    "module",
    "[ext-var [ava private]]\n"
    "[export 0 false private]\n"
    "[ext-var [ava public]]\n"
    "[export 2 true public]\n"
    "[fun false [ava init] [ava pos] [\"\"] [\n"
    "  [set-glob 0 v0]\n"
    "  [set-glob 2 v0]\n"
    "]]\n"
    "[init 4]\n");
}

deftest(unpublished_globals_do_not_conflict) {
  link_modules_like(
    "[var false [ava private]]\n"
    "[var false [ava private]]\n"
    "[fun false [ava init] [ava pos] [\"\"] [\n"
    "  [set-glob 0 v0]\n"
    "  [set-glob 1 v0]\n"
    "]]\n",
    ava_false,

    "module",
    "[macro false macro 5 0 [die]]\n"
    "[var false [ava private]]\n"
    "[var false [ava private]]\n"
    "[fun false [ava init] [ava pos] [\"\"] [\n"
    "  [set-glob 1 v0]\n"
    "  [set-glob 2 v0]\n"
    "]]\n");
}

deftest(redundant_externals_collapsed) {
  link_modules_like(
    "[ext-var [ava some-var]]\n"
    "[ext-fun [ava bar] [ava pos pos]]\n",
    ava_false,

    "module",
    "[ext-var [ava some-var]]\n"
    "[ext-fun [ava bar] [ava pos pos]]\n"
    "[ext-var [ava some-var]]\n"
    "[ext-fun [ava bar] [ava pos pos]]\n");
}

deftest(external_collapsed_into_prior_local) {
  link_modules_like(
    "[var true [ava foo]]\n",
    ava_false,

    "module",
    "[var true [ava foo]]\n"
    "[ext-var [ava foo]]\n");
}

deftest(external_collapsed_into_later_local) {
  link_modules_like(
    "[var true [ava foo]]\n",
    ava_false,

    "module",
    "[ext-var [ava foo]]\n"
    "[var true [ava foo]]\n");
}

deftest(globals_refs_relinked_after_cannonicalisation) {
  link_modules_like(
    "[var true [ava foo]]\n"
    "[fun false [ava init] [ava pos] [\"\"] [\n"
    "  [set-glob 0 v0]\n"
    "  [ld-glob v0 0]\n"
    "  [push d 1]\n"
    "  [ld-reg-s d0 v0]\n"
    "  [invoke-ss d0 3 0 1]\n"
    "  [pop d 1]\n"
    "]]\n"
    "[init 1]\n"
    "[fun true [ava doit] [ava pos] [x] [\n"
    "  [ret v0]\n"
    "]]\n",
    ava_false,

    "module",
    "[ext-var [ava foo]]\n"
    "[var true [ava foo]]\n"
    "[ext-fun [ava doit] [ava pos]]\n"
    "[fun false [ava init] [ava pos] [\"\"] [\n"
    "  [set-glob 1 v0]\n"
    "  [ld-glob v0 0]\n"
    "  [push d 1]\n"
    "  [ld-reg-s d0 v0]\n"
    "  [invoke-ss d0 2 0 1]\n"
    "  [pop d 1]\n"
    "]]\n"
    "[init 3]\n"
    "[fun true [ava doit] [ava pos] [x] [\n"
    "  [ret v0]\n"
    "]]\n");
}

deftest(struct_refs_relinked) {
  link_modules_like(
    "[ext-var [ava some-var]]\n"
    "[decl-sxt true [[struct foo] [value v]]]\n"
    "[fun false [ava init] [ava pos] [\"\"] [\n"
    "  [S-v-ld v0 v0 2 0 false]\n"
    "  [ret v0]\n"
    "]]\n",
    ava_false,

    "module",
    /* So some collapse happens before the refs */
    "[ext-var [ava some-var]]\n"
    "[ext-var [ava some-var]]\n"
    "[decl-sxt true [[struct foo] [value v]]]\n"
    "[fun false [ava init] [ava pos] [\"\"] [\n"
    "  [S-v-ld v0 v0 3 0 false]\n"
    "  [ret v0]\n"
    "]]\n");
}

deftest(structs_not_deduped) {
  link_modules_like(
    "[decl-sxt true [[struct foo] [value v]]]\n"
    "[decl-sxt false [[struct foo] [value v]]]\n",
    ava_false,

    "module",
    "[decl-sxt true [[struct foo] [value v]]]\n"
    "[decl-sxt false [[struct foo] [value v]]]\n");
}

deftest(nondependent_modules_concatenated) {
  link_modules_like(
    "[fun false [ava init] [ava pos] [\"\"] [\n"
    "  [set-glob 2 v0]\n"
    "]]\n"
    "[init 0]\n"
    "[var false [ava foo]]\n"
    "[fun false [ava init] [ava pos] [\"\"] [\n"
    "  [set-glob 5 v0]\n"
    "]]\n"
    "[init 3]\n"
    "[var false [ava bar]]\n",
    ava_false,

    "module-a",
    "[fun false [ava init] [ava pos] [\"\"] [\n"
    "  [set-glob 2 v0]\n"
    "]]\n"
    "[init 0]\n"
    "[var false [ava foo]]\n",

    "module-b",
    "[fun false [ava init] [ava pos] [\"\"] [\n"
    "  [set-glob 2 v0]\n"
    "]]\n"
    "[init 0]\n"
    "[var false [ava bar]]\n");
}

deftest(dependent_modules_concatenated_in_correct_order) {
  link_modules_like(
    "[fun false [ava init-b] [ava pos] [\"\"] [\n"
    "  [set-glob 2 v0]\n"
    "]]\n"
    "[init 0]\n"
    "[var false [ava from-module-b]]\n"
    "[fun false [ava init-a] [ava pos] [\"\"] [\n"
    "  [set-glob 5 v0]\n"
    "]]\n"
    "[init 3]\n"
    "[var false [ava from-module-a]]\n",
    ava_false,

    "module-a",
    "[fun false [ava init-a] [ava pos] [\"\"] [\n"
    "  [set-glob 2 v0]\n"
    "]]\n"
    "[init 0]\n"
    "[var false [ava from-module-a]]\n"
    "[load-mod module-b]\n",

    "module-b",
    "[fun false [ava init-b] [ava pos] [\"\"] [\n"
    "  [set-glob 2 v0]\n"
    "]]\n"
    "[init 0]\n"
    "[var false [ava from-module-b]]\n");
}

deftest(dependent_packages_concatenated_in_correct_order) {
  link_modules_like(
    "[fun false [ava init-b] [ava pos] [\"\"] [\n"
    "  [set-glob 2 v0]\n"
    "]]\n"
    "[init 0]\n"
    "[var false [ava from-package-b]]\n"
    "[fun false [ava init-a] [ava pos] [\"\"] [\n"
    "  [set-glob 5 v0]\n"
    "]]\n"
    "[init 3]\n"
    "[var false [ava from-package-a]]\n",
    ava_true,

    "package-a",
    "[fun false [ava init-a] [ava pos] [\"\"] [\n"
    "  [set-glob 2 v0]\n"
    "]]\n"
    "[init 0]\n"
    "[var false [ava from-package-a]]\n"
    "[load-pkg package-b]\n",

    "package-b",
    "[fun false [ava init-b] [ava pos] [\"\"] [\n"
    "  [set-glob 2 v0]\n"
    "]]\n"
    "[init 0]\n"
    "[var false [ava from-package-b]]\n");
}

deftest(unmatched_load_mod_or_pkg_kept_after_link) {
  link_modules_like(
    "[load-mod some-mod]\n"
    "[load-pkg some-pkg]\n",
    ava_false,

    "module",
    "[load-mod some-mod]\n"
    "[load-pkg some-pkg]\n");
}

deftest(duplicated_published_symbol_results_in_error) {
  ava_compile_error_list errors;
  ava_pcode_linker* linker;

  linker = ava_pcode_linker_new();
  TAILQ_INIT(&errors);

  ava_pcode_linker_add_module(
    linker, AVA_ASCII9_STRING("module-a"),
    parse_pcode("[var true [ava foo]]\n"));
  ava_pcode_linker_add_module(
    linker, AVA_ASCII9_STRING("module-b"),
    parse_pcode("[var true [ava foo]]\n"));

  (void)ava_pcode_linker_link(linker, &errors);

  ck_assert(!TAILQ_EMPTY(&errors));
}

deftest(cyclic_dependency_results_in_error) {
  ava_compile_error_list errors;
  ava_pcode_linker* linker;

  linker = ava_pcode_linker_new();
  TAILQ_INIT(&errors);

  ava_pcode_linker_add_module(
    linker, AVA_ASCII9_STRING("module-a"),
    parse_pcode("[load-mod module-a]\n"));

  (void)ava_pcode_linker_link(linker, &errors);

  ck_assert(!TAILQ_EMPTY(&errors));
}
