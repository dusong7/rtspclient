/*
 * reactor.h
 *
 *  Created on: 2014年10月21日
 *      Author: wangzhen
 */

#ifndef REACTOR_H_
#define REACTOR_H_

#include <stdio.h>
#include <errno.h>

#ifndef _WIN32
typedef int SOCKET;
#define closesocket close
#else
#include <winsock2.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct reactor_st;
typedef struct reactor_st **reactor_t;
typedef struct reactor_fd_st *reactor_fd_t;
typedef enum {
	type_CLOSED = 0x00,
	type_NORMAL = 0x01,
	type_LISTEN = 0x02,
	type_CONNECT = 0x10,
	type_CONNECT_READ = 0x11,
	type_CONNECT_WRITE = 0x12,
	type_WAITING = 0x20
} reactor_fd_type_t;

typedef struct reactor_timer_st *reactor_timer_t;

typedef enum {action_ACCEPT, action_READ, action_WRITE, action_CLOSE } reactor_action_t;
typedef int (*reactor_handler)(reactor_t, reactor_action_t, reactor_fd_t fd, void *data, void *arg);
typedef int (*reactor_timer_handler)(reactor_t, reactor_timer_t, void *arg);

struct reactor_timer_st{
	reactor_timer_handler callback;
	void *arg;
};

struct reactor_fd_st {
	SOCKET fd;
	int type;
	reactor_handler app;
	void *arg;
};

struct reactor_st{
	void (*rt_free)(reactor_t);
	reactor_fd_t (*rt_listen)(reactor_t, int port, const char *local, reactor_handler, void *arg);
	reactor_fd_t (*rt_connect)(reactor_t, int port, const char *host, const char* source, reactor_handler, void *arg);
	reactor_fd_t (*rt_register)(reactor_t, int fd, int type, reactor_handler, void *arg);
	void (*rt_app)(reactor_t, reactor_fd_t fd, reactor_handler, void *arg);
	void (*rt_close)(reactor_t, reactor_fd_t fd);
	void (*rt_read)(reactor_t, reactor_fd_t fd);
	void (*rt_write)(reactor_t, reactor_fd_t fd);

	void (*rt_loop)(reactor_t);
	void (*rt_loop_once)(reactor_t, int mili_timeout);
	void (*rt_loop_break)(reactor_t);

	reactor_timer_t (*rt_timer_create)(reactor_t, reactor_timer_handler, void* arg);
	void (*rt_timer_free)(reactor_t, reactor_timer_t timer);
	void (*rt_timer_add)(reactor_t, reactor_timer_t timer, int timeout);
	void (*rt_timer_del)(reactor_t, reactor_timer_t timer);
	void (*rt_unread)(reactor_t, reactor_fd_t fd);
	const char* rt_type;
};

reactor_t reactor_create(const char* type);

#define reactor_listen(r,p,l,hl,a)      (*(r))->rt_listen(r,p,l,hl,a)
#define reactor_connect(r,p,h,s,hl,a)   (*(r))->rt_connect(r,p,h,s,hl,a)
#define reactor_register(r,f,t,h,a)     (*(r))->rt_register(r,f,t,h,a)
#define reactor_app(r,f,h,a)            (*(r))->rt_app(r,f,h,a)
#define reactor_close(r,f)              (*(r))->rt_close(r,f)
#define reactor_read(r,f)               (*(r))->rt_read(r,f)
#define reactor_unread(r,f)             (*(r))->rt_unread(r,f)
#define reactor_write(r,f)              (*(r))->rt_write(r,f)
#define reactor_loop(r)                 (*(r))->rt_loop(r)
#define reactor_loop_once(r, t)         (*(r))->rt_loop_once(r, t)
#define reactor_loop_break(r)           (*(r))->rt_loop_break(r)
#define reactor_timer_add(r,t,to)       (*(r))->rt_timer_add(r,t,to)
#define reactor_timer_del(r,t)          (*(r))->rt_timer_del(r,t)
#define reactor_timer_create(r,h,a)     (*(r))->rt_timer_create(r,h,a)
#define reactor_timer_free(r,t)         (*(r))->rt_timer_free(r,t)
#define reactor_free(r)         		(*(r))->rt_free(r)
#define reactor_type(r)					(*(r))->rt_type

#define REACTOR_SETERROR(e) (errno = e)
#ifndef _WIN32
#define REACTOR_WOULDBLOCK (errno == EWOULDBLOCK || errno == EINTR || errno == EAGAIN)
#define REACTOR_InProgress (errno == EINPROGRESS)
#define REACTOR_ConnRefused (errno == ECONNREFUSED)
#else
#undef errno
#define errno WSAGetLastError()
#define REACTOR_WOULDBLOCK (WSAGetLastError() == WSAEWOULDBLOCK)
#define REACTOR_InProgress (WSAGetLastError() == WSAEINPROGRESS)
#define REACTOR_ConnRefused (WSAGetLastError() == WSAECONNREFUSED)
#endif

#ifdef __cplusplus
}
#endif
#endif /* REACTOR_H_ */
