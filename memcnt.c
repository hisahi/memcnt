/*

memcnt -- C function for counting bytes equal to value in a buffer
Copyright (c) 2021 Sampo Hippeläinen
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

/* memcnt.c automatically includes the correct .c files in your case.
   this file cannot and should not be used on its own. you need the other
   memcnt-*.c files (as well as memcnt.h and memcnt-impl.h) for this to work. */

#include "memcnt.h"

#define MEMCNT_TMP_REALLYLONGNAME 1
#ifndef MEMCNT_TMP_REALLYLONGNAME
#include "memcnt-strict.c"
#else
#undef MEMCNT_TMP_REALLYLONGNAME

#include <limits.h>
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#include <stdint.h>
#define MEMCNT_C99 1
#endif

#define MEMCNT_C 1
#include "memcnt-impl.h"

/* order from "most desirable" to "least desirable" */

#ifndef MEMCNT_WIDE
#define MEMCNT_WIDE 1
#endif

#if MEMCNT_NAIVE
#undef MEMCNT_WIDE
#define MEMCNT_WIDE 0
#endif

#if MEMCNT_C99 && !MEMCNT_NAIVE
#if MEMCNT_COMPILE_FOR(avx2)
#include "memcnt-avx2.c"
#ifndef MEMCNT_PICKED
#define MEMCNT_PICKED MEMCNT_NAME_RAW(avx2)
#endif
#endif

#if MEMCNT_COMPILE_FOR(sse2)
#include "memcnt-sse2.c"
#ifndef MEMCNT_PICKED
#define MEMCNT_PICKED MEMCNT_NAME_RAW(sse2)
#endif
#endif
#endif

#if MEMCNT_WIDE
#if defined(UINT64_MAX) && UINTPTR_MAX > UINT32_MAX
typedef uint64_t memcnt_word_t;
#define MEMCNT_WORD 64
#define MEMCNT_COUNT 8
#elif defined(UINT32_MAX)
typedef uint32_t memcnt_word_t;
#define MEMCNT_WORD 32
#define MEMCNT_COUNT 4
#else
#undef MEMCNT_WIDE
#define MEMCNT_WIDE 0
#endif
#endif

#if CHAR_BIT != 8 || !defined(MEMCNT_WORD) || !defined(UINTPTR_MAX)
/* unsupported */
#undef MEMCNT_WIDE
#define MEMCNT_WIDE 0
#endif

#if MEMCNT_WIDE
#if __GNUC__
#define popcount32 __builtin_popcount
#define popcount64 __builtin_popcountl
#else
INLINE int popcount32(uint32_t v) {
    int c = 0;
    for (; v; ++c)
        v &= v - 1;
    return c;
}
INLINE int popcount64(uint64_t v) {
    int c = 0;
    for (; v; ++c)
        v &= v - 1;
    return c;
}
#endif
#endif

#ifdef MEMCNT_PICKED
MEMCNT_DEFAULT(const void *ptr, int value, size_t num) {
#else
size_t memcnt(const void *ptr, int value, size_t num) {
#endif
    size_t c = 0;
    const unsigned char *p = (unsigned char *)ptr, v = value;
#if MEMCNT_WIDE
    if (num > MEMCNT_WORD * 4) {
        /* mask = bytes with only their lowest bit set in word */
#if MEMCNT_WORD == 32
        const memcnt_word_t mask = 0x01010101ULL;
#define POPCOUNT popcount32
#elif MEMCNT_WORD == 64
        const memcnt_word_t mask = 0x0101010101010101ULL;
#define POPCOUNT popcount64
#endif
        /* with * mask, we "broadcast" the byte all over the word */
        memcnt_word_t cmp = (memcnt_word_t)(v * mask), tmp;
        const memcnt_word_t *wp;
        /* handle unaligned ptr */
        while ((uintptr_t)p & (MEMCNT_COUNT - 1))
            --num, c += *p++ == value;
        wp = (const memcnt_word_t *)p;
        while (num >= MEMCNT_COUNT) {
            num -= MEMCNT_COUNT;
            /* XOR with cmp - now bytes equal to value are 00s */
            tmp = *wp++ ^ cmp;
            /* count zero bytes in word tmp and add to c */
            /* shifts make sure the low bits in each byte are set if any
               of the bits in the byte were set */
            tmp |= tmp >> 4;
            tmp |= tmp >> 2;
            tmp |= tmp >> 1;
            /* masks out anything else but the lowest bits in each byte */
            tmp &= mask;
            /* popcount will then give us the number of bytes that weren't 00.
               subtract from the number of bytes in the word to inverse */
            c += MEMCNT_COUNT - POPCOUNT(tmp);
        }
        /* assign pointer back to handle unaligned again */
        p = (const unsigned char *)wp;
    }
#endif
    while (num--)
        c += *p++ == v;
    return c;
}

#if defined(MEMCNT_PICKED) && MEMCNT_TRAMPOLINE
/* trampoline */
size_t memcnt(const void *ptr, int value, size_t num) {
    return MEMCNT_PICKED(ptr, value, num);
}
#endif

#if MEMCNT_DEBUG
const char *memcnt_impl_name_ =
#ifdef MEMCNT_PICKED
STRINGIFYVAL(MEMCNT_PICKED)
#else
STRINGIFYVAL(MEMCNT_NAME_RAW(default))
#endif
;
#endif

#endif
