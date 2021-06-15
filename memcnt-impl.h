/*

memcnt -- C function for counting bytes equal to value in a buffer
Copyright (c) 2021 Sampo Hippeläinen
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

#ifndef MEMCNT_IMPL_H
#define MEMCNT_IMPL_H

#include "stddef.h"

#ifndef MEMCNT_C

#if MEMCNT_NAMED
#define MEMCNT_IMPL(arch) size_t memcnt_##arch
#define MEMCNT_DEFAULT size_t memcnt
#else
#define MEMCNT_IMPL(arch) size_t memcnt
#define MEMCNT_DEFAULT size_t memcnt
#endif

#else

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define INLINE static inline
#else
#define INLINE static
#endif

#ifndef MEMCNT_PREFETCH
#define MEMCNT_PREFETCH 1
#endif

#if !MEMCNT_PREFETCH
#define PREFETCH(ptr)
#elif defined(__GNUC__)
#define PREFETCH(ptr) __builtin_prefetch(ptr)
#else
#define PREFETCH(ptr)
#endif

#define NOT_ALIGNED(p, m) ((uintptr_t)(p) & ((m)-1))

#define STRINGIFY(x) #x
#define STRINGIFYVAL(x) STRINGIFY(x)

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

#define MEMCNT_CHECK_sse2                                                      \
    defined(_M_AMD64) || defined(_M_X64) || _M_IX86_FP == 2
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
        (MEMCNT_COMPILE_ALL || !defined(MEMCNT_PICKED))
#else
#define MEMCNT_MULTIARCH 0
#define MEMCNT_COMPILE_FOR(arch)                                               \
    defined(MEMCNT_CHECK_##arch) && MEMCNT_CHECK_##arch &&                     \
        !defined(MEMCNT_PICKED)
#undef MEMCNT_IMPL
#define MEMCNT_IMPL(arch) MEMCNT_HEADER memcnt
#endif

#define MEMCNT_TRAMPOLINE MEMCNT_MULTIARCH && !MEMCNT_COMPILE_ALL
#if MEMCNT_TRAMPOLINE
#define MEMCNT_HEADER INLINE size_t
#else
#define MEMCNT_HEADER size_t
#endif

#endif

#endif /* MEMCNT_IMPL_H */
