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

#define UNW_LOCAL_ONLY

#include <stdio.h>
#include <setjmp.h>
#include <string.h>

#include <libunwind.h>

#define AVA__INTERNAL_INCLUDE 1
#include "avalanche/alloc.h"
#include "avalanche/exception.h"
#include "-context.h"
#include "bsd.h"

ava_stack_trace* ava_generate_stack_trace(void) {
  unw_context_t uc;
  unw_cursor_t cursor;
  unw_word_t ip, ip_off;

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
      new->function_type = "C";
      new->ip_offset = ip_off;
    }

    new->next = trace;
    trace = new;
  }

  return trace;
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
};
