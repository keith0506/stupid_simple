#ifndef SYS_CLCOK_H
#define SYS_CLCOK_H
#include <stdint.h>



#include <unistd.h>
#include <sys/time.h>

/* get system time */
static inline void itimeofday(long *sec, long *usec)
{
	/*#if defined(__unix)*/
	struct timeval time;
	gettimeofday(&time, NULL);
	if (sec) *sec = time.tv_sec;
	if (usec) *usec = time.tv_usec;
	/*
	#else
	static long mode = 0, addsec = 0;
	BOOL retval;
	static int64_t freq = 1;
	int64_t qpc;
	if (mode == 0) {
		retval = QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
		freq = (freq == 0)? 1 : freq;
		retval = QueryPerformanceCounter((LARGE_INTEGER*)&qpc);
		addsec = (long)time(NULL);
		addsec = addsec - (long)((qpc / freq) & 0x7fffffff);
		mode = 1;
	}
	retval = QueryPerformanceCounter((LARGE_INTEGER*)&qpc);
	retval = retval * 2;
	if (sec) *sec = (long)(qpc / freq) + addsec;
	if (usec) *usec = (long)((qpc % freq) * 1000000 / freq);
	#endif*/
}

/* get clock in millisecond 64 */
static inline int64_t iclock64(void)
{
	long s, u;
	int64_t value;
	itimeofday(&s, &u);
	value = ((int64_t)s) * 1000 + (u / 1000);
	return value;
}

static inline uint32_t iclock()
{
	return (uint32_t)(iclock64() & 0xfffffffful);
}
#include "quic_clock.h"
#include "quic_time.h"
namespace quic{
class SysClock:public QuicClock{
public:
    SysClock(){}
    ~SysClock(){}
QuicTime ApproximateNow() const override{
    return Now();
}
QuicTime Now() const override{
int64_t now=iclock64();
quic::QuicTime time=quic::QuicTime::Zero() + quic::QuicTime::Delta::FromMilliseconds(now);
return time;
}
QuicWallTime WallNow() const override{
 int64_t time_since_unix_epoch_micro=iclock64()*1000;
 return QuicWallTime::FromUNIXMicroseconds(time_since_unix_epoch_micro);
}
};
}
#endif // SYS_CLCOK_H
