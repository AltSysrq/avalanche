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

struct ava_subprocess_s {
  AO_t refcount;
  AO_t event_callback;

  const char* argv0;
  const char*const* argv;
  unsigned argc;
};

static void ava_subprocess_default_event_callback(
  const ava_sp_event* evt, uintptr_t a, uintptr_t b, uintptr_t c
) { }

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
  M.sp->event_callback = (AO_t)ava_subprocess_default_event_callback;
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

ava_sp_event_callback_f ava_subprocess_get_event_callback(
  const ava_subprocess* sp
) {
  return (ava_sp_event_callback_f)AO_load_acquire_read(&sp->event_callback);
}

ava_bool ava_subprocess_cas_event_callback(
  ava_subprocess* sp, ava_sp_event_callback_f old,
  ava_sp_event_callback_f neu
) {
  return !!AO_compare_and_swap_full(
    &sp->event_callback, (AO_t)old, (AO_t)neu);
}

AVA_END_DECLS
