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

/* memcnt_wasm_simd (for WebAssembly SIMD) */

#include "memcnt-impl.h"

#include <stdint.h>
#include <wasm_simd128.h>

/* for some bizarre reason these names aren't defined yet,
   so we have to use macros */
#define wasm_u8x16_sub wasm_i8x16_sub
#define wasm_u8x16_eq wasm_i8x16_eq
#define wasm_u8x16_splat wasm_i8x16_splat
#define wasm_u16x8_add wasm_i16x8_add
#define wasm_u32x4_add wasm_i32x4_add
#define wasm_u64x2_add wasm_i64x2_add

/* not the fastest. anyone have better ideas? */
INLINE unsigned long long wasm_simd_hsum_u8x16(__u8x16 v) {
    __u16x8 vh = wasm_u16x8_extend_high_u8x16(v);
    __u16x8 vl = wasm_u16x8_add(wasm_u16x8_extend_low_u8x16(v), vh);
    __u32x4 wh = wasm_u32x4_extend_high_u16x8(vl);
    __u32x4 wl = wasm_u32x4_add(wasm_u32x4_extend_low_u16x8(vl), wh);
    __u64x2 qh = wasm_u64x2_extend_high_u32x4(wl);
    __u64x2 ql = wasm_u64x2_add(wasm_u64x2_extend_low_u32x4(wl), qh);
    return ql[0] + ql[1];
}

INLINE __u8x16 wasm_simd_zero_u8x16(void) {
    return (__u8x16)wasm_i64x2_const(0, 0);
}

MEMCNT_IMPL(wasm_simd)(const void *ptr, int value, size_t num) {
    const unsigned char *p = (unsigned char *)ptr, v = (unsigned char)value;
    size_t c = 0;

    if (num >= 32) {
        __u8x16 cmp = wasm_u8x16_splat(v), sums = wasm_simd_zero_u8x16();
        uint8_t j = 1;
        const __u8x16 *wp;
        while (NOT_ALIGNED(p, 0x10))
            --num, c += *p++ == v;
        wp = (const __u8x16 *)p;

        while (num >= 0x10) {
            num -= 0x10;
            sums = wasm_u8x16_sub(sums, wasm_u8x16_eq(cmp, *wp++));

            if (++j == 0) {
                c += wasm_simd_hsum_u8x16(sums);
                sums = wasm_simd_zero_u8x16();
                j = 1;
            }
        }

        c += wasm_simd_hsum_u8x16(sums);
        p = (const unsigned char *)wp;
    }
    while (num--)
        c += *p++ == v;
    return c;
}
