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
