/*
 * time.h
 *
 *  Created on: 2015年6月8日
 *      Author: WangZhen
 */

#ifndef VMS_MAIN_UTIL_INCLUDE_UTIL_TIME_H_
#define VMS_MAIN_UTIL_INCLUDE_UTIL_TIME_H_

#ifdef _WIN32
#include <time.h>
typedef long long int64_t;
#else
#include <sys/time.h>
#endif

typedef struct __VMS_TIME
{
	short int 	year;			// validCheck: year >= 2014
	signed char	month;			// 1 <= month <= 12
	signed char date;			// 1 <= date <= 31
	signed char hour;			// 0 <= hour <= 23
	signed char minute;			// 0 <= minute <= 59
	unsigned char milli_aux:2;
	unsigned char second   :6;	// 0 <= second <= 59
	unsigned char milli_sec;  	// 0 <= (milli_aux << 8 | milli_sec) <= 999
}VmsTime;

typedef const VmsTime *pVmsTime;
#ifdef _MSC_VER
#define vmstime_get_milli(t) ((t)->milli_aux << 8 | (t)->milli_sec)
#else
#define vmstime_get_milli(t) \
	({\
		register const pVmsTime __t = (t); \
		(__t->milli_aux << 8 | __t->milli_sec);\
	})
#endif
#define vmstime_set_milli(t, milli) \
	do{\
		register VmsTime *__t = (t); \
		register int __m = (milli);\
		__t->milli_aux = __m >> 8; \
		__t->milli_sec = __m;\
	}while(0)

/* jabberd2 Windows DLL */
#  define JABBERD2_API
#ifndef JABBERD2_API
# ifdef _WIN32
#  ifdef JABBERD2_EXPORTS
#   define JABBERD2_API  __declspec(dllexport)
#  else /* JABBERD2_EXPORTS */
#   define JABBERD2_API  __declspec(dllimport)
#  endif /* JABBERD2_EXPORTS */
# else /* _WIN32 */
#  define JABBERD2_API extern
# endif /* _WIN32 */
#endif /* JABBERD2_API */


#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
typedef long long vms_time_t;
#else
typedef time_t vms_time_t;
#endif

JABBERD2_API pVmsTime 		vms_now(VmsTime *t);
JABBERD2_API pVmsTime 		vms_time(VmsTime *t, vms_time_t *time);
JABBERD2_API pVmsTime		vms_ptime(const char* str, VmsTime *t);
JABBERD2_API const char*	vms_ftime(pVmsTime t, char* out, int len);
JABBERD2_API vms_time_t		vms_time2int(pVmsTime t);
JABBERD2_API void 			vms_time_add_date(VmsTime *p, int d);
JABBERD2_API pVmsTime 		vms_time_format_in(VmsTime *t, const char* str, const char* fmt);
JABBERD2_API const char* 	vms_time_format_out(pVmsTime t, const char* fmt, char* out, int len);
#ifdef _WIN32
enum {J_CLOCK_MONOTONIC, J_CLOCK_REALTIME};
JABBERD2_API void           j_gettimeofday(struct timeval *tv, void *not_used);
JABBERD2_API void			j_clock_gettime(int, struct timespec *ts);
#else
#define j_clock_gettime clock_gettime
#define j_gettimeofday gettimeofday
#define J_CLOCK_MONOTONIC CLOCK_MONOTONIC
#define J_CLOCK_REALTIME CLOCK_REALTIME
#endif

#ifdef __cplusplus
}
#endif
#endif /* VMS_MAIN_UTIL_INCLUDE_UTIL_TIME_H_ */
