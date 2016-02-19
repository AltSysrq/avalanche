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

/* This file should not be included from any public Avalanche headers */

#ifndef AVALANCHE_BSD_H_
#define AVALANCHE_BSD_H_

#include <stddef.h>

#if HAVE_BSD_ERR_H
#include <bsd/err.h>
#elif HAVE_ERR_H
#include <err.h>
#else
#error "No BSD err.h could be found on your system. (See libbsd-dev on GNU.)"
#endif

#if HAVE_BSD_SYSEXITS_H
#include <bsd/sysexits.h>
#elif HAVE_SYSEXITS_H
#include <sysexits.h>
#else
#error "No BSD sysexits.h could be found on your system. (See libbsd-dev on GNU.)"
#endif

/* In an older libbsd on Debian, we have the following comment in cdefs.h:
 *
 * * Linux headers define a struct with a member names __unused.
 * * Debian bugs:_ #522773 (linux), #522774 (libc).
 * * Disable for now.
 *
 * Following this is an #if 0 surrounding the definition of __unused, which
 * breaks RB_*.
 */
#ifndef __unused
# if LIBBSD_GCC_VERSION >= 0x0300
#  define __unused __attribute__((unused))
# else
#  define __unused
# endif
#endif

/* This can't be in defs.h because of the issues surrounding __unused. */
#if HAVE_BSD_SYS_TREE_H
#include <bsd/sys/tree.h>
#elif HAVE_SYS_TREE_H
#include <sys/tree.h>
#else
#error "No BSD tree.h could be found on your system. (See libbsd-dev on GNU.)"
#endif

/* Older BSDs (including libbsd on Debian Sid as of 2015-11-02) strangely lack
 * LIST_PREV.
 */
#ifndef LIST_PREV
#define LIST_PREV(elm, head, type, field)                               \
        ((elm)->field.le_prev == &LIST_FIRST((head)) ? NULL :           \
            __containerof((elm)->field.le_prev, struct type, field.le_next))
#endif

/* cdefs needed by LIST_PREV */

#ifndef __GNUC_PREREQ__
#if defined(__GNUC__) && !defined(__INTEL_COMPILER)
#define __GNUC_PREREQ__(ma, mi) \
        (__GNUC__ > (ma) || __GNUC__ == (ma) && __GNUC_MINOR__ >= (mi))
#else
#define __GNUC_PREREQ__(ma, mi) 0
#endif
#endif /* !defined(__GNUC_PREREQ__) */

#ifndef __containerof
#if __GNUC_PREREQ__(3, 1)
#define __containerof(x, s, m) ({                                       \
        const volatile __typeof(((s *)0)->m) *__x = (x);                \
        __DEQUALIFY(s *, (const volatile char *)__x - __offsetof(s, m));\
})
#else
#define __containerof(x, s, m)                                          \
        __DEQUALIFY(s *, (const volatile char *)(x) - __offsetof(s, m))
#endif
#endif /* !defined(__containerof) */

/* This definition differs from the FreeBSD source, as the original seems to
 * confuse GCC (perhaps a missing definition somewhere else?).
 */
#ifndef __DEQUALIFY
#define __DEQUALIFY(type, var)  ((type)(var))
#endif

#ifndef __offsetof
#define __offsetof offsetof
#endif

#endif /* AVALANCHE_BSD_H_ */
