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

defsuite(exception);

static void throw_something(void* ignore) {
  ava_throw_str(&ava_format_exception,
                AVA_ASCII9_STRING("foobar"));
}

deftest(exceptions_basically_work) {
  ava_exception ex;

  ck_assert(ava_catch(&ex, throw_something, NULL));
  ck_assert_ptr_eq(&ava_format_exception, ex.type);
  assert_value_equals_str("foobar", ava_exception_get_value(&ex));
}
