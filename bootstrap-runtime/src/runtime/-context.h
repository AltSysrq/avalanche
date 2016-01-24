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
/* This is an internal header file */
#ifndef AVA_RUNTIME__CONTEXT_H
#define AVA_RUNTIME__CONTEXT_H

#include "avalanche/defs.h"

/* Adapted from
 * http://stackoverflow.com/questions/18298280/how-to-declare-a-variable-as-thread-local-portably
 */
#ifndef thread_local
#if __STDC_VERSION__ >= 201112 && !defined(__STDC_NO_THREADS__)
#define thread_local _Thread_local
#elif defined(_WIN32) && (defined(_MSC_VER) || defined(__ICL) || \
                          defined(__DMC__) || defined(__BORLANDC__))
#define thread_local __declspec(thread)
#elif defined(__GNUC__) || defined(__SUNPRO_C) || defined(__xlC__)
#define thread_local __thread
#else
#error "Don't know how to say 'thread_local' on this platform"
#endif
#endif /* !defined(thread_local) */

struct ava_exception_handler_s;
typedef struct {
  /**
   * The current exception handler stack for this context.
   */
  struct ava_exception_handler_s* exception_handlers;
} ava_context;

extern thread_local ava_context* ava_current_context;

#endif /* AVA_RUNTIME__CONTEXT_H */
