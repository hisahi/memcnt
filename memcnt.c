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

/* memcnt.c automatically includes the correct .c files in your case.
   this file cannot and should not be used on its own. you need the other
   memcnt-*.c files (as well as memcnt.h and memcnt-impl.h) for this to work. */

#include "memcnt.h"

/* try to test identifier limits */
#define MEMCNT_TMP_REALLYLONGNAME 1
#if !defined(MEMCNT_TMP_REALLYLONGNAME) || defined(MEMCNT_TMP_REALLYLONGNAME2)
#include "memcnt-strict.c"
#else
#undef MEMCNT_TMP_REALLYLONGNAME

#include <limits.h>
/* C99? */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#include <stdint.h>
#define MEMCNT_C99 1
#endif

#define MEMCNT_C 1
#include "memcnt-impl.h"

#define STRINGIFY(x) #x
#define STRINGIFYVAL(x) STRINGIFY(x)

/* =============================
    architecture detection code
   ============================= */

/* multiarch stuff.
   expected:
      MEMCNT_COMPILE_ALL  0 if only one impl is compiled in, 1 if all of them
      MEMCNT_IMPL         function signature stub (return type and name, but
                            not the parameters) for implementation compiled
                            for arch
      MEMCNT_DEFAULT      function signature stub for default/naive memcnt
      MEMCNT_CHECK_*      the check for compiling arch A(=*)
                            ignored if MEMCNT_COMPILE_ALL=1, but must be defined
                            for the implementation for arch A to be built */

#if defined(UINTPTR_MAX)
#define MEMCNT_CAN_MULTIARCH 1
#else
#define MEMCNT_CAN_MULTIARCH 0
#endif

#define MEMCNT_NAME_RAW(arch) memcnt_##arch
#if MEMCNT_COMPILE_ALL
#define MEMCNT_NAME(arch) MEMCNT_HEADER memcnt
#else
#define MEMCNT_NAME(arch) MEMCNT_HEADER MEMCNT_NAME_RAW(arch)
#endif

#if defined(__INTEL_COMPILER)

#define MEMCNT_COMPILE_ALL 0
#define MEMCNT_IMPL(arch) MEMCNT_NAME(arch)
#define MEMCNT_DEFAULT MEMCNT_IMPL(default)

#define MEMCNT_CHECK_sse2 __SSE2__
#define MEMCNT_CHECK_avx2 __AVX2__

#elif defined(_MSC_VER)

#define MEMCNT_COMPILE_ALL 0
#define MEMCNT_IMPL(arch) MEMCNT_NAME(arch)
#define MEMCNT_DEFAULT MEMCNT_IMPL(default)

#define MEMCNT_CHECK_sse2 _M_AMD64 || _M_X64 || _M_IX86_FP == 2
#define MEMCNT_CHECK_avx2 __AVX2__
#define MEMCNT_CHECK_neon __ARM_NEON

#elif defined(__GNUC__)

#define MEMCNT_COMPILE_ALL 0
/* GCC does not support multiversioning for C, sadly */
#if MEMCNT_COMPILE_ALL
#define MEMCNT_IMPL(arch)                                                      \
    __attribute__((target(MEMCNT_TARGET_##arch))) MEMCNT_NAME(arch)
#else
#define MEMCNT_IMPL(arch) MEMCNT_NAME(arch)
#endif
#define MEMCNT_DEFAULT MEMCNT_IMPL(default)

#define MEMCNT_CHECK_sse2 __SSE2__
#define MEMCNT_CHECK_avx2 __AVX2__
#define MEMCNT_CHECK_neon __ARM_NEON

#define MEMCNT_TARGET_default "default"
#define MEMCNT_TARGET_sse2 "sse2"
#define MEMCNT_TARGET_avx2 "avx2"
#define MEMCNT_TARGET_neon "fpu=neon"

#endif

#if MEMCNT_CAN_MULTIARCH && defined(MEMCNT_IMPL)
#define MEMCNT_MULTIARCH 1
#define MEMCNT_COMPILE_FOR(arch)                                               \
    MEMCNT_CHECK_##arch && !MEMCNT_NO_ARCH_##arch &&                           \
        (MEMCNT_COMPILE_ALL || !MEMCNT_PICKED)
#else
#define MEMCNT_MULTIARCH 0
#define MEMCNT_COMPILE_FOR(arch)                                               \
    MEMCNT_CHECK_##arch && !MEMCNT_NO_ARCH_##arch && !MEMCNT_PICKED
#undef MEMCNT_IMPL
#define MEMCNT_IMPL(arch) MEMCNT_HEADER memcnt
#endif

#define MEMCNT_TRAMPOLINE MEMCNT_MULTIARCH && !MEMCNT_COMPILE_ALL
#if MEMCNT_TRAMPOLINE
#define MEMCNT_HEADER INLINE size_t
#else
#define MEMCNT_HEADER size_t
#endif

/* =============================
     optimized implementations
   ============================= */

#if MEMCNT_C99 && !MEMCNT_NAIVE
/* order from "most desirable" to "least desirable" within the same arch */

/* Intel AVX2 */
#if MEMCNT_COMPILE_FOR(avx2)
#include "memcnt-avx2.c"
#ifndef MEMCNT_PICKED
#define MEMCNT_PICKED MEMCNT_NAME_RAW(avx2)
#endif
#endif

/* Intel SSE2 */
#if MEMCNT_COMPILE_FOR(sse2)
#include "memcnt-sse2.c"
#ifndef MEMCNT_PICKED
#define MEMCNT_PICKED MEMCNT_NAME_RAW(sse2)
#endif
#endif

/* ARM Neon */
#if MEMCNT_COMPILE_FOR(neon)
#include "memcnt-neon.c"
#ifndef MEMCNT_PICKED
#define MEMCNT_PICKED MEMCNT_NAME_RAW(neon)
#endif
#endif

#endif

/* =============================
       basic  implementation
   ============================= */

/* "wide" memcnt deals with machine words */
#ifndef MEMCNT_WIDE
#define MEMCNT_WIDE 1
#endif

/* MEMCNT_NAIVE forces basic implementation */
#if MEMCNT_NAIVE
#undef MEMCNT_WIDE
#define MEMCNT_WIDE 0
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

/* trampoline */
#if defined(MEMCNT_PICKED) && MEMCNT_TRAMPOLINE
size_t memcnt(const void *ptr, int value, size_t num) {
    return MEMCNT_PICKED(ptr, value, num);
}
#endif

/* debug info */
#if MEMCNT_DEBUG
/* name of "best" implementation compiled in */
const char *memcnt_impl_name_ =
#ifdef MEMCNT_PICKED
    STRINGIFYVAL(MEMCNT_PICKED)
#else
    STRINGIFYVAL(MEMCNT_NAME_RAW(default))
#endif
    ;
#endif

#endif
