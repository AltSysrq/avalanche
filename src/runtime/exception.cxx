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

#include <exception>

#include <stdio.h>
#include <string.h>
#include <assert.h>

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

typedef struct {
  ava_exception_location* dst;
  ava_string error;
} ava_exception_get_trace_context;

/* Stay inside `extern "C"` */

static void ava_exception_make_backtrace(ava_exception_throw_info* info);
static int ava_exception_trace(void* vinfo, uintptr_t ip);
static void ava_exception_init_error_callback(
  void* ignored, const char* msg, int errnum) AVA_UNUSED;
static int ava_exception_get_trace_success(
  void* vcxt, uintptr_t ip,
  const char* filename, int lineno, const char* function) AVA_UNUSED;
static void ava_exception_get_trace_error(
  void* vcxt, const char* msg, int code) AVA_UNUSED;

static void ava_exception_terminate_handler(void);
static std::terminate_handler ava_next_terminate_handler;

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

  /*
    This strange-looking construction is needed to work around what appears to
    be a bug in Clang 3.5's AMD64 code generation (which is used to build
    libgcc_s on FreeBSD).

    In libcxxrt's exception.cc, we have at the end of throw_exception

      _Unwind_Reason_Code err = _Unwind_RaiseException(...);
      report_failure(err, ex);

    and report_failure does

      switch (err) {
        case _URC_END_OF_STACK:
          __cxa_begin_throw(...);
          std::terminate();

        (* other cases we don't care about *)
        default: std::terminate();
      }

    Note that _URC_END_OF_STACK is 5.

    An abridged version of libgcc_s's _Unwind_RaiseException() from unwind.inc:

      (* lots of stuff *)

      while (1) {
        (* snip *)
        if (code == _URC_END_OF_STACK)
          return _URC_END_OF_STACK;
        (* snip *)
      }

      (* snip *)

      uw_install_context(...);
      (* unreachable *)

    However, the code Clang generates for this is

      mov ecx, eax ; eax = err
      mov eax, 0x5 ; eventual return value
      cmp ecx, 0x5 ; case _URC_END_OF_STACK
      je do_return

      do_return:
      add rsp, 0x368
      pop rax ; there goes the return value
      ; pop more registers
      ret

    This means that _Unwind_RaiseException always returns a garbage value when
    it does return, preserving whatever it happened to be when the function was
    called (it appears to balance a `push rax`). This in turn is generally a
    pointer with eax overwritten with a smaller integer.

    The end result is that report_failure() always takes the default case,
    which calls std::terminate() without making the uncaught exception the
    current exception.

    This hack works because `throw;` maps to __cxa_rethrow(), which rethrows
    the exception without dropping its status as the "current exception". The
    latter normally happens via __cxa_end_throw() which occurs at the end of
    the catch block in a cleanup handler. However, if there are *only* cleanup
    handlers available (ie, the exception is uncaught), libcxxrt (and G++'s C++
    runtime, for what it's worth) calls std::terminate() without running the
    cleanup handlers, primarily so that the stack is still in-tact for debugger
    inspection.

    This *also* means, though, that std::terminate() will be called between the
    __cxa_begin_throw() and __cxa_end_throw() of this catch block, and thus we
    can access the exception in our terminate handler.

   */
  try {
    throw ex;
  } catch (const ava_exception& e) {
    throw;
  }
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

  ava_next_terminate_handler =
    std::set_terminate(ava_exception_terminate_handler);
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

size_t ava_exception_get_trace_length(const ava_exception* ex) {
  return ex->throw_info->bt_len;
}

ava_intptr ava_exception_get_trace_ip(const ava_exception* ex, size_t frame) {
  assert(frame < ex->throw_info->bt_len);
  return ex->throw_info->bt[frame].ip;
}

ava_string ava_exception_get_trace_location(
  ava_exception_location* dst, const ava_exception* ex, size_t frame
) {
  AVA_STATIC_STRING(unknown_source, "<unknown-source>");
  AVA_STATIC_STRING(unknown_function, "<unknown-function>");
  ava_exception_get_trace_context cxt = {
    .dst = dst,
    .error = ava_exception_why_backtrace_unavailable,
  };

  assert(frame < ex->throw_info->bt_len);

  dst->ip = 0;
  dst->filename = unknown_source;
  dst->filename_known = ava_false;
  dst->function.scheme = ava_nms_none;
  dst->function.name = unknown_function;
  dst->function_known = ava_false;
  dst->lineno = -1;

#if BACKTRACE_SUPPORTED
  if (ava_exception_backtrace_context) {
    backtrace_pcinfo(ava_exception_backtrace_context,
                     ex->throw_info->bt[frame].ip,
                     ava_exception_get_trace_success,
                     ava_exception_get_trace_error,
                     &cxt);
  }
#endif

  return cxt.error;
}

static int ava_exception_get_trace_success(
  void* vcxt, uintptr_t ip,
  const char* filename, int lineno, const char* function
) {
  ava_exception_get_trace_context* cxt = (ava_exception_get_trace_context*)vcxt;

  cxt->dst->ip = ip;
  if (filename) {
    cxt->dst->filename = ava_string_of_cstring(filename);
    cxt->dst->filename_known = ava_true;
  }

  if (function) {
    cxt->dst->function = ava_name_demangle(ava_string_of_cstring(function));
    cxt->dst->function_known = ava_true;
  }

  cxt->dst->lineno = lineno? lineno : -1;

  return 0;
}

static void ava_exception_get_trace_error(
  void* vcxt, const char* msg, int code
) {
  ava_exception_get_trace_context* cxt = (ava_exception_get_trace_context*)vcxt;

  cxt->error = ava_string_of_cstring(msg);
}

ava_string ava_exception_trace_to_string(const ava_exception* ex) {
  AVA_STATIC_STRING(at_line_prefix, "\tat line\t");
  AVA_STATIC_STRING(in_fun_prefix, "\tin fun\t\t");
  const ava_string lf = AVA_ASCII9_STRING("\n");

  ava_string accum, error;
  size_t frame, n;
  ava_exception_location loc;
  char tmp[32];

  accum = AVA_EMPTY_STRING;
  n = ava_exception_get_trace_length(ex);
  for (frame = 0; frame < n; ++frame) {
    error = ava_exception_get_trace_location(&loc, ex, frame);

    /*
      The most important part of the trace is the line numbers, so place them
      first, make sure they line up, and that nothing interferes with that
      column.

      When line numbers are available, format as (">" = tab)
        0.......1.......2.......3........4
        at line> LINENO FILENAME...
        in fun> >       FUNCTION...

      When not available:
        0.......1.......2.......3........4
        in fun> >       FUNCTION @ IP (ERROR)
     */
    if (-1 != loc.lineno) {
      accum = ava_strcat(accum, at_line_prefix);
      snprintf(tmp, sizeof(tmp), "%7d ", loc.lineno);
      accum = ava_strcat(accum, ava_string_of_cstring(tmp));
      accum = ava_strcat(accum, loc.filename);
      accum = ava_strcat(accum, lf);
    }
    accum = ava_strcat(accum, in_fun_prefix);
    accum = ava_strcat(accum, loc.function.name);
    if (-1 == loc.lineno) {
      accum = ava_strcat(accum, AVA_ASCII9_STRING(" @ "));
      snprintf(tmp, sizeof(tmp), "%p", (void*)loc.ip);
      accum = ava_strcat(accum, ava_string_of_cstring(tmp));
    }

    if (ava_string_is_present(error)) {
      accum = ava_strcat(accum, AVA_ASCII9_STRING(" ("));
      accum = ava_strcat(accum, error);
      accum = ava_strcat(accum, AVA_ASCII9_STRING(")"));
    }

    accum = ava_strcat(accum, lf);
  }

  return accum;
}

static void ava_exception_terminate_handler(void) {
  std::exception_ptr ex = std::current_exception();

  if (ex) {
    try {
      std::rethrow_exception(ex);
    } catch (ava_exception ae) {
      fprintf(stderr, "Uncaught %s: %s\n%s", ae.type->uncaught_description,
              ava_string_to_cstring(
                ava_to_string(ava_exception_get_value(&ae))),
              ava_string_to_cstring(
                ava_exception_trace_to_string(&ae)));
      fflush(stderr);
    } catch (...) {
      /* Not ours, fall through */
    }
  }

  (*ava_next_terminate_handler)();
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
