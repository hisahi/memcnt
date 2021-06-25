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

/* memcnt_sse2 (for Intel SSE2) */

#include "memcnt-impl.h"

#include <emmintrin.h>
#include <limits.h>
#include <stdint.h>

#if __amd64__ || __x86_64__ || _WIN64 || _M_X64 || _M_AMD64 ||                 \
    (defined(UINTPTR_MAX) && UINTPTR_MAX >= UINT64_MAX)
#define SSE2_64 1
#endif

#ifndef UNROLL
#define UNROLL 4
/*
#if SSE2_64
#define UNROLL 4
#else
#define UNROLL 2
#endif
*/
#endif

INLINE size_t sse2_hsum_mm128_epu64(__m128i v) {
    __m128i hi = _mm_shuffle_epi32(v, 78);
#if SSE2_64
    return (size_t)_mm_cvtsi128_si64(_mm_add_epi64(v, hi));
#else
    __m128i vv = _mm_add_epi64(v, hi);
    size_t s = (size_t)_mm_cvtsi128_si32(vv);
#if !defined(SIZE_MAX) || SIZE_MAX > UINT32_MAx
    s |= (size_t)_mm_extract_epi32(vv, 1) << 32;
#endif
    return s;
#endif
}

MEMCNT_IMPL(sse2)(const void *ptr, int value, size_t num) {
    const unsigned char *p = (unsigned char *)ptr, v = (unsigned char)value;
    size_t c = 0;

    if (num >= 32) {
        int k;
        __m128i cmp = _mm_set1_epi8((char)v), sums[UNROLL],
                totals = _mm_setzero_si128();
        uint8_t j = 1;
        const __m128i *wp;
        while (NOT_ALIGNED(p, 0x10))
            --num, c += *p++ == v;
        wp = (const __m128i *)p;

#if UNROLL > 1
        for (k = 0; k < UNROLL; ++k)
            sums[k] = _mm_setzero_si128();
        while (num >= 0x10 * UNROLL) {
            __m128i tmp[UNROLL];
            num -= 0x10 * UNROLL;
            for (k = 0; k < UNROLL; ++k)
                tmp[k] = *wp++;
            for (k = 0; k < UNROLL; ++k)
                sums[k] = _mm_sub_epi8(sums[k], _mm_cmpeq_epi8(cmp, tmp[k]));

            if (++j == 0) {
                for (k = 0; k < UNROLL; ++k)
                    totals = _mm_add_epi64(
                        totals, _mm_sad_epu8(sums[k], _mm_setzero_si128()));
                for (k = 0; k < UNROLL; ++k)
                    sums[k] = _mm_setzero_si128();
                j = 1;
            }
        }

        for (k = 0; k < UNROLL; ++k)
            totals = _mm_add_epi64(totals,
                                   _mm_sad_epu8(sums[k], _mm_setzero_si128()));
        j = 1;
#endif
        sums[0] = _mm_setzero_si128();

        while (num >= 0x10) {
            num -= 0x10;
            sums[0] = _mm_sub_epi8(sums[0], _mm_cmpeq_epi8(cmp, *wp++));

            if (++j == 0) {
                totals = _mm_add_epi64(
                    totals, _mm_sad_epu8(sums[0], _mm_setzero_si128()));
                sums[0] = _mm_setzero_si128();
                j = 1;
            }
        }

        totals =
            _mm_add_epi64(totals, _mm_sad_epu8(sums[0], _mm_setzero_si128()));
        c += sse2_hsum_mm128_epu64(totals);
        p = (const unsigned char *)wp;
    }
    while (num--)
        c += *p++ == v;
    return c;
}
