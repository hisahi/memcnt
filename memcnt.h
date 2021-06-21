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

#ifndef MEMCNT_H
#define MEMCNT_H

/* define MEMCNT_DYNALINK if you are *building* a dynamic link library.
   define MEMCNT_IMPORT if you are *using* a dynamic link library. */
#ifndef PUBLIC
#if MEMCNT_DYNALINK
#if defined(__GNUC__)
#define PUBLIC __attribute__((visibility("default")))
#elif defined(_MSC_VER) && (defined(_WIN32) || defined(_WIN64))
#define PUBLIC __declspec(dllexport)
#else
#pragma message("cannot define PUBLIC -- memcnt might not get exported")
#define PUBLIC
#endif
#elif MEMCNT_IMPORT
#if defined(_MSC_VER) && (defined(_WIN32) || defined(_WIN64))
#define PUBLIC __declspec(dllimport)
#else
#define PUBLIC
#endif
#else
#define PUBLIC
#endif
#endif

#ifdef __cplusplus
#include <cstddef>
using std::size_t;
extern "C" {
#else
#include <stddef.h>
#endif

/* Counts the number of bytes (characters) equal to c (converted to an
   unsigned char) in the initial n characters in an array pointed to by s. The
   values in the array will be interpreted as unsigned chars and compared to c.
   Returns 0 if n is 0, undefined if s is NULL or n is greater than the number
   of unsigned chars allocated in s. */
PUBLIC size_t memcnt(const void *s, int c, size_t n);

/* if dynamic dispatching is compiled in, memcnt_optimize will automatically
   choose the best implementation and make memcnt call it the next time around.
   memcnt or memcnt_optimize MUST not be called while memcnt_optimize is
   (already) running; doing so results in undefined behavior.

   if the dynamic dispatcher is not used, memcnt_optimize does nothing.
   if you are using a dynamically linked memcnt, you don't have to call
   memcnt_optimize - the library should do that for you.

   you probably don't have to worry about calling this -- see README */
void memcnt_optimize(void);

#ifdef __cplusplus
}
#endif

#endif /* MEMCNT_H */
