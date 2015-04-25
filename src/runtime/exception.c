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

int ava_pop_handler(int do_pop) {
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

void ava_rethrow(ava_exception_handler*restrict handler) {
  ava_throw(handler->exception_type, handler->value, handler->stack_trace);
}

const ava_exception_type ava_user_exception_type = {
  .uncaught_description = "user exception"
}, ava_error_exception_type = {
  .uncaught_description = "programming error"
}, ava_format_exception_type = {
  .uncaught_description = "string format error"
}, ava_internal_exception_type = {
  .uncaught_description = "internal error"
}, ava_interrupt_exception_type = {
  .uncaught_description = "interruption"
};
