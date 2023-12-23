/*
#ifndef gettime_h
#define gettime_h

#ifdef __APPLE__
typedef enum
{
	CLOCK_REALTIME,
	CLOCK_MONOTONIC,
	CLOCK_PROCESS_CPUTIME_ID,
	CLOCK_THREAD_CPUTIME_ID
} clockid_t;

int clock_gettime(clockid_t clk_id, struct timespec *tp);
#endif

#endif
*/

#ifndef gettime_h
#define gettime_h

#ifdef __APPLE__
#include <Availability.h>

// 只有在目标 macOS 版本低于 10.12 版本的情况下才定义
#if __MAC_OS_X_VERSION_MIN_REQUIRED < __MAC_10_12

typedef enum {
    CLOCK_REALTIME,
    CLOCK_MONOTONIC,
    CLOCK_PROCESS_CPUTIME_ID,
    CLOCK_THREAD_CPUTIME_ID
} clockid_t;

int clock_gettime(clockid_t clk_id, struct timespec *tp);

#endif // __MAC_OS_X_VERSION_MIN_REQUIRED < __MAC_10_12

#endif // __APPLE__

#endif // gettime_h
