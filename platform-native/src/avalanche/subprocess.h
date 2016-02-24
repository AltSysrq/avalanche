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
#ifndef AVA_PLATFORM_NATIVE_SUBPROCESS_H_
#define AVA_PLATFORM_NATIVE_SUBPROCESS_H_

#include <stdarg.h>

#include "abi.h"

AVA_BEGIN_DECLS

/**
 * Encapsulates a single "subprocess" within a platform process.
 *
 * Subprocesses primarily arise as an artefact of the way the memory manager
 * functions. They are exposed as a first-class concept since they may be
 * useful to certain classes of applications, such as monitoring sidecars.
 *
 * Suprocesses are completely shared-nothing environments as far as managed
 * resources go. For example, there is no safe way to read memory from one
 * subprocess's heap from outside that subprocess. The benefit is that
 * stop-the-world events only affect one subprocess; this is where a separate
 * subprocess could be useful for a monitoring sidecar, as even a pathological
 * garbage collection cycle won't interrupt its ability to record information.
 *
 * The ava_subprocess object itself is not considered to exist within the
 * subprocess, and can safely be manipulated externally. Since it exists
 * outside of a managed heap, it is instead reference-counted. Any code
 * executing within the subprocess need not worry about this, since the
 * subprocess object will necessarily continue to exist in that case. However,
 * external uses must maintain the reference count with ava_subprocess_incref()
 * and ava_subprocess_decref().
 *
 * All functions in this header are fully thread-safe.
 */
typedef struct ava_subprocess_s ava_subprocess;

/**
 * Identifies a type of subprocess event.
 *
 * Event types are identified strictly by pointer identity; the contents of
 * this struct are used only for diagnostic purposes.
 */
typedef struct ava_sp_event_s {
  /**
   * A printf-style format-string which can be used (together with the varargs
   * passed into ava_sp_event_callback_f) to display this event in a
   * human-readable way.
   */
  const char* display;
} ava_sp_event;

/**
 * Callback for low-level subprocess events.
 *
 * This is invoked directly from the strand performing whatever action the
 * event is notifying about. It therefore should execute quickly (at least in
 * normal circumstances). It is not permitted to throw exceptions or take any
 * action that may call back into the threading system or the memory manager.
 *
 * The lack of a context variable is by design, to minimise the overhead in the
 * common case of having no event callback. Callbacks can use the current
 * subprocess as context when necessary.
 *
 * Callback functions return a pointer to the _next_ callback function, or NULL
 * if that is the end of the chain. This is admittedly a somewhat bizarre
 * construction, but the alternatives all have issues. (Keep in mind this is
 * called from some relatively hot paths.)
 *
 * - Each event could have an associated struct type, and a pointer to that
 *   passed in. Supporting event stringification would require extra code per
 *   event type, and even then results in an awkward system. (E.g., the
 *   stringifier would either need to print to a fixed buffer, allocate a new
 *   string with malloc(), or call into another callback with varargs so it
 *   could call vprintf()/vsprintf()/etc.) This system also requires spilling
 *   everything onto the stack.
 *
 * - The argument list could be fixed. The issue here is with argument types;
 *   using qwords is easy, but they can't be passed to %s in the format string,
 *   and then we have the struct case above. uintptr_ts don't have that issue,
 *   but 64-bit values would need to be awkwardly split.
 *
 * - The argument list is passed in as a va_list. This solves most of the
 *   issues, but requires an extra layer of function call to move the varargs
 *   into a va_list, and a va_list writes half the register file to the stack
 *   on the System V AMD64 ABI.
 *
 * It is in fact possible to selectively handle events without needing to use
 * va_list in this scheme. The entry function can inspect type. If it does not
 * match anything the client code wishes to handle, it simply returns the next
 * function in the chain. If it does want to handle it, it returns a function
 * with the correct prototype, and that function then returns the next one in
 * the chain.
 */
typedef union ava_sp_event_callback_u {
  union ava_sp_event_callback_u (*f)(const ava_sp_event* type, ...);
} ava_sp_event_callback;
/**
 * Convenience for the type of ava_sp_event_callback.f
 */
typedef union ava_sp_event_callback_u (*ava_sp_event_callback_f)(
  const ava_sp_event* type, ...);

/**
 * "Main" function for a subprocess.
 *
 * @param userdata The userdata passed to ava_subprocess_run().
 * @return The value to return from ava_subprocess_run().
 */
typedef int (*ava_sp_main_f)(AVA_DEF_ARGS1(void* userdata));

/**
 * Non-interactive error types produced by ava_subprocess_run().
 */
typedef enum {
  /**
   * Indicates that main() was executed and returned normally.
   */
  ava_se_no_error = 0,
  /**
   * Indicates that an invalid "--ava-" argument was encountered and main() was
   * not executed.
   */
  ava_se_usage,
  /**
   * Indicates that an "--ava-help" argument was encountered and main() was not
   * executed.
   */
  ava_se_help,
  /**
   * Indicates that insufficient memory was available to set the subprocess up.
   * main() was not executed.
   */
  ava_se_out_of_memory
} ava_sp_error;

/**
 * Creates and executes a subprocess.
 *
 * This call does not return until either an error with setup occurs or the
 * main function terminates. The host thread and stack are used to initialise a
 * single threadpool with a single thread and a dedicated fibre.
 *
 * If an exception escapes main, it is allowed to propagate out of this call.
 * Note that if an Avalanche exception is actually *caught*, there is
 * relatively little that can be done with it, since its references into the
 * heap will no longer be valid.
 *
 * It is possible to call this from a strand within another subprocess, though
 * in this regard it must be treated as a foreign function call which blocks
 * the strand's ability to use the heaps of the subprocess that actually owns
 * it, since the owning subprocess will be unable to suspend the strand for
 * stop-the-world events, etc. If this is done, it is advisable that the strand
 * be the only one in its fibre.
 *
 * The argument vector is parsed for arguments begining with "--ava-". Such
 * arguments (which must be contiguously at the beginning of the argument list
 * after index 0) are used to configure parameters of the subprocess, and are
 * not passed on to the user code itself. If an argument beginning with
 * "--ava-" is not recognised, this call prints a diagnostic to stderr and
 * returns EX_USAGE without executing main. "--ava-help" and "--ava-version"
 * cause information to be written to stdout before returning 0 without
 * executing main.
 *
 * @param noninteract If non-NULL, suppresses interactive behaviour. No
 * messages will be written to stdout or stderr. The pointee will be set to a
 * value indicating why ava_subprocess_run().
 * @param argv Array of arguments to the sub-process, as with C main().
 * @param argc Length of the argv array.
 * @param main The "main" function for the subprocess.
 * @param userdata Argument to pass to main. Note that this cannot point into
 * any managed heap, since main will execute in a different subprocess than the
 * caller.
 * @return The return value from main, as with C main().
 */
int ava_subprocess_run(ava_sp_error* noninteractive,
                       const char*const* argv, unsigned argc,
                       ava_sp_main_f main, void* userdata) AVA_RTAPI;
/**
 * Returns the subprocess the current thread is running within, or NULL if not
 * in a subprocess.
 *
 * The reference count on the returned subprocess is *not* incremented, since
 * the caller can expect the object to continue existing as long as it is still
 * running. If the pointer is to be sent to something outside the subprocess,
 * ava_subprocess_incref() must be called.
 */
ava_subprocess* ava_subprocess_current(void) AVA_RTAPI AVA_PURE;

/**
 * Increments the reference count on the given subprocess by one.
 *
 * @return sp
 */
ava_subprocess* ava_subprocess_incref(ava_subprocess* sp) AVA_RTAPI;
/**
 * Decrements the reference count on the given subprocess by one, destroying it
 * if that was the last reference.
 */
void ava_subprocess_decref(ava_subprocess* sp) AVA_RTAPI;

/**
 * Generates a 63-bit integer identifier unique to the given subprocess.
 *
 * Ids are in general predictable, but no algorithm is guaranteed to be used.
 * In the absence of concurrent access, generated id sequences are the same for
 * different subprocesses with the same library version.
 *
 * Provided there exists a happens-before relationship between two calls to
 * this function, the later call will generate a qword which compares greater
 * than the prior call.
 *
 * In the incredibly unlikely event that the id space is exhausted, the process
 * aborts. Note that this will most likely never happen; exhausting the id
 * space even on a 32-bit system (which only guarantees 63 useful bits of
 * uniqueness) generating 1 billion ids per second, overflow would not happen
 * for over 250 years.
 *
 * Realistically, even generating 1 million ids per second would be considered
 * extremely unusual.
 */
ava_qword ava_subprocess_genid(ava_subprocess* sp) AVA_RTAPI;

/**
 * Returns the current event callback for the given subprocess.
 *
 * Obtaining the callback provides a load-acquire barrier.
 *
 * @see AVA_SP_EVENT()
 */
ava_sp_event_callback ava_subprocess_get_event_callback(
  const ava_subprocess* sp) AVA_RTAPI AVA_PURE;
/**
 * Changes the event callback for the given subprocess.
 *
 * The new callback may begin receiving events before this call returns.
 *
 * When installing a callback, one should generally arrange to forward events
 * to whatever was there previously. Note that there is no way to uninstall a
 * callback if something else has been layered on top since installation.
 *
 * This call provides a full memory barrier.
 *
 * @param sp The subprocess to mutate.
 * @param old The expected prior callback. This call will fail if this is not
 * the actual current callback.
 * @param neu The new callback.
 * @return True if the expected old value matched what it actually was and the
 * callback is now neu. False if the expected value did not match.
 */
ava_bool ava_subprocess_cas_event_callback(
  ava_subprocess* sp, ava_sp_event_callback old,
  ava_sp_event_callback neu) AVA_RTAPI;

/**
 * Convenience for sending a subprocess event through the callback of the
 * current subprocess.
 *
 * @param type The type of event. The leading & is added implicitly.
 */
#define AVA_SP_EVENT(type, ...)                                         \
  do {                                                                  \
    for (ava_sp_event_callback _ava_spec_it =                           \
           ava_subprocess_get_event_callback(ava_subprocess_current()); \
         AVA_UNLIKELY(_ava_spec_it.f); )                                \
      _ava_spec_it = (*_ava_spec_it.f)(&type, __VA_ARGS__);             \
  } while (0)

AVA_END_DECLS

#endif /* AVA_PLATFORM_NATIVE_SUBPROCESS_H_ */
