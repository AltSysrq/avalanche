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
#ifndef AVA_COMMON_BSD_DEFS_H_
#define AVA_COMMON_BSD_DEFS_H_

#if defined(HAVE_SYS_QUEUE_H)
#include <sys/queue.h>
#elif defined(HAVE_BSD_SYS_QUEUE_H)
#include <bsd/sys/queue.h>
#else
#error "No BSD sys/queue.h found. You may need to install libbsd-dev."
#endif

/* As of 2015-09-08, even libbsd in debian sid doesn't have TAILQ_SWAP. Provide
 * it if the host does not.
 */
#ifndef TAILQ_SWAP
#define TAILQ_SWAP(head1, head2, type, field) do {			\
	struct type *swap_first = (head1)->tqh_first;			\
	struct type **swap_last = (head1)->tqh_last;			\
	(head1)->tqh_first = (head2)->tqh_first;			\
	(head1)->tqh_last = (head2)->tqh_last;				\
	(head2)->tqh_first = swap_first;				\
	(head2)->tqh_last = swap_last;					\
	if ((swap_first = (head1)->tqh_first) != NULL)			\
		swap_first->field.tqe_prev = &(head1)->tqh_first;	\
	else								\
		(head1)->tqh_last = &(head1)->tqh_first;		\
	if ((swap_first = (head2)->tqh_first) != NULL)			\
		swap_first->field.tqe_prev = &(head2)->tqh_first;	\
	else								\
		(head2)->tqh_last = &(head2)->tqh_first;		\
} while (0)
#endif

/* Same for TAILQ_FOREACH_SAFE */
#ifndef TAILQ_FOREACH_SAFE
#define	TAILQ_FOREACH_SAFE(var, head, field, tvar)			\
	for ((var) = TAILQ_FIRST((head));				\
	    (var) && ((tvar) = TAILQ_NEXT((var), field), 1);		\
	    (var) = (tvar))
#endif

#ifdef __cplusplus
#define AVA_BEGIN_DECLS extern "C" {
#define AVA_END_DECLS }
/* So indentation works as desired */
#define AVA_BEGIN_FILE_PRIVATE namespace {
#define AVA_END_FILE_PRIVATE }
#else
#define AVA_BEGIN_DECLS
#define AVA_END_DECLS
#endif

#if defined(__GNUC__) || defined(__clang__)
#define AVA_MALLOC __attribute__((__malloc__))
#define AVA_PURE __attribute__((__pure__))
#define AVA_CONSTFUN __attribute__((__const__))
#define AVA_NORETURN __attribute__((__noreturn__))
#define AVA_UNUSED __attribute__((__unused__))
#define AVA_LIKELY(x) __builtin_expect(!!(x), 1)
#define AVA_UNLIKELY(x) __builtin_expect((x), 0)
#define AVA_ALIGN(n) __attribute__((__aligned__(n)))
#else
#define AVA_MALLOC
#define AVA_PURE
#define AVA_CONSTFUN
#define AVA_NORETURN
#define AVA_UNUSED
#define AVA_LIKELY(x) (x)
#define AVA_UNLIKELY(x) (x)
#define AVA_ALIGN(n)
#endif

#endif /* AVA_COMMON_BSD_DEFS_H_ */
