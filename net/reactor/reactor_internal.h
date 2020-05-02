/*
 * reactor_internal.h
 *
 *  Created on: 2014年10月22日
 *      Author: wangzhen
 */

#ifndef REACTOR_INTERNAL_H_
#define REACTOR_INTERNAL_H_

struct reactor_priv_st
{
	struct reactor_st *r_impl;
	int defer_free;
	reactor_fd_t *defer_free_fds;
    size_t defer_free_fd_count;
};

#define ACT(r,f,a,d) (*(f->app))(r,a,f,d,f->arg)

reactor_fd_t   _rt_setup_fd(reactor_t r, SOCKET sock,
                        reactor_handler app, void *arg, reactor_fd_t (*allocator)(reactor_t, SOCKET));
reactor_fd_t   _rt_listen(reactor_t r, int port, const char *sourceip,
                        reactor_handler app, void *arg, reactor_fd_t (*allocator)(reactor_t, SOCKET));
reactor_fd_t   _rt_connect(reactor_t r, int port, const char *hostip, const char* srcip,
                        reactor_handler app, void *arg,
                        reactor_fd_t (*allocator)(reactor_t, SOCKET),
                        void (*set_write)(reactor_t, reactor_fd_t));
reactor_fd_t   _rt_register(reactor_t r, SOCKET fd, int type, reactor_handler app, void *arg,
                        reactor_fd_t (*allocator)(reactor_t, SOCKET));
void           _rt_app(reactor_t r, reactor_fd_t fd, reactor_handler app, void *arg);
void           _rt_close(reactor_t r, reactor_fd_t fd,
                        void (*remove_fd)(reactor_t, reactor_fd_t),
                        void (*free_fd)(reactor_t, reactor_fd_t));
void           _rt_read(reactor_t r, reactor_fd_t fd, void (*set_read)(reactor_t r, reactor_fd_t fd));
void           _rt_write(reactor_t r, reactor_fd_t fd, void (*set_write)(reactor_t r, reactor_fd_t fd));
void 		   _rt_accept(reactor_t r, reactor_fd_t fd,
						reactor_fd_t (*allocator)(reactor_t, SOCKET),
						void (*remove_fd)(reactor_t, reactor_fd_t),
						void (*free_fd)(reactor_t, reactor_fd_t));
void 		   _rt__connect(reactor_t r, reactor_fd_t fd, void(*unset_write)(reactor_t, reactor_fd_t));

#endif /* REACTOR_INTERNAL_H_ */
