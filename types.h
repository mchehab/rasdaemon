/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2013 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#ifndef __TYPES_H
#define __TYPES_H

#include <asm/bitsperlong.h>
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#define STR(x) #x

/* Please keep the macros as much as possible alined with Linux Kernel ones */

/*
 * Force a compilation error if condition is true, but also produce a
 * result (of value 0 and type int), so the expression can be used
 * e.g. in a structure initializer (or where-ever else comma expressions
 * aren't permitted).
 */
#define BUILD_BUG_ON_ZERO(e) ((int)(sizeof(struct { int:(-!!(e)); })))

/* Are two types/vars the same type (ignoring qualifiers)? */
#define __same_type(a, b) __builtin_types_compatible_p(typeof(a), typeof(b))

/* &a[0] degrades to a pointer: a different type from an array */
#define __must_be_array(a)	BUILD_BUG_ON_ZERO(__same_type((a), &(a)[0]))

/*
 * This returns a constant expression while determining if an argument is
 * a constant expression, most importantly without evaluating the argument.
 * Glory to Martin Uecker <Martin.Uecker@med.uni-goettingen.de>
 *
 * Details:
 * - sizeof() return an integer constant expression, and does not evaluate
 *   the value of its operand; it only examines the type of its operand.
 * - The results of comparing two integer constant expressions is also
 *   an integer constant expression.
 * - The first literal "8" isn't important. It could be any literal value.
 * - The second literal "8" is to avoid warnings about unaligned pointers;
 *   this could otherwise just be "1".
 * - (long)(x) is used to avoid warnings about 64-bit types on 32-bit
 *   architectures.
 * - The C Standard defines "null pointer constant", "(void *)0", as
 *   distinct from other void pointers.
 * - If (x) is an integer constant expression, then the "* 0l" resolves
 *   it into an integer constant expression of value 0. Since it is cast to
 *   "void *", this makes the second operand a null pointer constant.
 * - If (x) is not an integer constant expression, then the second operand
 *   resolves to a void pointer (but not a null pointer constant: the value
 *   is not an integer constant 0).
 * - The conditional operator's third operand, "(int *)8", is an object
 *   pointer (to type "int").
 * - The behavior (including the return type) of the conditional operator
 *   ("operand1 ? operand2 : operand3") depends on the kind of expressions
 *   given for the second and third operands. This is the central mechanism
 *   of the macro:
 *   - When one operand is a null pointer constant (i.e. when x is an integer
 *     constant expression) and the other is an object pointer (i.e. our
 *     third operand), the conditional operator returns the type of the
 *     object pointer operand (i.e. "int *"). Here, within the sizeof(), we
 *     would then get:
 *       sizeof(*((int *)(...))  == sizeof(int)  == 4
 *   - When one operand is a void pointer (i.e. when x is not an integer
 *     constant expression) and the other is an object pointer (i.e. our
 *     third operand), the conditional operator returns a "void *" type.
 *     Here, within the sizeof(), we would then get:
 *       sizeof(*((void *)(...)) == sizeof(void) == 1
 * - The equality comparison to "sizeof(int)" therefore depends on (x):
 *     sizeof(int) == sizeof(int)     (x) was a constant expression
 *     sizeof(int) != sizeof(void)    (x) was not a constant expression
 */
#define __is_constexpr(x) \
	(sizeof(int) == sizeof(*(8 ? ((void *)((long)(x) * 0l)) : (int *)8)))

/**
 * ARRAY_SIZE - get the number of elements in array @arr
 * @arr: array to be sized
 */
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]) + __must_be_array(arr))

/* BIT handling */

#define _AC(X, Y)	(X##Y)

#define _UL(x)          (_AC(x, UL))
#define _ULL(x)         (_AC(x, ULL))

#define GENMASK_INPUT_CHECK(h, l) \
	(BUILD_BUG_ON_ZERO(__builtin_choose_expr( \
		__is_constexpr((l) > (h)), (l) > (h), 0)))

#define __GENMASK(h, l) \
	(((~_UL(0)) - (_UL(1) << (l)) + 1) & \
	 (~_UL(0) >> (__BITS_PER_LONG - 1 - (h))))

#define __GENMASK_ULL(h, l) \
	(((~_ULL(0)) - (_ULL(1) << (l)) + 1) & \
	 (~_ULL(0) >> (__BITS_PER_LONG_LONG - 1 - (h))))

#define GENMASK(h, l) \
	(GENMASK_INPUT_CHECK(h, l) + __GENMASK(h, l))
#define GENMASK_ULL(h, l) \
	(GENMASK_INPUT_CHECK(h, l) + __GENMASK_ULL(h, l))

#define BIT(nr)                 (_UL(1) << (nr))
#define BIT_ULL(nr)             (_ULL(1) << (nr))

/* Useful constants */

#define MAX_PATH			1024
#define SZ_512				0x200

/* String copy */

/**
 * strscpy - safe implementation of strcpy, with a buffer size
 * @dst:	destination buffer
 * @src:	source buffer
 * @dsize:	size of the destination buffer
 *
 * Copy string up to @dsize-1 characters. String will end with a '\0'
 * character at the end.
 *
 * Returns: number of characters at the destination buffer or -E2BIG if
 * the string was too big to fit.
 */
static inline size_t strscpy(char *dst, const char *src, size_t dsize)
{
	size_t i = 0;

	for (; i < dsize; i++, dst++, src++) {
		*dst = *src;
		if (!*dst)
			return i;
	}

	if (i)
		dst--;

	*dst = '\0';

	return -E2BIG;
}

/**
 * strscat - safe implementation of strcat, with a buffer size
 * @dst:	destination buffer
 * @src:	source buffer
 * @dsize:	size of the destination buffer
 *
 * Append string until @dst is up to @dsize-1 characters. String will end
 * with a '\0' character at the end.
 *
 * Returns: number of characters at the destination buffer or -E2BIG if
 * the string was too big to fit.
 */
static inline size_t strscat(char *dst, const char *src, size_t dsize)
{
	int i, rc;

	for (i = 0; dsize > 0; dsize--, dst++, i++)
		if (!*dst)
			break;

	rc = strscpy(dst, src, dsize);
	if (rc < 0)
		return rc;

	return rc + i;
}

/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:	the pointer to the member.
 * @type:	the type of the container struct this is embedded in.
 * @member:	the name of the member within the struct.
 *
 * WARNING: any const qualifier of @ptr is lost.
 */
#define container_of(ptr, type, member) ({				\
	void *__mptr = (void *)(ptr);					\
	static_assert(__same_type(*(ptr), ((type *)0)->member) ||	\
		      __same_type(*(ptr), void),			\
		      "pointer type mismatch in container_of()");	\
	((type *)(__mptr - offsetof(type, member))); })

#define LOGLEVEL_DEFAULT	-1	/* default (or last) loglevel */
#define LOGLEVEL_EMERG		0	/* system is unusable */
#define LOGLEVEL_ALERT		1	/* action must be taken immediately */
#define LOGLEVEL_CRIT		2	/* critical conditions */
#define LOGLEVEL_ERR		3	/* error conditions */
#define LOGLEVEL_WARNING	4	/* warning conditions */
#define LOGLEVEL_NOTICE		5	/* normal but significant condition */
#define LOGLEVEL_INFO		6	/* informational */
#define LOGLEVEL_DEBUG		7	/* debug-level messages */

extern const char *loglevel_str[];
#endif
