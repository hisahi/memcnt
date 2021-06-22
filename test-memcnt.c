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

#if IMPORT
#define MEMCNT_C 0
#define MEMCNT_IMPORT 1
#endif
#include "memcnt.h"

#if !defined(MEMCNT_C) || MEMCNT_C
#define MEMCNT_DEBUG 1
#include "memcnt.c"
#endif

#define MAX_ARRAY_SIZE 100000000
#define LARGE_ARRAY_THRESHOLD_MS 100

/* mask &0xFF before giving to function. the implementation should work
   regardless of whether this is 0 or 1, but this might help with debugging */
#ifndef MASK
#define MASK 0
#endif

#if defined(__has_include) && !defined(HAVE_UNISTD_H)
#if __has_include(<unistd.h>)
#define HAVE_UNISTD_H 1
#endif
#endif

#if (HAVE_UNISTD_H || defined(__unix__) || defined(__unix) ||                  \
     defined(__QNX__) || (defined(__APPLE__) && defined(__MACH__)))
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif
#include <unistd.h>
#define IS_POSIX 1
#else
#define IS_POSIX 0
#endif

#if defined(__APPLE__) && defined(__MACH__)
#define IS_MACH 1
#endif

#if defined(_WIN32) || defined(_WIN64)
#define IS_WINDOWS 1
#include <Windows.h>
#endif

#define CHAR_COUNT (1 << CHAR_BIT)

#if (defined(__i386__) || defined(__amd64__) || defined(_M_IX86) ||            \
     defined(_M_X64)) &&                                                       \
    (defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(_MSC_VER))
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
#define BM2_OK 1
#define BM2_METHOD "x86 rdtsc"
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
#define BM2_OK 1
#define BM2_METHOD "ARM cntvct_el0"
#else
typedef int cputime_t;
#define BM2_OK 0
#define BM2_METHOD "(none)"
cputime_t getcputime(void) { return 0; }
#endif

#if PAPI
#include <papi.h>
#define BM4_OK 1
int papi_evt[] = {PAPI_TOT_CYC};
#define PAPI_EVENTS_COUNT (sizeof(papi_evt) / sizeof(papi_evt[0]))
long long papi_val[PAPI_EVENTS_COUNT] = {0};
void bm4_start(void) {
    int err = PAPI_start_counters(papi_evt, PAPI_EVENTS_COUNT);
    if (err != PAPI_OK) {
        printf("\nPAPI_start_counters error 0x%08x (%s)\n"
               "Try checking papi_avail and papi_component_avail\n",
               err, PAPI_strerror(err));
        abort();
    }
}
void bm4_stop(void) {
    int err = PAPI_stop_counters(papi_val, PAPI_EVENTS_COUNT);
    if (err != PAPI_OK) {
        printf("\nPAPI_stop_counters error 0x%08x (%s)\n", err,
               PAPI_strerror(err));
        abort();
    }
}
void bm4_init(void) {
    bm4_start();
    bm4_stop();
}
unsigned long long bm4_get_cyclecount(void) {
    return (unsigned long long)papi_val[0];
}
#else
#define BM4_OK 0
void bm4_init(void) {}
void bm4_start(void) {}
void bm4_stop(void) {}
unsigned long long bm4_get_cyclecount(void) { return 0; }
#endif

#if IS_WINDOWS
#define BM3_TYPE LARGE_INTEGER
#define BM3_OK 1
#define BM3_METHOD "Win32 QueryPerformanceCounter"
BM3_TYPE timefreq;
static inline void timeinit() {
    if (!QueryPerformanceFrequency(&timefreq)) {
        printf("\nQueryPerformanceFrequency error 0x%08x\n", GetLastError());
        abort();
    }
}
static inline void timecapture(BM3_TYPE *r) {
    if (!QueryPerformanceCounter(r)) {
        printf("\nQueryPerformanceCounter error 0x%08x\n", GetLastError());
        abort();
    }
}
static inline void timediff(BM3_TYPE *r, const BM3_TYPE *a, const BM3_TYPE *b) {
    r->QuadPart = b->QuadPart - a->QuadPart;
}
static inline unsigned long long timegetus(const BM3_TYPE *r) {
    return r->QuadPart * 1000000ULL / timefreq.QuadPart;
}
#elif IS_MACH
#define BM3_TYPE uint64_t
#define BM3_OK 1
#define BM3_METHOD "POSIX/Apple clock_gettime_nsec_np(CLOCK_UPTIME_RAW)"
static inline void timeinit() {}
static inline void timecapture(BM3_TYPE *r) {
    *r = clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
}
static inline void timediff(BM3_TYPE *r, const BM3_TYPE *a, const BM3_TYPE *b) {
    *r = *b - *a;
}
static inline unsigned long long timegetus(const BM3_TYPE *r) {
    return *r / 1000;
}
#elif IS_POSIX
#define BM3_TYPE struct timespec
#define BM3_OK 1
#define BM3_METHOD "POSIX clock_gettime(CLOCK_MONOTONIC)"
static inline void timeinit() {}
static inline void timecapture(BM3_TYPE *r) {
    clock_gettime(CLOCK_MONOTONIC, r);
}
static inline void timediff(BM3_TYPE *r, const BM3_TYPE *a, const BM3_TYPE *b) {
    if (b->tv_nsec < a->tv_nsec) {
        r->tv_sec = b->tv_sec - a->tv_sec - 1;
        r->tv_nsec = b->tv_nsec - a->tv_nsec + 1000000000LL;
    } else {
        r->tv_sec = b->tv_sec - a->tv_sec;
        r->tv_nsec = b->tv_nsec - a->tv_nsec;
    }
}
static inline unsigned long long timegetus(const BM3_TYPE *r) {
    unsigned long long interval = (r->tv_nsec + 999) / 1000;
    interval += r->tv_sec * 1000000ULL;
    return interval;
}
#else
#define BM3_TYPE int
#define BM3_OK 0
#define BM3_METHOD "(none)"
static inline void timeinit() {}
static inline void timecapture(BM3_TYPE *r) { (void)r; }
static inline void timediff(BM3_TYPE *r, const BM3_TYPE *a, const BM3_TYPE *b) {
    (void)r;
    (void)a;
    (void)b;
}
static inline unsigned long long timegetus(const BM3_TYPE *r) { return 0; }
#endif

#define TEST_ARRAY_SIZE 800000
unsigned char buf_test[TEST_ARRAY_SIZE];
unsigned char *buf;

static int rng_seed = 0;

static inline int rng(void) {
    rng_seed = (1784265361 * rng_seed + 252197837);
    return (rng_seed >> 7) & 0xFFFFFF;
}

void fill_array(int arraySize, int tryCount, int *tries, int *counts,
                int benchmark) {
    int i;
    for (i = 0; i < CHAR_COUNT; ++i)
        counts[i] = 0;
    if (benchmark)
        for (i = 0; i < tryCount; ++i)
            tries[i] = rng();
    else
        for (i = 0; i < tryCount; ++i)
            tries[i] = i;

    /* fill array */
    if (arraySize > MAX_ARRAY_SIZE) {
        unsigned char j = (unsigned char)rng();
        unsigned char k = (unsigned char)rng() | 1;
        for (i = 0; i < arraySize; ++i)
            ++counts[(unsigned char)(buf[i] = (j += k))];
    } else
        for (i = 0; i < arraySize; ++i)
            ++counts[(unsigned char)(buf[i] = rng())];
}

static void free_buf(void) { free(buf); }

int main(int argc, char *argv[]) {
    int t, i, tries[CHAR_COUNT], counts[CHAR_COUNT], batchNum, tryCount;
    size_t arraySize, arraySizeIter, trueCount, testCount;
    clock_t testStart_bm1, testEnd_bm1;
    cputime_t testStart_bm2, testEnd_bm2;
    unsigned long long clockFreq, maxArraySize;
    BM3_TYPE testStart_bm3, testEnd_bm3, testDiff_bm3;

    int benchmark = 0, batchCount, divider, normTryCount, maxTryCount,
        minArraySize = 0, arrayMul, bm_large = 0;
    for (i = 1; i < argc; ++i) {
        if (*argv[i] == '-') {
            const char *c = argv[i] + 1;
            if (*c == 'b') {
                ++c;
                benchmark = atoi(c);
            } else if (*c == '-') {
                break;
            }
        }
    }
    batchCount = benchmark ? 1 : 5;
    divider = benchmark ? 1 : 2;
    normTryCount = benchmark ? 3 : CHAR_COUNT;
    arrayMul = benchmark ? 10 : 3;
    maxTryCount = benchmark ? 6 : CHAR_COUNT;
    maxArraySize = benchmark ? MAX_ARRAY_SIZE : TEST_ARRAY_SIZE;
#if MEMCNT_C
#if MEMCNT_DYNAMIC
    puts("Testing implementation resolved by dynamic dispatch");
    memcnt_optimize();
    printf("    --> '%s'\n", memcnt_impl_name_);
#else
    printf("Testing implementation '%s'\n", memcnt_impl_name_);
#endif
#elif IMPORT
    puts("Testing dynamically linked implementation");
#else
    puts("Testing manually statically linked implementation");
#endif
    if (benchmark == 1) {
        puts("Benchmark: measuring approximate CPU time in microseconds");
        puts("              method: C clock()");
    } else if (benchmark == 2) {
#if !BM2_OK
        puts("Benchmark 2 not supported on this build");
        return 1;
#else
#if UNIT_CYCLES
        puts("Benchmark: measuring (CPU) runtime in reference cycles");
        puts("              method: x86 RDTSC");
#else
        puts("Benchmark: measuring (CPU) runtime in microseconds");
        puts("              method: " BM2_METHOD);
        clockFreq = getcpufreq();
#endif
#endif
    } else if (benchmark == 3) {
#if !BM3_OK
        puts("Benchmark 3 not supported on this build");
        return 1;
#else
        puts("Benchmark: measuring system monotonic runtime in microseconds");
        puts("              method: " BM3_METHOD);
#endif
        timeinit();
    } else if (benchmark == 4) {
#if !BM4_OK
        puts("Benchmark 4 not supported on this build (must define PAPI=1)");
        return 1;
#else
        puts("Benchmark: measuring perf counters via PAPI");
#endif
        bm4_init();
    } else if (benchmark) {
        puts("Invalid benchmark setting");
        return 2;
    }
    if (benchmark) {
        buf = malloc(maxArraySize * arrayMul);
        if (!buf)
            buf = malloc(maxArraySize);
        if (!buf) {
            puts("Could not allocate the buffer for testing.");
            puts("Maybe MAX_ARRAY_SIZE is too big for your system?");
            return 1;
        }
        atexit(free_buf);
    } else {
        buf = buf_test;
    }
    srand(time(NULL));
    rng_seed = rand() ^ (rand() << 11) ^ (rand() << 23);

    if (!benchmark) {
        puts("Running simple sanity tests");
        arraySize = TEST_ARRAY_SIZE;
        memset(buf, UCHAR_MAX, arraySize);
        testCount = memcnt(buf, UCHAR_MAX, arraySize);
        if (testCount != arraySize) {
            printf(
                "Simple test failed! memcnt should have returned %zu for an\n"
                "array filled with the check value, but it returned %zu.\n"
                "Go fix it!\n",
                arraySize, testCount);
            return 1;
        }
        testCount = memcnt(buf, 0, arraySize);
        if (testCount != 0) {
            printf(
                "Simple test failed! memcnt should have returned %zu for an\n"
                "array filled with some other value, but it returned %zu.\n"
                "Go fix it!\n",
                (size_t)0, testCount);
            return 1;
        }
        testCount = memcnt(NULL, 0, 0);
        if (testCount != 0) {
            printf("Simple test failed! memcnt should have returned %zu for a\n"
                   "NULL array with n=0, but it returned %zu.\n"
                   "Go fix it!\n",
                   (size_t)0, testCount);
            return 1;
        }
        arraySize = 9000;
        puts("Running unaligned tests");
        for (i = 0; i < 256 && i < arraySize / 2; ++i) {
            testCount = memcnt(buf + i, UCHAR_MAX, arraySize - 2 * i);
            if (testCount != arraySize - 2 * i) {
                printf("1 (i,-i) i=%d SZ=%zu buf=%p\n", i, arraySize, buf);
                printf(
                    "Unaligned test failed! memcnt should have returned %zu "
                    "for an\n"
                    "array filled with the check value, but it returned %zu.\n"
                    "Go fix it!\n",
                    arraySize - 2 * i, testCount);
                return 1;
            }

            testCount = memcnt(buf + i, UCHAR_MAX, arraySize - i);
            if (testCount != arraySize - i) {
                printf("1 (i,i) i=%d SZ=%zu buf=%p\n", i, arraySize, buf);
                printf(
                    "Unaligned test failed! memcnt should have returned %zu "
                    "for an\n"
                    "array filled with the check value, but it returned %zu.\n"
                    "Go fix it!\n",
                    arraySize - i, testCount);
                return 1;
            }

            testCount = memcnt(buf, UCHAR_MAX, arraySize - i);
            if (testCount != arraySize - i) {
                printf("1 (0,-i) i=%d SZ=%zu buf=%p\n", i, arraySize, buf);
                printf(
                    "Unaligned test failed! memcnt should have returned %zu "
                    "for an\n"
                    "array filled with the check value, but it returned %zu.\n"
                    "Go fix it!\n",
                    arraySize - i, testCount);
                return 1;
            }

            testCount = memcnt(buf + i, 0, arraySize - 2 * i);
            if (testCount != 0) {
                printf("0 (i,-i) i=%d SZ=%zu buf=%p\n", i, arraySize, buf);
                printf(
                    "Unaligned test failed! memcnt should have returned %zu "
                    "for an\n"
                    "array filled with the check value, but it returned %zu.\n"
                    "Go fix it!\n",
                    (size_t)0, testCount);
                return 1;
            }

            testCount = memcnt(buf + i, 0, arraySize - i);
            if (testCount != 0) {
                printf("0 (i,i) i=%d SZ=%zu buf=%p\n", i, arraySize, buf);
                printf(
                    "Unaligned test failed! memcnt should have returned %zu "
                    "for an\n"
                    "array filled with the check value, but it returned %zu.\n"
                    "Go fix it!\n",
                    (size_t)0, testCount);
                return 1;
            }

            testCount = memcnt(buf, 0, arraySize - i);
            if (testCount != 0) {
                printf("0 (0,-i) i=%d SZ=%zu buf=%p\n", i, arraySize, buf);
                printf(
                    "Unaligned test failed! memcnt should have returned %zu "
                    "for an\n"
                    "array filled with the check value, but it returned %zu.\n"
                    "Go fix it!\n",
                    (size_t)0, testCount);
                return 1;
            }
        }
        puts("Running random stress tests");
    }
    if (benchmark)
        printf("%12s | %3s | %6s | %15s | %s\n", "Size", "Try", "Status",
               "Runtime", "Average Speed");
    else
        printf("%5s | %12s | %9s\n", "Batch", "Size", "Status");
    for (batchNum = 0; batchNum < batchCount; ++batchNum) {

        for (arraySizeIter = minArraySize;
             arraySizeIter <= maxArraySize * divider;
             arraySizeIter ? arraySizeIter *= arrayMul : ++arraySizeIter) {
            arraySize = arraySizeIter / divider;
            tryCount = arraySize == maxArraySize * divider ||
                               arraySize * arrayMul > maxArraySize * divider
                           ? maxTryCount
                           : normTryCount;
            fill_array(arraySize, tryCount, tries, counts, benchmark);

            i = 0;
            if (!benchmark) {
                printf("%5d | ", batchNum + 1);
                printf("%12zu | ", arraySize);
            }
            for (t = 0; t < tryCount; ++t) {
                if (benchmark)
                    printf("%12zu | %3d | ", arraySize, t + 1);
                fflush(stdout);
                i = tries[t];
#if MASK
                i &= 255;
#endif
                trueCount = counts[i & 255];
                if (benchmark == 1)
                    testStart_bm1 = clock();
                else if (benchmark == 2)
                    testStart_bm1 = clock(), testStart_bm2 = getcputime();
                else if (benchmark == 3)
                    timecapture(&testStart_bm3);
                else if (benchmark == 4)
                    testStart_bm1 = clock(), bm4_start();
                testCount = memcnt(buf, i, arraySize);
                if (benchmark == 1)
                    testEnd_bm1 = clock();
                else if (benchmark == 2)
                    testEnd_bm2 = getcputime(), testEnd_bm1 = clock();
                else if (benchmark == 3) {
                    timecapture(&testEnd_bm3);
                    timediff(&testDiff_bm3, &testStart_bm3, &testEnd_bm3);
                } else if (benchmark == 4)
                    bm4_stop(), testEnd_bm1 = clock();
                if (testCount == trueCount) {
                    if (benchmark == 1) {
                        unsigned long long interval =
                            testEnd_bm1 - testStart_bm1;
                        printf("OK     | ~%11llu us | ",
                               interval * 1000000ULL / CLOCKS_PER_SEC);
                        if (arraySize > 10 && interval > 0)
                            printf("%11.2f MB/s_CPU\n",
                                   arraySize /
                                       ((double)interval / CLOCKS_PER_SEC) /
                                       1000000ULL);
                        else
                            puts("-");
                        bm_large = interval * 1000000ULL / CLOCKS_PER_SEC <
                                   LARGE_ARRAY_THRESHOLD_MS * 1000ULL;
                    } else if (benchmark == 2) {
                        unsigned long long interval_bm1 =
                            testEnd_bm1 - testStart_bm1;
#if UNIT_CYCLES
                        unsigned long long interval =
                            testEnd_bm2 - testStart_bm2;
                        printf("OK     | %12llu rc | ", interval);
                        if (arraySize > 10 && interval > 0)
                            printf("%6.2f B/rc\n",
                                   arraySize / (double)interval);
                        else
                            puts("-");
                        bm_large = interval_bm1 * 1000000ULL / CLOCKS_PER_SEC <
                                   LARGE_ARRAY_THRESHOLD_MS * 1000ULL;
#else
                        unsigned long long interval =
                            (testEnd_bm2 - testStart_bm2) * 1000000ULL /
                            clockFreq;
                        printf("OK     | %12llu us | ", interval);
                        if (arraySize > 10 && interval > 0)
                            printf("%6.2f MB/s_CPU\n",
                                   arraySize / (double)interval);
                        else
                            puts("-");
                        bm_large =
                            interval < LARGE_ARRAY_THRESHOLD_MS * 1000ULL;
#endif
                    } else if (benchmark == 3) {
                        unsigned long long interval = timegetus(&testDiff_bm3);
                        printf("OK     | ~%11llu us | ", interval);
                        if (arraySize > 10 && interval > 0)
                            printf("%11.2f MB/s\n",
                                   arraySize / (double)interval);
                        else
                            puts("-");
                        bm_large =
                            interval < LARGE_ARRAY_THRESHOLD_MS * 1000ULL;
                    } else if (benchmark == 4) {
                        unsigned long long interval_bm1 =
                            testEnd_bm1 - testStart_bm1;
                        unsigned long long cycles = bm4_get_cyclecount();
                        printf("OK     | %11llu cyc | ", cycles);
                        if (arraySize > 10 && cycles > 0)
                            printf("%11.2f B/cyc\n",
                                   arraySize / (double)cycles);
                        else
                            puts("-");
                        bm_large = interval_bm1 * 1000000ULL / CLOCKS_PER_SEC <
                                   LARGE_ARRAY_THRESHOLD_MS * 1000ULL;
                    }
                } else {
                    puts("FAIL!");
                    printf("Returned value (c=%8x=%2x): %zu\n", i,
                           (unsigned char)i, testCount);
                    printf("  Actual value (c=%8x=%2x): %zu\n", i,
                           (unsigned char)i, trueCount);
                    return 1;
                }
            }
            if (!benchmark)
                puts("OK");
            if (benchmark && arraySizeIter == MAX_ARRAY_SIZE) {
                if (bm_large)
                    maxArraySize *= arrayMul;
                else
                    puts("Too slow, will not do large array benchmark");
            }
        }
    }
    puts("ALL OK");
    if (!benchmark)
        puts("Now try benchmarking... -b1-4");
    return 0;
}
