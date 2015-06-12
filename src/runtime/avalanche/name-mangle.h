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
#error "Don't include avalanche/name-mangle.h directly; just include avalanche.h"
#endif

#ifndef AVA_RUNTIME_NAME_MANGLE_H_
#define AVA_RUNTIME_NAME_MANGLE_H_

#include "string.h"

/**
 * @file
 *
 * Facilities for mangling and demangling names.
 *
 * Since Avalanche permits a much wider range of characters in identifiers than
 * most underlying systems (ie, any arbitrary byte-string), avalanche
 * identifiers must be mangled before being exposed to the underlying system.
 *
 * Name mangling is performed as follows:
 *
 * - Any hyphen not preceded by a hyphen, period, or colon is transliterated to
 *   a single underscore.
 *
 * - Any period not preceded by a hyphen, period, or colon is replaced with two
 *   underscores.
 *
 * - Any colon not pecered by a hyphen, period, or colon is replaced with three
 *   underscores.
 *
 * - The characters in the set [a-zA-Z0-9] are left untouched.
 *
 * - Any byte not handled specially above is replaced with a dollar sign
 *   followed by two upper-case hexadecimal characters encoding the integer
 *   value of that byte.
 *
 * - The resulting mangled name as a whole is prefixed by the string "a$".
 *
 * For example, the identifier "avast.ava-lang.org:prelude.+" is mangled to
 * "a$avast__ava_lang__org___prelude__$2B".
 */

/**
 * Indicates the manner in which a name has been mangled.
 */
typedef enum {
  /**
   * Indicates an unmangled name.
   */
  ava_nms_none = 0,
  /**
   * Indicates a name subject to mangling in the manner described in the
   * documentation for name-mangle.h.
   */
  ava_nms_ava
} ava_name_mangling_scheme;

/**
 * A name split into its mangling scheme and its unmangled form.
 */
typedef struct {
  /**
   * The way in which the name is to be / was mangled.
   */
  ava_name_mangling_scheme scheme;
  /**
   * The unmangled name.
   */
  ava_string name;
} ava_demangled_name;

/**
 * Identifies the mangling scheme used on the given mangled name, returning the
 * scheme itself and the unmangled form of the given name.
 *
 * This always succeeds; if the function cannot interpret the string, it
 * assumes no mangling was enacted.
 */
ava_demangled_name ava_name_demangle(ava_string mangled);

/**
 * Mangles a name using the given scheme and raw form.
 */
ava_string ava_name_mangle(ava_demangled_name name);

#endif /* AVA_RUNTIME_NAME_MANGLE_H_ */
