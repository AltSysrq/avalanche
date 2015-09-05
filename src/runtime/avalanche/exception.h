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
 * A list of stack frames at which an exception occurred, for debugging
 * purposes.
 */
typedef struct ava_stack_trace_s ava_stack_trace;
struct ava_stack_trace_s {
  /**
   * The "type" of function of this frame, based on the name mangling (or lack
   * thereof) in the function name.
   */
  const char* function_type;
  /**
   * The demangled function name of this frame.
   */
  char in_function[256];

  /**
   * The position of the instruction pointer in this frame.
   */
  const void* ip;
  /**
   * The offset of ip from the start of the function.
   */
  size_t ip_offset;

  /**
   * The next frame *down* the stack, or NULL if this is the final callee.
   */
  ava_stack_trace* next;
};

/**
 * Stores information on a level of exception handling.
 *
 * The mechanism by which Avalanche handles exceptions is not defined.
 *
 * When a handler catches an exception, the public fields of this struct are
 * populated with the exception information.
 */
typedef struct ava_exception_handler_s ava_exception_handler;
struct ava_exception_handler_s {
  /**
   * After an exception has been caught, the type of exception that was caught.
   */
  const ava_exception_type* exception_type;
  /**
   * After an exception has been caught, the value that was thrown.
   */
  ava_value value;
  /**
   * After an exception has been caught, the (heap-allocated) stack trace
   * between the throw point and the catch point. *stack_trace indicates the
   * direct callee of the catcher; ava_next_frame(stack_trace->next) is
   * the callee's callee, and so on.
   */
  ava_stack_trace* stack_trace;

  /**
   * Implementation detail: Executions are implemented by setjmp()ing into this
   * buffer, then longjmp()ing back up the stack on a throw.
   */
  jmp_buf resume_point;

  /**
   * The next handler in the context's exception handler context.
   */
  ava_exception_handler* next;
};

/**
 * This is an internal function.
 *
 * *dst is initialised to be a valid exception handler element, except that the
 * jump buffer is not populated, and the handler is pushed to the top of the
 * stack for the current context. Returns a pointer to the jump buffer, so that
 * ava_try() can setjmp() it in the same expression.
 *
 * @param dst The handler to initialise.
 */
jmp_buf* ava_push_handler(ava_exception_handler*restrict dst);
/**
 * This is an internal function.
 *
 * If do_pop is true, pops a handler from the top of the exception handler
 * stack for the current context.
 *
 * @return do_pop
 */
ava_bool ava_pop_handler(ava_bool do_pop);
/**
 * Throws an exception of the given type and with the given value up the stack,
 * to the first available handler.
 *
 * @param type The exception type being thrown.
 * @param value The value being thrown.
 * @param trace A heap-allocated series of stack frames as in
 * ava_exception_handler.stack_trace. Usually NULL, indicating that this
 * is the original throw point. If non-NULL, this value will be used as the
 * stack trace instead of the current trace.
 */
void ava_throw(const ava_exception_type* type, ava_value value,
               ava_stack_trace* trace) AVA_NORETURN;
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
 * Convenience for ava_throw(handler->exception_type, handler->value,
 *                           handler->next, handler->stack_trace);
 */
void ava_rethrow(ava_exception_handler*restrict handler)
AVA_NORETURN;

/**
 * Executes its body; if the body throws an exception, the handler will be
 * populated and control transfers into catch blocks.
 *
 * Typical usage:
 *
 *   ava_exception_handler handler
 *   ava_try (handler) {
 *     call_some_functions();
 *   } ava_catch (handler, ava_user_exception) {
 *     handle_exception();
 *   } ava_catch_all {
 *     ava_rethrow(&handler);
 *   }
 *
 * Note that failing to have an ava_catch_all block that rethrows any handled
 * exception will *cause exceptions to be silently swallowed*.
 *
 * Semantics: ava_try initialises the given handler and pushes it onto the
 * exception handler stack of the current context. When the try body completes
 * or an exception is thrown within it, the handler is popped off the exception
 * handler stack. In the case of an exception, control passes to the catch
 * blocks in sequence, and control falls into the first whose type matches the
 * exception type that was caught.
 *
 * @param handler The exception handler struct to initialise and which will
 * receive any thrown data. Within the try block, this should be used as the
 * top-of-stack instead of stack.
 * @param stack The top-of-stack outside the try block.
 */
#define ava_try(handler)                                        \
  if (!ava_pop_handler(setjmp(*ava_push_handler(&(handler)))))  \
    for (unsigned AVA_GLUE(i,__LINE__) = 0;                     \
         AVA_GLUE(i,__LINE__) < 1;                              \
         ava_pop_handler(++AVA_GLUE(i,__LINE__)))
/**
 * Handles a single exception type.
 *
 * @see ava_try
 * @param handler The handler that caught an exception
 * @param expected_type If the handler caught an exception of this type, the
 * body of this block is executed. Otherwise, control transfers to the next
 * block.
 */
#define ava_catch(handler, expected_type)                       \
  else if ((handler).exception_type == &(expected_type))
/**
 * Handles all exception types.
 *
 * This is always used as the last clause in an ava_try, usually to rethrow the
 * exception.
 *
 * @see ava_try
 */
#define ava_catch_all else

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

#endif /* AVA_RUNTIME_EXCEPTION_H_ */
