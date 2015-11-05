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

/* The macro AVA_NOEXTRACE can be used to disable all libunwind usage, ie,
 * exceptions will have no stack trace.
 *
 * This is useful for running within valgrind, since valgrind and libunwind do
 * not get along very well.
 *
 * Note that this *will* break the exception unit tests, which expect the stack
 * trace to exist.
 */

#define UNW_LOCAL_ONLY

#include <stdio.h>
#include <setjmp.h>
#include <string.h>

#ifdef HAVE_BSD_STRING_H
#include <bsd/string.h>
#endif

#ifndef AVA_NOEXTRACE
#include <libunwind.h>
#endif

#define AVA__INTERNAL_INCLUDE 1
#include "avalanche/alloc.h"
#include "avalanche/name-mangle.h"
#include "avalanche/list.h"
#include "avalanche/exception.h"
#include "-context.h"
#include "bsd.h"

ava_stack_trace* ava_generate_stack_trace(void) {
#ifndef AVA_NOEXTRACE
  unw_context_t uc;
  unw_cursor_t cursor;
  unw_word_t ip, ip_off;
  ava_demangled_name demangled;
  ava_str_tmpbuff strtmp;
  ava_stack_trace* trace = NULL, * new;

  unw_getcontext(&uc);
  unw_init_local(&cursor, &uc);
  while (unw_step(&cursor) > 0) {
    new = AVA_NEW(ava_stack_trace);

    if (0 == unw_get_reg(&cursor, UNW_REG_IP, &ip))
      new->ip = (const void*)ip;

    if (unw_get_proc_name(&cursor, new->in_function,
                          sizeof(new->in_function),
                          &ip_off) < 0) {
      memcpy(new->in_function, "<unknown>", sizeof("<unknown>"));
      new->function_type = "???";
    } else {
      demangled = ava_name_demangle(ava_string_of_cstring(new->in_function));

      (void)strlcpy(new->in_function,
                    ava_string_to_cstring_buff(strtmp, demangled.name),
                    sizeof(new->in_function));
      switch (demangled.scheme) {
      case ava_nms_none:
        new->function_type = "C";
        break;

      case ava_nms_ava:
        new->function_type = "AVA";
        break;
      }

      new->ip_offset = ip_off;
    }

    new->next = trace;
    trace = new;
  }

  return trace;
#else
  return NULL;
#endif
}

jmp_buf* ava_push_handler(ava_exception_handler*restrict dst) {
  dst->next = ava_current_context->exception_handlers;
  ava_current_context->exception_handlers = dst;
  return &dst->resume_point;
}

ava_bool ava_pop_handler(ava_bool do_pop) {
  if (do_pop)
    ava_current_context->exception_handlers =
      ava_current_context->exception_handlers->next;
  return do_pop;
}

void ava_throw(const ava_exception_type* type, ava_value value,
               ava_stack_trace* trace) {
  if (!trace)
    trace = ava_generate_stack_trace();

  if (ava_current_context->exception_handlers) {
    ava_exception_handler*restrict handler =
      ava_current_context->exception_handlers;

    handler->exception_type = type;
    handler->value = value;
    handler->stack_trace = trace;
    longjmp(handler->resume_point, 1);
    /* unreachable */
  }

  warnx("panic: uncaught %s: %s",
        type->uncaught_description,
        ava_string_to_cstring(ava_to_string(value)));
  while (trace) {
    warnx("\tat %3s %s + 0x%x [%p]", trace->function_type, trace->in_function,
          (int)trace->ip_offset, trace->ip);
    trace = trace->next;
  }

  abort();
}

void ava_throw_str(const ava_exception_type* type, ava_string str) {
  ava_throw(type, ava_value_of_string(str), NULL);
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

  ava_throw_str(type, ava_to_string(outer.v));
}

void ava_rethrow(ava_exception_handler*restrict handler) {
  ava_throw(handler->exception_type, handler->value, handler->stack_trace);
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
