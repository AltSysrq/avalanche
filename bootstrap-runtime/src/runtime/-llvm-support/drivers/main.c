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

/**
 * @file
 *
 * Provides the main() function for statically-compiled Avalanche programs. It
 * initialises the Avalanche runtime, then calls into the codegen-provided
 * \program-entry() function.
 */

#define AVA__INTERNAL_INCLUDE 1
#include "../../avalanche/defs.h"
#include "../../avalanche/string.h"
#include "../../avalanche/value.h"
#include "../../avalanche/context.h"

/* Provided by codegen */
extern void a$$5Cprogram_entry(void);

static ava_value ava_main_impl$(void* ignored);

int main(void) {
  ava_init();
  (void)ava_invoke_in_context(ava_main_impl$, 0);
  return 0;
}

ava_value ava_main_impl$(void* ignored) {
  a$$5Cprogram_entry();
  return ava_value_of_string(AVA_EMPTY_STRING);
}
