/*
 * event.c
 * providing reactor_event: use libevent to do the jobs
 *
 *  Created on: 2014年10月21日
 *      Author: wangzhen
 */

#include <stdio.h>
#include <event2/event.h>
#include "reactor.h"
#include "reactor_internal.h"
#include <stdlib.h> // for calloc
#include <errno.h>
#ifndef _MSC_VER
#include <unistd.h> // usleep
#else
#define usleep(z) Sleep((z) / 1000)
#endif
struct reactor_priv_st;
struct reactor_event_st
{
	struct reactor_priv_st base;
	struct event_base *eb;
	struct event 	*ev_loop, *ev_freeing;
	int 			internal_eb;
	int 			loop_breaking;
	reactor_fd_t 	*defer_fds;
	int 			defer_fd_count;
};

struct reactor_event_fd_st
{
	struct reactor_fd_st fd;
	reactor_t r;
	struct event *ev_read, *ev_write;
};

struct reactor_event_timer_st
{
	struct reactor_timer_st proc;
	struct event *ev;
	reactor_t r;
};


/*一下三个函数很巧妙的利用了结构体的第一个元素地址和结构体地址相同*/
#define R(r) ((struct reactor_event_st *)r)
#define FD(r,f) ((struct reactor_event_fd_st *)f)
#define T(r,t) ((struct reactor_event_timer_st *)t)

static reactor_fd_t _fd_allocator(reactor_t r, SOCKET fd){
	struct reactor_event_fd_st *rfd = calloc(1, sizeof(struct reactor_event_fd_st));
	if(!rfd)return NULL;
	rfd->fd.fd = fd;
	rfd->r = r;
	rfd->ev_write = event_new(R(r)->eb, -1, 0, NULL, rfd);// creat write event
	rfd->ev_read = event_new(R(r)->eb, -1, 0, NULL, rfd);// creat write event,then you should config it with event_add/event_assign
	return (reactor_fd_t)rfd;
}

static void _fd_remove(reactor_t r, reactor_fd_t fd)
{
	event_del(FD(r, fd)->ev_read);
	event_del(FD(r, fd)->ev_write);
}

static void _delayed_fd_free(evutil_socket_t fd, short flag, void *arg)
{
	reactor_t r = (reactor_t)arg;
	int count = R(r)->defer_fd_count;
	while(-- count >= 0){
		reactor_fd_t fd = R(r)->defer_fds[count];
		event_free(FD(r, fd)->ev_read);
		event_free(FD(r, fd)->ev_write);
		free(FD(r, fd));
	}
	R(r)->defer_fd_count = 0;
	free(R(r)->defer_fds);
	R(r)->defer_fds = NULL;
}

static void _fd_free(reactor_t r, reactor_fd_t fd){
	int count = R(r)->defer_fd_count;
	struct timeval tv = {0};
	R(r)->defer_fds = (reactor_fd_t*)realloc(R(r)->defer_fds, (count + 1) * sizeof(fd));
	R(r)->defer_fds[count] = fd;
	R(r)->defer_fd_count ++;
	event_add(R(r)->ev_freeing, &tv);
}
static void _rt_event_can_read(evutil_socket_t fd, short flag, void *arg);
static void _rt_event_can_write(evutil_socket_t fd, short flag, void *arg);
static void _fd_set_read(reactor_t r, reactor_fd_t fd){
	event_del(FD(r, fd)->ev_read);
	event_assign(FD(r, fd)->ev_read, R(r)->eb, fd->fd, EV_READ|EV_PERSIST, _rt_event_can_read, fd);
	event_add(FD(r, fd)->ev_read, NULL);
}

static void _fd_set_write(reactor_t r, reactor_fd_t fd){
	event_del(FD(r, fd)->ev_write);
	event_assign(FD(r, fd)->ev_write, R(r)->eb, fd->fd, EV_WRITE, _rt_event_can_write, fd);
	event_add(FD(r, fd)->ev_write, NULL);
}

static void _fd_unset_read(reactor_t r, reactor_fd_t fd){
	event_del(FD(r, fd)->ev_read);
}

static void _fd_unset_write(reactor_t r, reactor_fd_t fd){
	event_del(FD(r, fd)->ev_write);
}

static void _rt_event_can_read(evutil_socket_t fd, short flag, void *arg)
{
	reactor_t r = (((struct reactor_event_fd_st*)arg))->r;
	reactor_fd_t rfd = (reactor_fd_t) arg;
	if(rfd->type == type_CLOSED)
		return;

    /* new conns on a listen socket */
    if(rfd->type == type_LISTEN)
    {
    	if(rfd->app){
    		_rt_accept(r, rfd, _fd_allocator, _fd_remove, _fd_free);
    	}
        return;
    }

    /* check for connecting sockets */
    if(rfd->type & type_CONNECT)
    {
        _rt__connect(r, rfd, _fd_unset_write);
        return;
    }

    /* read from ready sockets */
    if(rfd->type == type_NORMAL)
    {
		void *data = NULL;
        if(ACT(r, rfd, action_READ, data) == 0){
        	/* if they don't want to read any more right now */
            _fd_unset_read(r, rfd);
        }
    }
}
static void _rt_event_can_write(evutil_socket_t fd, short flag, void *arg)
{
	reactor_t r = (((struct reactor_event_fd_st*)arg))->r;
	reactor_fd_t rfd = (reactor_fd_t) arg;
	if(rfd->type == type_CLOSED)
		return;

    /* check for connecting sockets */
    if(rfd->type & type_CONNECT)
    {
        _rt__connect(r, rfd, _fd_unset_write);
        return;
    }

    /* read from ready sockets */
    if(rfd->type == type_NORMAL)
    {
        /* don't wait for writeability if nothing to write anymore */
        if(ACT(r, rfd, action_WRITE, NULL) == 0) return;

        _fd_set_write(r, rfd);
    }
}

static void _r_event_free(reactor_t r){
	event_del(R(r)->ev_loop);
	event_free(R(r)->ev_loop);
	event_del(R(r)->ev_freeing);
	event_free(R(r)->ev_freeing);
	_delayed_fd_free(-1, 0, r);
	if(R(r)->internal_eb){
		event_base_free(R(r)->eb);
	}
	free(r);
}

static void _dummy(evutil_socket_t fd, short f, void* arg){}
static void _dummy_persistent(evutil_socket_t fd, short f, void* arg){
	struct timeval tv = {86400, 0};
	event_add(arg, &tv);
}

static void _r_event_loop(reactor_t r){
	int ret;
	struct timeval tv = {86400, 0};
	R(r)->loop_breaking = 0;
	if(!R(r)->internal_eb){
		fprintf(stderr, "AssertionFailed, never call reactor_loop series functions when using external event_base");
		abort();
	}
	event_del(R(r)->ev_loop);
	event_assign(R(r)->ev_loop, R(r)->eb, -1, 0, _dummy_persistent, R(r)->ev_loop);
	event_add(R(r)->ev_loop, &tv);
	do{
		ret = event_base_loop(R(r)->eb, 0);
		usleep(10000);
	}while(ret == 1 && 0 == R(r)->loop_breaking);
}

static void _r_event_looponce(reactor_t r, int mili_timeout)
{
	/*if(!R(r)->internal_eb){
		fprintf(stderr, "AssertionFailed, never call reactor_loop series functions when using external event_base");
		abort();
	}*/
	if(mili_timeout > 0){
		struct timeval tv = {0};
		if(mili_timeout > 1000){
			tv.tv_sec = mili_timeout / 1000;
			mili_timeout %= 1000;
		}
		tv.tv_usec = mili_timeout * 1000;
		event_del(R(r)->ev_loop);
		event_assign(R(r)->ev_loop, R(r)->eb, -1, 0, _dummy, R(r)->ev_loop);
		event_add(R(r)->ev_loop, &tv);
		event_base_loop(R(r)->eb, EVLOOP_ONCE);
		event_del(R(r)->ev_loop);
	} else if(mili_timeout < 0){
		event_base_loop(R(r)->eb, EVLOOP_ONCE);
	} else {
		event_base_loop(R(r)->eb, EVLOOP_ONCE | EVLOOP_NONBLOCK);
	}
}

static void _r_event_loopbreak(reactor_t r)
{
	R(r)->loop_breaking = 1;
	event_base_loopbreak(R(r)->eb);
}

static reactor_fd_t _r_event_listen(reactor_t r, int port, const char *sourceip,
		reactor_handler app, void *arg)
{
	return _rt_listen(r, port, sourceip, app, arg, _fd_allocator);
}

static reactor_fd_t _r_event_connect(reactor_t r, int port, const char* dest,
		const char *local, reactor_handler app, void *arg)
{
	return _rt_connect(r, port, dest, local, app, arg, _fd_allocator, _fd_set_write);
}

static reactor_fd_t _r_event_register(reactor_t r, int fd, int type, reactor_handler app, void *arg)
{
	return _rt_register(r, fd, type, app, arg, _fd_allocator);
}
static void _r_event_close(reactor_t r, reactor_fd_t fd){
	_rt_close(r, fd, _fd_remove, _fd_free);
}
static void _r_event_read(reactor_t r, reactor_fd_t fd)
{
	_rt_read(r, fd, _fd_set_read);
}
static void _r_event_unread(reactor_t r, reactor_fd_t fd){
	_fd_unset_read(r, fd);
}
static void _r_event_write(reactor_t r, reactor_fd_t fd)
{
	_rt_write(r, fd, _fd_set_write);
}

static void _rt_event_timeout(evutil_socket_t fd, short flag, void *arg)
{
	reactor_timer_t timer = (reactor_timer_t)arg;
	timer->callback(T(NULL, timer)->r, timer, timer->arg);
}

static reactor_timer_t _r_event_timer_create(reactor_t r, reactor_timer_handler app, void* arg)
{
	struct reactor_event_timer_st * timer = calloc(1, sizeof(struct reactor_event_timer_st));
	timer->proc.callback = app;
	timer->proc.arg = arg;
	timer->r = r;
	timer->ev = event_new(R(r)->eb, -1, 0, _rt_event_timeout, timer);
	return (reactor_timer_t)timer;
}

static void _r_event_timer_add(reactor_t r, reactor_timer_t t, int milisecond)
{
	struct timeval tv = {0};
	if(milisecond > 1000){
		tv.tv_sec = milisecond / 1000;
		milisecond %= 1000;
	}
	tv.tv_usec = milisecond * 1000;
	event_add(T(r,t)->ev, &tv);
}

static void _r_event_timer_del(reactor_t r, reactor_timer_t t)
{
	event_del(T(r, t)->ev);
}

static void _r_event_timer_free(reactor_t r, reactor_timer_t t)
{
	event_free(T(r, t)->ev);
	free(t);
}

reactor_t reactor_event_create(struct event_base *eb)
{
#ifdef _MSC_VER
	static struct reactor_st r_impl = {
		_r_event_free,
		_r_event_listen,
		_r_event_connect,
		_r_event_register,//(reactor_t, int fd, int type, reactor_handler, void *arg);
		_rt_app, //(reactor_t, reactor_fd_t fd, reactor_handler, void *arg);
		_r_event_close, //(reactor_t, reactor_fd_t fd);
		_r_event_read,
		_r_event_write,

		_r_event_loop,//(reactor_t);
		_r_event_looponce, //(reactor_t, int mili_timeout);
		_r_event_loopbreak,//(reactor_t);

		_r_event_timer_create,//(reactor_t, reactor_timer_handler, void* arg);
		_r_event_timer_free,//(reactor_t, reactor_timer_t timer);
		_r_event_timer_add,//(reactor_t, reactor_timer_t timer, int milisecond);
		_r_event_timer_del, //(reactor_t, reactor_timer_t timer);*/
		_r_event_unread,
		"event"
	};
#else
    static struct reactor_st r_impl = {
    	.rt_free = _r_event_free,
    	.rt_listen = _r_event_listen,
    	.rt_connect = _r_event_connect,
    	.rt_register = _r_event_register,//(reactor_t, int fd, int type, reactor_handler, void *arg);
    	.rt_app = _rt_app, //(reactor_t, reactor_fd_t fd, reactor_handler, void *arg);
    	.rt_close = _r_event_close, //(reactor_t, reactor_fd_t fd);
    	.rt_read = _r_event_read,
    	.rt_write = _r_event_write,

    	.rt_loop = _r_event_loop,//(reactor_t);
    	.rt_loop_once = _r_event_looponce, //(reactor_t, int mili_timeout);
    	.rt_loop_break = _r_event_loopbreak,//(reactor_t);

    	.rt_timer_create = _r_event_timer_create,//(reactor_t, reactor_timer_handler, void* arg);
    	.rt_timer_free = _r_event_timer_free,//(reactor_t, reactor_timer_t timer);
    	.rt_timer_add = _r_event_timer_add,//(reactor_t, reactor_timer_t timer, int milisecond);
    	.rt_timer_del = _r_event_timer_del, //(reactor_t, reactor_timer_t timer);*/
    	.rt_unread = _r_event_unread,
		.rt_type = "event"
    };
#endif

    struct reactor_event_st *r = calloc(1, sizeof(struct reactor_event_st));
    r->base.r_impl = &r_impl;
    if(eb){
    	r->eb = eb;
    	// external event_base
    } else {
    	r->eb = event_base_new();
		if(NULL == r->eb){
			abort();
		}
    	r->internal_eb = 1;
    }
    r->ev_loop = event_new(r->eb, -1, 0, _dummy, NULL);
    r->ev_freeing = event_new(r->eb, -1, 0, _delayed_fd_free, r);
    return (reactor_t)r;
}
