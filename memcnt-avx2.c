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

#define AVX2_UNALIGNED 0

#include "immintrin.h"
#include <stdint.h>

INLINE unsigned avx2_hsum_mm256_epi32(__m256i v) {
    __m128i lo = _mm256_castsi256_si128(v), hi = _mm256_extracti128_si256(v, 1),
            hs, ls;
    lo = _mm_add_epi32(lo, hi);
    hs = _mm_add_epi32(lo, _mm_unpackhi_epi64(lo, lo));
    ls = _mm_add_epi32(hs, _mm_shuffle_epi32(hs, 0xb1));
    return (unsigned)_mm_cvtsi128_si32(ls);
}

INLINE unsigned avx2_hsum_mm256_epu8(__m256i v) {
    __m128i lo = _mm256_castsi256_si128(v), hi = _mm256_extracti128_si256(v, 1);
    __m256i w0 = _mm256_cvtepu8_epi32(lo),
            w1 = _mm256_cvtepu8_epi32(_mm_shuffle_epi32(lo, 0xee)),
            w2 = _mm256_cvtepu8_epi32(hi),
            w3 = _mm256_cvtepu8_epi32(_mm_shuffle_epi32(hi, 0xee));
    return avx2_hsum_mm256_epi32(
        _mm256_add_epi32(_mm256_add_epi32(w0, w1), _mm256_add_epi32(w2, w3)));
}

MEMCNT_IMPL(avx2)(const void *ptr, int value, size_t num) {
    const unsigned char *p = (unsigned char *)ptr, v = (unsigned char)value;
    size_t c = 0;

    if (num >= 64) {
        __m256i cmp = _mm256_set1_epi8((char)v), sums = _mm256_setzero_si256();
        uint8_t j = 1;
#if !AVX2_UNALIGNED
        const __m256i *wp;
        while (NOT_ALIGNED(p, 0x20))
            --num, c += *p++ == v;
        wp = (const __m256i *)p;
#endif

        while (num >= 0x20) {
            __m256i tmp;
            num -= 0x20;
#if AVX2_UNALIGNED
            tmp = _mm256_loadu_si256((const void *)p);
            p += 0x20;
#else
            tmp = *wp++;
#endif
            sums = _mm256_sub_epi8(sums, _mm256_cmpeq_epi8(cmp, tmp));

            if (++j == 0) {
                c += avx2_hsum_mm256_epu8(sums);
                sums = _mm256_setzero_si256();
                j = 1;
            }
        }

        c += avx2_hsum_mm256_epu8(sums);
#if !AVX2_UNALIGNED
        p = (const unsigned char *)wp;
#endif
    }
    while (num--)
        c += *p++ == v;
    return c;
}
