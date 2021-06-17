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

/* memcnt_altivec (for PowerPC/POWER AltiVec/VMX) */

#include "memcnt-impl.h"

#include <altivec.h>
#include <stdint.h>

INLINE __vector unsigned char altivec_bcast_u8(unsigned char v) {
    return (__vector unsigned char){v, v, v, v, v, v, v, v,
                                    v, v, v, v, v, v, v, v};
}

INLINE unsigned altivec_hsum_u8(__vector unsigned char v) {
    __vector unsigned int psums = vec_sum4s(v, (__vector unsigned int){0});
    __vector signed int gsum =
        vec_sums((vector signed int)psums, (__vector signed int){0});
    return (unsigned)(vec_extract(gsum, 3));
}

MEMCNT_IMPL(altivec)(const void *ptr, int value, size_t num) {
    const unsigned char *p = (unsigned char *)ptr, v = (unsigned char)value;
    size_t c = 0;

    if (num >= 32) {
        __vector unsigned char cmp = altivec_bcast_u8(v),
                               sums = (__vector unsigned int){0};
        uint8_t j = 1;
        const __vector unsigned char *wp;
        while (NOT_ALIGNED(p, 0x10))
            --num, c += *p++ == v;
        wp = (const __vector unsigned char *)p;

        while (num >= 0x10) {
            num -= 0x10;
            sums = vec_sub(sums, vec_cmpeq(cmp, vec_ld(0, wp++)));

            if (++j == 0) {
                c += altivec_hsum_u8(sums);
                sums = (__vector unsigned int){0};
                j = 1;
            }
        }

        c += altivec_hsum_u8(sums);
        p = (const unsigned char *)wp;
    }
    while (num--)
        c += *p++ == v;
    return c;
}
