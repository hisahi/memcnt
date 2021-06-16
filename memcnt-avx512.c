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

/* memcnt_avx512 (for Intel AVX-512BW) */

#include "memcnt-impl.h"

#include "immintrin.h"
#include <stdint.h>

#ifndef UNROLL
#define UNROLL 1
#endif

MEMCNT_IMPL(avx512)(const void *ptr, int value, size_t num) {
    const unsigned char *p = (unsigned char *)ptr, v = (unsigned char)value;
    size_t c = 0;

    if (num >= 128) {
        int k;
        __m512i totals = _mm512_setzero_si512();
        __m512i cmp = _mm512_set1_epi8((char)v), sums[UNROLL],
                ones = _mm512_set1_epi8(1);
        uint8_t j = 1;
        const __m512i *wp;
        while (NOT_ALIGNED(p, 0x40))
            --num, c += *p++ == v;
        wp = (const __m512i *)p;
        for (k = 0; k < UNROLL; ++k)
            sums[k] = _mm512_setzero_si512();

#if UNROLL > 1
        while (num >= 0x40 * UNROLL) {
            __m512i tmp[UNROLL];
            __mmask64 masks[UNROLL];
            num -= 0x40 * UNROLL;
            for (k = 0; k < UNROLL; ++k)
                tmp[k] = *wp++;
            for (k = 0; k < UNROLL; ++k)
                masks[k] = _mm512_cmpeq_epu8_mask(cmp, tmp[k]);
            for (k = 0; k < UNROLL; ++k)
                sums[k] = _mm512_mask_add_epi8(sums[k], masks[k],
                                                sums[k], ones);

            if (++j == 0) {
                for (k = 0; k < UNROLL; ++k)
                    totals = _mm512_add_epi64(totals,
                        _mm512_sad_epu8(sums[k], _mm512_setzero_si512()));
                for (k = 0; k < UNROLL; ++k)
                    sums[k] = _mm512_setzero_si512();
                j = 1;
            }
        }
        
        for (k = 0; k < UNROLL; ++k)
            totals = _mm512_add_epi64(totals,
                _mm512_sad_epu8(sums[k], _mm512_setzero_si512()));
        sums[0] = _mm512_setzero_si512();
#endif

        while (num >= 0x40) {
            num -= 0x40;
            sums[0] = _mm512_mask_add_epi8(sums[0],
                                        _mm512_cmpeq_epu8_mask(cmp, *wp++),
                                        sums[0], ones);

            if (++j == 0) {
                totals = _mm512_add_epi64(totals,
                    _mm512_sad_epu8(sums[0], _mm512_setzero_si512()));
                sums[0] = _mm512_setzero_si512();
                j = 1;
            }
        }

        totals = _mm512_add_epi64(totals,
            _mm512_sad_epu8(sums[0], _mm512_setzero_si512()));
        c += _mm512_reduce_add_epi64(totals);
        p = (const unsigned char *)wp;
    }
    while (num--)
        c += *p++ == v;
    return c;
}
