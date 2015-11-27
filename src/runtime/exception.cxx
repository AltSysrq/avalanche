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
#include "../../contrib/libbacktrace/backtrace.h"
#include "../../contrib/libbacktrace/backtrace-supported.h"

#define AVA__INTERNAL_INCLUDE 1
#include "avalanche/defs.h"
AVA_BEGIN_DECLS
#include "avalanche/alloc.h"
#include "avalanche/string.h"
#include "avalanche/value.h"
#include "avalanche/list.h"
#include "avalanche/exception.h"

/**
 * Information about a single frame in an exception backtrace.
 */
typedef struct {
  /**
   * The IP/PC of the function at the point where the exception was thrown.
   */
  uintptr_t ip;
} ava_exception_frame;

struct ava_exception_throw_info_s {
  /**
   * The number of entries in the bt array and its capacity, respectively.
   */
  size_t bt_len, bt_cap;
  /**
   * An array of stack elements comprising the stack trace, where the zeroth
   * element is the most recent frame in the call chain. The first bt_len
   * elements are initialised; there is space for bt_cap elements.
   */
  ava_exception_frame* bt;
};

/* Stay inside `extern "C"` */

static void ava_exception_make_backtrace(ava_exception_throw_info* info);
static int ava_exception_trace(void* vinfo, uintptr_t ip);
static void ava_exception_init_error_callback(
  void* ignored, const char* msg, int errnum) AVA_UNUSED;

#if BACKTRACE_SUPPORTED
static struct backtrace_state* ava_exception_backtrace_context;
#endif
static ava_string ava_exception_why_backtrace_unavailable;

void ava_throw(const ava_exception_type* type, ava_value value) {
  ava_exception ex;
  ava_exception_throw_info* throw_info;

  throw_info = AVA_NEW(ava_exception_throw_info);
  ava_exception_make_backtrace(throw_info);

  ex.type = type;
  ex.throw_info = throw_info;
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

void ava_exception_init(void) {
#if BACKTRACE_SUPPORTED && BACKTRACE_SUPPORTS_THREADS
  ava_exception_backtrace_context = backtrace_create_state(
    NULL, ava_true, ava_exception_init_error_callback, NULL);
#else
#if BACKTRACE_SUPPORTED
  AVA_STATIC_STRING(
    msg, "libbacktrace doesn't support multi-threaded applications");
#else
  AVA_STATIC_STRING(
    msg, "libbacktrace not supported on this system.");
#endif
  ava_exception_why_backtrace_unavailable = msg;
#endif
}

static void ava_exception_init_error_callback(
  void* ignored, const char* msg, int errnum
) {
  ava_exception_why_backtrace_unavailable = ava_string_of_cstring(msg);
}

static void ava_exception_make_backtrace(ava_exception_throw_info* info) {
  info->bt_len = 0;
  info->bt_cap = 8;
  info->bt = (ava_exception_frame*)
    ava_alloc_atomic(sizeof(ava_exception_frame) * info->bt_cap);

#if BACKTRACE_SUPPORTED
  if (ava_exception_backtrace_context) {
    /* We don't care about the return value; if anything breaks, it just means
     * the trace is truncated.
     */
    (void)backtrace_simple(ava_exception_backtrace_context,
                           1, ava_exception_trace, NULL, info);
  } else
#endif
  {
    /* Backtrace unavailable; push back one placeholder slot */
    info->bt[info->bt_len++].ip = 0;
  }
}

static int ava_exception_trace(void* vinfo, uintptr_t ip) {
  ava_exception_throw_info* info = (ava_exception_throw_info*)vinfo;
  ava_exception_frame* neu;

  if (info->bt_len == info->bt_cap) {
    neu = (ava_exception_frame*)
      ava_alloc_atomic(sizeof(ava_exception_frame) * 2 * info->bt_cap);
    memcpy(neu, info->bt, sizeof(ava_exception_frame) * info->bt_cap);
    info->bt_cap *= 2;
    info->bt = neu;
  }

  info->bt[info->bt_len].ip = ip;
  ++info->bt_len;

  return 0;
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
