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
  Runs tests defined via Avalanche source code.

  Tests take the form of separate files under the DIRECTORY, relative to the
  "test" directory. Each filename specifies zero or more error strings,
  terminated by underscores, followed by the test name. If a test defines no
  errors, the program is expected to execute successfully and call test_pass()
  exactly once, with an argument of 42. If it does define errors, compilation
  is expected to fail and contain all the given error strings.

  The declaration for test_pass is
    extern pass-test pass_test c void [int pos]
 */

#define SUITE_NAME "avalanche"
#define DIRECTORY "runtime/ava-tests"

static ava_bool test_passed;
void pass_test(int i) {
  ck_assert_int_eq(42, i);
  ck_assert(!test_passed);
  test_passed = ava_true;
}

static const char** inputs;
static void run_test(int ix);
static ava_value run_test_impl(void* arg);

int main(void) {
  glob_t globbed;
  const char* name;
  Suite* suite;
  SRunner* sr;
  TCase* kase;
  unsigned failures, i;

  ava_init();

  /* If being run from the project root, change to the correct directory */
  (void)chdir("test");

  suite = suite_create(SUITE_NAME);

  if (glob(DIRECTORY "/*.ava", GLOB_NOSORT, NULL, &globbed))
    err(EX_NOINPUT, "Failed to list test cases");

  if (0 == globbed.gl_pathc)
    errx(EX_NOINPUT, "No test cases found");

  inputs = calloc(sizeof(char*), globbed.gl_pathc);

  for (i = 0; i < globbed.gl_pathc; ++i) {
    inputs[i] = globbed.gl_pathv[i];
    name = strrchr(globbed.gl_pathv[i], '/') + 1;
    kase = tcase_create(name);
    /* The only way to pass i into the test function it o use a loop test, even
     * though we only execute it for one index.
     */
    tcase_add_loop_test(kase, run_test, i, i+1);
    suite_add_tcase(suite, kase);
  }

  sr = srunner_create(suite);
  srunner_run_all(sr, CK_VERBOSE);
  failures = srunner_ntests_failed(sr);

  srunner_free(sr);
  globfree(&globbed);

  return failures < 255? failures : 255;
}

static void run_test(int ix) {
  ava_invoke_in_context(run_test_impl, &ix);
}

static ava_value run_test_impl(void* arg) {
  unsigned ix = *(int*)arg;
  FILE* infile;
  ava_string source;
  char buff[4096];
  size_t nread;
  ava_compile_error_list errors;
  ava_parse_unit parse_root;
  ava_macsub_context* macsub_context;
  ava_ast_node* root_node;
  ava_pcode_global_list* pcode;
  const char* underscore, * name, * error_str;
  char expected_error[64];

  TAILQ_INIT(&errors);

  infile = fopen(inputs[ix], "r");
  if (!infile) err(EX_NOINPUT, "fopen");

  source = AVA_EMPTY_STRING;
  do {
    nread = fread(buff, 1, sizeof(buff), infile);
    source = ava_string_concat(
      source, ava_string_of_bytes(buff, nread));
  } while (nread == sizeof(buff));
  fclose(infile);

  if (!ava_parse(&parse_root, &errors, source,
                 /* Don't use the actual filename so that expected errors
                  * aren't found trivially.
                  */
                 AVA_ASCII9_STRING("testinput")))
    goto done;

  macsub_context = ava_macsub_context_new(
    ava_symtab_new(NULL), &errors,
    AVA_ASCII9_STRING("input:"));
  ava_register_intrinsics(macsub_context);
  root_node = ava_macsub_run(macsub_context, &parse_root.location,
                             &parse_root.v.statements,
                             ava_isrp_void);
  ava_ast_node_postprocess(root_node);
  if (!TAILQ_EMPTY(&errors)) goto done;

  pcode = ava_codegen_run(root_node, &errors);
  if (!TAILQ_EMPTY(&errors)) goto done;

  test_passed = ava_false;
  ava_interp_exec(pcode);

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
