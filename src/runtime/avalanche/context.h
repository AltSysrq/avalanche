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
