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
#ifndef AVA_PLATFORM_NATIVE_THREADS_H_
#define AVA_PLATFORM_NATIVE_THREADS_H_

#include "defs.h"
#include "abi.h"
#include "subprocess.h"

/**
 * @file
 *
 * Exposes an API for manipulating Avalanche's thread constructs. See
 * THREADS.md for more information.
 *
 * Note that most objects are reference-counted so that client code need not
 * worry about them disappearing out from under it. (Garbage collection is not
 * an option since this is lower-level than the GC.)
 */

/**
 * Opaque type representing a thread-pool.
 *
 * References to a thread-pool are reference-counted so that client code need
 * not worry about it getting deleted out from under it.
 *
 * A thread-pool ceases to exist when it has no remaining fibres and no
 * external references. At this point all threads within terminate on their
 * own.
 *
 * @see ava_threadpool_incref()
 * @see ava_threadpool_decref()
 */
typedef struct ava_threadpool_s ava_threadpool;
/**
 * Opaque type representing a thread participating in a thread-pool.
 *
 * References to a thread are reference-counted so that client code need not
 * worry about it getting deleted out from under it.
 *
 * A reference to a thread does not keep the thread alive; it simply keeps
 * around the bookkeeping memory that can be used to inspect the status of the
 * thread.
 *
 * @see ava_thread_incref()
 * @see ava_thread_decref()
 */
typedef struct ava_thread_s ava_thread;
/**
 * Opaque type representing a fibre in a thread-pool.
 *
 * References to a fibre are reference-counted so that client code need not
 * worry about it getting deleted out from under it.
 *
 * A fibre ceases to exist when it no longer contains any strands and has no
 * external references.
 *
 * @see ava_fibre_incref()
 * @see ava_fibre_decref()
 */
typedef struct ava_fibre_s ava_fibre;
/**
 * Opaque type representing a strand in a fibre.
 *
 * References to a strand are reference-counted so that client code need not
 * worry about it getting deleted out from under it.
 *
 * A reference to a strand only keeps the bookkeeping memory used to inspect
 * the strand's state around. A strand may be locked to temporarily prevent
 * relocation or deletion of the strand's execution stack.
 *
 * @see ava_strand_incref()
 * @see ava_strand_decref()
 */
typedef struct ava_strand_s ava_strand;

/**
 * Function type used as the top of the call stack for strands.
 *
 * @param arg The argument passed to ava_strand_spawn()
 * @param caller_map The stack map / heap handle from the caller.
 * @return The return value of the fibre.
 */
typedef ava_stdval (*ava_strand_main_f)(
  ava_stdval arg, uintptr_t _padding, const ava_stack_map* caller_map);

/**
 * Allocates a new, empty thread-pool.
 *
 * The returned thread-pool includes a reference belonging to the caller.
 *
 * @param sp The subprocess to which the new thread-pool belongs.
 * @param name The name of the thread-pool, used for diagnostics. The name need
 * not be unique, and is never used to identify the pool programatically.
 * @return The new thread-pool, with a reference owned by the caller.
 */
ava_threadpool* ava_threadpool_new(ava_subprocess* sp, const char* name);
/**
 * Returns the numeric id of the given thread-pool.
 */
ava_spid ava_threadpool_get_id(const ava_threadpool* tp);
/**
 * Increases the reference count of the given thread-pool by 1.
 *
 * @param tp The thread-pool. May be NULL.
 * @return tp. This is for "fluent" duplication of a thread-pool reference, eg,
 * `copy = ava_threadpool_incref(orig);`
 */
ava_threadpool* ava_threadpool_incref(ava_threadpool* tp);
/**
 * Decrements the reference count of the given thread-pool by 1, destroying it
 * if it reaches 0.
 *
 * @param tp The thread-pool. May be NULL.
 */
void ava_threadpool_decref(ava_threadpool* tp);
/**
 * Retrieves a thread-pool in a subprocess by numeric id.
 */
ava_threadpool* ava_subprocess_get_threadpool(
  const ava_subprocess* sp, ava_spid id);
/**
 * Used to iterate over the thread-pools in a subprocess.
 *
 * @param tpp Pointer to adjust. If initially NULL, it is set to the first
 * thread-pool in the subprocess; otherwise, it is set to the thread-pool in
 * the subprocess following the old value. In both cases, it is set to NULL if
 * there are no more thread-pools in the subprocess. ava_threadpool_decref() is
 * implicitly called on the old value, and ava_threadpool_incref() on the new
 * value.
 */
void ava_subprocess_next_threadpool(
  ava_threadpool** tpp, const ava_subprocess* sp);
/**
 * Changes the number of threads the given thread-pool should have.
 *
 * This immediately spawns new threads or schedules existing threads for
 * termination as necessary.
 */
void ava_threadpool_resize(ava_threadpool* tp, unsigned count);
/**
 * Returns the current desired size of the given thread-pool.
 *
 * This may not reflect the actual thread count, as threads must go into an
 * idle state before they can terminate.
 */
unsigned ava_threadpool_get_size(const ava_threadpool* tp);
/**
 * Returns whether the given thread-pool is active, ie, whether it contains any
 * fibres.
 */
ava_bool ava_threadpool_is_active(const ava_threadpool* tp);
/**
 * Returns the subprocess to which the given thread-pool belongs.
 */
ava_subprocess* ava_threadpool_get_subprocess(const ava_threadpool* tp);

/**
 * Increments the reference count on the given thread by 1.
 *
 * @param th The thread; may be NULL.
 * @return th
 */
ava_thread* ava_thread_incref(ava_thread* th);
/**
 * Decrements the reference count on the given thread by 1, freeing remaining
 * resources if the count becomes 0.
 *
 * @param th The thread; may be NULL.
 */
void ava_thread_decref(ava_thread* th);
/**
 * Returns the unique integer id of the given thread.
 */
ava_spid ava_thread_get_id(const ava_thread* th);
/**
 * Returns the thread-pool which contains the given thread.
 */
ava_threadpool* ava_thread_get_threadpool(const ava_thread* th);
/**
 * Returns the currently-executing fibre of the given thread, or NULL if the
 * thread is not in the Busy state.
 *
 * @return The current fibre of the given thread, with a reference belonging to
 * the caller, or NULL if no currently-executing fibre.
 */
ava_fibre* ava_thread_current_fibre(const ava_thread* th);
/**
 * Returns true if the thread is in the Polling state.
 */
ava_bool ava_thread_is_polling(const ava_thread* th);
/**
 * Returns true if the thread is currently alive.
 */
ava_bool ava_thread_is_alive(const ava_thread* th);
/**
 * Returns the thread of the given id in the given thread-pool.
 *
 * @param tp The thread-pool to search.
 * @param id The id of the thread to find.
 * @return The thread, with a reference owned by the caller, or NULL if there
 * is no such thread.
 */
ava_thread* ava_threadpool_get_thread(const ava_threadpool* tp, ava_spid id);
/**
 * Used to iterate over the threads in a thread-pool.
 *
 * This behaves like ava_subprocess_next_threadpool(), except with different
 * types.
 */
void ava_threadpool_next_thread(ava_thread** thp, const ava_threadpool* tp);

/**
 * Increments the reference count on the given fibre by 1.
 *
 * @param fib The fibre; may be NULL.
 * @return fib
 */
ava_fibre* ava_fibre_incref(ava_fibre* fib);
/**
 * Decrements the reference count on the given fibre by 1, destroying it if it
 * becomes zero.
 *
 * @param fib The fibre; may be NULL.
 */
void ava_fibre_decref(ava_fibre* fib);
/**
 * Returns the integer id of this fibre.
 */
ava_spid ava_fibre_get_id(const ava_fibre* fib);
/**
 * Returns the currently-active strand for the given fibre, or NULL if there is
 * none.
 *
 * Note that this can return NULL even for non-empty fibres, if the most recent
 * strand has been destroyed and no other strand has been activated since.
 *
 * @return The last strand, with a reference belonging to the caller, or NULL
 * if there is none.
 */
ava_strand* ava_fibre_get_active_strand(const ava_fibre* fib);
/**
 * Returns true iff the given fibre has no strands.
 */
ava_bool ava_fibre_is_empty(const ava_fibre* fib);
/**
 * Returns true iff the given fibre has at least one runnable strand.
 */
ava_bool ava_fibre_is_runnable(const ava_fibre* fib);
/**
 * Returns whether this fibre is "dedicated".
 *
 * A dedicated fibre hosts exactly one strand, which is never suspended. This
 * is primarily used to represent the execution stack of threads from outside
 * the managed environment.
 */
ava_bool ava_fibre_is_dedicated(const ava_fibre* fib);
/**
 * Returns the id of the thread that most recently executed the given fibre.
 *
 * There is no guarantee that this thread still exists. If it does, it is found
 * within the same threadpool that hosts the fibre.
 */
ava_spid ava_fibre_get_last_executor(const ava_fibre* fib);
/**
 * Return the threadpool that owns the given fibre.
 */
ava_threadpool* ava_fibre_get_threadpool(const ava_fibre* fib);
/**
 * Finds a fibre by numeric id within a thread-pool.
 *
 * @return A fibre in thread-pool tp with id id with a reference owned by the
 * caller, or NULL if there is none.
 */
ava_fibre* ava_threadpool_get_fibre(const ava_threadpool* tp, ava_spid id);
/**
 * Used to iterate over the fibres in a thread-pool.
 *
 * Behaves like ava_subprocess_next_threadpool(), but with different types.
 */
void ava_threadpool_next_fibre(ava_fibre** fibp, const ava_threadpool* tp);
/**
 * Creates a new, empty fibre within a thread-pool.
 *
 * @param tp The thread-pool which owns the new fibre.
 * @param stack_size Minimum amount of virtual memory to reserve for the
 * fibre's execution stack.
 * @return The empty fibre, with a reference owned by the caller.
 */
ava_fibre* ava_threadpool_create_fibre(
  ava_threadpool* tp, uintptr_t stack_size);

/**
 * Increments the reference count on the given strand.
 *
 * @param str The strand; may be NULL.
 * @return str
 */
ava_strand* ava_strand_incref(ava_strand* str);
/**
 * Decrements the reference count on the given strand, freeing any remaining
 * resources if it becomes zero.
 */
void ava_strand_decref(ava_strand* str);
/**
 * Locks the execution stack of the given strand so that it can be inspected
 * externally.
 *
 * This should be used with care, since while locked, any attempt to activate
 * or deactivate the fibre will block synchronously. It is mainly intended for
 * use by the garbage collector.
 *
 * The fibre stack lock is not reentrant.
 *
 * @see ava_strand_unlock_stack()
 */
void ava_strand_lock_stack(ava_strand* str);
/**
 * Unlocks the stack, removing the effect of ava_strand_lock_stack().
 */
void ava_strand_unlock_stack(ava_strand* str);
/**
 * Returns the numeric id of the given strand.
 */
ava_spid ava_strand_get_id(const ava_strand* str);
/**
 * Returns whether the given strand was the active strand in its fibre.
 *
 * This value is only stable while the strand's stack is locked.
 */
ava_bool ava_strand_is_active(const ava_strand* str);
/**
 * Returns whether the given strand is currently runnable.
 */
ava_bool ava_strand_is_runnable(const ava_strand* str);
/**
 * Returns whether the given strand is currently alive.
 */
ava_bool ava_strand_is_alive(const ava_strand* str);
/**
 * Returns the return value of the given strand.
 *
 * Behaviour is undefined if the strand is still alive.
 */
ava_stdval ava_strand_get_return_value(const ava_strand* str);
/**
 * Returns a human-readable description of why the strand is not runnable, for
 * use with diagnostics. This should always be a statically-allocated string
 * constant.
 */
const char* ava_strand_why_not_runnable(const ava_strand* str);
/**
 * Returns whether this strand has an allocated stack at all.
 *
 * This value is only stable while the strand's stack is locked.
 */
ava_bool ava_strand_has_stack(const ava_strand* str);
/**
 * Returns the current displacement of the strand's execution stack relative to
 * its natural location.
 *
 * This can be used to add to pointers into the strand's stack to derive the
 * actual location of what they point to.
 *
 * This value is only stable while the strand's stack is locked. It is not
 * meaningful if !ava_strand_has_stack().
 */
intptr_t ava_strand_get_stack_displacement(const ava_strand* str);
/**
 * Returns the size of the stack the last time the given strand became inactive
 * in its fibre.
 *
 * Returns 0 if the fibre has never been inactive.
 */
uintptr_t ava_strand_get_last_inactive_stack_size(const ava_strand* str);
/**
 * Returns the fibre that owns this strand.
 *
 * @return The containing fibre, with a reference belonging to the caller.
 */
ava_fibre* ava_strand_get_fibre(const ava_strand* str);
/**
 * Returns the parent strand of this strand.
 *
 * @return The parent strand with a reference owned by the caller, or NULL if
 * this is the root strand of the subprocess.
 */
ava_strand* ava_strand_get_parent(const ava_strand* str);
/**
 * Returns the stack map representing the stack frame in the parent strand
 * which owns the given strand, or NULL if there is no parent.
 *
 * @return A pointer into the natural location of the parent strand's stack
 * which corresponds to the call frame which owns this strand. Note that the
 * parent strand may be inactive, in which case the stack has been relocated
 * away from the natural location and this value must be adjusted by
 * ava_strand_get_stack_displacement().
 */
const ava_stack_map* ava_strand_get_parent_frame(const ava_strand* str);
/**
 * Finds the strand of the given id within the given fibre.
 *
 * @return A strand belonging to fib with id id with a reference owned by the
 * caller, or NULL if there is no such strand.
 */
ava_strand* ava_fibre_get_strand(const ava_fibre* fib, ava_spid id);
/**
 * Used to iterate over the strands in a fibre.
 *
 * Behaves like ava_subprocess_next_threadpool(), except the types are
 * different.
 */
void ava_fibre_next_strand(ava_strand** strp, const ava_fibre* fib);
/**
 * Creates a new strand within the given fibre.
 *
 * Behaviour is undefined if the fibre is dedicated.
 *
 * @param fib The fibre which hosts the strand.
 * @param parent_frame The stack map representing the call frame of the caller.
 * @param fun The function to invoke to run the fibre.
 * @param arg The argument to pass to the strand. This is visible to the
 * garbage collector between this call and the activation of fun.
 * @return The new strand, with a reference owned by the caller.
 */
ava_strand* ava_strand_spawn(
  ava_fibre* fib, const ava_stack_map* parent_frame,
  ava_strand_main_f fun, ava_stdval arg);

/**
 * Returns the strand the current thread is executing.
 *
 * @return The current strand, or NULL if there is none. This does *not* give a
 * reference to the caller, since most usages will simply want to
 * observe/adjust the status, and the strand is guaranteed to keep existing as
 * long as it keeps executing.
 */
ava_strand* ava_strand_current(void);
/**
 * Marks the given strand as blocked, with the given reason. Note that this
 * does not cause the strand to suspend execution immediately; call
 * ava_strand_yield() for that.
 *
 * @param str The strand to block. NULL indicates the current strand.
 * @param why The reason to display in diagnostics. Should be a
 * statically-allocated string.
 * @param interruptable Whether interruptions will clear the blocked status
 * early.
 */
void ava_strand_block(ava_strand* str, const char* why, ava_bool interruptable);
/**
 * Clears the blocked status of the given strand.
 */
void ava_strand_unblock(ava_strand* str);
/**
 * Indicates that this is a safe point to cease executing the current strand or
 * the current fibre.
 *
 * It is the caller's responsibility to ensure that other runtime components,
 * most importantly the garbage collector, have been informed that the strand
 * may cease execution.
 *
 * If there are any other runnable strands in the same thread-pool as the
 * current strand, they all begin execution before this call returns.
 *
 * If the strand is blocked, this will not return until it is unblocked (which
 * may be due to interruption).
 */
void ava_strand_yield(void);
/**
 * Polls the current strand's interrupt status.
 *
 * If the strand has been interrupted, the interruption exception is thrown.
 *
 * This should be called after recovering from ava_strand_yield() and tearing
 * all state down. Eg,
 *
 *   configure_wakeup_event();
 *   ava_gc_suspend_strand();
 *   ava_strand_block(NULL, "yawn", true);
 *   ava_strand_yield();
 *   ava_gc_resume_strand();
 *   deconfigure_wakeup_event();
 *   ava_strand_check_interrupt();
 */
void ava_strand_check_interrupt(void);

/* TODO: Support for interrupting strands. That will need to wait until we have
 * a better idea of how exceptions will work.
 */

#endif /* AVA_PLATFORM_NATIVE_THREADS_H_ */
