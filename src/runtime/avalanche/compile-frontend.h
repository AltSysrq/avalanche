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
#error "Don't include avalanche/compile-frontend.h directly; just include avalanche.h"
#endif

#ifndef AVA_RUNTIME_COMPILE_FRONTEND_H_
#define AVA_RUNTIME_COMPILE_FRONTEND_H_

#include "string.h"
#include "pcode.h"
#include "pcode-validation.h"
#include "errors.h"

/**
 * Performs all steps necessary to compile a given source file (as a monolithic
 * string) to validated P-Code.
 *
 * @param pcode If non-NULL, a pointer in which to store the generated P-Code,
 * if any. This is always set to NULL or a structurally valid P-Code object.
 * @param xcode If non-NULL, a pointer in which to store the generated X-Code,
 * if any. This is always set to NULL or a structurally valid X-Code object.
 * @param errors Initialised list to which any errors encountered are appended.
 * @param package The package prefix to apply to symbols defined by the file.
 * @param filename The name of the source file.
 * @param source The full source of the given file.
 * @return Whether the errors list is empty. If this is false, the pcode and
 * xcode values cannot be assumed to be completely logically consistent.
 */
ava_bool ava_compile_file(
  ava_pcode_global_list** pcode,
  ava_xcode_global_list** xcode,
  ava_compile_error_list* errors,
  ava_string package,
  ava_string filename, ava_string source);

#endif /* AVA_RUNTIME_COMPILE_FRONTEND_H_ */
