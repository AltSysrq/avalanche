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

#ifndef TEST_H_
#define TEST_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <check.h>
#include <stdlib.h>
#include <signal.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#include "runtime/avalanche.h"

extern const char* suite_names[1024];
extern void (*suite_impls[1024])(Suite*);
extern unsigned suite_num;

static const char* test_names[1024];
static TFun test_impls[1024];
static int test_signals[1024];
static unsigned test_num;

static void (*setups[256])(void);
static void (*teardowns[256])(void);
static unsigned setup_num, teardown_num;

static inline void run_suite(Suite* suite) {
  unsigned i, j;
  TCase* kase;

  for (i = 0; i < test_num; ++i) {
    /* Run the tests in reverse registration order, since GCC runs the
     * constructors in the file from the bottom up.
     */
    kase = tcase_create(test_names[test_num-i-1]);
    for (j = 0; j < setup_num && j < teardown_num; ++j)
      tcase_add_checked_fixture(kase, setups[j], teardowns[j]);
    if (!test_signals[test_num-i-1])
      tcase_add_test(kase, test_impls[test_num-i-1]);
    else
      tcase_add_test_raise_signal(
        kase, test_impls[test_num-i-1], test_signals[test_num-i-1]);
    suite_add_tcase(suite, kase);
  }
}

static ava_value run_function(void* f) {
  (*(void(*)(void))f)();
  return ava_value_of_string(AVA_EMPTY_STRING);
}

#define defsuite(name)                          \
  static void _register_suite_##name(void)      \
    __attribute__((constructor));               \
  static void _register_suite_##name(void) {    \
    suite_names[suite_num] = #name;             \
    suite_impls[suite_num] = run_suite;         \
    ++suite_num;                                \
  }                                             \
  void dummy()

#if defined(HAVE_SETRLIMIT) && defined(RLIMIT_CORE)
#define SUPPRESS_DUMP_CORE() do {               \
    struct rlimit rlimit_zero = { 0, 0 };       \
    setrlimit(RLIMIT_CORE, &rlimit_zero);       \
  } while (0)
#else
#define SUPPRESS_DUMP_CORE() do { } while (0)
#endif

#define deftest_signal(name,sig)                \
  static void _register_##name(void)            \
    __attribute__((constructor));               \
  static void name##_impl(void);                \
  START_TEST(name) {                            \
    /* If we expect a signal, suppress */       \
    /* dumping core, it just wastes time. */    \
    if (sig)                                    \
      SUPPRESS_DUMP_CORE();                     \
    ava_invoke_in_context(run_function,         \
                          (void*)name##_impl);  \
  }                                             \
  END_TEST                                      \
  static void _register_##name(void) {          \
    test_names[test_num] = #name;               \
    test_impls[test_num] = name;                \
    test_signals[test_num] = sig;               \
    ++test_num;                                 \
  }                                             \
  static void name##_impl(void)
#define deftest(name) deftest_signal(name, 0)

#define GLUE(a,b) _GLUE(a,b)
#define _GLUE(a,b) a##b
#define defsetup                                        \
  static void GLUE(_setup_,__LINE__)(void);             \
  static void GLUE(_registersetup_,__LINE__)(void)      \
    __attribute__((constructor));                       \
  static void GLUE(_registersetup_,__LINE__)(void) {    \
    setups[setup_num++] = GLUE(_setup_,__LINE__);       \
  }                                                     \
  static void GLUE(_setup_,__LINE__)(void)

#define defteardown                                        \
  static void GLUE(_teardown_,__LINE__)(void);             \
  static void GLUE(_registerteardown_,__LINE__)(void)      \
    __attribute__((constructor));                          \
  static void GLUE(_registerteardown_,__LINE__)(void) {    \
    teardowns[teardown_num++] = GLUE(_teardown_,__LINE__); \
  }                                                        \
  static void GLUE(_teardown_,__LINE__)(void)

#endif /* TEST_H_ */
