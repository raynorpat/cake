
// **********************************************
//        Windows high performance counter
// **********************************************
//lifted from r1q2 :)

// this version outputs to the debugger window
// instead of to the console log. 

//#include "q_shared.h"

#ifdef _WIN32
#ifdef _DEBUG

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <stdio.h>

LARGE_INTEGER start;
double totalTime = 0;

void _START_PERFORMANCE_TIMER (void)
{
	QueryPerformanceCounter (&start);
}

void _STOP_PERFORMANCE_TIMER (void)
{
	double res;
	LARGE_INTEGER stop;
	__int64 diff;
	LARGE_INTEGER freq;
	char string[64];

	QueryPerformanceCounter (&stop);
	QueryPerformanceFrequency (&freq);
	diff = stop.QuadPart - start.QuadPart;
	res = ((double)((double)diff / (double)freq.QuadPart));
	sprintf(string, "Function executed in %.6f secs.\n", res);
	OutputDebugString(string);
//	Com_Printf (string);
	totalTime += res;
}
#endif
#endif

