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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <glob.h>

#include "runtime/avalanche.h"
#include "bsd.h"

/*
  Runs tests defined via Avalanche source code, but which are compiled into
  this executable.

  When this program starts, it first initialises the `ava-tests` package, which
  is expected to use `ava_register_test` to add test functions.  Each of these
  functions becomes one check test, which runs that function and expects it to
  call pass_test with an argument of exactly 42 exactly one time.
 */

static ava_list_value test_functions;
extern void a$ava_tests___$28init$29(void);

/* Functions available to tests */
static ava_bool test_passed;
void pass_test(int i) {
  ck_assert_int_eq(42, i);
  ck_assert(!test_passed);
  test_passed = ava_true;
}

ava_value lindex(ava_value list, ava_value index) {
  ava_integer ix = ava_integer_of_value(index, 0);

  if (ix < 0 || (size_t)ix >= ava_list_length(list))
    return ava_empty_list().v;
  else
    return ava_list_index(list, ix);
}

ava_value iadd(ava_value a, ava_value b) {
  return ava_value_of_integer(
    ava_integer_of_value(a, 0) +
    ava_integer_of_value(b, 0));
}

ava_value iless(ava_value a, ava_value b) {
  return ava_value_of_integer(
    ava_integer_of_value(a, 0) <
    ava_integer_of_value(b, 0));
}

ava_value lnot(ava_value a) {
  return ava_value_of_integer(
    !ava_integer_of_value(a, 0));
}

ava_value ava_register_test(ava_value name, ava_value fun) {
  test_functions = ava_list_append(test_functions, name);
  test_functions = ava_list_append(test_functions, fun);
  return ava_empty_list().v;
}

static void run_test(int ix) {
  ava_function_parameter parm;

  parm.type = ava_fpt_static;
  parm.value = ava_empty_list().v;

  test_passed = ava_false;
  (void)ava_function_bind_invoke(
    ava_function_of_value(ava_list_index(test_functions, ix*2 + 1)),
    1, &parm);
  ck_assert_msg(test_passed, "pass_test never called");
}

static void nop(void) { }

static ava_value main_impl(void* ignore) {
  Suite* suite;
  SRunner* sr;
  TCase* kase;
  size_t i, n;
  ava_integer failures;
  const char** names;

  test_functions = ava_empty_list();
  a$ava_tests___$28init$29();

  suite = suite_create("compiled-avalanche");
  n = ava_list_length(test_functions) / 2;
  names = ava_alloc(sizeof(char*) * n);
  for (i = 0; i < n; ++i) {
    names[i] = ava_string_to_cstring(
      ava_to_string(ava_list_index(test_functions, i*2)));
    kase = tcase_create(names[i]);
    /* Add a fixture to make libcheck 0.10.0 happy */
    tcase_add_checked_fixture(kase, nop, nop);
    tcase_add_loop_test(kase, run_test, i, i+1);
    suite_add_tcase(suite, kase);
  }

  sr = srunner_create(suite);
  srunner_run_all(sr, CK_ENV);
  failures = srunner_ntests_failed(sr);

  srunner_free(sr);

  return ava_value_of_integer(failures);
}

int main(void) {
  ava_integer result;

  ava_init();
  result =  ava_integer_of_value(
    ava_invoke_in_context(main_impl, NULL), -1);

  return result < 255? result : 255;
}
