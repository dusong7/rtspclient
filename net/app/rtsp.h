/*
 * rtsp.h
 *
 *  Created on: 2016年3月17日
 *      Author: WangZhen
 */

#ifndef VMS_MAIN_NET_APP_RTSP_H_
#define VMS_MAIN_NET_APP_RTSP_H_

#include "../protocol/binary_io.h"
///////////////////////  rtsp client //////////////
///
typedef struct rtsp_client_st *rtsp_client_t;
typedef struct rtsp_server_st *rtsp_server_t;
typedef struct rtsp_subsession_st *rtsp_subsession_t;
typedef struct rtsp_mediasession_st *rtsp_mediasession_t;
typedef struct rtp_source_st * rtp_source_t;
typedef struct rtp_sink_st * rtp_sink_t;
typedef struct generic_sink_st * generic_sink_t;
typedef struct rtsp_request_st *rtsp_req_t;
typedef union rtsp_context_ut *rtsp_context_t;
typedef int (*rtspclient_callback)(rtsp_client_t m, rtsp_context_t, void *arg);

struct rtsp_client_st{
	reactor_t       reactor;
	bio_client_t    client;
	int 		    sequence;
	int 			tcp_channel;
	int             timeout;
	char            *urlbase;
	char            *hostip;
	char            *session;
	char            *auth_user;
	char            *auth_pass;
	char            *auth_realm;
	char            *auth_nonce;
	jqueue_t        pending_out;
	jqueue_t        req_queue;
	pthread_mutex_t req_mutex;
	rtspclient_callback  on_rtp_call;
	void            *on_rtp_call_arg;
};

struct rtsp_subsession_st
{
	char 			*media_name;
	const char 		*protocol; // static const, no freeable.
	char			*control_path; // SDP gives this
	char 			*conn_endpoint;
	char 			*media_codec;
	unsigned int	rtp_freq;
	unsigned int 	channels;
	unsigned int 	bandwidth;     // in kilobits-per-second, from b= line
	unsigned short 	port;
	unsigned short 	rtp_payload_format;
	xht 			fmtp_attr;
	rtsp_mediasession_t parent_session;
	rtsp_subsession_t   prev;
	rtsp_subsession_t   next;
	char			*sessionid;

	int 			rtp_port;
	float 			curr_scale;
	unsigned short 	server_port;
	char 			*server_addr;
	int 			rtp_id;
	int 			rtcp_id;
	struct {
		unsigned short is_new; // not part of the RTSP header; instead, set whenever this struct is filled in
		unsigned short seq;
		unsigned int timestamp;
	} rtpinfo;
};

struct rtsp_mediasession_st
{
	char  *session_url;  // SDP gives this
	char  *name;
	char  *description;
	char  *conn_endpoint;
	char  *mediatype;
	float curr_scale;    // PLAY response gives this
	rtsp_subsession_t subsessions;
};

union rtsp_context_ut{
	struct{
		int 		code;
		const char	*msg;
		xht 		headers;
		const char	*payload;
		int 		payload_len;
	}rtsp_ctx;
	struct {
		int 		channel;
		int 		rtplen;
		const unsigned char* rtp;
	}rtp_ctx;
};
struct rtsp_request_st{
	unsigned int        seq;
	char                *cmdname;
	char                *cmdurl;
	char                *extra;
	rtspclient_callback call;
	void                *arg;
	float               play_start;
	float               play_end;
	float               play_scale;
	rtsp_subsession_t   subsess;
	rtsp_mediasession_t sess;
};

typedef struct rtp_interface_st
{
	reactor_t reactor;
	int udp_sock;
}*rtp_interface_t;

struct generic_sink_st
{
	rtp_source_t source;
};
struct rtp_source_st
{
	rtp_interface_t rtp;
};
rtsp_client_t rtspclient_create(reactor_t r, const char* url, const char* user, const char* pass);
void          rtspclient_on_rtp_call(rtsp_client_t, rtspclient_callback on_rtp_call, void *arg);
void          rtspclient_send_options(rtsp_client_t, rtspclient_callback on_options_done, void *arg);
void          rtspclient_send_describe(rtsp_client_t, rtspclient_callback on_describe_done, void *arg);
void          rtspclient_send_setup(rtsp_client_t rc, rtsp_subsession_t subsess, rtspclient_callback on_setup_done, void *arg);
void          rtspclient_send_play(rtsp_client_t rc, rtsp_mediasession_t sess, float scale, rtspclient_callback on_play_done, void *arg);
void          rtspclient_send_pause(rtsp_client_t rc, rtsp_mediasession_t sess, rtspclient_callback on_pause_done, void *arg);
void          rtspclient_send_teardown(rtsp_client_t rc, rtsp_mediasession_t sess, rtspclient_callback on_teardown_done, void *arg);
void          rtspclient_send_rtcp(rtsp_client_t rc, uintptr_t udp_sock, rtsp_context_t rtcp);
void          rtspclient_free(rtsp_client_t m);

rtsp_mediasession_t rtspclient_create_mediasession(const char* sdp, int sdplen);
void 				rtspclient_free_mediasession(rtsp_mediasession_t sess);

/////////////////// rtsp server ///////////////////
struct rtsp_server_st{
	reactor_t 		reactor;
	bio_server_t  	server;
};

struct rtp_sink_st
{
	int a;
};

#endif /* VMS_MAIN_NET_APP_RTSP_H_ */
