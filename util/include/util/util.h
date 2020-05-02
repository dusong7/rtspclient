/*
 * jabberd - Jabber Open Source Server
 * Copyright (c) 2002 Jeremie Miller, Thomas Muldowney,
 *                    Ryan Eatmon, Robert Norris
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA02111-1307USA
 */
#ifndef INCL_UTIL_H
#define INCL_UTIL_H
#ifndef _MSC_VER
#include "stdint.h"
#else
#include <io.h>
#include <direct.h>
#define snprintf _snprintf
#define strcasecmp stricmp
#define strncasecmp strnicmp
#define chdir(d) _chdir(d)
#define access(x,y) _access(x,y)
#define sleep(s) Sleep((s) * 1000)
#define usleep(z) Sleep((z) / 1000)
#define nanosleep(n,z) Sleep(((n)->tv_nsec) / 1000000)
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <assert.h>

# include <sys/types.h>

#include <ctype.h>
#ifndef _MSC_VER
# include <unistd.h>
#endif
#ifndef _WIN32
# include <sys/time.h>
# include <sys/socket.h>
# include <syslog.h>
# include <netinet/in.h>
# include <arpa/inet.h>
#include <sys/un.h>
#else
// in win32
#if defined(__cplusplus) && defined(__MINGW__)
#undef __cplusplus
extern "C"{
#include <winsock2.h> // mingw bugs exists when used by c++
}
#define __cplusplus 1
#else
#include <winsock2.h>
#endif
#include <ws2tcpip.h>
#endif

#ifndef PATH_MAX
#ifndef MAXPATHLEN
# define PATH_MAX 512
#else
# define PATH_MAX MAXPATHLEN
#endif
#endif

#ifndef NULL
#define NULL (void*)0
#endif

#include "util/util_compat.h"
#include <util/time.h>

/* jabberd2 Windows DLL */
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

/* crypto hashing utils */
#include "sha1.h"
#include "md5.h"

#include <util/pool.h>
#include <util/xhash.h>

JABBERD2_API unsigned int j_fnv_32a_str_c(const char *str, int len);

/* --------------------------------------------------------- */
/*                                                           */
/* String management routines                                */
/*                                                           */
/** --------------------------------------------------------- */
JABBERD2_API char *j_strdup(const char *str); /* provides NULL safe strdup wrapper */
JABBERD2_API char *j_strcat(char *dest, char *txt); /* strcpy() clone */
JABBERD2_API int j_strcmp(const char *a, const char *b); /* provides NULL safe strcmp wrapper */
JABBERD2_API int j_strcasecmp(const char *a, const char *b); /* provides NULL safe strcasecmp wrapper */
JABBERD2_API int j_strncmp(const char *a, const char *b, int i); /* provides NULL safe strncmp wrapper */
JABBERD2_API int j_strncasecmp(const char *a, const char *b, int i); /* provides NULL safe strncasecmp wrapper */
JABBERD2_API int j_strlen(const char *a); /* provides NULL safe strlen wrapper */
JABBERD2_API int j_atoi(const char *a, int def); /* checks for NULL and uses default instead, convienence */
JABBERD2_API char *j_attr(const char** atts, const char *attr); /* decode attr's (from expat) */
JABBERD2_API char *j_strnchr(const char *s, int c, int n); /* like strchr, but only searches n chars */
JABBERD2_API char *j_strndup(const char* str, int len);
JABBERD2_API char *j_strerror_r(int err, char dst[], int dstlen);
JABBERD2_API char *j_strtok_r(char *s, const char *delim, char **state);
JABBERD2_API void *j_memmem(const void *src, int len_src, const void *search, int len_s);
JABBERD2_API void *j_memdup(const void* src, int len);
JABBERD2_API void j_free(void*);
JABBERD2_API char* j_transcode(const char* src, int srclen, const char* src_locale, const char* dst_locale);
JABBERD2_API int j_atoi_ex(const char* a, int len, int def);
JABBERD2_API char* j_str_ltrim(const char* src);
JABBERD2_API char* j_str_rtrim(char* src);
#ifdef _MSC_VER
extern JABBERD2_API int opterr;        /* if error message should be printed */
extern JABBERD2_API int optind;        /* index into parent argv vector */
extern JABBERD2_API int optopt;            /* character checked for validity */
extern JABBERD2_API int optreset;        /* reset getopt */
extern JABBERD2_API char *optarg;        /* argument associated with option */
JABBERD2_API int getopt(int argc, char * const argv[], const char *optstring);
#define strtoll _strtoi64
#endif

#ifdef _WIN32
#define strerror_r j_strerror_r
#undef strtok_r
#define strtok_r j_strtok_r
#define memmem j_memmem
#undef rename
#define rename(x,y) MoveFileExA(x, y, MOVEFILE_REPLACE_EXISTING)
#undef localtime_r
#define localtime_r(x,y) localtime_s(y,x)
#elif !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#include <string.h>
#endif

/** old convenience function, now in str.c */
JABBERD2_API void shahash_r(const char* str, char hashbuf[41]);
JABBERD2_API void shahash_raw(const char* str, unsigned char hashval[20]);

/* --------------------------------------------------------- */
/*                                                           */
/* XML escaping utils                                        */
/*                                                           */
/* --------------------------------------------------------- */
JABBERD2_API char *strescape(pool_t p, char *buf, int len); /* Escape <>&'" chars */
JABBERD2_API char *strunescape(pool_t p, char *buf);


/* --------------------------------------------------------- */
/*                                                           */
/* String pools (spool) functions                            */
/*                                                           */
/* --------------------------------------------------------- */
struct spool_node
{
    char *c;
    struct spool_node *next;
};

typedef struct spool_struct
{
    pool_t p;
    int len;
    struct spool_node *last;
    struct spool_node *first;
} *spool;

JABBERD2_API spool spool_new(pool_t p); /* create a string pool */
JABBERD2_API void spooler(spool s, ...); /* append all the char * args to the pool, terminate args with s again */
JABBERD2_API char *spool_print(spool s); /* return a big string */
JABBERD2_API void spool_add(spool s, char *str); /* add a single string to the pool */
JABBERD2_API void spool_escape(spool s, char *raw, int len); /* add and xml escape a single string to the pool */
JABBERD2_API char *spools(pool_t p, ...); /* wrap all the spooler stuff in one function, the happy fun ball! */


/*
 * rate limiting
 */

typedef struct rate_st
{
    int             total;      /* if we exceed this many events */
    int             seconds;    /* in this many seconds */
    int             wait;       /* then go bad for this many seconds */

    time_t          time;       /* time we started counting events */
    int             count;      /* event count */

    time_t          bad;        /* time we went bad, or 0 if we're not */
} *rate_t;

JABBERD2_API rate_t      rate_new(int total, int seconds, int wait);
JABBERD2_API void        rate_free(rate_t rt);
JABBERD2_API void        rate_reset(rate_t rt);

/**
 * Add a number of events to the counter.  This takes care of moving
 * the sliding window, if we've moved outside the previous window.
 */
JABBERD2_API void        rate_add(rate_t rt, int count);

/**
 * @return The amount of events we have left before we hit the rate
 *         limit.  This could be number of bytes, or number of
 *         connection attempts, etc.
 */
JABBERD2_API int         rate_left(rate_t rt);

/**
 * @return 1 if we're under the rate limit and everything is fine or
 *         0 if the rate limit has been exceeded and we should throttle
 *         something.
 */
JABBERD2_API int         rate_check(rate_t rt);

/*
 * helpers for ip addresses
 */

#include "inaddr.h"        /* used in mio as well */

/*
 * serialisation helper functions
 */

JABBERD2_API int         ser_string_get(char **dest, int *source, const char *buf, int len);
JABBERD2_API int         ser_int_get(int *dest, int *source, const char *buf, int len);
JABBERD2_API void        ser_string_set(char *source, int *dest, char **buf, int *len);
JABBERD2_API void        ser_int_set(int source, int *dest, char **buf, int *len);

/*
 * priority queues
 */

typedef struct _jqueue_node_st  *_jqueue_node_t;
struct _jqueue_node_st {
    void            *data;

    int             priority;

    _jqueue_node_t  next;
    _jqueue_node_t  prev;
};

typedef struct _jqueue_st {
    pool_t          p;
    _jqueue_node_t  cache;

    _jqueue_node_t  front;
    _jqueue_node_t  back;

    int             size;
    int				cachesize;
    char            *key;
    time_t          init_time;
} *jqueue_t;

JABBERD2_API jqueue_t    jqueue_new(void);
JABBERD2_API void        jqueue_free(jqueue_t q);
JABBERD2_API void        jqueue_push(jqueue_t q, void *data, int pri);
JABBERD2_API void        *jqueue_pull(jqueue_t q);
JABBERD2_API void        *jqueue_peek(jqueue_t q);
JABBERD2_API int         jqueue_size(jqueue_t q);
JABBERD2_API time_t      jqueue_age(jqueue_t q);
JABBERD2_API void        jqueue_remove(jqueue_t q, void *data);
JABBERD2_API int 		 jqueue_locate(jqueue_t q, void *data);

JABBERD2_API jqueue_t    jqueue_new_safe();
JABBERD2_API void 		 *jqueue_pull_safe(jqueue_t q, int wait);
JABBERD2_API void		 jqueue_push_safe(jqueue_t q, void* data, int pri);
JABBERD2_API void 		 jqueue_cancel_wait(jqueue_t q);
JABBERD2_API int         jqueue_size_safe(jqueue_t q);
JABBERD2_API void        *jqueue_peek_safe(jqueue_t q);
JABBERD2_API void        jqueue_remove_safe(jqueue_t q, void *data);
JABBERD2_API int 		 jqueue_locate_safe(jqueue_t q, void *data);

/* ISO 8601 / JEP-0082 date/time manipulation */
typedef enum {
    dt_DATE     = 1,
    dt_TIME     = 2,
    dt_DATETIME = 3,
    dt_LEGACY   = 4
} datetime_t;

JABBERD2_API time_t  datetime_in(char *date);
JABBERD2_API void    datetime_out(time_t t, datetime_t type, char *date, int datelen);


/* base64 functions */
JABBERD2_API int apr_base64_decode_len(const char *bufcoded, int buflen);
JABBERD2_API int apr_base64_decode(char *bufplain, const char *bufcoded, int buflen);
JABBERD2_API int apr_base64_encode_len(int len);
JABBERD2_API int apr_base64_encode(char *encoded, const char *string, int len);

/* convenience, result string must be free()'d by caller */
JABBERD2_API char *b64_encode(const char *buf, int len);
JABBERD2_API char *b64_decode(const char *buf);
JABBERD2_API char *b64_decode_ex(const char *buf, int n);


/* hex conversion utils */
JABBERD2_API void hex_from_raw(const char *in, int inlen, char *out);
JABBERD2_API int hex_to_raw(const char *in, int inlen, char *out);
JABBERD2_API int j_uri_encode(const char* in, int inlen, char* out);
JABBERD2_API int j_uri_decode(const char* in, int inlen, char* out);

/* stats */
#include <pthread.h>
typedef struct stats_st{
	xht stats;
	int interval;
	pthread_mutex_t mutex;
}* stats_t;

JABBERD2_API void stats_free(stats_t s);
JABBERD2_API stats_t stats_new();
JABBERD2_API void stats_inc(stats_t s, const char *key);
JABBERD2_API void stats_dec(stats_t s, const char *key);
JABBERD2_API void stats_add(stats_t s, const char *key, unsigned long long add);
JABBERD2_API void stats_sub(stats_t s, const char *key, unsigned long long sub);
JABBERD2_API void stats_set(stats_t s, const char *key, unsigned long long ll_val);
JABBERD2_API unsigned long long stats_get(stats_t s, const char *key);
JABBERD2_API unsigned long long stats_getmax(stats_t s, const char *key);
JABBERD2_API void stats_dump(stats_t s);
JABBERD2_API void stats_clear(stats_t s, const char* key);
JABBERD2_API void stats_delete(stats_t s, const char* key);

/* * array manipulating * */
typedef struct J_array_st{
	void **zen;
	int size;
	int capacity;
	pool_t p;
}* jarray_t;

JABBERD2_API jarray_t jarray_new();
JABBERD2_API jarray_t jarray_new_p(pool_t p);
JABBERD2_API void jarray_clear(jarray_t arr);
JABBERD2_API void jarray_free(jarray_t arr);
JABBERD2_API void jarray_push(jarray_t arr, void *value);
JABBERD2_API int  jarray_find(jarray_t arr, void *value);
JABBERD2_API void jarray_push_at(jarray_t arr, int pos, void *value);
JABBERD2_API void *jarray_get(jarray_t arr, int pos);
JABBERD2_API void jarray_put(jarray_t arr, int pos, void *value);
JABBERD2_API void *jarray_pop_at(jarray_t arr, int pos);
JABBERD2_API int  jarray_size(jarray_t arr);

/**
 * iterator for array
 */
#define jarray_foreach(arr, item, Type) \
	int __i##item, __n##item = jarray_size(arr); Type item;\
	for(__i##item=0; (__i##item < __n##item ? (item = (Type)jarray_get((arr), __i##item), 1) : 0); ++__i##item)

#ifdef DEBUG
/* debug logging */
JABBERD2_API int get_debug_flag(void);
JABBERD2_API void set_debug_flag(int v);
#else
#define get_debug_flag(...) 0
#define set_debug_flag(...) do{}while(0)
#endif
JABBERD2_API void debug_log(const char *file, const char* func, int line, const char *msgfmt, ...);

#define ZONE __FILE__, __FUNCTION__, __LINE__
#define MAX_DEBUG 8192

/* if no debug, basically compile it out */
#ifdef DEBUG
#define log_debug(fmt,...) if(get_debug_flag()) debug_log(ZONE, fmt, ##__VA_ARGS__)
#else
#define log_debug(fmt,...) if(0) debug_log(ZONE, fmt, ##__VA_ARGS__)
#endif

/* Portable signal function */
typedef void jsighandler_t(int);
JABBERD2_API jsighandler_t* jabber_signal(int signo,  jsighandler_t *func);

#ifdef _WIN32
/* Windows service wrapper function */
typedef int (jmainhandler_t)(int argc, char** argv);
JABBERD2_API int jabber_wrap_service(int argc, char** argv, jmainhandler_t *wrapper, LPCSTR name, LPCSTR display, LPCSTR description, LPCSTR depends);
#define JABBER_MAIN(name, display, description, depends) jabber_main(int argc, char** argv); \
                    main(int argc, char** argv) { return jabber_wrap_service(argc, argv, jabber_main, name, display, description, depends); } \
                    jabber_main(int argc, char** argv)
#else /* _WIN32 */
#define JABBER_MAIN(name, display, description, depends) int main(int argc, char** argv)
#endif /* _WIN32 */

#ifdef __cplusplus
}
#endif

#if XML_MAJOR_VERSION > 1
/* XML_StopParser is present in expat 2.x */
#define HAVE_XML_STOPPARSER
#endif

#endif    /* INCL_UTIL_H */


