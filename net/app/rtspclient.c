/*
 * rtspclient.c
 *
 *  Created on: 2016年3月17日
 *      Author: WangZhen
 */
#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <string.h>
#include "rtsp.h"

static char *_ltrim(const char* src){
	if(!src) return NULL;
	while(*src != 0 && *src <= ' '){
		++src;
	}
	return (char *)src;
}
static void print_in_response(const char* data, int len){
	const char* p = data;
	const char* end = data + len;
	if(0 == len) return;
	do{
		char* q = strchr(p, '\r');
		if(!q) q = strchr(p, '\n');
		printf("  > %.*s\n", (int)(q - p), p);
		p = q + 1;
		while(*p < ' ' && p < end) ++p;
	}while(p && p < end);
}
static void print_out_req(bio_buf_t buf){
	char* p = buf->head;
	char* end = buf->head + buf->size;
	do{
		char* q = strchr(p, '\r');
		if(!q) q = strchr(p, '\n');
		printf(" <  %.*s\n", (int)(q - p), p);
		p = q + 1;
		while(*p < ' ' && p < end) ++p;
	}while(p && p < end);
}
static xht _generate_xht(const char* q, char seg_item, char seg_key_value){
	xht ret = xhash_new(11);
	q = _ltrim(q);
	while(q && *q){
		char *s = strchr(q, seg_item);
		char *e = strchr(q, seg_key_value);
		if(e && (e < s || !s)){
			// <name> = <value>
			char* name = pstrdupx(xhash_pool(ret), q, e - q);
			char* value = s ? pstrdupx(xhash_pool(ret), e + 1, s - (e + 1)) :
					pstrdup(xhash_pool(ret), e + 1);
			e = name;
			while(*e) {*e = tolower(*e); ++e;};
			xhash_put(ret, name, value);
		} else if(s != q) {
			// <name>
			char* name = pstrdupx(xhash_pool(ret), q, s ? s - q : strlen(q));
			char* value = (char*)(uintptr_t)1;
			e = name;
			while(*e) {*e = tolower(*e); ++e;};
			xhash_put(ret, name, value);
		}
		q = s ? _ltrim(s + 1) : NULL;
	}
	return ret;
}
static void _connect_server(rtsp_client_t rc, const char* host, int port);
static int compute_response_length(rtsp_client_t rc, const unsigned char* data, int len)
{
	if(len < 4)
		return -1;
	if(data[0] == '$'){
		// this is an rtp packet.
		int channel = data[1] & 0xFF;
		int len = (data[2] & 0xFF) << 8 | (data[3] & 0xFF);
		return len + 4;
	} else{
		// rtsp response data.
		unsigned char *p = (unsigned char *)memmem((void*)data, len, "\r\n\r\n", 4);
		int headlen;
		unsigned char* q;
		if(!p)
			return -1;
		if(strncasecmp(data, "RTSP/1.", 7))
			return 0;
		headlen = p - data;
		q = (unsigned char *)memmem(data, headlen, "\r\nContent-Length:", 17);
		if(!q)
			q = (unsigned char *)memmem(data, headlen, "\r\nContent-length:", 17);
		return headlen + 4 + (q ? atoi(q + 17) : 0);
	}
}
static const char* _get_header_item(xht h, const char* key, int index){
	void *v = xhash_get(h, key);
	xht a;
	if(!v) return NULL;
	a = (xht)xhash_get(h, "_A_");
	if(a){
		if(xhash_get(a, key) == (void*)1LL){
			return jarray_get((jarray_t)v, index);
		}
	}
	return (const char*)v;
}
static xht _parse_rtsp_response_headers(const unsigned char* data, int len){
	// get status code from the first line. RTSP/1.0 200 OK
	unsigned int code;
	char* info, *line_end;
	int infolen;
	xht ret;
	xht array_keys;
	if(1 != sscanf(data, "RTSP/%*s%u", &code)){
		return NULL;
	}
	info = strchr(data, ' ');
	while(*info == ' ' || *info == '\t')
		++info;
	line_end = strstr(data, "\r\n");
	infolen = line_end - info;
	ret = xhash_new(11);
	array_keys = xhash_new(3);
	pool_cleanup(xhash_pool(ret), (pool_cleanup_t)xhash_free, array_keys);
	xhash_put(ret, "_A_", array_keys);
	xhash_put(ret, "StatusCode", (void*)(uintptr_t)code);
	xhash_put(ret, "StatusInfo", pstrdupx(xhash_pool(ret), info, infolen));
	// next parse each line, to get the header key and value.
	do{
		char* line = line_end + 2;
		line_end = strstr(line, "\r\n");
		if(line_end == line){
			// this is the last line.
			line_end = NULL;
		} else {
			char* seg = memmem(line, line_end - line, ":", 1);
			if(!seg){
				// wrong header line.
			} else {
				int key_len = seg - line;
				int value_len;
				++ seg;
				while(*seg <= ' ' && seg < line_end) ++ seg;
				value_len = line_end - seg;
				if(value_len && key_len){
					void *val = pstrdupx(xhash_pool(ret), seg, value_len);
					void *old = xhash_getx(ret, line, key_len);
					if(old){
						// already have the old value for this item.
						jarray_t v = jarray_new_p(xhash_pool(ret));
						jarray_push(v, old);
						jarray_push(v, val);
						xhash_putx(ret, line, key_len, v);
						xhash_putx(array_keys, line, key_len, (void*)1LL);
					} else {
						xhash_putx(ret, line, key_len, val);
					}
				}
			}
		}
	}while(line_end);

	return ret;
}
static void _resend_request(rtsp_client_t rc, rtsp_req_t req);
static void _free_req(rtsp_req_t req){
	free(req->cmdurl);
	free(req->extra);
	free(req);
}
static int _handle_setup_response(rtsp_client_t rc, rtsp_req_t req, xht headers){
	char* sessionid = (char*)xhash_get(headers, "Session");
	char* transport = (char*)xhash_get(headers, "Transport");
	char *p;
	int timeout;
	if(!sessionid || !transport){
		return -1;
	}
	p = strchr(sessionid, ';');
	if(p && 1 == sscanf(p, "; timeout = %d", &timeout)){
		rc->timeout = timeout;
	}
	j_free(rc->session);
	j_free(req->subsess->sessionid);
	req->subsess->sessionid = j_strndup(sessionid, p ? p - sessionid : strlen(sessionid));
	rc->session = j_strdup(req->subsess->sessionid);
	{
		xht transit = _generate_xht(transport, ';', '=');
		int server_port = j_atoi((char*)xhash_get(transit, "server_port"), 0);
		int client_port = j_atoi((char*)xhash_get(transit, "client_port"), 0);
		char* server_addr = (char*)xhash_get(transit, "source");
		char* str_interleaved = (char*)xhash_get(transit, "interleaved");
		char* destination = (char*)xhash_get(transit, "destination");
		char* str_port = (char*)xhash_get(transit, "port");
		unsigned short rtp_port = 0, rtcp_port = 0;
		unsigned int rtp_id = 0, rtcp_id = 0;
		int is_unicast = !!xhash_get(transit, "unicast");
		if(str_interleaved) sscanf(str_interleaved, "%u-%u", &rtp_id, &rtcp_id);
		if(str_port) sscanf(str_port, "%hu-%hu", &rtp_port, &rtcp_port);


		req->subsess->rtp_id = rtp_id;
		req->subsess->rtcp_id = rtcp_id;
		if(!is_unicast && str_port && destination){
			j_free(req->subsess->server_addr);
			req->subsess->server_addr = j_strdup(destination);
			req->subsess->server_port = rtp_port;
		} else if(server_addr){
			j_free(req->subsess->server_addr);
			req->subsess->server_addr = j_strdup(server_addr);
			req->subsess->server_port = server_port ? server_port : client_port;
		}
		xhash_free(transit);
	}
	return 0;
}
static int _handle_play_response(rtsp_client_t rc, rtsp_req_t req, xht headers){
	char* str_scale = xhash_get(headers, "Scale");
	char* str_rtpinfo = xhash_get(headers, "RTP-Info");
	//char* str_range = xhash_get(headers, "Range");
	//char* str_speed = xhash_get(headers, "Speed");
	rtsp_subsession_t subsess;
	if(req->subsess){
		if(str_scale)
			req->subsess->curr_scale = strtod(str_scale, NULL);
		if(str_rtpinfo){
			xht rtpinfo = _generate_xht(str_rtpinfo, ';', '=');
			if(rtpinfo){
				req->subsess->rtpinfo.seq = j_atoi((char*)xhash_get(rtpinfo, "seq"), 0);
				req->subsess->rtpinfo.timestamp = j_atoi((char*)xhash_get(rtpinfo, "rtptime"), 0);
				req->subsess->rtpinfo.is_new = 1;
				xhash_free(rtpinfo);
			}
		}
		return 0;
	}
	if(str_scale)
		req->sess->curr_scale = strtod(str_scale, NULL);
	subsess = req->sess->subsessions;
	do{
		xht rtpinfo;
		int seq;
		unsigned int rtptime;
		char *q = strchr(str_rtpinfo, ',');
		if(q) *q = '\0';
		rtpinfo = _generate_xht(str_rtpinfo, ';', '=');
		if(!rtpinfo) break;
		seq = j_atoi((char*)xhash_get(rtpinfo, "seq"), 0);
		rtptime = j_atoi((char*)xhash_get(rtpinfo, "rtptime"), 0);
		subsess->rtpinfo.seq = seq;
		subsess->rtpinfo.timestamp = rtptime;
		subsess->rtpinfo.is_new = 1;
		subsess = subsess->next;
		xhash_free(rtpinfo);
		str_rtpinfo = q ? q + 1 : NULL;
	}while(str_rtpinfo);
	return 0;
}

static int process_response(rtsp_client_t rc, const unsigned char* data, int len)
{
	int headlen;
	char* cseq;
	unsigned int seq;
	rtsp_req_t req;
	char *status;
	xht headers;
	const char *contentbase;
	int url_changed = 0;
	int code;

	if(data[0] == '$'){
		union rtsp_context_ut ctx;
		ctx.rtp_ctx.channel = data[1];
		ctx.rtp_ctx.rtplen = (data[2] << 8) | data[3];
		ctx.rtp_ctx.rtp = &data[4];
		return rc->on_rtp_call(rc, &ctx, rc->on_rtp_call_arg);
	}
	// can process response data here. "\r\n\r\n" always exists.
	headlen = (unsigned char*)memmem(data, len, "\r\n\r\n", 4) - data;
	print_in_response(data, headlen);
	cseq = (char*)memmem(data, headlen, "\r\nCSeq:", 7);
	if(!cseq) {
		return -1; // bad response without cseq.
	}
	seq = strtoul(cseq + 7, NULL, 10);
	// get corresponding request from our request-queue.
	// check if seq equals.
	req = (rtsp_req_t)jqueue_pull(rc->req_queue);
	if(req->seq > seq){
		// got duplicated response? bad server?
		jqueue_push(rc->req_queue, req, 1);
		return 0;
	}
	while(req->seq < seq){
		//server gives no answer for the req.
		union rtsp_context_ut ctx;
		ctx.rtsp_ctx.code = -1;
		ctx.rtsp_ctx.msg = "Server gives no answer.";
		ctx.rtsp_ctx.payload_len = 0;
		ctx.rtsp_ctx.payload = NULL;
		req->call(rc, &ctx, req->arg);
		req = (rtsp_req_t)jqueue_pull(rc->req_queue);
	}
	// now got the exactly request. parse resultcode.
	status = (char *)memmem(data, headlen, " ", 1);
	if(!status){
		return -1; // no status code given in the header.
	}
	headers = _parse_rtsp_response_headers(data, headlen + 2);
	contentbase = _get_header_item(headers, "Location",0);
	if(!contentbase) contentbase = _get_header_item(headers, "Content-Base",0);
	if(contentbase && (!rc->urlbase || 0 != strcmp(rc->urlbase, contentbase))){
		// url changed.
		if(rc->urlbase) j_free(rc->urlbase);
		rc->urlbase = j_strdup(contentbase);
		url_changed = 1;
	}

	code = (intptr_t) xhash_get(headers, "StatusCode");
	if(code == 401){ // unauthorized, can we try again?
		int index = 0;
		const char* auth;
		int realm_changed = 0;
		int is_stale;
		while((auth = _get_header_item(headers, "WWW-Authenticate", index++))){
			const char* p_realm = strstr(auth, "realm=");
			const char* p_nonce = strstr(auth, "nonce=");
			const char* p_stale = strstr(auth, "stale=");
			is_stale = p_stale ? strncmp(p_stale + 7, "true", 4) : 0;
			if(p_realm){
				char* q_realm, *new_realm;
				p_realm += 6;
				while(*p_realm != '"') ++ p_realm;
				++ p_realm;
				q_realm = strchr(p_realm, '"');
				new_realm = j_strndup(p_realm, q_realm - p_realm);
				if((NULL == rc->auth_realm) || (0 != strcmp(rc->auth_realm, new_realm))){
					realm_changed = 1;
				}
				rc->auth_realm = new_realm;
			}
			if(p_nonce){
				char* q_nonce;
				p_nonce += 6;
				while(*p_nonce != '"') ++ p_nonce;
				++ p_nonce;
				q_nonce = strchr(p_nonce, '"');
				rc->auth_nonce = j_strndup(p_nonce, q_nonce - p_nonce);
			}
		}
		if((!realm_changed && !is_stale) || NULL == rc->auth_user || NULL == rc->auth_pass){
			// re-sending gives no help.
		} else {
			// now can re-send the request.
			xhash_free(headers);
			_resend_request(rc, req);
			return 0;
		}
	} else if((code == 301 || code == 302) && url_changed){
		// a new location is given.
		xhash_free(headers);
		// update req->cmdurl.
		j_free(req->cmdurl);
		req->cmdurl = j_strdup(rc->urlbase);
		// close the connection and then re-send.
		bio_client_free(rc->client);
		rc->client = NULL;
		// now can re-send the request.
		_resend_request(rc, req);
		return 0;
	} else if(code == 200){
		int r = 0;
		if(0 == strcmp(req->cmdname, "SETUP")){
			r = _handle_setup_response(rc, req, headers);
		} else if(0 == strcmp(req->cmdname, "PLAY")){
			r = _handle_play_response(rc, req, headers);
		}
		if(0 != r)
			code = -1;
	}
	{
		int payload_len = j_atoi((const char*)xhash_get(headers, "Content-Length"), 0);
		union rtsp_context_ut ctx;
		print_in_response(data + headlen + 4, payload_len);
		// RTSP/1.0 200 OK
		ctx.rtsp_ctx.code = code;
		ctx.rtsp_ctx.headers = headers;
		ctx.rtsp_ctx.msg = (char*) xhash_get(headers, "StatusInfo");
		ctx.rtsp_ctx.payload_len = payload_len;
		ctx.rtsp_ctx.payload = data + headlen + 4;
		req->call(rc, &ctx, req->arg);
	}
	_free_req(req);
	xhash_free(headers);
	return 0;
}

static int _rtspclient_handler(bio_conn_t conn, bio_event_t ev, void *data, void *arg){
	rtsp_client_t rc = (rtsp_client_t)arg;
	bio_buf_t buf = (bio_buf_t)data;
	switch(ev){
	case bio_READ_HEAD:
		return compute_response_length(rc, (unsigned char*)buf->head, buf->size);
	case bio_READ_BODY:
		return process_response(rc, (unsigned char*)buf->head, buf->size);
	case bio_CLOSED:
		// server connection dropped. requests needs to be resent.
		break;
	case bio_PKT_WRITTEN:
		// normal case.
		break;
	case bio_PKT_TIMEOUT:
		// data not written out. re-send it later.
		break;
	}
	return 0;
}

typedef struct rtsp_url_info_st{
	const char* u;
	const char* p;
	const char* h;
	const char* url;
	int ulen;
	int plen;
	int hlen;
	int port;
	int urllen;
}*rtsp_url_info_t;

static int _parse_url_info(const char* url, rtsp_url_info_t info){
	// try parse the url as format of user:pass@host:port/url-path
	const char* host_seg, *host_end, *port_seg;
	if(0 != strncasecmp(url, "RTSP://", 7))
		return -1;
	info->h = url + 7;
	host_seg = strchr(info->h, '@');
	host_end = strchr(info->h, '/');
	port_seg = strchr(info->h, ':');
	if(!host_end){
		//have no ending char. the url is like 'rtsp://127.0.0.1', set host_end to the terminating NULL char.
		host_end = info->h + strlen(info->h);
	}
	if(host_seg && host_seg < host_end){
		info->u = info->h;
		info->h = host_seg + 1;
		if(port_seg && port_seg < host_seg){
			info->p = port_seg + 1;
			info->ulen = port_seg - info->u;
			info->plen = host_seg - info->p;
		} else {
			info->ulen = host_seg - info->u;
			info->p = "";
			info->plen = 0;
		}
		port_seg = strchr(info->h, ':');
	} else {
		info->u = info->p = "";
		info->ulen = info->plen = 0;
	}
	// no port given in the url, use the default as 554.
	info->port = port_seg ? atoi(port_seg + 1) : 554;
	info->hlen = (port_seg ? port_seg : host_end) - info->h;
	return 0;
}
static void _connect_server(rtsp_client_t rc, const char* host, int port){
	assert(NULL == rc->client);
	rc->client = bio_client_create(rc->reactor, _rtspclient_handler, rc);
	bio_client_add_server(rc->client, host, port, 0, 1000);
}
rtsp_client_t rtspclient_create(reactor_t r, const char* url, const char* user, const char* pass){
	struct rtsp_url_info_st info;
	char *host, *hostip;
	rtsp_client_t c;
	if(_parse_url_info(url, &info))
		return NULL;
	host = j_strndup(info.h, info.hlen);
	// we need translate the domain-name to ip-address.
	hostip = j_inet_getaddr(host);
	j_free(host);

	c = (rtsp_client_t)calloc(1, sizeof(*c));
	c->reactor = r;
	c->urlbase = j_strdup(url);
	c->hostip = hostip;
	c->auth_user = user ? j_strdup(user) : info.ulen ? j_strndup(info.u, info.ulen) : NULL;
	c->auth_pass = pass ? j_strdup(pass) : info.plen ? j_strndup(info.p, info.plen) : NULL;
	_connect_server(c, hostip, info.port);
	// Notice: connection to the server can be re-connected automatically. so we keep no info for it here.
	c->req_queue = jqueue_new();
	c->sequence = 1;
	return c;
}
void rtspclient_on_rtp_call(rtsp_client_t rc, rtspclient_callback on_rtp_call, void *arg){
	rc->on_rtp_call = on_rtp_call;
	rc->on_rtp_call_arg = arg;
}
static char* _generate_auth(rtsp_client_t rc, const char* cmdname, const char* url){
	char auth_str[1024];
	if(rc->auth_nonce){
		// The "response" field is computed as:
		//    md5(md5(<username>:<realm>:<password>):<nonce>:md5(<cmd>:<url>))
		// or, if "fPasswordIsMD5" is True:
		//    md5(<password>:<nonce>:md5(<cmd>:<url>))
		char ha1Buf[33];
		char ha2Buf[33];
		char ha1Data[1024];
		unsigned char tmp[16];
		//md5(<username>:<realm>:<password>)
		{
			sprintf((char*)ha1Data, "%s:%s:%s", rc->auth_user, rc->auth_realm, rc->auth_pass);
			md5_hash(ha1Data, strlen(ha1Data), tmp);
			hex_from_raw(tmp, 16, ha1Buf);
		}
		//md5(<cmd>:<url>)
		sprintf((char*)ha1Data, "%s:%s", cmdname, url ? url: rc->urlbase);
		md5_hash(ha1Data, strlen(ha1Data), tmp);
		hex_from_raw(tmp, 16, ha2Buf);

		//md5(<password>:<nonce>:md5(<cmd>:<url>))
		sprintf((char*)ha1Data, "%s:%s:%s",ha1Buf, rc->auth_nonce, ha2Buf);
		md5_hash(ha1Data, strlen(ha1Data), tmp);
		hex_from_raw(tmp, 16, ha1Buf);
		snprintf(auth_str, 1024, "Digest username=\"%s\", realm=\"%s\", "
				"nonce=\"%s\", uri=\"%s\", response=\"%s\"", rc->auth_user,
				rc->auth_realm, rc->auth_nonce, (url ? url: rc->urlbase), ha1Buf);
	} else {
		char* out;
		snprintf(auth_str, sizeof(auth_str), "%s:%s", rc->auth_user, rc->auth_pass);
		out = b64_encode(auth_str, strlen(auth_str));
		snprintf(auth_str, sizeof(auth_str), "Basic %s", out);
		j_free(out);
	}
	return strdup(auth_str);
}
static void _rtspclient_send_commands(rtsp_client_t rc, rtsp_req_t req){
	bio_buf_t reqbuf = bio_buf_new(NULL, 1024);
	char str_seq[20];
	bio_buf_append(reqbuf, req->cmdname, strlen(req->cmdname));
	bio_buf_append(reqbuf, " ", 1);
	bio_buf_append(reqbuf, req->cmdurl, strlen(req->cmdurl));
	bio_buf_append(reqbuf, " RTSP/1.0\r\nCSeq: ", 17);
	snprintf(str_seq, 20, "%u", req->seq);
	bio_buf_append(reqbuf, str_seq, strlen(str_seq));
	if(rc->session){
		bio_buf_append(reqbuf, "\r\nSession: ", 11);
		bio_buf_append(reqbuf, rc->session, strlen(rc->session));
	}
	if(rc->auth_user && rc->auth_pass && rc->auth_realm){
		char* auth = _generate_auth(rc, req->cmdname, req->cmdurl);
		bio_buf_append(reqbuf, "\r\nAuthorization: ", 17);
		bio_buf_append(reqbuf, auth, strlen(auth));
		free(auth);
	}
	bio_buf_append(reqbuf, "\r\nUser-Agent: HPWANG_Net_Rtsp_Client\r\n", 38);
	if(req->extra &&req->extra[0])
		bio_buf_append(reqbuf, req->extra, strlen(req->extra));
	bio_buf_append(reqbuf, "\r\n", 2);
	print_out_req(reqbuf);

	if(!rc->client){
		struct rtsp_url_info_st info = {0};
		char* host, *hostip;
		_parse_url_info(req->cmdurl, &info);
		host = j_strndup(info.h, info.hlen);
		hostip = j_inet_getaddr(host);
		_connect_server(rc, hostip, info.port);
		j_free(host);
		j_free(rc->hostip);
		rc->hostip = hostip;
	}

	if(0 == bio_send_by_key(rc->client, 0, reqbuf->head, reqbuf->size)) {
		reqbuf->heap = NULL;
		// request has been sent out.
	} else {
		// not sent,
		union rtsp_context_ut ctx = {0};
		ctx.rtsp_ctx.code = -2;
		ctx.rtsp_ctx.msg = "Out-Queue-Full";
		req->call(rc, &ctx, req->arg);
		_free_req(req);
	}
	bio_buf_free(reqbuf);
}
static void _resend_request(rtsp_client_t rc, rtsp_req_t req)
{
	req->seq = rc->sequence ++;
	jqueue_push(rc->req_queue, req, 0);
	_rtspclient_send_commands(rc, req);
}
void rtspclient_send_options(rtsp_client_t rc, rtspclient_callback on_options_done, void *arg){
	rtsp_req_t req = (rtsp_req_t)calloc(1, sizeof(*req));
	req->seq = rc->sequence ++;
	req->call = on_options_done;
	req->arg = arg;
	req->cmdname = "OPTIONS";
	req->cmdurl = strdup(rc->urlbase);
	jqueue_push(rc->req_queue, req, 0);
	_rtspclient_send_commands(rc, req);
}
void rtspclient_send_describe(rtsp_client_t rc, rtspclient_callback on_describe_done, void *arg){
	rtsp_req_t req = (rtsp_req_t)calloc(1, sizeof(*req));
	req->seq = rc->sequence ++;
	req->call = on_describe_done;
	req->arg = arg;
	req->cmdname = "DESCRIBE";
	req->cmdurl = strdup(rc->urlbase);
	req->extra = strdup("Accept: application/sdp\r\n");
	jqueue_push(rc->req_queue, req, 0);
	_rtspclient_send_commands(rc, req);
}
static const char* _get_sess_url(rtsp_client_t rc, rtsp_mediasession_t sess){
	return sess->session_url ? sess->session_url : rc->urlbase;
}
static char* _get_control_url(rtsp_client_t rc, rtsp_subsession_t subsess) {
	const char* url = subsess->control_path;
	// check if the url is an absolute path.
	char* p = strchr(url, ':');
	char* q = strchr(url, '/');
	char str[1024];
	const char* fmt;
	if(p && q && (p < q)) // this is an absolute path, use it.
		return strdup(url);

	url = _get_sess_url(rc, subsess->parent_session);
	fmt = (url[strlen(url)-1] == '/') ? "%s%s" : "%s/%s";
	snprintf(str, 1024, fmt, url, subsess->control_path);
	return strdup(str);
}
static char* _get_setup_info(rtsp_client_t rc, rtsp_subsession_t subsess){
	char info[1024];
	if(0 == subsess->rtp_port){
		int rtp_ch = rc->tcp_channel++;
		int rtcp_ch = rc->tcp_channel++;
		snprintf(info, sizeof(info), "Transport: RTP/AVP/TCP;unicast;interleaved=%d-%d\r\n",
				rtp_ch, rtcp_ch);
	} else {
		snprintf(info, sizeof(info), "Transport: RTP/AVP;unicast;client_port=%d-%d\r\n",
				subsess->rtp_port, subsess->rtp_port + 1);
	}
	return strdup(info);
}
static char* _get_play_info(rtsp_req_t req, rtsp_mediasession_t sess){
	char info[1024];
	int len = 0;
	if(req->play_scale == 1.0f && req->play_scale == sess->curr_scale){
	} else {
		len = snprintf(info, sizeof(info), "Scale: %f\r\n", req->play_scale);
	}
	snprintf(&info[len], sizeof(info) - len, "Range: npt=0.000-\r\n");
	return strdup(info);
}
void rtspclient_send_setup(rtsp_client_t rc, rtsp_subsession_t subsess,
			rtspclient_callback on_setup_done, void *arg) {
	rtsp_req_t req = (rtsp_req_t)calloc(1, sizeof(*req));
	req->seq = rc->sequence ++;
	req->call = on_setup_done;
	req->arg = arg;
	req->subsess = subsess;
	req->cmdname = "SETUP";
	jqueue_push(rc->req_queue, req, 0);
	req->cmdurl = _get_control_url(rc, subsess);
	req->extra = _get_setup_info(rc, subsess);
	_rtspclient_send_commands(rc, req);
}
void rtspclient_send_play(rtsp_client_t rc, rtsp_mediasession_t sess, float scale,
			rtspclient_callback on_play_done, void *arg) {
	rtsp_req_t req = (rtsp_req_t)calloc(1, sizeof(*req));
	req->seq = rc->sequence ++;
	req->call = on_play_done;
	req->arg = arg;
	req->play_scale = scale;
	req->cmdname = "PLAY";
	req->sess = sess;
	jqueue_push(rc->req_queue, req, 0);
	req->cmdurl = strdup(_get_sess_url(rc, sess));
	req->extra = _get_play_info(req, sess);
	_rtspclient_send_commands(rc, req);
}
void rtspclient_send_pause(rtsp_client_t rc, rtsp_mediasession_t sess,
			rtspclient_callback on_pause_done, void *arg){
	rtsp_req_t req = (rtsp_req_t)calloc(1, sizeof(*req));
	req->seq = rc->sequence ++;
	req->call = on_pause_done;
	req->arg = arg;
	req->sess = sess;
	req->cmdname = "PAUSE";
	jqueue_push(rc->req_queue, req, 0);
	req->cmdurl = strdup(_get_sess_url(rc, sess));
	_rtspclient_send_commands(rc, req);
}
void rtspclient_send_teardown(rtsp_client_t rc, rtsp_mediasession_t sess,
			rtspclient_callback on_teardown_done, void *arg){
	rtsp_req_t req = (rtsp_req_t)calloc(1, sizeof(*req));
	req->seq = rc->sequence ++;
	req->call = on_teardown_done;
	req->arg = arg;
	req->sess = sess;
	req->cmdname = "TEARDOWN";
	jqueue_push(rc->req_queue, req, 0);
	req->cmdurl = strdup(_get_sess_url(rc, sess));
	_rtspclient_send_commands(rc, req);
}
void rtspclient_send_rtcp(rtsp_client_t rc, uintptr_t udp_sock, rtsp_context_t rtcp){
	if(-1 == udp_sock){
		// create packet for rtp-over-tcp.
		bio_buf_t buf = bio_buf_dup("$1\0\0", 4);
		buf->head[1] = rtcp->rtp_ctx.channel & 0xFF;
		buf->head[2] = (rtcp->rtp_ctx.rtplen >> 8) & 0xFF;
		buf->head[3] = rtcp->rtp_ctx.rtplen & 0xFF;
		bio_buf_append(buf, rtcp->rtp_ctx.rtp, rtcp->rtp_ctx.rtplen);
		if(0 == bio_send_by_key(rc->client, 0, buf->head, buf->size)){
			buf->heap = NULL;
		}
		bio_buf_free(buf);
	} else {
		// send the packet by protocol of udp.
		unsigned short udp_port = rtcp->rtp_ctx.channel; // use the field as udp port number.
		struct sockaddr_storage addr;
		j_inet_pton(rc->hostip, &addr);
		j_inet_setport(&addr, udp_port);
		sendto(udp_sock, rtcp->rtp_ctx.rtp, rtcp->rtp_ctx.rtplen, 0,
				(const struct sockaddr*)&addr, sizeof(addr));
	}
}
void rtspclient_free(rtsp_client_t rc){
	bio_client_free(rc->client);
	jqueue_free(rc->req_queue);
	j_free(rc->urlbase);
	j_free(rc->hostip);
	j_free(rc->auth_nonce);
	j_free(rc->auth_realm);
	j_free(rc->auth_user);
	j_free(rc->auth_pass);
	j_free(rc->session);
	free(rc);
}

///////////////////////////
static int _guessRtpFrequency(char* codecName, char* mediumName){
	// By default, we assume that audio sessions use a frequency of 8000,
	// video sessions use a frequency of 90000,
	// and text sessions use a frequency of 1000.
	// Begin by checking for known exceptions to this rule
	// (where the frequency is known unambiguously (e.g., not like "DVI4"))
	if (strcmp(codecName, "L16") == 0) return 44100;
	if (strcmp(codecName, "MPA") == 0
	  || strcmp(codecName, "MPA-ROBUST") == 0
	  || strcmp(codecName, "X-MP3-DRAFT-00") == 0) return 90000;

	// Now, guess default values:
	if (strcmp(mediumName, "video") == 0) return 90000;
	else if (strcmp(mediumName, "text") == 0) return 1000;
	return 8000; // for "audio", and any other medium
}
static int _lookupPayloadFormat(rtsp_subsession_t subsess){
	const char* temp = NULL;
	static struct{
		const char* codec;
		unsigned int freq;
		unsigned int nch;
	}formats[] = {
		{"PCMU", 8000, 1}, {NULL, 0, 0},       {"G726-32", 8000, 1},{"GSM", 8000, 1},
		{"G723", 8000, 1}, {"DVI4", 8000, 1},  {"DVI4", 16000, 1},  {"LPC", 8000, 1},
		{"PCMA", 8000, 1}, {"G722", 8000, 1},  {"L16", 44100, 2},   {"L16", 44100, 1},
		{"QCELP", 8000, 1},{NULL, 0, 0},       {"MPA", 90000, 1},   {"G728", 8000, 1},
		{"DVI4", 11025, 1},{"DVI4", 22050, 1}, {"G729", 8000, 1},   {NULL, 0, 0},
		{NULL, 0, 0},      {NULL, 0, 0},       {NULL, 0, 0},        {NULL, 0, 0},
		{NULL, 0, 0},      {"CELB", 90000, 1}, {"JPEG", 90000, 1},  {NULL, 0, 0},
		{"NV", 90000, 1},  {NULL, 0, 0},       {NULL, 0, 0},        {"H261", 90000, 1},
		{"MPV", 90000, 1}, {"MP2T", 90000, 1}, {"H263", 90000, 1}
	};
	if(subsess->rtp_payload_format <= 34 &&
			formats[subsess->rtp_payload_format].codec)
	{
		subsess->media_codec = j_strdup(formats[subsess->rtp_payload_format].codec);
		subsess->rtp_freq = formats[subsess->rtp_payload_format].freq;
		subsess->channels = formats[subsess->rtp_payload_format].nch;
		return 0;
	}
	return -1;
}
static int _complete_subsess(rtsp_subsession_t subsess){
	if(NULL == subsess->media_codec){
		// try assign codec by rtp-payload-format.
		if(_lookupPayloadFormat(subsess)){
			return -1;
		}
	}
	if(0 == subsess->rtp_freq){
		// try guess frequency.
		subsess->rtp_freq = _guessRtpFrequency(subsess->media_codec, subsess->media_name);
	}
	return 0;
}
static void _free_subsession(rtsp_subsession_t subsess){
	xhash_free(subsess->fmtp_attr);
	j_free(subsess->conn_endpoint);
	j_free(subsess->control_path);
	j_free(subsess->media_codec);
	j_free(subsess->media_name);
	j_free(subsess->server_addr);
	j_free(subsess->sessionid);
	free(subsess);
}
void rtspclient_free_mediasession(rtsp_mediasession_t sess){
	rtsp_subsession_t subsess = sess->subsessions;
	rtsp_subsession_t end_sess = sess->subsessions;
	if(subsess) do{
		rtsp_subsession_t next_sess = subsess->next;
		_free_subsession(subsess);
		subsess = next_sess;
	}while(subsess != end_sess);
	j_free(sess->conn_endpoint);
	j_free(sess->description);
	j_free(sess->mediatype);
	j_free(sess->name);
	j_free(sess->session_url);
	free(sess);
}
rtsp_mediasession_t rtspclient_create_mediasession(const char* sdp, int sdplen)
{
	rtsp_mediasession_t sess = calloc(1, sizeof(*sess));
	const char* line_end = sdp - 2;
	const char* sdp_end = sdp + sdplen;
	rtsp_subsession_t subsess = NULL;
	int finding_m_line = 0;
	sess->curr_scale = 1.0f;
	do{
		const char* line = line_end + 2;
		if(sdp_end <= line) break;
		line_end = memmem(line, (sdp_end - line), "\r\n", 2);
		if(!line_end)
			line_end = sdp_end;
		if(line_end == line){
			// this is an empty line.
			continue;
		} else if(line[1] == '=') {
			int value_stolen = 0;
			const char* p = &line[2];
			char* value;
			if(finding_m_line && line[0] != 'm')
				continue;
			// one sdp line.
			while(*p <= ' ' && p != line_end) ++ p;
			value = j_strndup(p, line_end - p);
			switch(line[0]){
			case 'v': // v=0
				if(0 != strcmp(value, "0"))
				{
					// sdp version of no-zero is not supported.
					line_end = NULL;
					j_free(value);
					goto bad_sdp;
				}
				break;
			case 's': // s=<session name>
				sess->name = value;
				value_stolen = 1;
				break;
			case 'i': // i=<session description>
				sess->description = value;
				value_stolen = 1;
				break;
			case 'c': // c=IN IP4 <connection-endpoint> or c=IN IP4 <connection-endpoint>/<ttl+numAddresses>
			{
				if(0 == strncmp(value, "IN IP4 ", 7)){
					char* endpoint = value + 7;
					while(*endpoint <= ' ' && *endpoint != '\0' && *endpoint != '/') ++ endpoint;
					if(*endpoint){
						char* q = strchr(endpoint, '/');
						char ** tgt = subsess ? &subsess->conn_endpoint : &sess->conn_endpoint;
						*tgt = q ? j_strndup(endpoint, q - endpoint) : j_strdup(endpoint);
					}
				}
				break;
			}
			case 'b':
			{
				// b=<bwtype>:<bandwidth> with bwtype is 'AS' in RTP.
				if(!subsess)
					break;
				sscanf(value, "AS:%u", &subsess->bandwidth);
				break;
			}
			case 'a': //a=control:xxxxx or a=type:xxxxx etc.
			{
				// NOTICE: other attributes are not handled here.
				if(0 == strncmp(value, "control:", 8)){
					// a=control:xxxxx
					char* ctrl = _ltrim(value + 8);
					char **tgt = subsess ? &subsess->control_path : &sess->session_url;
					*tgt = j_strdup(ctrl);
				} else if(!subsess && 0 == strncmp(value, "type:", 5)){
					// a=type:xxxx
					char* type = _ltrim(value + 5);
					sess->mediatype = j_strdup(type);
				} else if(subsess) {
					if(0 == strncmp(value, "rtpmap:", 7)){
						//a=rtpmap:<fmt> <codec>[/<freq>[/<numChannels>]]
						unsigned int fmt = atoi(value + 7);
						char *q = strchr(_ltrim(value + 7), ' ');
						if(fmt == subsess->rtp_payload_format && NULL != q){
							char* s,*codec;
							unsigned int freq = 0;
							unsigned int channels = 1;
							q = _ltrim(q);
							s = strchr(q, '/');
							codec = s ? j_strndup(q, s - q) : j_strdup(q);
							if(s){
								char * end = NULL;
								freq = strtoul(s + 1, &end, 10);
								if(end && *end == '/'){
									channels = strtoul(end + 1, NULL, 10);
								}
							}
							q = codec;
							while(*q) { *q = toupper(*q); ++q;};
							subsess->media_codec = codec;
							subsess->channels = channels;
							subsess->rtp_freq = freq;
						}
 					} else if(0 == strncmp(value, "rtcp-mux", 8)){
 						// not supported yet.
 					} else if(0 == strncmp(value, "fmtp:", 5)){
 						// check payload number first.
 						unsigned int fmt = atoi(value + 5);
 						char *q = strchr(_ltrim(value + 5), ' ');
 						if(fmt == subsess->rtp_payload_format && q){
 	 						// get extra attributes here. set them to a xht.
 							// <name>; or <name>=<value>;
 							subsess->fmtp_attr = _generate_xht(q, ';', '=');
 						}
					}
				}
				break;
			}
			case 'm':
			{
				char* q;
				unsigned short port;
				unsigned payloadFormat;
				char *media_name = NULL;
				char const* protocolName = NULL;
				// this begins a new media.
				// complete last subsess first.
				if(subsess && _complete_subsess(subsess))
					goto bad_sdp;
				q = strchr(value, ' ');
				if(q){
					// now parse the m= line to get the media name and protocol.
					media_name = j_strndup(value, q - value);
					port = strtoul(q + 1, NULL, 10);
					q = _ltrim(strchr(_ltrim(q), ' ')); // find out protocol
					if(!q){
					} else if(0 == strncmp(q, "RTP/AVP ", 8)){
						payloadFormat = strtoul(q + 8, NULL, 10);
						protocolName = "RTP";
					} else if(0 == strncasecmp(q, "UDP ", 4) || 0 == strncmp(q, "RAW/RAW/UDP ", 12)){
						payloadFormat = strtoul(strchr(q, ' '), NULL, 10);
						if(payloadFormat <=127) protocolName = "UDP";
					}
				}
				if(!protocolName){
					// no protocol given .
					// or invalid 'm=' line. need to find next 'm=' line.
					finding_m_line = 1;
					break;
				}
				finding_m_line = 0;
				subsess = (rtsp_subsession_t)calloc(1, sizeof(* subsess));
				subsess->parent_session = sess;
				subsess->media_name = media_name;
				subsess->protocol = protocolName;
				subsess->port = port;
				subsess->rtp_payload_format = payloadFormat;
				// now insert the subsession to its parent media session.
				if(!sess->subsessions){
					sess->subsessions = subsess;
					subsess->next = subsess->prev = subsess;
				} else {
					subsess->next = sess->subsessions;
					subsess->prev = sess->subsessions->prev;
					sess->subsessions->prev->next = subsess;
					sess->subsessions->prev = subsess;
				}
				break;
			}
			default:
				break;
			}
			if(!value_stolen)
				j_free(value);
		}
	}while(line_end < sdp_end);

	// complete the last sub-session.
	if(!subsess || _complete_subsess(subsess))
		goto bad_sdp;
	return sess;
bad_sdp:
	// clean all subsessions with mediasession created.
	rtspclient_free_mediasession(sess);
	return NULL;
}
