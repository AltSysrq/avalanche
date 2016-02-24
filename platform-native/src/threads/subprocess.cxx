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

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <atomic_ops.h>

#include "../../../common/bsd.h"
#include "../avalanche/defs.h"
#include "../avalanche/abi.h"
#include "../avalanche/subprocess.h"

/* This file is in C++ only so that it can clean up if an exception is thrown,
 * without needing to rely on a questionable C extension.
 */

AVA_BEGIN_DECLS

static __thread ava_subprocess* ava_subprocess_for_thread = NULL;

#define AVADO_OBJECT_DECL AVADO_BEGIN(ava_subprocess)   \
  AVADO_INT(AO_t, refcount)                             \
  AVADO_INT(AO_t, event_callback)                       \
  AVADO_INT(AO_t, genid_high)                           \
  AVADO_INT(AO_t, genid_low)                            \
  AVADO_INT(const char*, argv0)                         \
  AVADO_INT(const char*const*, argv)                    \
  AVADO_INT(unsigned, argc)                             \
  AVADO_END;
#include "../avalanche/decl-obj.h"

int ava_subprocess_run(ava_sp_error* noninteractive,
                       const char*const* argv, unsigned argc,
                       ava_sp_main_f main, void* userdata) {
  struct resources_holder {
    ava_subprocess* sp, *old_thread_sp;

    resources_holder()
    : sp(NULL), old_thread_sp(ava_subprocess_for_thread)
    { }

    ~resources_holder() {
      if (sp) ava_subprocess_decref(sp);
      ava_subprocess_for_thread = old_thread_sp;
    }
  } M;

  assert(argc >= 1);

  M.sp = (ava_subprocess*)malloc(sizeof(ava_subprocess));;
  if (!M.sp) {
    if (noninteractive) {
      *noninteractive = ava_se_out_of_memory;
    } else {
      warnx("out of memory");
    }
    return EX_UNAVAILABLE;
  }

  M.sp->refcount = 1;
  M.sp->genid_high = 0;
  M.sp->genid_low = 0;
  M.sp->event_callback = 0;
  M.sp->argv0 = argv[0];
  /* TODO: Parse arguments */
  M.sp->argv = argv + 1;
  M.sp->argc = argc - 1;

  ava_subprocess_for_thread = M.sp;
  /* TODO: Set memory manager up */
  *noninteractive = ava_se_no_error;
  return (*main)(userdata, AVA_UNDEFINED_UINTPTR, 0);
}

ava_subprocess* ava_subprocess_current(void) {
  return ava_subprocess_for_thread;
}

ava_subprocess* ava_subprocess_incref(ava_subprocess* sp) {
  AO_fetch_and_add1(&sp->refcount);
  return sp;
}

void ava_subprocess_decref(ava_subprocess* sp) {
  if (1 == AO_fetch_and_sub1(&sp->refcount))
    free(sp);
}

ava_qword ava_subprocess_genid(ava_subprocess* sp) {
  ava_qword result;
  ava_dword low, high;

  if (sizeof(ava_qword) <= sizeof(AO_t)) {
    result = AO_fetch_and_add1(&sp->genid_low);
  } else {
    /* We need to generate ids from two dwords, awkwardly. The low dword is
     * simple: just atomically increment it. For the upper dword, we use the
     * following rule: The high bit of the low dword equals the low bit of the
     * high dword. If we read something not in this configuration, we
     * atomically set the high dword to one plus the value we read.
     *
     * This means that there is a 2**31 increment window to actually apply the
     * change before another increment could incorrectly see the unincremented
     * high dword. However, every increment of the low dword during which the
     * high dword hasn't been incremented corresponds to one platform thread
     * executing this exact code. Therefore, to exhaust that window, we'd need
     * 2**31 platform threads preempted here, which is impossible on a system
     * with 32-bit words.
     */
    low = AO_fetch_and_add1_full(&sp->genid_low);
    high = AO_load_acquire_read(&sp->genid_high);

    if ((high & 1) != (low >> 31)) {
      /* atomic_ops's documentation for compare_and_swap is worded in a way
       * that would permit spurious failures, whereas fetch_compare_and_swap
       * explicitly does not permit spurious failures, so use the latter to be
       * conservative.
       */
      (void)AO_fetch_compare_and_swap_full(
        &sp->genid_high, high, high + 1);
    }

    result = ((ava_qword)high << 32) | low;
  }

  /* Check for the incredibly unlikely case that we exhausted the id space */
  if (AVA_UNLIKELY(!~result)) {
    fprintf(stderr, "avalanche: identifier space exhausted\n");
    abort();
  }

  return result;
}

ava_sp_event_callback ava_subprocess_get_event_callback(
  const ava_subprocess* sp
) {
  ava_sp_event_callback cb;

  cb.f = (ava_sp_event_callback_f)AO_load_acquire_read(&sp->event_callback);
  return cb;
}

ava_bool ava_subprocess_cas_event_callback(
  ava_subprocess* sp, ava_sp_event_callback old,
  ava_sp_event_callback neu
) {
  return !!AO_compare_and_swap_full(
    &sp->event_callback, (AO_t)old.f, (AO_t)neu.f);
}

AVA_END_DECLS
