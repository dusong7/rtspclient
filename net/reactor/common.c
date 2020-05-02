/*
 * common.c
 *
 *  Created on: 2014年10月21日
 *      Author: wangzhen
 */

#include <stdio.h>
#include <fcntl.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif
#include <sys/types.h>
#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/ip.h>
#endif
#include <errno.h>
#include <string.h> // for memset

#include "reactor.h"
#include "reactor_internal.h"
#include "util/util.h"

static void _make_fd_nonblock(SOCKET fd)
{
#ifndef _WIN32
	int flags;
    /* set the socket to non-blocking */
    flags = fcntl(fd, F_GETFL);
    flags |= O_NONBLOCK;
    fcntl(fd, F_SETFL, flags);
#else
    unsigned long nbio = 1;
    ioctlsocket(fd, FIONBIO, &nbio);
#endif
}

reactor_fd_t _rt_setup_fd(reactor_t r, SOCKET sock,
		reactor_handler app, void *arg, reactor_fd_t (*allocator)(reactor_t, SOCKET))
{
    reactor_fd_t fd = allocator(r, sock);
	if(fd == NULL) return NULL;

    /* ok to process this one, welcome to the family */
    fd->type = type_NORMAL;
    fd->app = app;
    fd->arg = arg;
    _make_fd_nonblock(sock);
    return fd;
}

reactor_fd_t _rt_listen(reactor_t r, int port, const char *sourceip,
		reactor_handler app, void *arg, reactor_fd_t (*allocator)(reactor_t, SOCKET))
{
    SOCKET fd;
    int flag = 1;
    reactor_fd_t rfd;
    struct sockaddr_storage sa;

    if(r == NULL) return NULL;

    memset(&sa, 0, sizeof(sa));

    /* if we specified an ip to bind to */
    if(sourceip != NULL && !j_inet_pton(sourceip, &sa))
        return NULL;

    if(sa.ss_family == 0)
        sa.ss_family = AF_INET;

    /* attempt to create a socket */
    if((fd = socket(sa.ss_family, SOCK_STREAM,0)) < 0) return NULL;
    if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&flag, sizeof(flag)) < 0) return NULL;

    /* set up and bind address info */
    j_inet_setport(&sa, port);
    if(bind(fd,(struct sockaddr*)&sa,j_inet_addrlen(&sa)) < 0)
    {
        closesocket(fd);
        return NULL;
    }

    /* start listening with a max accept queue of 10 */
    if(listen(fd, 200) < 0)
    {
    	closesocket(fd);
        return NULL;
    }

    /* now set server's fd */
	/*here we will set fd's APP (is _bio_server_call in binary_io.c) , so when you read/write fd, */
	/*actually call _bio_server_call with param action_ACCEPT, action_READ, action_WRITE, action_CLOSE*/
	/*同时也将 fd r 配置到了rfd中----- _fd_allocator()*/
    rfd = _rt_setup_fd(r, fd, app, arg, allocator);
    if(rfd == NULL)
    {
    	closesocket(fd);
        return NULL;
    }
    rfd->type = type_LISTEN;//在read handler中会首先判断type
    /* by default we read for new sockets */
	
	/*同时为 rfd配置ev_read=_rt_event_can_read 并通过event_add进入待触发状态 */
    reactor_read(r, rfd);

    return rfd;
}

reactor_fd_t _rt_connect(reactor_t r, int port, const char *hostip, const char* srcip,
		reactor_handler app, void *arg,
		reactor_fd_t (*allocator)(reactor_t, SOCKET),
		void (*set_write)(reactor_t, reactor_fd_t))
{
    SOCKET fd;
    int ret;
    reactor_fd_t rfd;
    struct sockaddr_storage sa, src;

    memset(&sa, 0, sizeof(sa));

    if(r == NULL || port <= 0 || hostip == NULL) return NULL;

	/* convert the hostip */
	if(j_inet_pton(hostip, &sa)<=0) {
		return NULL;
	}

	if(!sa.ss_family) sa.ss_family = AF_INET;

    /* attempt to create a socket */
    if((fd = socket(sa.ss_family,SOCK_STREAM,0)) < 0) return NULL;

    /* Bind to the given source IP if it was specified */
    if (srcip != NULL) {
        /* convert the srcip */
        if(j_inet_pton(srcip, &src)<=0) {
            return NULL;
        }
        if(!src.ss_family) src.ss_family = AF_INET;
        j_inet_setport(&src, INADDR_ANY);
        if(bind(fd,(struct sockaddr*)&src,j_inet_addrlen(&src)) < 0) {
            closesocket(fd);
            return NULL;
        }
    }

    /* set the socket to non-blocking before connecting */
    _make_fd_nonblock(fd);

    /* set up address info */
    j_inet_setport(&sa, port);

    /* try to connect */
    ret = connect(fd,(struct sockaddr*)&sa,j_inet_addrlen(&sa));

    /* already connected?  great! */
    if(ret == 0)
    {
        rfd = _rt_setup_fd(r,fd,app,arg, allocator);
        if(rfd != NULL) return rfd;
    }

    /* gotta wait till later */
    if(ret == -1 && (REACTOR_InProgress || REACTOR_WOULDBLOCK))
    {
        rfd = _rt_setup_fd(r,fd,app,arg, allocator);
        if(rfd != NULL)
        {
            rfd->type = type_CONNECT;
            set_write(r, rfd);
            //MIO_SET_WRITE(r,FD(r,rfd));
            return rfd;
        }
    }

    /* bummer dude */
    closesocket(fd);
    return NULL;
}

reactor_fd_t _rt_register(reactor_t r, SOCKET fd, int type, reactor_handler app, void *arg,
		reactor_fd_t (*allocator)(reactor_t, SOCKET))
{
	reactor_fd_t rfd = _rt_setup_fd(r, fd, app, arg, allocator);
	if(rfd == NULL) return NULL;
	if(type == 1){
		rfd->type = type_LISTEN;
	    /* by default we read for new sockets */
	    reactor_read(r, rfd);
	}
	return rfd;
}

void _rt_app(reactor_t r, reactor_fd_t fd, reactor_handler app, void *arg)
{
	fd->app = app;
	fd->arg = arg;
}

void _rt_close(reactor_t r, reactor_fd_t fd,
		void (*remove_fd)(reactor_t, reactor_fd_t),
		void (*free_fd)(reactor_t, reactor_fd_t))
{
    if(fd->type == type_CLOSED)
        return;

    /* take out of poll sets */
    remove_fd(r, fd);

    /* let the app know, it must process any waiting write data it has and free it's arg */
    if (fd->app != NULL)
        ACT(r, fd, action_CLOSE, NULL);

    /* close the socket, and reset all memory */
    closesocket(fd->fd);
    fd->type = type_CLOSED;
    fd->app = NULL;
    fd->arg = NULL;

    if (!((struct reactor_priv_st*)r)->defer_free) {
    	free_fd(r, fd);
    } else {
    	register struct reactor_priv_st* rp = (struct reactor_priv_st*)r;
        rp->defer_free_fds = realloc(rp->defer_free_fds, (1 + rp->defer_free_fd_count) * (sizeof(reactor_fd_t *)));
        rp->defer_free_fds[rp->defer_free_fd_count] = fd;
        rp->defer_free_fd_count++;
    }
}

void _rt_read(reactor_t r, reactor_fd_t fd, void (*set_read)(reactor_t r, reactor_fd_t fd))
{
    if(r == NULL || fd == NULL) return;

    /* if connecting, do this later */
    if(fd->type & type_CONNECT)
    {
        fd->type |= type_CONNECT_READ;
        return;
    }

    set_read(r, fd);
}

void _rt_write(reactor_t r, reactor_fd_t fd, void (*set_write)(reactor_t r, reactor_fd_t fd))
{
    if(r == NULL || fd == NULL) return;

    /* if connecting, do this later */
    if(fd->type & type_CONNECT)
    {
        fd->type |= type_CONNECT_WRITE;
        return;
    }

    if(fd->type != type_NORMAL)
        return;

    if(ACT(r, fd, action_WRITE, NULL) == 0) return;

    set_write(r, fd);
}


/** internally accept an incoming connection from a listen sock */
void _rt_accept(reactor_t r, reactor_fd_t fd,
		reactor_fd_t (*allocator)(reactor_t, SOCKET),
		void (*remove_fd)(reactor_t, reactor_fd_t),
		void (*free_fd)(reactor_t, reactor_fd_t))
{
    struct sockaddr_storage serv_addr;
    socklen_t addrlen = (socklen_t) sizeof(serv_addr);
    SOCKET newfd;
    reactor_fd_t rfd;
    char ip[INET6_ADDRSTRLEN];

    /* pull a socket off the accept queue and check */
    newfd = accept(fd->fd, (struct sockaddr*)&serv_addr, &addrlen);
    if(newfd <= 0){
		if(errno == EMFILE || errno == ENFILE){
			/* file descriptors limited, we should drop it from event-listening to avoid busy loop. */
			//rt_accept_pause(r, fd);
		}
		return;
	}
    if(addrlen <= 0)
	{
        closesocket(newfd);
        return;
    }

    j_inet_ntop(&serv_addr, ip, sizeof(ip));

    /* set up the entry for this new socket */
	/*设置了新socket的event，并进入待触发状态 ->type=type_NORMAL*/
    rfd = _rt_setup_fd(r, newfd, fd->app, fd->arg, allocator);
	if(!rfd){
		closesocket(newfd);
		return;
	}
    /* tell the app about the new socket, if they reject it clean up */
    if (ACT(r, rfd, action_ACCEPT, ip))
    {
    	remove_fd(r, rfd);

        /* close the socket, and reset all memory */
    	closesocket(newfd);
        free_fd(r, rfd);
    }
    return;
}

void _rt__connect(reactor_t r, reactor_fd_t fd, void(*unset_write)(reactor_t, reactor_fd_t))
{
    reactor_fd_type_t type = fd->type;

    /* reset type and clear the "write" event that flags connect() is done */
    fd->type = type_NORMAL;
    unset_write(r, fd);

    /* if the app had asked to do anything in the meantime, do those now */
    if((type & type_CONNECT_READ) == type_CONNECT_READ) reactor_read(r,fd);
    if((type & type_CONNECT_WRITE) == type_CONNECT_WRITE) reactor_write(r,fd);
}
