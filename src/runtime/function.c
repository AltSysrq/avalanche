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

#include <stdio.h>
#include <setjmp.h>

#define AVA__INTERNAL_INCLUDE 1
#include "avalanche/alloc.h"
#include "avalanche/function.h"
#include "bsd.h"

jmp_buf* ava_set_handler(ava_stack_exception_handler*restrict dst,
                         ava_stack_element* restrict tos) {
  dst->header.type = ava_set_exception_handler;
  dst->header.next = tos;
  return &dst->resume_point;
}

void ava_throw(const ava_exception_type* type, ava_value value,
               ava_stack_element*restrict stack,
               const ava_stack_frame*restrict trace) {
  while (stack) {
    switch (stack->type) {
    case ava_set_frame: {
      ava_stack_frame* frame = ava_clone(
        stack, sizeof(ava_stack_frame));
      frame->header.next = (ava_stack_element*)trace;
      trace = frame;

      stack = stack->next;
    } break;

    case ava_set_exception_handler: {
      ava_stack_exception_handler*restrict handler =
        (ava_stack_exception_handler*restrict)stack;

      handler->exception_type = type;
      handler->value = value;
      handler->stack_trace = trace;
      longjmp(handler->resume_point, 1);
    } break;
    }
  }

  warnx("panic: uncaught %s: %s",
        type->uncaught_description,
        ava_string_to_cstring(ava_to_string(value)));
  while (trace) {
    warnx("\tat %s:%d", trace->filename, trace->line_number);
    trace = (const ava_stack_frame*restrict)trace->header.next;
  }

  abort();
}

void ava_rethrow(ava_stack_exception_handler*restrict handler) {
  ava_throw(handler->exception_type, handler->value,
            handler->header.next, handler->stack_trace);
}

const ava_exception_type ava_user_exception_type = {
  .uncaught_description = "user exception"
}, ava_error_exception_type = {
  .uncaught_description = "programming error"
}, ava_internal_exception_type = {
  .uncaught_description = "internal error"
}, ava_interrupt_exception_type = {
  .uncaught_description = "interruption"
};
