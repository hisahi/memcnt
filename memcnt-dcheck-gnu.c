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

/* runtime architecture checks (for dynamic dispatcher), GNU C edition */

#if !MEMCNT_C
#error Use memcnt.c, not this!
#endif

#if MEMCNT_ARCH_X86
static int called_init_ = 0;

INLINE int memcnt_dcheck_gnu_sse2_(void) {
    if (sizeof(size_t) > 32)
        return 1;
    if (!called_init_) {
        __builtin_cpu_init();
        called_init_ = 1;
    }
    return __builtin_cpu_supports("sse2");
}
#define MEMCNT_DCHECK_sse2 memcnt_dcheck_gnu_sse2_()

INLINE int memcnt_dcheck_gnu_avx2_(void) {
    if (!called_init_) {
        __builtin_cpu_init();
        called_init_ = 1;
    }
    return __builtin_cpu_supports("avx2");
}
#define MEMCNT_DCHECK_avx2 memcnt_dcheck_gnu_avx2_()

INLINE int memcnt_dcheck_gnu_avx512_(void) {
    if (!called_init_) {
        __builtin_cpu_init();
        called_init_ = 1;
    }
    return __builtin_cpu_supports("avx512bw");
}
#define MEMCNT_DCHECK_avx512 memcnt_dcheck_gnu_avx512_()

#endif

#if MEMCNT_ARCH_ARM

#if defined(__linux__)
#include <asm/hwcap.h>
#include <sys/auxv.h>
INLINE int memcnt_dcheck_gnu_neon_vendor_(void) {
#if defined(__aarch64__)
    return !!(getauxval(AT_HWCAP) & HWCAP_ASIMD);
#elif defined(__aarch32__)
    return !!(getauxval(AT_HWCAP2) & HWCAP2_ASIMD);
#else
    return !!(getauxval(AT_HWCAP) & HWCAP_NEON);
#endif
}
#define MEMCNT_DCHECK_NEON_VENDOR 1
#elif defined(__ANDROID__)
#include <cpu-features.h>
INLINE int memcnt_dcheck_gnu_neon_vendor_(void) {
    return (!!(android_getCpuFamily() & ANDROID_CPU_FAMILY_ARM64) &&
            !!(android_getCpuFeatures() & ANDROID_CPU_ARM64_FEATURE_ASIMD)) ||
           (!!(android_getCpuFamily() & ANDROID_CPU_FAMILY_ARM) &&
            !!(android_getCpuFeatures() & ANDROID_CPU_ARM_FEATURE_NEON));
}
#define MEMCNT_DCHECK_NEON_VENDOR 1
#elif defined(_WIN64)
#include <Windows.h>
INLINE int memcnt_dcheck_gnu_neon_vendor_(void) {
    return IsProcessorFeaturePresent(PF_ARM_V8_INSTRUCTIONS_AVAILABLE);
}
#define MEMCNT_DCHECK_NEON_VENDOR 1
#endif

INLINE int memcnt_dcheck_gnu_neon_(void) {
#ifdef MEMCNT_DCHECK_NEON_VENDOR
    return memcnt_dcheck_gnu_neon_vendor_();
#else
    /* fall back on compile time check */
    return MEMCNT_CHECK_neon;
#endif
}
#define MEMCNT_DCHECK_neon memcnt_dcheck_gnu_neon_()

#endif

#if MEMCNT_ARCH_WASM
INLINE int memcnt_dcheck_gnu_wasm_simd_(void) {
    /* TODO */
    /* fall time on compile time check */
    return MEMCNT_CHECK_wasm_simd;
}
#define MEMCNT_DCHECK_wasm_simd memcnt_dcheck_gnu_wasm_simd_()

#endif

#if MEMCNT_ARCH_MIPS
INLINE int memcnt_dcheck_gnu_msa_(void) {
    /* TODO */
    /* fall time on compile time check */
    return MEMCNT_CHECK_msa;
}
#define MEMCNT_DCHECK_msa memcnt_dcheck_gnu_msa_()

#endif
