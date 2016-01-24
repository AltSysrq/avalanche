/*-
 * Copyright (c) 2016, Jason Lingle
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
#ifndef AVA__INTRINSICS_KEYSYM_H_
#define AVA__INTRINSICS_KEYSYM_H_

#include "../avalanche/parser.h"
#include "../avalanche/macsub.h"

/**
 * The #keysym# intrinsic control macro.
 *
 * Syntax: #keysym# identifier
 *
 * identifier is a bareword.
 *
 * Semantics: identifier must resolve to a unique keysym symbol. The expands to
 * a bareword whose text is the full name of the symbol referenced.
 */
AVA_DEF_MACRO_SUBST(ava_intr_keysym_subst);

/**
 * The keysym, Keysym, and KEYSYM control macros.
 *
 * Syntax: keysym (#keysym# identifier)
 * (Desugared form of `keysym \identifier`)
 *
 * Semantics: Creates a keysym in the current scope with the given simple name.
 *
 * The macro's userdata is a pointer to the ava_visibility of the symbol it
 * defines.
 */
AVA_DEF_MACRO_SUBST(ava_intr_defkeysym_subst);

#endif /* AVA__INTRINSICS_KEYSYM_H_ */
