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
#include "runtime/-llvm-support/drivers.h"
#include "bsd.h"

/*
  Runs tests defined via Avalanche source code.

  Tests take the form of separate files under the DIRECTORY, relative to the
  "test" directory. Each filename specifies zero or more error strings,
  terminated by underscores, followed by the test name. If a test defines no
  errors, the program is expected to execute successfully and call test_pass()
  exactly once, with an argument of 42. If it does define errors, compilation
  is expected to fail and contain all the given error strings.

  The declaration for test_pass is
    extern pass-test pass_test c void [int pos]

  Other functions provied for the tests include:

  - extern lindex lindex ava varargs pos
    Returns the second argumentth element in the list in the first argument. If
    the index is out of range, returns the empty string.

  - extern iadd iadd ava pos pos
    Interprets both arguments as integers, adds them, and returns the result.

  - extern iless iless ava pos pos
    If the first integer argument is less than the second, returns 1.
    Otherwise, returns 0.

  - extern lnot lnot ava pos
    Interprets the argument as an integer, and returns its logical negation.
 */

#define SUITE_NAME "avalanche"
#define DIRECTORY "ava-tests"

static void cache_avast_pcode() {
  ava_compenv* compenv;
  ava_compile_error_list errors;

  compenv = ava_compenv_new(AVA_ASCII9_STRING("input:"));
  ava_compenv_use_simple_source_reader(compenv, AVA_EMPTY_STRING);
  ava_compenv_use_standard_macsub(compenv);

  (void)ava_compenv_standard_new_macsub(compenv, &errors);
}

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

ava_value ava_register_test(ava_value name, ava_value function) {
  ava_function_parameter parm;

  parm.type = ava_fpt_static;
  parm.value = ava_empty_list().v;
  (void)ava_function_bind_invoke(
    ava_function_of_value(function), 1, &parm);
  return ava_empty_list().v;
}

static const char** inputs;
static void run_test(int ix);
static ava_value run_test_impl(void* arg);
static void execute_xcode(ava_compenv* compenv,
                          const ava_xcode_global_list* xcode);

static void nop(void) { }

int main(void) {
  glob_t globbed;
  const char* name;
  Suite* suite;
  SRunner* sr;
  TCase* kase;
  unsigned failures, i;

  ava_init();
  cache_avast_pcode();

  /* If being run from the project root, change to the correct directory */
  (void)chdir("test");

  suite = suite_create(SUITE_NAME);

  if (glob(DIRECTORY "/*.ava", 0, NULL, &globbed))
    err(EX_NOINPUT, "Failed to list test cases");

  if (0 == globbed.gl_pathc)
    errx(EX_NOINPUT, "No test cases found");

  inputs = calloc(sizeof(char*), globbed.gl_pathc);

  for (i = 0; i < globbed.gl_pathc; ++i) {
    inputs[i] = globbed.gl_pathv[i];
    name = strrchr(globbed.gl_pathv[i], '/') + 1;
    kase = tcase_create(name);
    /* libcheck 0.10.0 dies if running in NOFORK mode if there are no fixtures */
    tcase_add_checked_fixture(kase, nop, nop);
    /* The only way to pass i into the test function it o use a loop test, even
     * though we only execute it for one index.
     */
    tcase_add_loop_test(kase, run_test, i, i+1);
    suite_add_tcase(suite, kase);
  }

  sr = srunner_create(suite);
  srunner_run_all(sr, CK_ENV);
  failures = srunner_ntests_failed(sr);

  srunner_free(sr);
  globfree(&globbed);

  return failures < 255? failures : 255;
}

static void run_test(int ix) {
  ava_invoke_in_context(run_test_impl, &ix);
}

static ava_bool read_source_mask_filename(
  ava_value* dst, ava_string* error, ava_string filename,
  ava_compenv* compenv
) {
  if (ava_compenv_simple_read_source(dst, error, filename, compenv)) {
    /* Mask the reported filename away so that error tests don't trivially pass
     * by virtue of having the expected message in the filename.
     */
    *dst = ava_list_set(*dst, 0, ava_value_of_string(
                          AVA_ASCII9_STRING("testinput")));
    return ava_true;
  } else {
    return ava_false;
  }
}

static ava_value run_test_impl(void* arg) {
  AVA_STATIC_STRING(prefix, DIRECTORY "/");

  unsigned ix = *(int*)arg;
  ava_compile_error_list errors;
  ava_compenv* compenv;
  ava_pcode_global_list* pcode;
  ava_xcode_global_list* xcode;
  const char* underscore, * name, * error_str;
  char expected_error[64];

  TAILQ_INIT(&errors);

  compenv = ava_compenv_new(AVA_ASCII9_STRING("input:"));
  ava_compenv_use_simple_source_reader(compenv, prefix);
  compenv->read_source = read_source_mask_filename;
  ava_compenv_use_standard_macsub(compenv);

  if (!ava_compenv_compile_file(&pcode, &xcode, compenv,
                                ava_string_of_cstring(
                                  inputs[ix] + ava_strlen(prefix)),
                                &errors, NULL))
    goto done;

  test_passed = ava_false;
  execute_xcode(compenv, xcode);

  done:
  name = strrchr(inputs[ix], '/') + 1;
  underscore = strchr(name, '_');
  if (underscore) {
    ck_assert_msg(
      !TAILQ_EMPTY(&errors), "Compilation succeeeded unexpectedly.");
    error_str = ava_string_to_cstring(
      ava_error_list_to_string(&errors, 50, ava_false));
    for (; underscore; name = underscore+1, underscore = strchr(name, '_')) {
      ck_assert_int_lt(underscore - name, sizeof(expected_error));
      memcpy(expected_error, name, underscore - name);
      expected_error[underscore-name] = 0;
      ck_assert_msg(!!strstr(error_str, expected_error),
                    "Error %s not emitted; errors were:\n%s",
                    expected_error, error_str);
    }
  } else {
    ck_assert_msg(
      TAILQ_EMPTY(&errors), "Compilation failed unexpectedly.\n%s",
      ava_string_to_cstring(
        ava_error_list_to_string(&errors, 50, ava_false)));
    ck_assert_msg(
      test_passed, "Test failed to call test_passed().");
  }

  return ava_empty_list().v;
}

static void add_dependent_modules(
  ava_jit_context* jit,
  ava_compenv* compenv,
  const ava_xcode_global_list* xcode,
  ava_map_value* loaded_modules
) {
  size_t i;
  const ava_pcg_load_mod* lm;
  ava_value mnv;
  ava_map_cursor cursor;
  ava_compile_error_list errors;
  ava_xcode_global_list* submod;

  for (i = 0; i < xcode->length; ++i) {
    if (ava_pcgt_load_mod == xcode->elts[i].pc->type) {
      lm = (const ava_pcg_load_mod*)xcode->elts[i].pc;
      mnv = ava_value_of_string(lm->name);
      cursor = ava_map_find(*loaded_modules, mnv);
      if (AVA_MAP_CURSOR_NONE == cursor) {
        TAILQ_INIT(&errors);
        if (!ava_compenv_compile_file(
              NULL, &submod, compenv,
              ava_strcat(lm->name, AVA_ASCII9_STRING(".ava")),
              &errors, NULL)) {
          ck_abort_msg("Compilation of submodule %s failed:\n%s",
                       ava_string_to_cstring(lm->name),
                       ava_string_to_cstring(
                         ava_error_list_to_string(
                           &errors, 50, ava_false)));
        }

        ava_jit_add_module(jit, submod, lm->name, lm->name,
                           AVA_ASCII9_STRING("input:"));
        *loaded_modules = ava_map_add(*loaded_modules,
                                      mnv, ava_empty_map().v);
        add_dependent_modules(jit, compenv, submod, loaded_modules);
      }
    }
  }
}

static void execute_xcode(ava_compenv* compenv,
                          const ava_xcode_global_list* xcode) {
  ava_string jit_error;
  ava_jit_context* jit;
  ava_map_value loaded_modules;

  loaded_modules = ava_empty_map();

  jit = ava_jit_context_new();
  ava_jit_add_driver(jit, ava_driver_isa_unchecked_data,
                     ava_driver_isa_unchecked_size);
  ava_jit_add_driver(jit, ava_driver_avast_checked_2_data,
                     ava_driver_avast_checked_2_size);
  add_dependent_modules(jit, compenv, xcode, &loaded_modules);
  jit_error = ava_jit_run_module(
    jit, xcode,
    AVA_ASCII9_STRING("testinput"),
    AVA_ASCII9_STRING("main"),
    AVA_ASCII9_STRING("input:"));

  if (ava_string_is_present(jit_error))
    ck_abort_msg("JIT failed: %s", ava_string_to_cstring(jit_error));

  ava_jit_context_delete(jit);
}
