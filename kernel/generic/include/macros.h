/*
 * Copyright (c) 2005 Jakub Jermar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/** @addtogroup generic
 * @{
 */
/** @file
 */

#ifndef KERN_MACROS_H_
#define KERN_MACROS_H_

#ifndef __ASM__

#include <arch/types.h>

/** Return true if the intervals overlap.
 *
 * @param s1  Start address of the first interval.
 * @param sz1 Size of the first interval.
 * @param s2  Start address of the second interval.
 * @param sz2 Size of the second interval.
 */
static inline int overlaps(uintptr_t s1, size_t sz1, uintptr_t s2, size_t sz2)
{
	uintptr_t e1 = s1 + sz1;
	uintptr_t e2 = s2 + sz2;
	
	return ((s1 < e2) && (s2 < e1));
}

#endif /* __ASM__ */

#define isdigit(d)     (((d) >= '0') && ((d) <= '9'))
#define islower(c)     (((c) >= 'a') && ((c) <= 'z'))
#define isupper(c)     (((c) >= 'A') && ((c) <= 'Z'))
#define isalpha(c)     (is_lower((c)) || is_upper((c)))
#define isalphanum(c)  (is_alpha((c)) || is_digit((c)))
#define isspace(c) \
	(((c) == ' ') || ((c) == '\t') || ((c) == '\n') || ((c) == '\r'))

#define min(a, b)  ((a) < (b) ? (a) : (b))
#define max(a, b)  ((a) > (b) ? (a) : (b))

#define min3(a, b, c)  ((a) < (b) ? (min(a, c)) : (min(b, c)))
#define max3(a, b, c)  ((a) > (b) ? (max(a, c)) : (max(b, c)))

/* Compute overlapping of physical addresses */
#define PA_overlaps(x, szx, y, szy) \
	overlaps(KA2PA((x)), (szx), KA2PA((y)), (szy))

#define SIZE2KB(size)  ((size) >> 10)
#define SIZE2MB(size)  ((size) >> 20)

#define KB2SIZE(kb)  ((kb) << 10)
#define MB2SIZE(mb)  ((mb) << 20)

#define STRING(arg)      STRING_ARG(arg)
#define STRING_ARG(arg)  #arg

/** Pseudorandom generator
 *
 * A pretty standard linear congruential pseudorandom
 * number generator (m = 2^32 or 2^64 depending on architecture).
 *
 */
#define RANDI(seed) \
	({ \
		(seed) = 1103515245 * (seed) + 12345; \
		(seed); \
	})

#endif

/** @}
 */
