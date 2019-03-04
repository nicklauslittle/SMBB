
/**
Copyright (c) 2019 Nick Little

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef SMBB_TIME_H
#define SMBB_TIME_H

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

#include "utilities/IntegerTypes.h"

namespace smbb {

typedef uint64_t Time;

// Gets the time of the monotonic counter
Time GetMonotonicTime() {
#ifdef _WIN32
	LARGE_INTEGER value;
	(void)QueryPerformanceCounter(&value);
	return Time(value.QuadPart);
#else
	timespec value;

	if (clock_gettime(CLOCK_MONOTONIC, &value) != 0)
		return 0;

	return (uint64_t)value.tv_sec * 1000000000 + value.tv_nsec;
#endif
}

// Gets the frequency of the monotonic counter (Note: only call this once and cache the result for the best performance)
Time GetMonotonicFrequency() {
#ifdef _WIN32
	LARGE_INTEGER frequency;
	(void)QueryPerformanceFrequency(&frequency);
	return Time(frequency.QuadPart);
#else
	return 1000000000;
#endif
}

// Gets the UTC time in nanoseconds
Time GetUTCNs() {
#ifdef _WIN32
	const Time offset = Time(134774) /* days between 1601-01-01 and 1970-01-01 */ * 86400 /* seconds per day */ * 10000000;

	FILETIME winTime;
	GetSystemTimeAsFileTime(&winTime);
	return ((Time(winTime.dwHighDateTime) << 32) + winTime.dwLowDateTime - offset) * 100;
#else
	timespec value;

	if (clock_gettime(CLOCK_REALTIME, &value) != 0)
		return 0;

	return (uint64_t)value.tv_sec * 1000000000 + value.tv_nsec;
#endif
}

}

#endif
