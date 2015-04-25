/*-
 * Copyright (c) 2015 Jason Lingle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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

static void throw0(ava_value value) {
  ava_throw(&ava_user_exception, value, NULL);
}

static void throw1(ava_value value) {
  throw0(value);
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

  throw1(value);
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
