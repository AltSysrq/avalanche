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
#error "Don't include avalanche/exception.h directly; just include avalanche.h"
#endif

#ifndef AVA_RUNTIME_EXCEPTION_H_
#define AVA_RUNTIME_EXCEPTION_H_

#include "defs.h"
#include "value.h"

/**
 * @file
 *
 * Avalanche's exception system is built on top of the host's C++ exception
 * handling mechanism.
 *
 * All Avalanche exceptions are instances of ava_exception. This structure
 * should not be initialised by clients, but rather by the ava_throw_*()
 * functions or ava_rethrow(). C++ clients may meaningfully catch
 * ava_exception, and may rethrow it with `throw;`. C clients can use
 * ava_catch() to intercept exceptions, albeit somewhat awkwardly and less
 * efficiently.
 *
 * An exception is identified by two fields: type and value. The type is a
 * pointer to a C structure describing the type of the exception, and indicates
 * the low-level category of the exception, such as a normal user exception, a
 * programming error, etc. The exact meaning of the value varies based on the
 * type, but it is generally an exception message, or a structure identifying
 * higher-level information about the exception.
 */

/**
 * Used by identity to describe an exception type. These usually are statically
 * allocated, but may be allocated otherwise for, eg, lexical exception
 * handling.
 *
 * Note that this is distinct from user exception types, which are all covered
 * by the ava_user_exception tpe.
 */
typedef struct {
  /**
   * Human readable of this type, for use if the exception propagates out of
   * Avalanche-aware code.
   *
   * Eg, "user exception", "runtime error".
   */
  const char* uncaught_description;
} ava_exception_type;

/**
 * Opaque type storing extra information about an exception at the point it was
 * thrown.
 */
typedef struct ava_exception_throw_info_s ava_exception_throw_info;

/**
 * An exception.
 *
 * This struct is not normally initialised by clients; use ava_throw() and
 * friends to do that.
 */
typedef struct {
  /**
   * The low-level type of this exception, as far as native libraries usually
   * care about it.
   */
  const ava_exception_type* type;
  /**
   * The Avalanche-visibile high-level exception value.
   *
   * This is not an actual ava_value because the C++ exception ABI might not
   * produce a suitably-aligned allocation, which would lead to a bus error
   * when attempting to write into it.
   */
  void* value[sizeof(ava_value) / sizeof(void*)];

  /**
   * Information about the exception at the point where it was thrown.
   */
  const ava_exception_throw_info* throw_info;
} ava_exception;

/**
 * Throws an exception of the given type and with the given value up the stack,
 * to the first available handler.
 *
 * @param type The exception type being thrown.
 * @param value The value being thrown.
 */
void ava_throw(const ava_exception_type* type, ava_value value) AVA_NORETURN;
/**
 * Convenience for ava_throw(type, ava_value_of_string(str), NULL).
 */
void ava_throw_str(const ava_exception_type* type, ava_string str) AVA_NORETURN;
/**
 * Throws an exception in ava_user_exception format with the given type and
 * message.
 */
void ava_throw_uex(const ava_exception_type* type, ava_string user_type,
                   ava_string message) AVA_NORETURN;
/**
 * Rethrows the given exception, without regenerating any debug information,
 * etc.
 */
void ava_rethrow(ava_exception handler) AVA_NORETURN;

/**
 * Wrapper function permitting C code to catch Avalanche exceptions.
 *
 * Evaluates (*f)(ud). If it throws, the exception is written into *ex and true
 * is returned. Otherwise, *ex is unchanged and false is returned.
 *
 * This does not catch foreign exceptions. There is currently no way for C code
 * to achieve the equivalent of a finally block in the presence of foreign
 * exceptions. The Avalanche runtime never throws foreign exceptions, nor is it
 * possible for Avalanche code to do so unassisted, so in many contexts this
 * construct should be sufficient anyway.
 */
ava_bool ava_catch(ava_exception* ex, void (*f)(void*), void* ud);

/**
 * Returns the value embedded in the given exception.
 */
ava_value ava_exception_get_value(const ava_exception* ex);

/**
 * Initialises global state needed by the exception system.
 *
 * This must be called exactly once at process startup. Most programs will want
 * to use ava_init() instead of calling this directly.
 */
void ava_exception_init(void);

/**
 * Standard exception type for user exceptions.
 *
 * User exceptions are the normal exception type that user code throws and
 * catches. User exceptions are contractural, in that they are normally
 * described in a function's interface, and relying on catching them makes
 * sense.
 *
 * The value of a user exception is a 2-tuple of the user exception type name,
 * and a dict of arbitrary properties.
 *
 * Eg,
 *   example-exception \{message "something broke" cause \{...\} \}
 */
extern const ava_exception_type ava_user_exception;
/**
 * Standard exception type for programming errors.
 *
 * Errors are the result of errors on the part of the programmer, and are not
 * normally caught, except for debugging or as last-resort workarounds.
 *
 * They follow the same format as user exceptions.
 */
extern const ava_exception_type ava_error_exception;
/**
 * Standard exception type for string format errors.
 *
 * String format errors are the result of attempting to assign an
 * interpretation to a string which cannot be interpreted that way. (Eg,
 * performing integer arithmetic on the string "foo".)
 *
 * Standard practise is to either immediately convert them to user exceptions
 * or to let them bubble the whole way up the stack.
 *
 * The format is simply an explanatory message.
 */
extern const ava_exception_type ava_format_exception;
/**
 * Exception type for errors internal to the runtime that are not fatal to the
 * process.
 *
 * These are not normally caught. They do not have any particular value format.
 */
extern const ava_exception_type ava_internal_exception;
/**
 * Exception type for interrupts.
 *
 * These are thrown if a strand is being forcibly interrupted from a blocking
 * call; the format of the value is up to the thrower, as usually it and the
 * catcher are in direct cooperation.
 */
extern const ava_exception_type ava_interrupt_exception;
/**
 * Exception type for undefined behaviour which was caught by a runtime check.
 *
 * This exception type has no defined semantics at all, since it is only thrown
 * when undefined behaviour is invoked.
 */
extern const ava_exception_type ava_undefined_behaviour_exception;

#endif /* AVA_RUNTIME_EXCEPTION_H_ */
