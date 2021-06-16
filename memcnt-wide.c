/*

memcnt -- C function for counting bytes equal to value in a buffer
Copyright (c) 2021 Sampo Hippeläinen (hisahi)
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

/* memcnt_wide (implementation that uses machine words) */
/* you must define at least MEMCNT_WORD, which is the bit width of the machine
   word, and typedef memcnt_word_t, the corresponding type for that word */

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include "memcnt-impl.h"

#ifndef MEMCNT_WORD
#error must define MEMCNT_WORD and memcnt_word_t (typedef) for memcnt-wide.c
#elif CHAR_BIT != 8 || ((MEMCNT_WORD) % 8) != 0
#error memcnt-wide.c only supported for CHAR_BIT == 8 and MEMCNT_WORD % 8 == 0
#else

#if MEMCNT_WORD == 32
static const memcnt_word_t mask_ = 0x01010101ULL;
#define POPCOUNT popcount32
#elif MEMCNT_WORD == 64
static const memcnt_word_t mask_ = 0x0101010101010101ULL;
#define POPCOUNT popcount64
#else
#error given MEMCNT_WORD value not supported
#endif

#if __GNUC__
#define popcount32 __builtin_popcount
#define popcount64 __builtin_popcountl
#else
INLINE int popcount(memcnt_word_t v) {
    int c = 0;
    for (; v; ++c)
        v &= v - 1;
    return c;
}
#define popcount32 popcount
#define popcount64 popcount
#endif

#ifndef MEMCNT_COUNT
#define MEMCNT_COUNT (MEMCNT_WORD / CHAR_BIT)
#endif

MEMCNT_IMPL(wide)(const void *ptr, int value, size_t num) {
    size_t c = 0;
    const unsigned char *p = (unsigned char *)ptr, v = (unsigned char)value;
    if (num > MEMCNT_WORD * 4) {
        /* mask = bytes with only their lowest bit set in word */
        const memcnt_word_t mask = mask_;
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
    while (num--)
        c += *p++ == v;
    return c;
}

#endif
