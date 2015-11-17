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

#include <stdio.h>
#include <string.h>

#define AVA__INTERNAL_INCLUDE 1
#include "avalanche/defs.h"
AVA_BEGIN_DECLS
#include "avalanche/string.h"
#include "avalanche/value.h"
#include "avalanche/list.h"
#include "avalanche/exception.h"

/* Stay inside `extern "C"` */

void ava_throw(const ava_exception_type* type, ava_value value) {
  ava_exception ex;

  ex.type = type;
  memcpy(&ex.value, &value, sizeof(value));
  throw ex;
}

void ava_throw_str(const ava_exception_type* type, ava_string str) {
  ava_throw(type, ava_value_of_string(str));
}

void ava_throw_uex(const ava_exception_type* type, ava_string user_type,
                   ava_string message) {
  ava_list_value inner, outer;
  ava_value outer_values[2];
  ava_value inner_values[2] = {
    ava_value_of_string(AVA_ASCII9_STRING("message")),
    ava_value_of_string(message),
  };

  outer_values[0] = ava_value_of_string(user_type);
  inner = ava_list_of_values(inner_values, 2);
  outer_values[1] = inner.v;
  outer = ava_list_of_values(outer_values, 2);

  ava_throw(type, outer.v);
}

void ava_rethrow(ava_exception ex) {
  throw ex;
}

ava_bool ava_catch(ava_exception* dst, void (*f)(void*), void* ud) {
  try {
    (*f)(ud);
    return ava_false;
  } catch (ava_exception ex) {
    *dst = ex;
    return ava_true;
  }
}

ava_value ava_exception_get_value(const ava_exception* ex) {
  ava_value dst;

  memcpy(&dst, ex->value, sizeof(ava_value));
  return dst;
}

const ava_exception_type ava_user_exception = {
  .uncaught_description = "user exception"
}, ava_error_exception = {
  .uncaught_description = "programming error"
}, ava_format_exception = {
  .uncaught_description = "string format error"
}, ava_internal_exception = {
  .uncaught_description = "internal error"
}, ava_interrupt_exception = {
  .uncaught_description = "interruption"
}, ava_undefined_behaviour_exception = {
  .uncaught_description = "undefined behaviour error"
};

AVA_END_DECLS
