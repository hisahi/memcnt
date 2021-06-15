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

/* useful for testing.
   this file includes memcnt.c and thus should be compiled on its own */

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "memcnt.h"

#define MEMCNT_DEBUG 1
#include "memcnt.c"

/* mask &0xFF before giving to function. the implementation should work
   regardless of whether this is 0 or 1, but this might help with debugging */
#ifndef MASK
#define MASK 0
#endif

/* 0 = no benchmark */
/* 1 = benchmark using clock() */
/* 2 = benchmark using x86 rdtsc */
#ifndef BENCHMARK
#define BENCHMARK 1
#endif

#define CHAR_COUNT (1 << CHAR_BIT)

#if BENCHMARK
#define MIN_ARRAY_SIZE 0
#define MAX_ARRAY_SIZE 100000000
#define ARRAY_MUL 10
#define TRY_COUNT 3
#else
#define MIN_ARRAY_SIZE 0
#define MAX_ARRAY_SIZE 531441
#define ARRAY_MUL 3
#define TRY_COUNT CHAR_COUNT
#endif

#if BENCHMARK == 2
#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif
typedef unsigned long long rdtsc_t;
rdtsc_t rdtsc() {
    _mm_lfence();
    return __rdtsc();
}
#endif

unsigned char buf[MAX_ARRAY_SIZE];

static int rng_seed = 0;

static inline int rng() {
    rng_seed = (1784265361 * rng_seed + 252197837);
    return (rng_seed >> 7) & 0xFFFFFF;
}

void fill_array(int arraySize, int tryCount, int *tries, int *counts) {
    int i;
    for (i = 0; i < CHAR_COUNT; ++i)
        counts[i] = 0;
#if BENCHMARK
    for (i = 0; i < tryCount; ++i)
        tries[i] = rng();
#else
    for (i = 0; i < tryCount; ++i)
        tries[i] = i;
#endif

    /* fill array */
    for (i = 0; i < arraySize; ++i)
        ++counts[(unsigned char)(buf[i] = rng())];
}

int main(int argc, char *argv[]) {
    int arraySize, try
        , i, testValue;
    int tries[TRY_COUNT], counts[CHAR_COUNT] = {};
    size_t trueCount, testCount;
#if BENCHMARK == 1
    clock_t testStart, testEnd;
#elif BENCHMARK == 2
    rdtsc_t testStart, testEnd;
#endif
    srand(time(NULL));
    rng_seed = rand() ^ (rand() << 11) ^ (rand() << 23);
    printf("Testing implementation '%s'\n", memcnt_impl_name_);
#if BENCHMARK
    printf("%9s | %3s | %6s | %4s\n", "Size", "Try", "Status", "Time");
#else
    printf("%9s | %9s\n", "Size", "Status");
#endif
    for (arraySize = MIN_ARRAY_SIZE; arraySize <= MAX_ARRAY_SIZE;
         arraySize ? arraySize *= ARRAY_MUL : ++arraySize) {
        fill_array(arraySize, TRY_COUNT, tries, counts);

        i = 0;
#if !BENCHMARK
        printf("%9d | ", arraySize);
#endif
        for (try = 0; try < TRY_COUNT; ++try) {
#if BENCHMARK
            printf("%9d | %3d | ", arraySize, try + 1);
#endif
            fflush(stdout);
            i = tries[try];
#if MASK
            i &= 255;
#endif
            trueCount = counts[i & 255];
#if BENCHMARK == 1
            testStart = clock();
#elif BENCHMARK == 2
            testStart = rdtsc();
#endif
            testCount = memcnt(buf, i, arraySize);
#if BENCHMARK == 1
            testEnd = clock();
#elif BENCHMARK == 2
            testEnd = rdtsc();
#endif
            if (testCount == trueCount)
#if !BENCHMARK
                ;
#elif BENCHMARK == 1
                printf("OK     | %12ld clk\n", testEnd - testStart);
#elif BENCHMARK == 2
                printf("OK     | %12llu refcyc\n", testEnd - testStart);
#else
                puts("OK");
#endif
            else {
                puts("FAIL!");
                printf("Returned value (c=%8x=%2x): %lu\n", i, (unsigned char)i,
                       trueCount);
                printf("  Actual value (c=%8x=%2x): %lu\n", i, (unsigned char)i,
                       testCount);
                return 1;
            }
        }
#if !BENCHMARK
        puts("OK");
#endif
    }
    puts("ALL OK");
    return 0;
}
