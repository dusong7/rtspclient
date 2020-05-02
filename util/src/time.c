/*
 * time.c
 *
 *  Created on: 2015年6月8日
 *      Author: WangZhen
 */

#define _GNU_SOURCE // for strptime
#include <time.h>

#include "pthread.h"
#include <util/time.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <Windows.h>
#define localtime_r(a,b) localtime_s(b,a)
#define snprintf _snprintf
#endif


pVmsTime vms_now_ex(VmsTime *t)
{
	struct tm tm;
	int milli;
#ifdef _MSC_VER
	struct timeval tv;
	time_t tt;
	j_gettimeofday(&tv, NULL);
	tt = tv.tv_sec;
	localtime_s(&tm, &tt);
	milli = tv.tv_usec / 1000;
#else
	struct timespec tp;
	clock_gettime(CLOCK_REALTIME, &tp);
	localtime_r(&tp.tv_sec, &tm);
	milli = tp.tv_nsec / 1000000;
#endif
	t->year = tm.tm_year + 1900;
	t->month = tm.tm_mon + 1;
	t->date = tm.tm_mday;
	t->hour = tm.tm_hour;
	t->minute = tm.tm_min;
	t->second = tm.tm_sec;

	t->milli_sec = milli;
	t->milli_aux = milli >> 8;

	return t;
}
pVmsTime vms_time_ex(VmsTime *t, struct timeval *tv)
{
	struct tm tm;
	time_t tt = tv->tv_sec;
	localtime_r(&tt, &tm);
	t->year = tm.tm_year + 1900;
	t->month = tm.tm_mon + 1;
	t->date = tm.tm_mday;
	t->hour = tm.tm_hour;
	t->minute = tm.tm_min;
	t->second = tm.tm_sec;
	vmstime_set_milli(t, tv->tv_usec / 1000);
	return t;
}

pVmsTime vms_now(VmsTime *t)
{
	struct tm tm;
#ifdef _WIN32
	__time64_t now;
	_time64(&now);
	_localtime64_s(&tm, &now);
#else
	time_t now;
	time(&now);
	localtime_r(&now, &tm);
#endif
	t->year = tm.tm_year + 1900;
	t->month = tm.tm_mon + 1;
	t->date = tm.tm_mday;
	t->hour = tm.tm_hour;
	t->minute = tm.tm_min;
	t->second = tm.tm_sec;
	t->milli_sec = 0;
	t->milli_aux = 0;

	return t;
}
pVmsTime vms_time(VmsTime *t, vms_time_t *time)
{
	struct tm tm;
#ifdef _WIN32
	_localtime64_s(&tm, time);
#else
	time_t myt = *time;
	localtime_r(&myt, &tm);
#endif
	t->year = tm.tm_year + 1900;
	t->month = tm.tm_mon + 1;
	t->date = tm.tm_mday;
	t->hour = tm.tm_hour;
	t->minute = tm.tm_min;
	t->second = tm.tm_sec;
	t->milli_sec = 0;
	t->milli_aux = 0;
	return t;
}

pVmsTime vms_ptime(const char* str, VmsTime *t)
{
	int year, month, day, hour = 0, minute = 0, second = 0;
	int milli = 0;
	int n = 0;
	// check string minimal length as format "2000-1-1"
	if(!str || strlen(str) < 8){
		memset(t, 0, sizeof(*t));
		return NULL;
	}

	do{
		if(str[4] >= '0' && str[4] <= '9') {
			n = sscanf(str, "%02d%02d%02d%02d%04d.%02d",
					&month, &day, &hour, &minute, &year, &second);
			if(n == 6)
				break;
			n = sscanf(str, "%04d%02d%02d%02d%02d%02d.%03d",
					&year, &month, &day, &hour, &minute, &second, &milli);
			if(n >= 6)
				break;
		} else {
			n = sscanf(str, "%04d%*c%02d%*c%02d%*[./ -]%02d:%02d:%02d.%03d",
					&year, &month, &day, &hour, &minute, &second, &milli);
			if(n >= 3){
				break;
			}
		}
		// finally, we can not parse it.
		memset(t, 0, sizeof(*t));
		return NULL;
	}while(0);

	t->year = year;
	t->month = month;
	t->date = day;
	t->hour = hour;
	t->minute = minute;
	t->second = second;	
	t->milli_sec = milli;
	t->milli_aux = milli >> 8;
	return t;
}

const char* vms_ftime(pVmsTime t, char* out, int len)
{
	int milli = vmstime_get_milli(t);
	if(milli)
		snprintf(out, len, "%04d%02d%02d%02d%02d%02d.%03d",
				t->year, t->month, t->date, t->hour, t->minute, t->second, milli);
	else
		snprintf(out, len, "%04d%02d%02d%02d%02d%02d",
				t->year, t->month, t->date, t->hour, t->minute, t->second);
	return out;
}

const char* vms_time_format_out(pVmsTime t, const char* fmt, char* out, int len){
	struct tm tm;
	time_t timer = vms_time2int(t);
	localtime_r(&timer, &tm);
	strftime(out, len, fmt, &tm);
	return out;
}
pVmsTime vms_time_format_in(VmsTime *t, const char* str, const char* fmt){
	struct tm tm;
	strptime(str, fmt, &tm);
	t->year = tm.tm_year + 1900;
	t->month = tm.tm_mon + 1;
	t->date = tm.tm_mday;
	t->hour = tm.tm_hour;
	t->minute = tm.tm_min;
	t->second = tm.tm_sec;
	t->milli_aux = 0;
	t->milli_sec = 0;

	return t;
}

////////////////
static char dayInMonth[2][13] = {
		{0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
		{0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
};

static int isleapyear(int y)
{
	if (y % 4)
		return 0;
	if (y % 100 == 0 && y % 400)
		return 0;
	return 1;
}

void vms_time_add_date(VmsTime *p, int d)
{
	p->date += d;
	if(d < 0){
		while(p->date <= 0){
			-- p->month;
			if(p->month <= 0) {
				p->month = 12;
				-- p->year;
			}
			p->date += dayInMonth[isleapyear(p->year)][p->month];
		}
		return;
	}
	while(p->date > dayInMonth[isleapyear(p->year)][p->month]){
		p->date -= dayInMonth[isleapyear(p->year)][p->month];
		++ p->month;
		if(p->month > 12){
			p->month = 1;
			++ p->year;
		}
	}
}

vms_time_t vms_time2int(pVmsTime t)
{
	struct tm tm = {0};
	tm.tm_year = t->year - 1900;
	tm.tm_mon = t->month - 1;
	tm.tm_mday = t->date;
	tm.tm_hour = t->hour;
	tm.tm_min = t->minute;
	tm.tm_sec = t->second;
	tm.tm_isdst = -1;
#ifdef _WIN32
	return _mktime64(&tm);
#else
	return mktime(&tm);
#endif
}
#ifdef _WIN32
void j_gettimeofday(struct timeval *tv, void *not_used)
{
	SYSTEMTIME st;
	FILETIME ft;
	LARGE_INTEGER li;
	static LARGE_INTEGER base_time;  
	static char get_base_time_flag=0;  
#define SECS_TO_FT_MULT 10000000 
	if (get_base_time_flag == 0)  
	{  
		SYSTEMTIME st;  
		FILETIME ft;  

		memset(&st,0,sizeof(st));  
		st.wYear=1970;  
		st.wMonth=1;  
		st.wDay=1;  
		SystemTimeToFileTime(&st, &ft);  

		base_time.LowPart = ft.dwLowDateTime;  
		base_time.HighPart = ft.dwHighDateTime;  
		base_time.QuadPart /= SECS_TO_FT_MULT;   
	}  

	GetSystemTime(&st);
	SystemTimeToFileTime(&st, &ft);

	li.LowPart = ft.dwLowDateTime;  
	li.HighPart = ft.dwHighDateTime;  
	li.QuadPart /= SECS_TO_FT_MULT;  
	li.QuadPart -= base_time.QuadPart;  

	tv->tv_sec = li.LowPart;  
	tv->tv_usec = st.wMilliseconds * 1000; 
}
void j_clock_gettime(int type, struct timespec *ts)
{
	switch(type){
		case J_CLOCK_MONOTONIC:
		{
			clock_t c = clock();
			ts->tv_sec = c / CLOCKS_PER_SEC;
			ts->tv_nsec = (unsigned long long)(c % CLOCKS_PER_SEC) * 1000000000 / CLOCKS_PER_SEC;
			break;
		}
		case J_CLOCK_REALTIME:
		{
			struct timeval tv;
			j_gettimeofday(&tv, NULL);
			ts->tv_sec = tv.tv_sec;
			ts->tv_nsec = tv.tv_usec * 1000;
			break;
		}
	}
}
#endif
