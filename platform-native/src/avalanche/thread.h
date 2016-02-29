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
#ifndef AVA_PLATFORM_NATIVE_THREAD_H_
#define AVA_PLATFORM_NATIVE_THREAD_H_

#include "abi.h"

AVA_BEGIN_DECLS

/**
 * @file
 *
 * Defines the thread-like primitives below the subprocess level.
 */

struct uv_loop_s;

/**
 * A threadpool is a group of platform threads, fibres to run on them, and
 * filaments representing ongoing async operations.
 *
 * Every subprocess has at least one threadpool, and there is only one
 * initially, but more can be created and destroyed as the program executes.
 *
 * The maximum concurrency of a threadpool is set by the number of threads it
 * contains, called the _size_ of the threadpool. The size can be adjusted
 * dynamically.
 *
 * Work is assigned to threads by choosing runnable fibres which have been
 * added to the threadpool.
 *
 * Each thread owns a single libuv event loop, which is polled when there are
 * idle threads in the threadpool. Generally, events in a threadpool's event
 * loop only service actions happening within that threadpool, but
 * cross-threadpool actions are also possible (which is primarily used for
 * locking constructs).
 */
typedef struct ava_threadpool_s ava_threadpool;
/**
 * An ava_thread corresponds to a thread as provided by the underlying
 * platform. Threads are merely vehicles for providing execution time, and thus
 * are relatively uninteresting in concept.
 *
 * Every thread belongs to one threadpool and cannot be reassigned to other
 * pools.
 *
 * A thread is _busy_ if it is executing a fibre, and _idle_ otherwise.
 */
typedef struct ava_thread_s ava_thread;
/**
 * A fibre represents a single execution stack, and is essentially the space
 * component counterpart to the time component provided by a thread.
 *
 * A fibre can have any number of strands assigned to it. Only one strand in a
 * fibre can execute at any time. A fibre containing more than one strand is
 * termed _overloaded_.
 *
 * A fibre may be running, runnable, or blocked. Running means that it is
 * currently executing on a thread. Runnable means it is suspended and has at
 * least one runnable strand. Blocked means it has no runnable strands.
 *
 * Each fibre is assigned to at most one threadpool. If reassigned while
 * running, it will continue executing on the same thread, despite being in a
 * different threadpool, until it is suspended.
 *
 * Threads and fibres have a weak mutual affinity; if the last fibre executed
 * by a now-idle thread becomes runnable, it is continued on that thread.
 */
typedef struct ava_fibre_s ava_fibre;
/**
 * A strand represents a single execution state, and most closely resembles the
 * usual concept of a "thread of execution".
 *
 * A strand belongs to a single fibre and cannot be reassigned. Strands in the
 * same fibre share the same execution stack, and thus only one can execute at
 * a time. At most one strand in a fibre is _liquid_, having its execution
 * stack at the correct address, making it immediately executable. Other
 * strands are _frozen_, and have the used portion relocated to a different
 * address.
 *
 * Each strand other than the root strand has a parent strand, and is
 * associated with a single call frame in that parent. The parent strand may be
 * in a different fibre, and even in a different threadpool.
 *
 * A strand may be _blocked_ by one or more conditions. A strand with no blocks
 * is _runnable_. This is orthogonal to whether the strand is _running_ or
 * _suspended_; typically, there is a very short time between when the strand
 * becomes blocked and when it actually suspends.
 *
 * A strand may be _cancelled_. When this is done, if the strand attempts to
 * block interruptably, an exception is thrown instead. Cancellation may be
 * temporarily suspended from within the strand should it absolutely need to do
 * some blocking operations in cleanup anyway.
 *
 * At a higher level, strands are modelled as Futures. A strand is started with
 * an input value and an execution function; the result of the strand is the
 * value that function produces. If an exception escapes the strand function,
 * it propagates into whatever attempts to read the value.
 */
typedef struct ava_strand_s ava_strand;

ava_threadpool* ava_threadpool_new(AVA_DEF_ARGS0()) AVA_RTAPI;
void ava_threadpool_destroy(AVA_DEF_ARGS1(ava_threadpool* tp)) AVA_RTAPI;
size_t ava_threadpool_get_desired_size(
  AVA_DEF_ARGS1(const ava_threadpool* tp)) AVA_RTAPI;
void ava_threadpool_set_desired_size(
  AVA_DEF_ARGS2(ava_threadpool* tp, size_t sz)) AVA_RTAPI;
size_t ava_threadpool_get_actual_size(
  AVA_DEF_ARGS1(const ava_threadpool* tp)) AVA_RTAPI;
ava_threadpool* ava_threadpool_prev_threadpool(
  AVA_DEF_ARGS1(const ava_threadpool* tp)) AVA_RTAPI;
ava_threadpool* ava_threadpool_next_threadpool(
  AVA_DEF_ARGS1(const ava_threadpool* tp)) AVA_RTAPI;
ava_thread* ava_threadpool_first_thread(
  AVA_DEF_ARGS1(const ava_threadpool* tp)) AVA_RTAPI;
ava_fibre* ava_threadpool_first_fibre(
  AVA_DEF_ARGS1(const ava_threadpool* tp)) AVA_RTAPI;

/**
 * Obtains a pointer to the uv loop controlling the given threadpool.
 *
 * This carries an implicit lock on the pool. ava_threadpool_release_loop()
 * must be called to relinquish access to the loop and allow the threadpool to
 * continue.
 *
 * Note that this lock is *blocking*, ie, the strands will not suspend if they
 * contend for it. Therefore this should be held for as short a time as
 * possible, and there should not be any possibility of throwing an exception
 * between this and the release call.
 */
struct uv_loop_s* ava_threadpool_acquire_loop(
  ava_threadpool* tp) AVA_RTAPI;
/**
 * Releases the lock held by ava_threadpool_acquire_loop().
 */
void ava_threadpool_release_loop(ava_threadpool* tp) AVA_RTAPI;

/**
 * If the given threadpool currently has a thread waiting on the event loop,
 * wakes that thread to reinspect the runnability of non-running fibres in the
 * pool.
 */
void ava_threadpool_awake(ava_threadpool* tp) AVA_RTAPI;

AVA_END_DECLS

#endif /* AVA_PLATFORM_NATIVE_THREAD_H_ */
