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
#ifndef AVA_AVALANCHE_H_
#define AVA_AVALANCHE_H_

#define AVA__INTERNAL_INCLUDE 1

#include "avalanche/defs.h"

AVA_BEGIN_DECLS

#include "avalanche/alloc.h"
#include "avalanche/string.h"
#include "avalanche/lex.h"
#include "avalanche/value.h"
#include "avalanche/integer.h"
#include "avalanche/real.h"
#include "avalanche/exception.h"
#include "avalanche/context.h"
#include "avalanche/list.h"
#include "avalanche/list-proj.h"
#include "avalanche/map.h"
#include "avalanche/pointer.h"
#include "avalanche/function.h"
#include "avalanche/name-mangle.h"
#include "avalanche/parser.h"
#include "avalanche/symbol-table.h"
#include "avalanche/macsub.h"

/**
 * Initialises the avalanche runtime.
 *
 * This should be called exactly once at process startup.
 */
void ava_init(void);

AVA_END_DECLS

#undef AVA__INTERNAL_INCLUDE

#endif /* AVA_AVALANCHE_H_ */
