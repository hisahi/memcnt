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

#include "emmintrin.h"
#include <stdint.h>

INLINE unsigned sse2_hsum_mm128_epu8(__m128i v) {
    __m128i vs = _mm_sad_epu8(v, _mm_setzero_si128());
    return _mm_cvtsi128_si32(vs) + _mm_extract_epi16(vs, 4);
}

MEMCNT_IMPL(sse2)(const void *ptr, int value, size_t num) {
    const unsigned char *p = (unsigned char *)ptr, v = (unsigned char)value;
    size_t c = 0;

    if (num >= 32) {
        __m128i cmp = _mm_set1_epi8((char)v), sums = _mm_setzero_si128();
        uint8_t j = 1;
        const __m128i *wp;
        while (NOT_ALIGNED(p, 0x10))
            --num, c += *p++ == v;
        wp = (const __m128i *)p;

        while (num >= 0x10) {
            num -= 0x10;
            sums = _mm_sub_epi8(sums, _mm_cmpeq_epi8(cmp, *wp++));

            if (++j == 0) {
                c += sse2_hsum_mm128_epu8(sums);
                sums = _mm_setzero_si128();
                j = 1;
            }
        }

        c += sse2_hsum_mm128_epu8(sums);
        p = (const unsigned char *)wp;
    }
    while (num--)
        c += *p++ == v;
    return c;
}
