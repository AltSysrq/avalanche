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
#include "test.c"

#include <stdlib.h>
#include <string.h>

#define AVA__INTERNAL_INCLUDE 1
#include "runtime/avalanche/defs.h"
#include "runtime/avalanche/string.h"
#include "runtime/avalanche/value.h"
#include "runtime/avalanche/pcode.h"
#include "runtime/avalanche/interp.h"

defsuite(interp);

int eval_a, eval_b;
void doit(int a, int b) {
  eval_a = a;
  eval_b = b;
}

deftest(interpreter_basically_works) {
  const ava_pcode_global_list* pcode;

  pcode = ava_pcode_global_list_of_string(
    ava_string_of_cstring(
      "\\{ext-fun \"none doit\" \\{c void \"int pos\" \"int pos\"\\}\\}\n"
      "\\{fun false \"ava test-main\" \\{ava pos\\} _ \\{\n"
      "         \\{push d 2\\}\n"
      "         \\{ld-imm-vd d0 42\\}\n"
      "         \\{ld-imm-vd d1 56\\}\n"
      "         \\{invoke-ss d0 0 0 2\\}\n"
      "         \\{ret d0\\}\n"
      "         \\{pop d 2\\}\n"
      "\\}\\}\n"
      "\\{init 1\\}\n"));
  ava_interp_exec(pcode);

  ck_assert_int_eq(eval_a, 42);
  ck_assert_int_eq(eval_b, 56);
}
