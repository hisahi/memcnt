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

/* useful for testing.
   this file includes memcnt.c and thus should be compiled on its own */

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "memcnt.h"

#if !defined(MEMCNT_C) || MEMCNT_C
#define MEMCNT_DEBUG 1
#include "memcnt.c"
#endif

/* mask &0xFF before giving to function. the implementation should work
   regardless of whether this is 0 or 1, but this might help with debugging */
#ifndef MASK
#define MASK 0
#endif

/* 0 = no benchmark */
/* 1 = benchmark using clock() (CPU time) */
/* 2 = benchmark using x86 rdtsc */
/* 3 = benchmark using clock_gettime() (monotonic time) */
#ifndef BENCHMARK
#define BENCHMARK 0
#endif

#define CHAR_COUNT (1 << CHAR_BIT)

#if BENCHMARK
#define MIN_ARRAY_SIZE 0
#if LARGE
#define MAX_ARRAY_SIZE 1000000000
#else
#define MAX_ARRAY_SIZE 100000000
#endif
#define ARRAY_MUL 10
#define TRY_COUNT 3
#define MAX_TRY_COUNT 6
#else
#define MIN_ARRAY_SIZE 0
#define MAX_ARRAY_SIZE 531441
#define ARRAY_MUL 3
#define TRY_COUNT CHAR_COUNT
#define MAX_TRY_COUNT CHAR_COUNT
#endif

#if BENCHMARK == 2
#if defined(__i386__) || defined(__amd64__) || defined(_M_IX86) || defined(_M_X64)
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
#elif defined(__aarch64__)
typedef long long rdtsc_t;
rdtsc_t rdtsc() {
    rdtsc_t x;
    asm volatile("mrs %0, cntvct_el0" : "=r"(x));
    return x;
}
#else
#error BENCHMARK=2 not supported on your platform
#endif
#endif

#if BENCHMARK == 3
static inline void timediff(struct timespec *r, struct timespec *a,
                            struct timespec *b) {
    if (b->tv_nsec < a->tv_nsec) {
        r->tv_sec = b->tv_sec - a->tv_sec - 1;
        r->tv_nsec = b->tv_nsec - a->tv_nsec + 1000000000LL;
    } else {
        r->tv_sec = b->tv_sec - a->tv_sec;
        r->tv_nsec = b->tv_nsec - a->tv_nsec;
    }
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
    int t, i, tries[MAX_TRY_COUNT], counts[CHAR_COUNT];
    size_t arraySize, trueCount, testCount;
#if BENCHMARK == 1
    clock_t testStart, testEnd;
#elif BENCHMARK == 2
    rdtsc_t testStart, testEnd;
#elif BENCHMARK == 3
    struct timespec testStart, testEnd, testDiff;
#endif
    srand(time(NULL));
    rng_seed = rand() ^ (rand() << 11) ^ (rand() << 23);
#if MEMCNT_C
    printf("Testing implementation '%s'\n", memcnt_impl_name_);
#else
    printf("Testing manually linked implementation");
#endif
#if BENCHMARK == 1
    puts("Benchmark: measuring approximate CPU time in microseconds");
#elif BENCHMARK == 2
    puts("Benchmark: measuring runtime in reference cycles");
#elif BENCHMARK == 3
    puts("Benchmark: measuring monotonic runtime in microseconds");
#endif
    puts("");
#if BENCHMARK
    printf("%12s | %3s | %6s | %15s | %s\n", "Size", "Try", "Status", "Runtime",
           "Average Speed");
#else
    printf("%12s | %9s\n", "Size", "Status");
#endif
    for (arraySize = MIN_ARRAY_SIZE; arraySize <= MAX_ARRAY_SIZE;
         arraySize ? arraySize *= ARRAY_MUL : ++arraySize) {
        int tryCount = arraySize == MAX_ARRAY_SIZE ||
                               arraySize * ARRAY_MUL > MAX_ARRAY_SIZE
                           ? MAX_TRY_COUNT
                           : TRY_COUNT;
        fill_array(arraySize, tryCount, tries, counts);

        i = 0;
#if !BENCHMARK
        printf("%12zu | ", arraySize);
#endif
        for (t = 0; t < tryCount; ++t) {
#if BENCHMARK
            printf("%12zu | %3d | ", arraySize, t + 1);
#endif
            fflush(stdout);
            i = tries[t];
#if MASK
            i &= 255;
#endif
            trueCount = counts[i & 255];
#if BENCHMARK == 1
            testStart = clock();
#elif BENCHMARK == 2
            testStart = rdtsc();
#elif BENCHMARK == 3
            clock_gettime(CLOCK_MONOTONIC, &testStart);
#endif
            testCount = memcnt(buf, i, arraySize);
#if BENCHMARK == 1
            testEnd = clock();
#elif BENCHMARK == 2
            testEnd = rdtsc();
#elif BENCHMARK == 3
            clock_gettime(CLOCK_MONOTONIC, &testEnd);
            timediff(&testDiff, &testStart, &testEnd);
#endif
            if (testCount == trueCount) {
#if !BENCHMARK
#elif BENCHMARK == 1
                unsigned long long interval = testEnd - testStart;
                printf("OK     | ~%11llu us | ",
                       interval * 1000000ULL / CLOCKS_PER_SEC);
                if (arraySize > 10 && interval > 0)
                    printf("%11.2f MB/s\n",
                           arraySize / ((double)interval / CLOCKS_PER_SEC) /
                               1000000ULL);
                else
                    puts("-");
#elif BENCHMARK == 2
                printf("OK     | %12llu rc | ", testEnd - testStart);
                if (arraySize > 10)
                    printf("%6.2f B/rc\n",
                           arraySize / (double)(testEnd - testStart));
                else
                    puts("-");
#elif BENCHMARK == 3
                unsigned long long interval = (testDiff.tv_nsec + 999) / 1000;
                interval += testDiff.tv_sec * 1000000ULL;
                printf("OK     | ~%11llu us | ", interval);
                if (arraySize > 10 && interval > 0)
                    printf("%11.2f MB/s\n",
                           arraySize / ((double)interval / CLOCKS_PER_SEC) /
                               1000000ULL);
                else
                    puts("-");
#else
                puts("OK");
#endif
            } else {
                puts("FAIL!");
                printf("Returned value (c=%8x=%2x): %zu\n", i, (unsigned char)i,
                       trueCount);
                printf("  Actual value (c=%8x=%2x): %zu\n", i, (unsigned char)i,
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
