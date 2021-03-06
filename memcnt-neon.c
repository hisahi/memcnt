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

/* memcnt_neon (for ARM Neon) */

#include "memcnt-impl.h"

#include <arm_neon.h>
#include <stdint.h>

INLINE unsigned neon_hsum_u8x16_u(uint8x16_t v) {
    int i;
    unsigned s = 0;
    for (i = 0; i < 16; ++i)
        s += v[i];
    return s;
}

MEMCNT_IMPL(neon)(const void *ptr, int value, size_t num) {
    const unsigned char *p = (unsigned char *)ptr, v = (unsigned char)value;
    size_t c = 0;

    if (num >= 32) {
        uint8x16_t cmp = vdupq_n_u8((uint8_t)v), sums = vdupq_n_u8(0);
        uint8_t j = 1;
        const uint8x16_t *wp;
        while (NOT_ALIGNED(p, 0x10))
            --num, c += *p++ == v;
        wp = (const uint8x16_t *)p;

        while (num >= 0x10) {
            num -= 0x10;
            sums = vsubq_u8(sums, vceqq_u8(cmp, *wp++));

            if (++j == 0) {
                c += neon_hsum_u8x16_u(sums);
                sums = vdupq_n_u8(0);
                j = 1;
            }
        }

        c += neon_hsum_u8x16_u(sums);
        p = (const unsigned char *)wp;
    }
    while (num--)
        c += *p++ == v;
    return c;
}
