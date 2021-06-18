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
#include <string.h>
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
/* 2 = benchmark using CPU specific timers
                amd64/x86: rdtsc
                ARM: cntvct_el0 */
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
#define BATCH_COUNT 1
#define DIVIDER 1
#else
#define MIN_ARRAY_SIZE 0
#define MAX_ARRAY_SIZE 800000
#define ARRAY_MUL 3
#define TRY_COUNT CHAR_COUNT
#define MAX_TRY_COUNT CHAR_COUNT
#define BATCH_COUNT 5
#define DIVIDER 2
#endif

#if BENCHMARK == 2
#if defined(__i386__) || defined(__amd64__) || defined(_M_IX86) ||             \
    defined(_M_X64)
#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif
typedef unsigned long long cputime_t;
cputime_t getcputime(void) {
    cputime_t x;
    _mm_lfence();
    x = __rdtsc();
    _mm_lfence();
    return x;
}
#define UNIT_CYCLES 1
#elif defined(__aarch64__)
typedef unsigned long long cputime_t;
cputime_t getcputime(void) {
    cputime_t x;
    asm volatile("isb; mrs %0, cntvct_el0" : "=r"(x));
    return x;
}
long long getcpufreq(void) {
    unsigned long long x;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(x));
    return x;
}
#define UNIT_CYCLES 0
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

#if MAX_ARRAY_SIZE < 1000000
#define USE_MALLOC 0
#else
#define USE_MALLOC 1
#endif

#if USE_MALLOC
unsigned char *buf;
#else
unsigned char buf[MAX_ARRAY_SIZE];
#endif

static int rng_seed = 0;

static inline int rng(void) {
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

#if USE_MALLOC
static void free_buf(void) {
    free(buf);
}
#endif

int main(int argc, char *argv[]) {
    int t, i, tries[MAX_TRY_COUNT], counts[CHAR_COUNT], batchNum, tryCount;
    size_t arraySize, arraySizeIter, trueCount, testCount;
#if BENCHMARK == 1
    clock_t testStart, testEnd;
#elif BENCHMARK == 2
    cputime_t testStart, testEnd;
#if !UNIT_CYCLES
    unsigned long long clockFreq;
#endif
#elif BENCHMARK == 3
    struct timespec testStart, testEnd, testDiff;
#elif BENCHMARK
#error invalid BENCHMARK setting
#endif
#if USE_MALLOC
    buf = malloc(MAX_ARRAY_SIZE);
    if (!buf) {
        puts("Could not allocate the buffer for testing.");
        puts("Maybe MAX_ARRAY_SIZE is too big for your system?");
        return 1;
    }
    atexit(free_buf);
#endif
    srand(time(NULL));
    rng_seed = rand() ^ (rand() << 11) ^ (rand() << 23);
#if MEMCNT_C
    printf("Testing implementation '%s'\n", memcnt_impl_name_);
#else
    puts("Testing manually linked implementation");
#endif
#if BENCHMARK == 1
    puts("Benchmark: measuring approximate CPU time in microseconds");
#elif BENCHMARK == 2
#if UNIT_CYCLES
    puts("Benchmark: measuring (CPU) runtime in reference cycles");
#else
    puts("Benchmark: measuring (CPU) runtime in microseconds");
    clockFreq = getcpufreq();
#endif
#elif BENCHMARK == 3
    puts("Benchmark: measuring monotonic runtime in microseconds");
#elif !BENCHMARK
    arraySize = MAX_ARRAY_SIZE;
    memset(buf, UCHAR_MAX, arraySize);
    testCount = memcnt(buf, UCHAR_MAX, arraySize);
    puts("Running simple sanity tests");
    if (testCount != arraySize) {
        printf("Simple test failed! memcnt should have returned %zu for an\n"
               "array filled with the check value, but it returned %zu.\n"
               "Go fix it!\n", arraySize, testCount);
        return 1;
    }
    testCount = memcnt(buf, 0, arraySize);
    if (testCount != 0) {
        printf("Simple test failed! memcnt should have returned %zu for an\n"
               "array filled with some other value, but it returned %zu.\n"
               "Go fix it!\n", (size_t)0, testCount);
        return 1;
    }
    arraySize = 9000;
    puts("Running unaligned tests");
    for (i = 0; i < 256 && i < arraySize / 2; ++i) {
        testCount = memcnt(buf + i, UCHAR_MAX, arraySize - 2 * i);
        if (testCount != arraySize - 2 * i) {
            printf("1 (i,-i) i=%d SZ=%zu buf=%p\n", i, arraySize, buf);
            printf(
                "Unaligned test failed! memcnt should have returned %zu for an\n"
                "array filled with the check value, but it returned %zu.\n"
                "Go fix it!\n", arraySize - 2 * i, testCount);
            return 1;
        }
        
        testCount = memcnt(buf + i, UCHAR_MAX, arraySize - i);
        if (testCount != arraySize - i) {
            printf("1 (i,i) i=%d SZ=%zu buf=%p\n", i, arraySize, buf);
            printf(
                "Unaligned test failed! memcnt should have returned %zu for an\n"
                "array filled with the check value, but it returned %zu.\n"
                "Go fix it!\n", arraySize - i, testCount);
            return 1;
        }
        
        testCount = memcnt(buf, UCHAR_MAX, arraySize - i);
        if (testCount != arraySize - i) {
            printf("1 (0,-i) i=%d SZ=%zu buf=%p\n", i, arraySize, buf);
            printf(
                "Unaligned test failed! memcnt should have returned %zu for an\n"
                "array filled with the check value, but it returned %zu.\n"
                "Go fix it!\n", arraySize - i, testCount);
            return 1;
        }
        
        testCount = memcnt(buf + i, 0, arraySize - 2 * i);
        if (testCount != 0) {
            printf("0 (i,-i) i=%d SZ=%zu buf=%p\n", i, arraySize, buf);
            printf(
                "Unaligned test failed! memcnt should have returned %zu for an\n"
                "array filled with the check value, but it returned %zu.\n"
                "Go fix it!\n", (size_t)0, testCount);
            return 1;
        }
        
        testCount = memcnt(buf + i, 0, arraySize - i);
        if (testCount != 0) {
            printf("0 (i,i) i=%d SZ=%zu buf=%p\n", i, arraySize, buf);
            printf(
                "Unaligned test failed! memcnt should have returned %zu for an\n"
                "array filled with the check value, but it returned %zu.\n"
                "Go fix it!\n", (size_t)0, testCount);
            return 1;
        }
        
        testCount = memcnt(buf, 0, arraySize - i);
        if (testCount != 0) {
            printf("0 (0,-i) i=%d SZ=%zu buf=%p\n", i, arraySize, buf);
            printf(
                "Unaligned test failed! memcnt should have returned %zu for an\n"
                "array filled with the check value, but it returned %zu.\n"
                "Go fix it!\n", (size_t)0, testCount);
            return 1;
        }
    }
    puts("Running random stress tests");
#endif
    puts("");
#if BENCHMARK
    printf("%12s | %3s | %6s | %15s | %s\n", "Size", "Try", "Status", "Runtime",
           "Average Speed");
#else
    printf("%5s | %12s | %9s\n", "Batch", "Size", "Status");
#endif
    for (batchNum = 0; batchNum < BATCH_COUNT; ++batchNum) {

        for (arraySizeIter = MIN_ARRAY_SIZE;
             arraySizeIter <= MAX_ARRAY_SIZE * DIVIDER;
             arraySizeIter ? arraySizeIter *= ARRAY_MUL : ++arraySizeIter) {
            arraySize = arraySizeIter / DIVIDER;
            tryCount = arraySize == MAX_ARRAY_SIZE * DIVIDER ||
                               arraySize * ARRAY_MUL > MAX_ARRAY_SIZE * DIVIDER
                           ? MAX_TRY_COUNT
                           : TRY_COUNT;
            fill_array(arraySize, tryCount, tries, counts);

            i = 0;
#if !BENCHMARK
            printf("%5d | ", batchNum + 1);
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
                testStart = getcputime();
#elif BENCHMARK == 3
                clock_gettime(CLOCK_MONOTONIC, &testStart);
#endif
                testCount = memcnt(buf, i, arraySize);
#if BENCHMARK == 1
                testEnd = clock();
#elif BENCHMARK == 2
                testEnd = getcputime();
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
                        printf("%11.2f MB/s_CPU\n",
                               arraySize / ((double)interval / CLOCKS_PER_SEC) /
                                   1000000ULL);
                    else
                        puts("-");
#elif BENCHMARK == 2
#if UNIT_CYCLES
                    unsigned long long interval = testEnd - testStart;
                    printf("OK     | %12llu rc | ", interval);
                    if (arraySize > 10 && interval > 0)
                        printf("%6.2f B/rc\n", arraySize / (double)interval);
                    else
                        puts("-");
#else
                    unsigned long long interval =
                        (testEnd - testStart) * 1000000ULL / clockFreq;
                    printf("OK     | %12llu us | ", interval);
                    if (arraySize > 10)
                        printf("%6.2f MB/s_CPU\n",
                               arraySize / (double)interval);
                    else
                        puts("-");
#endif
#elif BENCHMARK == 3
                    unsigned long long interval =
                        (testDiff.tv_nsec + 999) / 1000;
                    interval += testDiff.tv_sec * 1000000ULL;
                    printf("OK     | ~%11llu us | ", interval);
                    if (arraySize > 10 && interval > 0)
                        printf("%11.2f MB/s\n",
                               arraySize / ((double)interval / CLOCKS_PER_SEC) /
                                   1000000ULL);
                    else
                        puts("-");
#endif
                } else {
                    puts("FAIL!");
                    printf("Returned value (c=%8x=%2x): %zu\n", i,
                           (unsigned char)i, trueCount);
                    printf("  Actual value (c=%8x=%2x): %zu\n", i,
                           (unsigned char)i, testCount);
                    return 1;
                }
            }
#if !BENCHMARK
            puts("OK");
#endif
        }
    }
    puts("ALL OK");
#if !BENCHMARK
    puts("Now try benchmarking... (define BENCHMARK with 1, 2 or 3)");
#endif
    return 0;
}
