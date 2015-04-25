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
#ifndef AVA__INTERNAL_INCLUDE
#error "Don't include avalanche/context.h directly; just include avalanche.h"
#endif

#ifndef AVA_RUNTIME_CONTEXT_H_
#define AVA_RUNTIME_CONTEXT_H_

#include "value.h"

/**
 * Executes (*f)(arg) in a fresh Avalanche context.
 *
 * A context tracks data such as the exception handling stack. Every thread is
 * associated with a current context, and every context is bound to at most one
 * thread.
 *
 * Exceptions cannot cross context boundaries; if (*f) throws, the process will
 * abort.
 *
 * When this function is entered, the current context (if any) is saved, and
 * the context is switched to a fresh one to execute f. When f returns, the new
 * context is destroyed and the old one restored.
 *
 * There currently isn't much reason to layer one context on top of another.
 *
 * @param f The function to run within the new context.
 * @param arg The argument to pass to f.
 * @return The return value of (*f)(arg).
 */
ava_value ava_invoke_in_context(ava_value (*f)(void* arg), void* arg);

#endif /* AVA_RUNTIME_CONTEXT_H_ */
