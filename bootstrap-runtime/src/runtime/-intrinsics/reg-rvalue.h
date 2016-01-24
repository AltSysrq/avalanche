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
#ifndef AVA_RUNTIME__INTRINSICS_REG_RVALUE_H_
#define AVA_RUNTIME__INTRINSICS_REG_RVALUE_H_

#include "../avalanche/macsub.h"
#include "../avalanche/pcode.h"

/**
 * A pseudo-AST-node which behaves as a pure rvalue which always produces the
 * value from a particular P-Code register.
 *
 * Use of this node type is very limited, since useful errors about it cannot
 * be reported to the user, and since it relies on another node to control
 * selection/population of its basis register. Its only real use is as input
 * for other intrinsics that need AST nodes as input and have well-understood
 * behaviour as to when they will evaluate those nodes.
 */
typedef struct {
  ava_ast_node header;
  /**
   * The register to copy on read.
   *
   * It is up to the owner of this node to populate this field.
   */
  ava_pcode_register reg;
} ava_intr_reg_rvalue;

/**
 * Initialises the AST node header of the given reg-rvalue pseudo-node.
 */
void ava_intr_reg_rvalue_init(ava_intr_reg_rvalue* this,
                              ava_macsub_context* context);

#endif /* AVA_RUNTIME__INTRINSICS_REG_RVALUE_H_ */
