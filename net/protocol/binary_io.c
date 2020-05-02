/*
 * binary_io.c
 *
 *  Created on: 2014年10月21日
 *      Author: lily
 */

#include "binary_io.h"
#include <sys/types.h>
#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif

#ifndef TCP_KEEPIDLE
#define TCP_KEEPIDLE            4       /* Start keeplives after this period */
#define TCP_KEEPINTVL           5       /* Interval between keepalives */
#define TCP_KEEPCNT             6       /* Number of keepalives before death */
#endif
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

//#define ENABLE_LOGGER

#ifdef ENABLE_LOGGER
static log_t g_logger;
static void _init_logger(){
	g_logger = log_get(NULL);
}
#endif

static void _bio_set_connection_info(bio_conn_t conn, SOCKET fd)
{
    struct sockaddr_storage sa = {0};
    socklen_t namelen = sizeof(sa);
    char peer_ip[200] = {'\0'};
    char local_ip[200] = {'\0'};

    getsockname(fd, (struct sockaddr*)&sa, &namelen);
    j_inet_ntop(&sa, local_ip, sizeof(local_ip));
    if(0 == strcmp(local_ip, "unix:@")){
    	snprintf(local_ip, sizeof(local_ip), "anonymous-sock-%d", fd);
    }
    strncpy(conn->local_ip, local_ip, sizeof(conn->local_ip) - 1);
    conn->local_port = j_inet_getport(&sa);

    /* Server mode: get remote info by fd,
	   Client mode: known when connecting */
    if(bio_SERVER == conn->type){
         memset(&sa, 0, namelen);
         getpeername(fd, (struct sockaddr *) &sa, &namelen);
         j_inet_ntop(&sa, peer_ip, sizeof(peer_ip));
         strncpy(conn->peer_ip, peer_ip, sizeof(conn->peer_ip) - 1);
         conn->peer_port = j_inet_getport(&sa);
         snprintf(conn->ipport, sizeof(conn->ipport), "%s:%d",
        		 conn->peer_ip, conn->peer_port);
    }
    snprintf(conn->ip_pair, sizeof(conn->ip_pair), "%s:%d@%s:%d",
    		conn->peer_ip, conn->peer_port, conn->local_ip, conn->local_port);
}

static void _bio_set_keepalive(SOCKET sock){
	int keep = 1;
	int keepidle = 30;
	int keepintvl = 10;
	int keepcnt = 3;
	setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (char*)&keep, sizeof(keep));
	setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, (char*)&keepidle, sizeof(keepidle));
	setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, (char*)&keepintvl, sizeof(keepintvl));
	setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, (char*)&keepcnt, sizeof(keepcnt));
}

#define _bio_event(c, e, d) ((c)->protocol ? (c)->protocol->event_call((c), (e), (d), (c)->protocol->event_arg) : 0)
/**
 * trigger uplevel call
 */
/*static inline int _bio_event(bio_conn_t c, struct bio_protocal_st *handle, bio_event_t e, void *data) {
	return handle->event_call(c, e, data, handle->event_arg);
}*/

static int _bio_process_read(bio_conn_t c)
{
	bio_buf_t buf = c->rbuf;
	int packetlen = c->cur_packetlen;
	do{
		if(packetlen == 0)
			packetlen = _bio_event(c, bio_READ_HEAD, buf);
		if(packetlen < 0){
			// header not complete. More data is wanted.
			c->cur_packetlen = 0;
			return 1;
		} else if (packetlen){
			// header is ok, check its payload now.
			if(buf->size < packetlen){
				c->cur_packetlen = packetlen;
				// packet is not ok, More data is wanted.
				return 1;
			} else {
				// packet is complete, user can process it now.
				int r = _bio_event(c, bio_READ_BODY, buf);
				if(r == -1){
					// some error ocurred when processing the body.
					return -2;
				}
				bio_buf_consume(buf, packetlen);
				packetlen = 0;
				// continue process following data.
			}
		} else {
			fprintf(stderr, "BadRequestHeader\n");
			// error occurred when processing request header.
			return -1;
		}
	}while(buf->size > 0);
	// we have processed all request data.
	c->cur_packetlen = 0;
	return 1;
}


static int _delayed_free_conn(reactor_t r, reactor_timer_t timer, void *arg){
	bio_conn_t conn = (bio_conn_t)arg;
    if(conn->fd) reactor_close(conn->r, conn->fd);
	return 0;
}

static void _bio_conn_close(bio_conn_t conn){
    if(conn->status == bio_connect_CLOSING && BIO_ERR_NOERR == conn->error){
    	// closing without error, it will be closed in event-loop later.
        return;
    }
    if(conn->status == bio_connect_CLOSED){
    	// already closed ? do nothing.
    	return;
    }
    // assign delayed job to do actual closing.
    conn->status = bio_connect_CLOSED;
    if(!conn->timer_delay_freeing)
    	conn->timer_delay_freeing = reactor_timer_create(conn->r, _delayed_free_conn, conn);
    reactor_timer_add(conn->r, conn->timer_delay_freeing, 0);
}
static int _bio_can_read(bio_conn_t c)
{
    int len = 0;
    bio_buf_t buf = c->rbuf;
	int r;
    bio_buf_extend(buf, 4096);

    if ((len = recv(c->fd->fd, buf->tail, 4096, 0)) < 0) {
        if (REACTOR_WOULDBLOCK) {
            // 继续监听 read
            return 1;
        } else {
			c->error = BIO_ERR_RECV_ERR;
			c->sys_errno = errno;
            _bio_conn_close(c);
            return 0;
        }
    } else if (len == 0) {
        // 表示收到close
		c->error = BIO_ERR_RECV_END;
		bio_close(c);
        //_bio_conn_close(c);
        return 0;
    }
    buf->size += len;
    buf->tail += len;

	c->close_defered = 1;
    r = _bio_process_read(c);
	c->close_defered = 0;
    if(r < 0){
		c->error = (r == -1) ? BIO_ERR_PROTOCOL : BIO_ERR_OTHER;
        _bio_conn_close(c);
        return 0;
    }

    if(jqueue_size(c->wbufq) == 0 && c->status == bio_connect_CLOSING){
        // close it.
        c->status = bio_connect_CLOSED;
        reactor_close(c->r, c->fd);
        return 0;
    }
    return 1;
}

static int _bio_can_write(bio_conn_t c)
{
    bio_buf_t out = NULL;
    int queue_size = 0;
    int count = 0;
    int len = 0;

    int err;
    len = sizeof(err);
	getsockopt(c->fd->fd, SOL_SOCKET, SO_ERROR, &err, &len);
	if (err) {
		// have some error on socket.
		c->error = BIO_ERR_OTHER;
		c->sys_errno = err;
        _bio_conn_close(c);
        return 0;
	}

    if(c->connecting){      //c->status == bio_connecting
    	c->connecting = 0; //c->status = bio_connect_OK
		_bio_event(c, bio_CONNECTED, NULL);
    }

    queue_size = jqueue_size(c->wbufq);
    while (queue_size > 0 && count < 100) {
        out = jqueue_peek(c->wbufq);

        len = send(c->fd->fd, out->head, out->size, MSG_NOSIGNAL);
        if (len < 0) {
            if (REACTOR_WOULDBLOCK) {
                return 1;
            } else {
            	fprintf(stderr, "[%s]write error %d\n", c->ipport, errno);
				c->error = BIO_ERR_WRITE_ERR;
				c->sys_errno = errno;
                _bio_conn_close(c);
                return 0;
            }
        } else if (len < out->size) {
            c->pending_out -= len;
        	bio_buf_consume(out, len);
            return 1;
        } else {
            count++;
            queue_size--;
            jqueue_pull(c->wbufq);
            c->pending_out -= len;
            if(0 == _bio_event(c, bio_PKT_WRITTEN, out))
            	bio_buf_free(out);
        }
    }
    if(queue_size == 0 && c->status == bio_connect_CLOSING){
        // close it.
        c->status = bio_connect_CLOSED;
    	fprintf(stderr, "[%s]client closing\n", c->ipport);
        reactor_close(c->r, c->fd);
        return 0;
    }
    if(queue_size > 0){
    	return 1;
    } else {
    	return 0;//数据发完了就不会启动reactor_write 
    }
}

static void _bio_conn_free(bio_conn_t conn, bio_client_t client)
{
	int i;
    if(conn->wbufq){
        bio_buf_t buf;
        while((buf = jqueue_pull(conn->wbufq))){
        	if(0 == _bio_event(conn, bio_PKT_TIMEOUT, buf))
        		bio_buf_free(buf);
        }
        jqueue_free(conn->wbufq);
    }
    if(conn->fd){
    	_bio_event(conn, bio_CLOSED, NULL);
    	conn->fd = NULL;
    }
    bio_buf_free(conn->rbuf);

    if(conn->timer_delay_freeing){
    	reactor_timer_del(conn->r, conn->timer_delay_freeing);
    	reactor_timer_free(conn->r, conn->timer_delay_freeing);
    	conn->timer_delay_freeing = NULL;
    }
    if(conn->timer_delay_reading){
    	reactor_timer_del(conn->r, conn->timer_delay_reading);
    	reactor_timer_free(conn->r, conn->timer_delay_reading);
    	conn->timer_delay_reading = NULL;
    }
	// all jobs done in server-mode
	if(conn->type == bio_SERVER){
		free(conn);
		return;
	}
	// following extra jobs should be done in client-mode
	reactor_timer_del(conn->r, conn->timer_reconnect);
	reactor_timer_free(conn->r, conn->timer_reconnect);
	if(client && !conn->need_free){
		// we should also change conn_num for this client.
		for (i = client->conn_num - 1; i >=0 && client->conn_array[i]; --i) {
			if(client->conn_array[i] == conn){
				-- client->conn_num;
				if(i != client->conn_num) // not the last one.
					memmove(&client->conn_array[i], &client->conn_array[i + 1],
							sizeof(void*) * (client->conn_num - i));
				break;
			}
		}
	}
	free(conn);
}
static int _bio_get_worker(bio_server_t s)
{
	register int i;
	register int index = 0;
	register int count = s->worker[0].client_count;
	for(i = 1; s->worker[i].r; ++i){
		if(count > s->worker[i].client_count){
			count = s->worker[i].client_count;
			index = i;
		}
	}
	return index;
}

static int _bio_server_call(reactor_t r, reactor_action_t a, reactor_fd_t fd, void *data, void *arg)
{
	bio_server_t s;
	bio_conn_t c = (bio_conn_t)arg;
	switch(a){
		case action_READ:
			return _bio_can_read(c);
		case action_WRITE:
			return _bio_can_write(c);
		case action_CLOSE:
            // 对于Server来说，不需要重连
            s = (bio_server_t) c->protocol;
            pthread_mutex_lock(&s->mutex);
            xhash_zap(s->connq, c->ip_pair);
            if(c->status < bio_connect_CLOSING)
            	c->status = bio_connect_CLOSING;
            if(s->worker){
            	int i;
            	for(i = 0; s->worker[i].r; ++i){
            		if(s->worker[i].r == r){
            			-- s->worker[i].client_count;
            			break;
            		}
            	}
            }
            pthread_mutex_unlock(&s->mutex);
            _bio_conn_free(c, NULL);
            return 0;
		case action_ACCEPT://newfd 已经分配了read write的event
		{
			bio_server_t s = (bio_server_t)arg;
			bio_conn_t c = (bio_conn_t) calloc(1, sizeof(*c));
			c->r = r;
			c->fd = fd;
			c->status = bio_connect_OK;
			c->protocol = &s->protocal;
			c->type = bio_SERVER;
			c->wbufq = jqueue_new();
			c->rbuf = bio_buf_new(NULL, 1024 * 8);
			c->error = BIO_ERR_NOERR;
			c->max_queue_size = 102400;

			_bio_set_connection_info(c, fd->fd);
			_bio_set_keepalive(fd->fd);

            pthread_mutex_lock(&s->mutex);
			xhash_put(s->connq, c->ip_pair, c);
            pthread_mutex_unlock(&s->mutex);

#ifndef _WIN32
			if(s->worker){
				// want an separate worker
				int nfd = dup(fd->fd);
				if(nfd == -1){
					// can not duplicate the socket fd, use as normal
				} else {
					int i;
		            pthread_mutex_lock(&s->mutex);
					i = _bio_get_worker(s);
					c->fd = reactor_register(s->worker[i].r, nfd, 0, _bio_server_call, c);
					if(c->fd){
						++ s->worker[i].client_count;
			            pthread_mutex_unlock(&s->mutex);
						c->r = s->worker[i].r;
						reactor_read(c->r, c->fd);
						// tell the server listen looper to close current fd.
						return 1;
					} else {
			            pthread_mutex_unlock(&s->mutex);
						// can not register duplicated fd.
						// restore old fd.
						closesocket(nfd);
						c->fd = fd;
					}
				}
			}
#endif
			reactor_app(r, fd, _bio_server_call, (void *) c);//这儿arg由server_st 变为了conn_st
			reactor_read(r, fd);
			_bio_event(c, bio_CONNECTED, NULL);
			return 0;
		}
	}
	return 0;
}

bio_conn_t bio_server_add_client_conn(bio_server_t s, SOCKET sockfd) {
	bio_conn_t c = (bio_conn_t) calloc(1, sizeof(*c));
	c->r = s->r;
	c->status = bio_connect_OK;
	c->protocol = &s->protocal;
	c->type = bio_SERVER;
	c->wbufq = jqueue_new();
	c->rbuf = bio_buf_new(NULL, 1024 * 8);
	c->error = BIO_ERR_NOERR;
	c->max_queue_size = 102400;

	_bio_set_connection_info(c, sockfd);
	_bio_set_keepalive(sockfd);

    pthread_mutex_lock(&s->mutex);
	xhash_put(s->connq, c->ip_pair, c);
    pthread_mutex_unlock(&s->mutex);

#ifndef _WIN32
	if(s->worker){
		// want an separate worker
		int i;
		pthread_mutex_lock(&s->mutex);
		i = _bio_get_worker(s);
		c->fd = reactor_register(s->worker[i].r, sockfd, 0, _bio_server_call, c);
		if(c->fd){
			++ s->worker[i].client_count;
			pthread_mutex_unlock(&s->mutex);
			c->r = s->worker[i].r;
			reactor_read(c->r, c->fd);
			return c;
		} else {
			pthread_mutex_unlock(&s->mutex);
			// can not register duplicated fd.
			// restore old fd.
			c->fd = reactor_register(s->r, sockfd, 0, _bio_server_call,c);;
		}
	} else {
		c->fd = reactor_register(s->r, sockfd, 0, _bio_server_call,c);
	}
#else
	c->fd = reactor_register(s->r, sockfd, 0, _bio_server_call,c);
#endif
	reactor_read(c->r, c->fd);
    return c;
}

bio_server_t bio_server_create(reactor_t r, int port, const char* local, bio_event_handler app, void *arg){
	bio_server_t s = calloc(1, sizeof(struct bio_server_st));
	s->r = r;
	s->port = port;
	
	/*listen 函数里面配置完socket后会进行read，而read会判断socket的type(比如type_LISTEN，就会进行accept操作)*/
	s->server_fd = reactor_listen(r, port, local, _bio_server_call, s);//fd->app = _bio_server_call which will be called by read/write
	if(s->server_fd == NULL){
		free(s);
		return NULL;
	}
#ifdef ENABLE_LOGGER
    if(!g_logger) _init_logger();
#endif
    s->connq = xhash_new(101);
    pthread_mutex_init(&s->mutex, NULL);
    if(local)
    	strncpy(s->ip, local, sizeof(s->ip) - 1);
    s->protocal.event_call = app;
    s->protocal.event_arg = arg;
	return s;
}

static void* _bio_run_worker_loop(void *arg){
	reactor_t r = (reactor_t)arg;
	reactor_loop(r);
	return NULL;
}
void bio_server_start_worker(bio_server_t s, int number)
{
	reactor_t r = s->r;
	int i;
	if(number <= 0)
		return;
	if(s->worker)
		return;
	s->worker = (bio_server_worker_t)calloc(number + 1, sizeof(struct bio_server_worker_st));
	for(i = 0; i < number; ++i){
		s->worker[i].r = reactor_create(reactor_type(r)); // use the same type of reactor with our server's.
		pthread_create(&s->worker[i].thread, NULL, _bio_run_worker_loop, s->worker[i].r);
	}
}
static void _bio_server_stop_worker(bio_server_t s){
	int i;
	if(!s->worker)
		return;
	// assume no connection is working.
	for(i = 0; NULL != s->worker[i].r; ++i){
		reactor_loop_break(s->worker[i].r);
		pthread_join(s->worker[i].thread, NULL);
		reactor_free(s->worker[i].r);
	}
	free(s->worker);
	s->worker = NULL;
}
bio_conn_t bio_server_getclient(bio_server_t s, const char* ip_pair){
	void *c;
    pthread_mutex_lock(&s->mutex);
	c = xhash_get(s->connq, ip_pair);
    pthread_mutex_unlock(&s->mutex);
	return (bio_conn_t)c;
}

static void _bio_client_on_close(bio_conn_t c);
static int _bio_client_call(reactor_t r, reactor_action_t a, reactor_fd_t fd, void *data, void *arg)
{
	bio_conn_t c = (bio_conn_t)arg;
	switch(a){
		case action_READ:
			return _bio_can_read(c);
		case action_WRITE:
			return _bio_can_write(c);
		case action_CLOSE:
            _bio_client_on_close(c);
            return 0;
		default:
			break;
	}
	return 0;
}

static int _bio_reconnect_server(reactor_t r, reactor_timer_t timer, void *arg)
{
    bio_conn_t c = (bio_conn_t) arg;
    if(c->status == bio_connect_CLOSING){
        _bio_conn_free(c, (bio_client_t)c->protocol);
        return 0;
    }
    bio_buf_consume(c->rbuf, c->rbuf->size);
    c->fd = reactor_connect(c->r, c->peer_port, c->peer_ip, NULL, _bio_client_call, c);
    if (NULL == c->fd) {
    	if(c->tm_reconnect > 0)
    		reactor_timer_add(c->r, timer, c->tm_reconnect);
        return 0;
    }

    _bio_set_connection_info(c, c->fd->fd);
    _bio_set_keepalive(c->fd->fd);

    c->status = bio_connecting;
    c->connecting = 1;
    reactor_read(c->r, c->fd);
    reactor_write(c->r, c->fd);// when the connection is established, give me a write Action.
	return 0;
}

static void _bio_client_on_close(bio_conn_t c)
{
    if(c->status == bio_connect_CLOSING || c->need_free)
    {
        _bio_conn_free(c, (bio_client_t)c->protocol);
    }
    else
    {
        // clean up connection
        //for each pending packet, they will finally timeout if response is
        //required
        bio_buf_t buf;
		uint32_t times;
        while((buf = jqueue_pull(c->wbufq))){
        	if(0 == _bio_event(c, bio_PKT_TIMEOUT, buf))
        		bio_buf_free(buf);
        }
        // re-create the queue to free the memory small segments.
        jqueue_free(c->wbufq);
        c->wbufq = jqueue_new();

    	_bio_event(c, bio_CLOSED, NULL);
    	c->fd = NULL;

        // 自动重连
        c->status = bio_connect_BAD;
        c->fail_conn_num ++;
        c->pending_out = 0;

        //calculate reconnect interval
        times = (c->fail_conn_num > 10) ? 10 : c->fail_conn_num;
        if (c->fail_req_num == 0) {
            times = 1;
        }
        c->fail_req_num = 0;
        if(c->tm_reconnect > 0)
        	reactor_timer_add(c->r, c->timer_reconnect, c->tm_reconnect * times);
    }
}

bio_client_t bio_client_create(reactor_t r, bio_event_handler app, void *arg){
	bio_client_t c = calloc(1, sizeof(struct bio_client_st));
	c->r = r;
    c->protocol.event_call = app;
    c->protocol.event_arg = arg;
    c->conn_array = calloc(32, sizeof(bio_conn_t));
    c->conn_capacity = 32;
#ifdef ENABLE_LOGGER
    if(!g_logger) _init_logger();
#endif
	return c;
}

bio_conn_t bio_client_add_server_conn(bio_client_t c, SOCKET sockfd) {
    bio_conn_t conn;
    if(c->conn_num >= c->conn_capacity){
        c->conn_capacity *= 2;
        c->conn_array = realloc(c->conn_array, c->conn_capacity * sizeof(bio_conn_t));
    }

    if ((conn = calloc(1, sizeof(*conn))) == NULL) {
        return NULL;
    }
    conn->r = c->r;
    conn->protocol = &c->protocol;
    conn->type = bio_CLIENT;
    conn->wbufq = jqueue_new();
    conn->rbuf = bio_buf_new(NULL, 1024 * 8);
    conn->tm_reconnect = -1;
    c->conn_array[c->conn_num] = conn;
    c->conn_num++;
    strncpy(conn->peer_ip, "dummy", sizeof(conn->peer_ip) - 1);
    conn->peer_port = 0;
    conn->max_queue_size = 102400;
    snprintf(conn->ipport, sizeof(conn->ipport), "dummy");

    conn->fd = reactor_register(c->r, sockfd, 0, _bio_client_call, conn);

	conn->timer_reconnect = reactor_timer_create(conn->r, _bio_reconnect_server, conn);
	conn->timer_delay_freeing = reactor_timer_create(conn->r, _delayed_free_conn, conn);

    conn->status = bio_connect_OK;

    _bio_set_connection_info(conn, conn->fd->fd);
    _bio_set_keepalive(conn->fd->fd);

    // 一直监听可读事件
    reactor_read(c->r, conn->fd);
    return conn;
}

bio_conn_t bio_client_add_server(bio_client_t c, const char *ip, unsigned short port,
		uint32_t timeout, int reconnect_timeout){

    bio_conn_t conn;

    if(c->conn_num >= c->conn_capacity){
        c->conn_capacity *= 2;
        c->conn_array = realloc(c->conn_array, c->conn_capacity * sizeof(bio_conn_t));
    }

    if ((conn = calloc(1, sizeof(*conn))) == NULL) {
        return NULL;
    }
    conn->r = c->r;
    conn->protocol = &c->protocol;
    conn->type = bio_CLIENT;
    conn->wbufq = jqueue_new();
    conn->rbuf = bio_buf_new(NULL, 1024 * 8);
    conn->tm_reconnect = reconnect_timeout;
    c->conn_array[c->conn_num] = conn;
    c->conn_num++;
    strncpy(conn->peer_ip, ip, sizeof(conn->peer_ip) - 1);
    conn->peer_port = port;
    conn->max_queue_size = 102400;
    snprintf(conn->ipport, sizeof(conn->ipport), "%s:%d", ip, port);

    if(reconnect_timeout < 0){
    	// we use it to indicate connect-on-need.
    	// this means no auto connect & reconnect for it.
    	// we will connect it when request is coming on it.
    } else {
    	conn->fd = reactor_connect(c->r, port, (char*)ip, NULL, _bio_client_call, conn);
    }

	conn->timer_reconnect = reactor_timer_create(conn->r, _bio_reconnect_server, conn);
	conn->timer_delay_freeing = reactor_timer_create(conn->r, _delayed_free_conn, conn);

    if (conn->fd == NULL) {
    	// can not connect currently, we must assign a delay task to try later.
    	conn->status = bio_connect_BAD;
    	conn->fail_conn_num ++;
    	if(reconnect_timeout > 0)
    		reactor_timer_add(conn->r, conn->timer_reconnect, reconnect_timeout);
        return conn;
    }
    conn->status = bio_connecting;
    conn->connecting = 1;

    _bio_set_connection_info(conn, conn->fd->fd);
    _bio_set_keepalive(conn->fd->fd);

    // 一直监听可读事件
    reactor_read(c->r, conn->fd);
    reactor_write(c->r, conn->fd);// when the connection is established, give me a write Action.
    return conn;
}

int bio_client_del_server(bio_client_t c, bio_conn_t conn) {
	/**
	 * @note: we can still get responses from the server after this.
	 */
	if(jqueue_size(conn->wbufq)){
        conn->status = bio_connect_CLOSING;
	} else if(conn->status == bio_connect_BAD && conn->tm_reconnect >= 0){
        conn->status = bio_connect_CLOSING;
    } else if(conn->status < bio_connect_CLOSING) {
        conn->status = bio_connect_CLOSING;
        if(!conn->close_defered){
            if(conn->fd)
            	reactor_close(c->r, conn->fd);
            else
            	_bio_client_on_close(conn);
        }
    }
    return 0;
}

int bio_close(bio_conn_t conn){
	if(conn->status < bio_connect_CLOSING){
		conn->status = bio_connect_CLOSING;
		reactor_write(conn->r, conn->fd); // trigger a write event.
	} else {
		// already closing or closed. do nothing.
		// the connection will be closed later in the event-loop.
	}
	return 0;
}
void bio_client_reset(bio_client_t c){
    int i;
    for (i = c->conn_num - 1; (i >= 0) && c->conn_array[i]; i--) {
        c->conn_array[i]->need_free = 1;
        c->conn_array[i]->protocol = NULL; // set protocol to NULL to avoid data conflicts.

        /**
         * we are resetting the client side,
         * conn_array will not be cleaned up in following close/free
         * after the protocol pointer set to NULL.
         * instead, we just set the conn_num to zero outside the loop with no harm.
         */
        //if we have not started to connect, no need to let reactor clean up
        if(c->conn_array[i]->status == bio_connect_CLOSING){
        	// the conn will be closed at proper time, nothing will be done here.
        } else if (c->conn_array[i]->status != bio_connect_BAD) {
        	//_bio_conn_close(c->conn_array[i]);
            reactor_close(c->r, c->conn_array[i]->fd);
        } else {
        	// free it.
        	_bio_conn_free(c->conn_array[i], c);
        }
    }
    c->conn_num = 0;
}
void bio_client_free(bio_client_t c) {
	bio_client_reset(c);
    free(c->conn_array);
    free(c);
}

void bio_server_free(bio_server_t s){
    bio_conn_t c;

    pthread_mutex_lock(&s->mutex);
    while (xhash_iter_first(s->connq)){
        do {
            xhash_iter_get(s->connq, NULL, NULL, (void *) &c);
            pthread_mutex_unlock(&s->mutex);
            _bio_conn_close(c);
            pthread_mutex_lock(&s->mutex);
        } while (xhash_iter_next(s->connq));
        pthread_mutex_unlock(&s->mutex);
        if(!s->worker){
        	// loop is needed to run cleanup.
        	reactor_loop_once(s->r, 10);
        } else {
        	usleep(10000);
        }
        pthread_mutex_lock(&s->mutex);
    }
    pthread_mutex_unlock(&s->mutex);
    pthread_mutex_destroy(&s->mutex);
    xhash_free(s->connq);
    _bio_server_stop_worker(s);
    reactor_app(s->r, s->server_fd, NULL, NULL);
    reactor_close(s->r, s->server_fd);
    free(s);
}

int bio_write(bio_conn_t c, void* data, int len){
    bio_buf_t buf = NULL;
	int need_call_write;

    do{
    if(c->status != bio_connect_OK && c->status != bio_connecting) {
    	if(c->tm_reconnect < 0){
    		// this means no auto-connect is done for it.
    		// we should call reconnect for it IF it runs as Client.
    		if(c->type == bio_CLIENT){
            	if(c->fd){
            		// NOTE: this call will assign an timer for reconnect.
            		//       so we must call reactor_timer_del after this call.
            		reactor_close(c->r, c->fd);
            		c->fd = NULL;
            	}
            	reactor_timer_del(c->r, c->timer_delay_freeing);
            	reactor_timer_del(c->r, c->timer_reconnect);
    			_bio_reconnect_server(c->r, c->timer_reconnect, c);
    			// connecting or connected
    			if(c->status == bio_connect_OK || c->status == bio_connecting)
    				break;
    		}
    	}
    	return 1;
    }
    }while(0);

    if(len == 0 || data == NULL){
    	// nothing to written, I'm glad to see it.
    	return 0;
    }
    if(jqueue_size(c->wbufq) > c->max_queue_size ||
    		(c->pending_out && c->pending_out + len > 0x4000000)) {
    	/* only drop it when
    	 * (1) the queue is too long or
    	 * (2) already some bytes is pending for written
    	 *     and the new data caused too much pending. */
    	return 1;
    }

    need_call_write = (0 == jqueue_size(c->wbufq));

    buf = bio_buf_new(data, len);
    jqueue_push(c->wbufq, buf, 0);
    c->pending_out += len;

    if(need_call_write){
    	// 监听可写事件
    	reactor_write(c->r, c->fd);//ev_write 是单次触发，所以每次都需要reactor_write(即set ev_write)
    }
    return 0;
}

static int _bio_send_by_key_internal(bio_client_t c, uint32_t *pkey, void* data, int len){

    bio_conn_t conn = NULL;
    uint32_t pos = 0;
    bio_buf_t buf = NULL;
    int reconnect_bad = 0;
	uint32_t ori_pkey;

    if (c->conn_num == 0) {
        return 1;
    }

    //1. assign packet key for this request
    ori_pkey = *pkey % c->conn_num;

    //2. select available connections
    retry:
    do {
        ++ *pkey ;//increase key
        pos = *pkey % c->conn_num;
        conn = c->conn_array[pos];
        if(reconnect_bad && conn->status != bio_connect_OK){
        	if(conn->fd){
        		// NOTE: this call will assign an timer for reconnect.
        		//       so we must call reactor_timer_del after this call.
        		reactor_close(conn->r, conn->fd);
            	conn->fd = NULL;
        	}
        	reactor_timer_del(conn->r, conn->timer_delay_freeing);
        	reactor_timer_del(conn->r, conn->timer_reconnect);
        	_bio_reconnect_server(conn->r, conn->timer_reconnect, conn);
        }
        //connection status is OK
        if (conn->status == bio_connect_OK) {
            //the sending buffer for this connection is not full
            if(jqueue_size(conn->wbufq) <= conn->max_queue_size &&
            		(conn->pending_out < 1024 || conn->pending_out + len < 0x4000000)) {
                buf = bio_buf_dup (data, len);
                jqueue_push (conn->wbufq, buf, 0);
                conn->pending_out += len;
                reactor_write(conn->r, conn->fd);
                //re-check the connection status, if connection failed already, we
                //take this opportunity to try the next
                if (conn->status == bio_connect_OK) {
                	free(data);
                    return 0;
                }
            }
        }
    } while (pos != ori_pkey);
    // no live server, do reconnect for some, and then retry.
    if(!reconnect_bad){
    	reconnect_bad = 1;
    	goto retry;
    }
    return 1;
}

int bio_send_by_rand(bio_client_t client, void* data, int len){
	static uint32_t key = 0;
	return _bio_send_by_key_internal(client, &key, data, len);
}
int bio_send_by_key(bio_client_t client, uint32_t key, void* data, int len){
    uint32_t tmpkey = key;
    return _bio_send_by_key_internal(client, &tmpkey, data, len);
}

int bio_read(bio_conn_t c)
{
	reactor_read(c->r, c->fd);
	return 0;
}

static int _bio_continue_reading(reactor_t r, reactor_timer_t timer, void *arg){
	bio_conn_t c = (bio_conn_t)arg;
	_bio_event(c, bio_WANT_READ, NULL);
	return 0;
}

int	bio_delay_reading(bio_conn_t c, int milisecond)
{
	reactor_unread(c->r, c->fd);
	if(NULL == c->timer_delay_reading)
		c->timer_delay_reading = reactor_timer_create(c->r, _bio_continue_reading, c);
	reactor_timer_add(c->r, c->timer_delay_reading, milisecond);
	return 0;
}

void bio_conn_set_max_queuesize(bio_conn_t c, int size) {
	c->max_queue_size = size;
}
