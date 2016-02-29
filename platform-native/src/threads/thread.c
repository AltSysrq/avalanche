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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#ifdef HAVE_UCONTEXT_H
#include <ucontext.h>
#endif

#include <uv.h>
#include <atomic_ops.h>

#include "../avalanche/thread.h"

/**
 * Non-movable data used by a threadpool.
 */
typedef struct {
  /**
   * The unique identifier for this pool.
   */
  ava_qword id;
  /**
   * The UV event loop that controls this threadpool.
   */
  uv_loop_t loop;
  /**
   * Handle used to wake up any thread waiting on the event loop.
   */
  uv_async_t wakeup;
  /**
   * The currently-configured number of threads of this threadpool.
   */
  AO_t desired_size;
  /**
   * The actual number of threads in this threadpool.
   */
  AO_t actual_size;
  /**
   * Mutex guarding the next_threadpool field and the prev_threadpool field of
   * *next_threadpool.
   *
   * Lock restrictions: No lock reachable from prev_threadpool may be taken
   * while this lock is held.
   */
  uv_mutex_t linkage_mutex;
  /**
   * Mutex guarding first_thread and first_fibre.
   *
   * Lock restrictions: This lock may not be taken while any lock reachable
   * from first_thread or first_fibre is held.
   */
  uv_mutex_t list_mutex;
  /**
   * Mutex used to control the uv loop.
   *
   * The uv loop may only be accessed while this is held. If the mutex needs to
   * be obtained immediately (ie, the thread is not trying to take it in
   * preparation for waiting on the uv loop), the proceedure is as follows:
   *
   * - Increment waiting_loop_ops
   * - Signal the wakeup handler
   * - Wait on the mutex
   *
   * After the operation is complete:
   * - Decrement waiting_loop_ops
   * - Signal loop_cond
   * - Release the mutex
   *
   * A thread must not enter a wait on the loop while waiting_loop_ops is
   * non-zero. Instead, it must wait on loop_cond until that value becomes
   * zero.
   */
  uv_mutex_t loop_mutex;
  /**
   * Used by threads preparing to wait on the loop to wait for waiting_loop_ops
   * to become zero.
   *
   * @see pool_mutex
   */
  uv_cond_t loop_cond;
  /**
   * Counter for how many threads are waiting to perform immediate operations.
   * When non-zero, a thread should not block on the loop, but instead wait on
   * loop_cond.
   *
   * @see loop_mutex
   */
  AO_t waiting_loop_ops;
} ava_threadpool_data;

#define AVADO_OBJECT_DECL AVADO_BEGIN(ava_threadpool)   \
  AVADO_HEADER                                          \
  /* Pointer to immovibale data. This is NULLed when */ \
  /* the threadpool is terminated. */                   \
  AVADO_INT(ava_threadpool_data*, data)                 \
  AVADO_PTR(obj, ava_threadpool*, next_threadpool)      \
  AVADO_PTR(obj, ava_threadpool*, prev_threadpool)      \
  AVADO_PTR(obj, ava_thread*, first_thread)             \
  AVADO_PTR(obj, ava_fibre*, first_fibre)               \
  AVADO_END;
#include "../avalanche/decl-obj.h"
