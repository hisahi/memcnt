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

/* runtime architecture checks (for dynamic dispatcher), Microsoft edition */

#if !MEMCNT_C
#error Use memcnt.c, not this!
#endif

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#endif

#if MEMCNT_ARCH_X86
#include <intrin.h>

#if defined(_WIN32) || defined(_WIN64)
INLINE int WindowsVersionCheck(int major, int minor, int spmajor, int spminor) {
    OSVERSIONINFOEX osvi;
    DWORDLONG dwlConditionMask = 0;
    int op = VER_GREATER_EQUAL;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    osvi.dwMajorVersion = major;
    osvi.dwMinorVersion = minor;
    osvi.wServicePackMajor = spmajor;
    osvi.wServicePackMinor = spminor;
    VER_SET_CONDITION(dwlConditionMask, VER_MAJORVERSION, op);
    VER_SET_CONDITION(dwlConditionMask, VER_MINORVERSION, op);
    VER_SET_CONDITION(dwlConditionMask, VER_SERVICEPACKMAJOR, op);
    VER_SET_CONDITION(dwlConditionMask, VER_SERVICEPACKMINOR, op);
    return VerifyVersionInfo(
            &osvi, 
            VER_MAJORVERSION | VER_MINORVERSION | 
            VER_SERVICEPACKMAJOR | VER_SERVICEPACKMINOR,
            dwlConditionMask);
}

INLINE int memcnt_dcheck_msvc_sse2_(void) {
    return WindowsVersionCheck(5, 1, 0, 0) &&
            IsProcessorFeaturePresent(PF_XMMI64_INSTRUCTIONS_AVAILABLE);
}
#define MEMCNT_DCHECK_sse2 memcnt_dcheck_msvc_sse2_()

#else
INLINE int memcnt_dcheck_msvc_sse2_(void) {
#if _M_AMD64
    int cpuinfo[4];
    __cpuid(cpuinfo, 0);
    if (cpuinfo[0] < 1)
        return 0;
    __cpuid(cpuinfo, 1);
    return cpuinfo[3] & (1 << 26);
#else
    return 0;
#endif
}
#endif

INLINE int memcnt_dcheck_msvc_avx2_(void) {
#if _M_AMD64
    int cpuinfo[4];
    __cpuid(cpuinfo, 0);
    if (cpuinfo[0] < 7)
        return 0;
    __cpuid(cpuinfo, 7);
    return cpuinfo[1] & (1 << 5);
#else
    return 0;
#endif
}
#define MEMCNT_DCHECK_avx2 memcnt_dcheck_msvc_avx2_()

INLINE int memcnt_dcheck_msvc_avx512_(void) {
#if _M_AMD64
    int cpuinfo[4];
    __cpuid(cpuinfo, 0);
    if (cpuinfo[0] < 7)
        return 0;
    __cpuid(cpuinfo, 7);
    return cpuinfo[1] & (1 << 30);
#else
    return 0;
#endif
}
#define MEMCNT_DCHECK_avx512 memcnt_dcheck_msvc_avx512_()

#endif

#if MEMCNT_ARCH_ARM

#if defined(_WIN64)
INLINE int memcnt_dcheck_msvc_neon_(void) {
    return IsProcessorFeaturePresent(PF_ARM_V8_INSTRUCTIONS_AVAILABLE);
}

#define MEMCNT_DCHECK_neon memcnt_dcheck_msvc_neon_()
#else
#define MEMCNT_DCHECK_neon 0
#endif

#endif
