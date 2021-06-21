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

/* memcnt.c automatically includes the correct .c files in your case.
   this file cannot and should not be used on its own. you need the other
   memcnt-*.c files (as well as memcnt.h and memcnt-impl.h) for this to work. */

#include "memcnt.h"

/* try to test identifier limits */
#define MEMCNT_TMP_REALLYLONGNAME 1
#if !defined(MEMCNT_TMP_REALLYLONGNAME) || defined(MEMCNT_TMP_REALLYLONGNAME2)
#include "memcnt-strict.c"
#else
#undef MEMCNT_TMP_REALLYLONGNAME

#include <limits.h>
/* C99? */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#include <stdint.h>
#define MEMCNT_C99 1
#endif

/* C11? */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define MEMCNT_C11 1
#endif

#define MEMCNT_C 1
#include "memcnt-impl.h"

#define STRINGIFY(x) #x
#define STRINGIFYVAL(x) STRINGIFY(x)

/* (try to) use the dynamic dispatcher. the target must support function
   pointers. using the dynamic dispatcher prevents inlining and increases code
   size (as every suitable implementation is compiled), but allows the fastest
   implementation to be optimized during runtime.
   in order to pick that implementation, you must call memcnt_optimize(),
   and while that function is running, memcnt calls are NOT allowed. */
#ifndef MEMCNT_DYNAMIC
#define MEMCNT_DYNAMIC 0
#endif

/* =============================
    architecture detection code
   ============================= */

/*
   to add a new implementation:
   1. add compile-time (and possibly runtime) checks to the
                        compiler-implementation table
   2. add the include to the implementation list
   3. add it to the dynamic dispatcher
*/

/*
   to add support for a new compiler:
   1. add a new section for the compiler in the compiler-implementation table
   2. implement memcnt_only_true_on_first_ and memcnt_atomic_set_
*/

/* Compiler-Implementation Table

   expected:
      MEMCNT_ARCH_*       the check for compiling for architecture *
      MEMCNT_CHECK_*      compile-time check for compiling implementation *
                            ignored if MEMCNT_DYNAMIC=1, but must be defined
                            for the implementation for arch A to be built,
                            and the architecture check will be done either way
      MEMCNT_DCHECK_*     runtime check. you can call functions etc., but if
                            you define a function, make it static (inline) and
                            include it from another file */

#define MEMCNT_NAME(impl) memcnt_##impl

#if defined(__INTEL_COMPILER)

#define MEMCNT_ARCH_X86 1

#define MEMCNT_CHECK_sse2 __SSE2__
#define MEMCNT_CHECK_avx2 __AVX2__
#define MEMCNT_CHECK_avx512 __AVX512BW__

#include <immintrin.h>

#define MEMCNT_DCHECK_sse2 _may_i_use_cpu_feature(_FEATURE_SSE2)
#define MEMCNT_DCHECK_avx2 _may_i_use_cpu_feature(_FEATURE_AVX2)
#define MEMCNT_DCHECK_avx512 _may_i_use_cpu_feature(_FEATURE_AVX512BW)

#elif defined(_MSC_VER)

#define MEMCNT_ARCH_X86 (_M_X86 || _M_AMD64 || _M_X64)
#define MEMCNT_ARCH_ARM (_M_ARM || _M_ARM64)
#define MEMCNT_ARCH_POWER _M_PPC

#define MEMCNT_CHECK_sse2 (_M_IX86_FP == 2 || _M_AMD64 || _M_X64)
#define MEMCNT_CHECK_avx2 __AVX2__
#define MEMCNT_CHECK_avx512 __AVX512BW__
#define MEMCNT_CHECK_neon __ARM_NEON

#if _MSC_VER >= 1700
#define MEMCNT_STDINT 1
#include <stdint.h>
#endif

/* runtime checks (DCHECK) */
#include "memcnt-dcheck-msvc.c"

#elif defined(__GNUC__)

#define MEMCNT_ARCH_X86 (__i386__ || __amd64__)
#define MEMCNT_ARCH_ARM (__arm__ || __aarch64__)
#define MEMCNT_ARCH_WASM __wasm__
#define MEMCNT_ARCH_MIPS __mips__
#define MEMCNT_ARCH_POWER (__PPC__ || __PPC64__)

#define MEMCNT_CHECK_sse2 __SSE2__
#define MEMCNT_CHECK_avx2 __AVX2__
#define MEMCNT_CHECK_avx512 __AVX512BW__
#define MEMCNT_CHECK_neon __ARM_NEON
#define MEMCNT_CHECK_wasm_simd __wasm_simd128__

/* runtime checks (DCHECK) */
#include "memcnt-dcheck-gnu.c"

#else

#define MEMCNT_CAN_MULTIARCH 0

#endif

#ifndef MEMCNT_STDINT
#define MEMCNT_STDINT MEMCNT_C99
#endif

#ifndef MEMCNT_CAN_MULTIARCH
#if defined(UINTPTR_MAX)
#define MEMCNT_CAN_MULTIARCH 1
#else
#define MEMCNT_CAN_MULTIARCH 0
#endif
#endif

#if MEMCNT_CAN_MULTIARCH
#define MEMCNT_MULTIARCH 1
#define MEMCNT_COMPILE_FOR(arch, impl)                                         \
    (MEMCNT_ARCH_##arch) && (MEMCNT_CHECK_##impl) &&                           \
        (!(MEMCNT_NO_IMPL_##impl)) && (MEMCNT_DYNAMIC || (!(MEMCNT_PICKED)))
#else
#define MEMCNT_MULTIARCH 0
#define MEMCNT_COMPILE_FOR(arch, impl)                                         \
    (MEMCNT_ARCH_##arch) && (MEMCNT_CHECK_##impl) &&                           \
        (!(MEMCNT_NO_IMPL_##impl)) && !(MEMCNT_PICKED)
#endif

#define MEMCNT_TRAMPOLINE MEMCNT_MULTIARCH && !MEMCNT_DYNAMIC
#if MEMCNT_TRAMPOLINE
#define MEMCNT_HEADER INLINE size_t
#else
#define MEMCNT_HEADER size_t
#endif

#if MEMCNT_MULTIARCH
#define MEMCNT_IMPL(impl) MEMCNT_HEADER MEMCNT_NAME(impl)
#else
#define MEMCNT_IMPL(impl) MEMCNT_HEADER memcnt
#endif
#define MEMCNT_DEFAULT MEMCNT_IMPL(default)

/* =============================
     optimized implementations
   ============================= */

/* implementation list */

#if MEMCNT_CAN_MULTIARCH && MEMCNT_STDINT && !MEMCNT_NAIVE
/* order from "most desirable" to "least desirable" within the same arch */

/* all compiled impls must define MEMCNT_COMPILED_* to 1 and
    MEMCNT_PICKED to themselves if not already defined */

/* Intel AVX-512(BW) */
#if MEMCNT_COMPILE_FOR(X86, avx512)
#include "memcnt-avx512.c"
#define MEMCNT_COMPILED_avx512 1
#ifndef MEMCNT_PICKED
#define MEMCNT_PICKED MEMCNT_NAME(avx512)
#endif
#endif

/* Intel AVX2 */
#if MEMCNT_COMPILE_FOR(X86, avx2)
#include "memcnt-avx2.c"
#define MEMCNT_COMPILED_avx2 1
#ifndef MEMCNT_PICKED
#define MEMCNT_PICKED MEMCNT_NAME(avx2)
#endif
#endif

/* Intel SSE2 */
#if MEMCNT_COMPILE_FOR(X86, sse2)
#include "memcnt-sse2.c"
#define MEMCNT_COMPILED_sse2 1
#ifndef MEMCNT_PICKED
#define MEMCNT_PICKED MEMCNT_NAME(sse2)
#endif
#endif

/* ARM Neon */
#if MEMCNT_COMPILE_FOR(ARM, neon)
#include "memcnt-neon.c"
#define MEMCNT_COMPILED_neon 1
#ifndef MEMCNT_PICKED
#define MEMCNT_PICKED MEMCNT_NAME(neon)
#endif
#endif

/* WebAssembly SIMD */
#if MEMCNT_COMPILE_FOR(WASM, wasm_simd)
#include "memcnt-wasm-simd.c"
#define MEMCNT_COMPILED_wasm_simd 1
#ifndef MEMCNT_PICKED
#define MEMCNT_PICKED MEMCNT_NAME(wasm_simd)
#endif
#endif

#endif

/* =============================
       basic  implementation
   ============================= */
/* (includes -default and -wide) */

/* "wide" memcnt deals with machine words */
#ifndef MEMCNT_WIDE
#define MEMCNT_WIDE 1
#endif

/* MEMCNT_NAIVE forces basic implementation */
#if MEMCNT_NAIVE
#undef MEMCNT_WIDE
#define MEMCNT_WIDE 0
#endif

#if MEMCNT_WIDE
#if defined(UINT64_MAX) && UINTPTR_MAX > UINT32_MAX
typedef uint64_t memcnt_word_t;
#define MEMCNT_WORD 64
#define MEMCNT_COUNT 8
#elif defined(UINT32_MAX)
typedef uint32_t memcnt_word_t;
#define MEMCNT_WORD 32
#define MEMCNT_COUNT 4
#else
#undef MEMCNT_WIDE
#define MEMCNT_WIDE 0
#endif
#endif

#if CHAR_BIT != 8 || !defined(MEMCNT_WORD) || !defined(UINTPTR_MAX)
/* unsupported */
#undef MEMCNT_WIDE
#define MEMCNT_WIDE 0
#endif

#if MEMCNT_CAN_MULTIARCH && MEMCNT_WIDE
#include "memcnt-wide.c"
#ifndef MEMCNT_PICKED
#define MEMCNT_PICKED MEMCNT_NAME(wide)
#endif
#endif

#ifndef MEMCNT_PICKED
#undef MEMCNT_DEFAULT
#define MEMCNT_DEFAULT size_t memcnt
#include "memcnt-default.c"
#elif MEMCNT_DYNAMIC
#include "memcnt-default.c"
#endif

/* =============================
            dispatchers
   ============================= */

/* trampoline (static dispatcher) */
#if defined(MEMCNT_PICKED) && MEMCNT_TRAMPOLINE
size_t memcnt(const void *s, int c, size_t n) { return MEMCNT_PICKED(s, c, n); }
#endif

/* dynamic dispatcher */
#if MEMCNT_MULTIARCH && MEMCNT_DYNAMIC
/* size_t memcnt(const void *s, int c, size_t n); */
typedef size_t (*memcnt_implptr_t)(const void *, int, size_t);

static memcnt_implptr_t memcnt_impl_;

/* debug info. MEMCNT_DEBUG makes memcnt nonreentrant! */
#if MEMCNT_DEBUG
const char *memcnt_impl_name_;
static char memcnt_impl_dbuf_[256];

memcnt_implptr_t memcnt_impl_choose_(memcnt_implptr_t fp, const char *s) {
    char *d = memcnt_impl_dbuf_,
         *e = memcnt_impl_dbuf_ + sizeof(memcnt_impl_dbuf_) - 1;
    while (d < e && *s)
        *d++ = *s++;
    *d = 0;
    memcnt_impl_name_ = memcnt_impl_dbuf_;
    return fp;
}

#define MEMCNT_DYNAMIC_CHOOSE(x)                                               \
    memcnt_impl_choose_(&(MEMCNT_NAME(x)), "memcnt_" #x)
#else
#define MEMCNT_DYNAMIC_CHOOSE(x) &(MEMCNT_NAME(x))
#endif

#define MEMCNT_DYNAMIC_CANDIDATE(implname)                                     \
    else if (MEMCNT_DCHECK_##implname) p = MEMCNT_DYNAMIC_CHOOSE(implname);

static int memcnt_optimized = 0;

void memcnt_optimize(void) {
    memcnt_implptr_t p;
    if (memcnt_optimized)
        return;
    memcnt_optimized = 1;
    if (0)
        ;

#if MEMCNT_COMPILED_avx512 && defined(MEMCNT_DCHECK_avx512)
    MEMCNT_DYNAMIC_CANDIDATE(avx512)
#endif

#if MEMCNT_COMPILED_avx2 && defined(MEMCNT_DCHECK_avx2)
    MEMCNT_DYNAMIC_CANDIDATE(avx2)
#endif

#if MEMCNT_COMPILED_sse2 && defined(MEMCNT_DCHECK_sse2)
    MEMCNT_DYNAMIC_CANDIDATE(sse2)
#endif

#if MEMCNT_COMPILED_neon && defined(MEMCNT_DCHECK_neon)
    MEMCNT_DYNAMIC_CANDIDATE(neon)
#endif

#if MEMCNT_COMPILED_wasm_simd && defined(MEMCNT_DCHECK_wasm_simd)
    MEMCNT_DYNAMIC_CANDIDATE(wasm_simd)
#endif

    else
#if MEMCNT_WIDE
        p = MEMCNT_DYNAMIC_CHOOSE(wide);
#else
        p = MEMCNT_DYNAMIC_CHOOSE(default);
#endif
    memcnt_impl_ = p;
}

#ifdef MEMCNT_PICKED
static memcnt_implptr_t memcnt_impl_ = &MEMCNT_PICKED;
#else
static memcnt_implptr_t memcnt_impl_ = &MEMCNT_NAME(default);
#endif

size_t memcnt(const void *s, int c, size_t n) {
    return (*memcnt_impl_)(s, c, n);
}
#else
#undef MEMCNT_DYNAMIC
#define MEMCNT_DYNAMIC 0
void memcnt_optimize(void) {}
#endif

/* debug info. MEMCNT_DEBUG makes memcnt nonreentrant! */
#if MEMCNT_DEBUG
/* name of "best" implementation compiled in */
const char *memcnt_impl_name_ =
#if MEMCNT_DYNAMIC
    STRINGIFYVAL(MEMCNT_PICKED)
#else
    STRINGIFYVAL(MEMCNT_NAME(default))
#endif
    ;
#endif

#endif
