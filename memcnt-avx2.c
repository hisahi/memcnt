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

/* memcnt_avx2 (for Intel AVX2) */

#include "memcnt-impl.h"

#include "immintrin.h"
#include <stdint.h>

#ifndef UNROLL
#define UNROLL 4
#endif

INLINE size_t avx2_hsum_mm128_epu64(__m128i v) {
    __m128i hi = _mm_shuffle_epi32(v, 78);
    return (size_t)_mm_cvtsi128_si64(_mm_add_epi64(v, hi));
}

INLINE size_t avx2_hsum_mm256_epu64(__m256i v) {
    __m128i lo = _mm256_castsi256_si128(v), hi = _mm256_extracti128_si256(v, 1);
    return avx2_hsum_mm128_epu64(_mm_add_epi64(lo, hi));
}

MEMCNT_IMPL(avx2)(const void *ptr, int value, size_t num) {
    const unsigned char *p = (unsigned char *)ptr, v = (unsigned char)value;
    size_t c = 0;

    if (num >= 64) {
        int k;
        __m256i cmp = _mm256_set1_epi8((char)v), sums[UNROLL],
                totals = _mm256_setzero_si256();
        uint8_t j = 1;
        const __m256i *wp;
        while (NOT_ALIGNED(p, 0x20))
            --num, c += *p++ == v;
        wp = (const __m256i *)p;

#if UNROLL > 1
        for (k = 0; k < UNROLL; ++k)
            sums[k] = _mm256_setzero_si256();
        while (num >= 0x20 * UNROLL) {
            __m256i tmp[UNROLL];
            num -= 0x20 * UNROLL;
            for (k = 0; k < UNROLL; ++k)
                tmp[k] = *wp++;
            for (k = 0; k < UNROLL; ++k)
                sums[k] =
                    _mm256_sub_epi8(sums[k], _mm256_cmpeq_epi8(cmp, tmp[k]));

            if (++j == 0) {
                for (k = 0; k < UNROLL; ++k)
                    totals = _mm256_add_epi64(
                        totals,
                        _mm256_sad_epu8(sums[k], _mm256_setzero_si256()));
                for (k = 0; k < UNROLL; ++k)
                    sums[k] = _mm256_setzero_si256();
                j = 1;
            }
        }

        for (k = 0; k < UNROLL; ++k)
            totals = _mm256_add_epi64(
                totals, _mm256_sad_epu8(sums[k], _mm256_setzero_si256()));
        j = 1;
#endif
        sums[0] = _mm256_setzero_si256();

        while (num >= 0x20) {
            __m256i tmp;
            num -= 0x20;
            tmp = *wp++;
            sums[0] = _mm256_sub_epi8(sums[0], _mm256_cmpeq_epi8(cmp, tmp));

            if (++j == 0) {
                totals = _mm256_add_epi64(
                    totals, _mm256_sad_epu8(sums[0], _mm256_setzero_si256()));
                sums[0] = _mm256_setzero_si256();
                j = 1;
            }
        }

        totals = _mm256_add_epi64(
            totals, _mm256_sad_epu8(sums[0], _mm256_setzero_si256()));
        c += avx2_hsum_mm256_epu64(totals);
        p = (const unsigned char *)wp;
    }
    while (num--)
        c += *p++ == v;
    return c;
}
