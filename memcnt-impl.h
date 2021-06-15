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

/* if this is not memcnt.c */
#if !MEMCNT_C

#if MEMCNT_NAMED
#define MEMCNT_IMPL(arch) size_t memcnt_##arch
#define MEMCNT_DEFAULT size_t memcnt
#else
#define MEMCNT_IMPL(arch) size_t memcnt
#define MEMCNT_DEFAULT size_t memcnt
#endif

#endif

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define INLINE static inline
#define NOT_ALIGNED(p, m) ((uintptr_t)(p) & ((m)-1))
#else
#define INLINE static
#define NOT_ALIGNED(p, m) ((unsigned long)(p) & ((m)-1))
#endif

#endif /* MEMCNT_IMPL_H */
