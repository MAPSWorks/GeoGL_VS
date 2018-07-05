#include <stdint.h>
#include <Windows.h>
//#include <unistd.h>
#include <ctime>

#include "time.h"



// Ref: https://stackoverflow.com/questions/5404277/porting-clock-gettime-to-windows
LARGE_INTEGER
getFILETIMEoffset()
{
	SYSTEMTIME s;
	FILETIME f;
	LARGE_INTEGER t;

	s.wYear = 1970;
	s.wMonth = 1;
	s.wDay = 1;
	s.wHour = 0;
	s.wMinute = 0;
	s.wSecond = 0;
	s.wMilliseconds = 0;
	SystemTimeToFileTime(&s, &f);
	t.QuadPart = f.dwHighDateTime;
	t.QuadPart <<= 32;
	t.QuadPart |= f.dwLowDateTime;
	return (t);
}

int clock_gettime(int X, struct timeval *tv)
{
	LARGE_INTEGER           t;
	FILETIME            f;
	double                  microseconds;
	static LARGE_INTEGER    offset;
	static double           frequencyToMicroseconds;
	static int              initialized = 0;
	static BOOL             usePerformanceCounter = 0;

	if (!initialized) {
		LARGE_INTEGER performanceFrequency;
		initialized = 1;
		usePerformanceCounter = QueryPerformanceFrequency(&performanceFrequency);
		if (usePerformanceCounter) {
			QueryPerformanceCounter(&offset);
			frequencyToMicroseconds = (double)performanceFrequency.QuadPart / 1000000.;
		}
		else {
			offset = getFILETIMEoffset();
			frequencyToMicroseconds = 10.;
		}
	}
	if (usePerformanceCounter) QueryPerformanceCounter(&t);
	else {
		GetSystemTimeAsFileTime(&f);
		t.QuadPart = f.dwHighDateTime;
		t.QuadPart <<= 32;
		t.QuadPart |= f.dwLowDateTime;
	}

	t.QuadPart -= offset.QuadPart;
	microseconds = (double)t.QuadPart / frequencyToMicroseconds;
	t.QuadPart = microseconds;
	tv->tv_sec = t.QuadPart / 1000000;
	tv->tv_usec = t.QuadPart % 1000000;
	return (0);
}




uint64_t get_time() {
	const uint64_t NSPS = 1000000000;

	//struct timespec {  __time_t tv_sec;    long int tv_nsec;  };  -- total 8 bytes
	struct timeval ts;

	// CLOCK_REALTIME - system wide real time clock
	clock_gettime(0, &ts);

	// to 8 byte     from   4 byte
	uint64_t uli_usec = static_cast<uint64_t>(ts.tv_usec);
	uint64_t uli_sec  = static_cast<uint64_t>(ts.tv_sec);

	uint64_t total_ns = uli_usec*1000 + (uli_sec * NSPS);

	return total_ns;
 }