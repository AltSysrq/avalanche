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

#define AVA__INTERNAL_INCLUDE 1
#include "runtime/avalanche/exception.h"

#if defined(__GNUC__) || defined(__clang__)
#define NOINLINE __attribute__((__noinline__))
#else
#define NOINLINE
#endif

defsuite(exception);

static void throw0(ava_value value) NOINLINE;
static void throw1(ava_value value) NOINLINE;
static void rethrow0(ava_value value) NOINLINE;
static void a$avast__ava_lang__org___prelude__$2B(ava_value value) NOINLINE;

static void throw0(ava_value value) {
  ava_throw(&ava_user_exception, value, NULL);
}

static void throw1(ava_value value) {
  throw0(value);
}

static void a$avast__ava_lang__org___prelude__$2B(ava_value value) {
  throw1(value);
}

static void rethrow0(ava_value value) {
  ava_exception_handler handler;
  ava_try (handler) {
    throw1(value);
    ck_abort_msg("Exception not thrown");
  } ava_catch (handler, ava_internal_exception) {
    ck_abort_msg("Wrong catch block");
  } ava_catch_all {
    ava_rethrow(&handler);
  }
}

deftest_signal(uncaught_exception, SIGABRT) {
  ava_value value = ava_value_of_string(ava_string_of_cstring("hello world"));

  a$avast__ava_lang__org___prelude__$2B(value);
}

deftest(caught_exception) {
  ava_value value = ava_value_of_string(ava_string_of_cstring("hello world"));

  ava_exception_handler handler;
  ava_try (handler) {
    throw1(value);
    ck_abort_msg("Exception not thrown");
  } ava_catch (handler, ava_interrupt_exception) {
    ck_abort_msg("Wrong catch block");
  } ava_catch (handler, ava_user_exception) {
    ck_assert_ptr_ne(NULL, handler.stack_trace);
    ck_assert_str_eq("hello world", ava_string_to_cstring(
                       ava_to_string(handler.value)));
  } ava_catch_all {
    ck_abort_msg("Entered catch-all block");
  }
}

deftest(rethrow) {
  ava_value value = ava_value_of_string(ava_string_of_cstring("hello world"));

  ava_exception_handler handler;
  ava_try (handler) {
    rethrow0(value);
    ck_abort_msg("Exception not thrown");
  } ava_catch (handler, ava_interrupt_exception) {
    ck_abort_msg("Wrong catch block");
  } ava_catch (handler, ava_user_exception) {
    ck_assert_ptr_ne(NULL, handler.stack_trace);
    ck_assert_str_eq("hello world", ava_string_to_cstring(
                       ava_to_string(handler.value)));
  } ava_catch_all {
    ck_abort_msg("Entered catch-all block");
  }
}
