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

#include "defs.h"
#include "abi.h"

/**
 * @file
 *
 * The top-level API for working with subprocesses. Subprocesses are discussed
 * as a concept in more detail in THREADS.md.
 *
 * Unless otherwise noted, all functions are thread-safe and may be called from
 * within the current subprocess, from another subprocess, or from a thread not
 * within any subprocess.
 */

/**
 * Opaque structure representing an Avalanche subprocess.
 */
typedef struct ava_subprocess_s ava_subprocess;
/**
 * General identifier for things within a subprocess.
 *
 * @see ava_subprocess_genid()
 */
typedef uint64_t ava_spid;

/**
 * Function type for the "main" function of a subprocess.
 *
 * @param userdata The userdata passed into ava_subprocess_run().
 * @return An exit status, as with main().
 */
typedef int (*ava_subprocess_main_f)(void* userdata);

/**
 * Creates and executes a subprocess.
 *
 * A new subprocess is created, containing a single thread pool with the
 * current thread as its only thread. The current thread is marked
 * non-terminatable. The thread pool starts with a single fibre using the
 * current stack of the current thread, and is dedicated to a single strand.
 *
 * The current thread must not already be part of a subprocess.
 *
 * After this call returns, the subprocess is destroyed.
 *
 * The caller is expected to pass in the original arguments to main(). These
 * are scanned for initial arguments begining with "--ava-", which permit
 * configuring the subprocess from the commandline. If an unknown
 * "--ava-"-prefixed argument is encountered, the subprocess does not run and
 * EX_USAGE is returned.
 *
 * Throwing an exception across the subprocess boundary is not meaningful.
 * Catching an Avalanche exception outside of this call has undefined
 * behaviour.
 *
 * @param main The function to execute.
 * @param userdata Argument to pass to main.
 * @param argv The original argv passed into main, or similar. This can later
 * be retrieved via ava_subprocess_argv().
 * @param argc The original argc passed into main, or similar. This can later
 * be retrieved via ava_subprocess_argc().
 * @return An exit status. Usually this is whatever main returns, but may be
 * other errors if the subprocess could not be created.
 */
int ava_subprocess_run(
  ava_subprocess_main_f main, void* userdata,
  const char*const* argv, unsigned argc);

/**
 * Returns the subprocess in which the current thread is executing, or NULL if
 * there is none.
 */
ava_subprocess* ava_subprocess_current(void);

/**
 * Returns a pointer to the first element of argv which was not consumed by
 * "--ava-"-prefixed argument processing. Note that this always excludes the
 * program name (argv[0] in normal C main()); for that, use
 * ava_subprocess_argv0().
 */
const char*const* ava_subprocess_argv(const ava_subprocess* sp);
/**
 * Returns the number of arguments in ava_subprocess_argv().
 *
 * As with ava_subprocess_argv(), this always excludes the program name and any
 * arguments consumed by "--ava-"-prefixed argument parsing.
 */
unsigned ava_subprocess_argc(const ava_subprocess* sp);
/**
 * Returns the original zeroth element of the argv array passed into
 * ava_subprocess_run().
 */
const char* ava_subprocess_argv0(const ava_subprocess* sp);

/**
 * Generates an integer identifier which is unique within this subprocess.
 * Between any two calls on the same process which have a happens-before
 * relationship, the later call always results in a numerically greater id than
 * the prior. There are no other guarantees about the nature of the generated
 * ids.
 */
ava_spid ava_subprocess_genid(ava_subprocess* sp);

#endif /* AVA_PLATFORM_NATIVE_SUBPROCESS_H_ */
