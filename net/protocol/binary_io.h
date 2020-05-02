/*
 * bio.h
 *
 *  Created on: 2014年10月22日
 *      Author: lily
 */

#ifndef BIO_H_
#define BIO_H_

#include "../reactor/reactor.h"
#include <util/util.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bio_buf_st{
	char *heap, *head, *tail;
	int capacity; //the whole buf size 
	int size;// buf that used
}*bio_buf_t;

/** 连接上的事件 */
typedef enum {
    /* mandatory callback */
    bio_READ_HEAD,          /* 读完包头, get packet body len data=read_ptr*/
    bio_READ_BODY,          /* 读完一个完整的包 data=read_ptr*/
    /* optional callback */
    bio_CLOSED,             /* connection will be closed */
	bio_PKT_WRITTEN,		/* packet written out */
	bio_PKT_TIMEOUT,		/* packet not send */
	bio_WANT_READ,			/* try continue reading after delayed */
	bio_CONNECTED
} bio_event_t;

typedef struct bio_conn_st* bio_conn_t;
typedef int (*bio_event_handler)(bio_conn_t c, bio_event_t e, void *data, void *arg);

struct bio_protocal_st{
	bio_event_handler event_call;
	void *event_arg;
};
struct bio_server_st;
typedef struct bio_server_worker_st
{
	reactor_t r;
	pthread_t thread;
	int client_count;
} *bio_server_worker_t;

typedef struct bio_server_st{
	struct bio_protocal_st protocal;
	reactor_t r;
	reactor_fd_t server_fd;
	int port;
	xht connq;
	pthread_mutex_t mutex;
	char ip[100];
	bio_server_worker_t worker;
} *bio_server_t;

typedef struct bio_client_st{
	struct bio_protocal_st protocol;
	reactor_t r;
    /* 重连间隔（秒） */
    unsigned int           sleep;
    /* 重连次数 */
    unsigned int           retry;
    /* 已连接的数目（一个服务器一个连接） */
    unsigned int           conn_num;
    /* 连接队列*/
    bio_conn_t             *conn_array;
    unsigned int		   conn_capacity;
}*bio_client_t;

struct bio_conn_st{
	reactor_t 	r;
	reactor_fd_t fd;
	struct bio_protocal_st *protocol;
	enum{
	    bio_connect_none = 0,
	    bio_connecting,         /* 连接中 */
	    bio_connect_OK = bio_connecting,          /* 连接成功 */
	    bio_connect_BAD,           /* 连接失败 */
	    bio_connect_CLOSING,
	    bio_connect_CLOSED
	} status;
	enum{bio_SERVER, bio_CLIENT} type;
	int 		pending_out;//等待的数据长度
	jqueue_t 	wbufq;
	jqueue_t 	ready_req;
	bio_buf_t 	rbuf;
	int 		cur_packetlen;
	char 		local_ip[100];
	int 		local_port;
	char 		peer_ip[100];
	int 		peer_port;
	char 		ipport[100];
	char 		ip_pair[200];
    int 	 	tm_reconnect;   /*< reconnect interval */
    uint32_t  	fail_req_num;/* consecutive failed request counter */
    uint32_t  	fail_conn_num; /* consecutive failed reconnect counter, used to enlarge reconnect interval */
    reactor_timer_t timer_reconnect;
    reactor_timer_t timer_delay_freeing;
    reactor_timer_t timer_delay_reading;
    int 		need_free;
	enum {BIO_ERR_NOERR, BIO_ERR_RECV_END, BIO_ERR_RECV_ERR, BIO_ERR_WRITE_ERR, BIO_ERR_PROTOCOL, BIO_ERR_OTHER}
				error;
	int			sys_errno;
	int			max_queue_size;
	int         close_defered;
	int			connecting;
};

bio_buf_t    bio_buf_dup(const void* src, int size);
bio_buf_t    bio_buf_new(void* src, int size);
void         bio_buf_free(bio_buf_t b);
int          bio_buf_extend(bio_buf_t b, int grow);
void         bio_buf_append(bio_buf_t b, const void* src, int len);
void         bio_buf_consume(bio_buf_t b, int use);

bio_server_t bio_server_create(reactor_t r, int port, const char* local, bio_event_handler app, void *arg);
void		 bio_server_start_worker(bio_server_t s, int number);
void         bio_server_free(bio_server_t s);
bio_conn_t   bio_server_getclient(bio_server_t s, const char* ip_pair);
bio_conn_t   bio_server_add_client_conn(bio_server_t s, SOCKET sockfd);

bio_client_t bio_client_create(reactor_t r, bio_event_handler app, void *arg);
bio_conn_t   bio_client_add_server(bio_client_t c, const char *ip, unsigned short port,
								uint32_t timeout, int reconnect_timeout);
bio_conn_t   bio_client_add_server_conn(bio_client_t c, SOCKET sockfd);
int          bio_client_del_server(bio_client_t c, bio_conn_t conn);
void 		 bio_client_reset(bio_client_t c);
void         bio_client_free(bio_client_t c);

int          bio_write(bio_conn_t c, void* data, int len);
int          bio_read(bio_conn_t c);
int			 bio_delay_reading(bio_conn_t c, int milisecond);
int 		 bio_close(bio_conn_t conn);

int          bio_send_by_rand(bio_client_t client, void* data, int len);
int          bio_send_by_key(bio_client_t client, uint32_t key, void* data, int len);
void		 bio_conn_set_max_queuesize(bio_conn_t c, int size);

#ifdef __cplusplus
}
#endif
#endif /* BIO_H_ */
