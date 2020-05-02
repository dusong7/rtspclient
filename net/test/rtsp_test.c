#include "../app/rtsp.h"

struct JobContext {
	rtsp_client_t rc;
	rtsp_mediasession_t sess;
	rtsp_subsession_t subsess;
};
static int ref;
int cb(rtsp_client_t rc, rtsp_context_t ctx, void *arg)
{
	-- ref;
	return 0;
}
int cb_teardown(rtsp_client_t rc, rtsp_context_t ctx, void *arg){
	cb(rc, ctx, arg);
	rtspclient_free_mediasession((rtsp_mediasession_t)arg);
	return 0;
}
int delay_teardown(reactor_t r, reactor_timer_t t, void *arg){
	struct JobContext * info = (struct JobContext * )arg;
	rtspclient_send_teardown(info->rc, info->sess, cb_teardown, info->sess);
	reactor_timer_free(r, t);
	free(info);
	return 0;
}
int cb_play(rtsp_client_t rc, rtsp_context_t ctx, void *arg){
	cb(rc, ctx, arg);
	rtsp_mediasession_t sess = (rtsp_mediasession_t)arg;
	// after a while
	struct JobContext * info = (struct JobContext *) calloc(1, sizeof(*info));
	info->rc = rc;
	info->sess = sess;
	reactor_timer_t t = reactor_timer_create(rc->client->r, delay_teardown, info);
	reactor_timer_add(rc->client->r, t, 200000);
	++ ref;
	return 0;
}
int cb_setup(rtsp_client_t rc, rtsp_context_t ctx, void *arg){
	cb(rc, ctx, arg);
	struct JobContext* jctx = (struct JobContext* )arg;
	if(jctx->subsess->next != jctx->sess->subsessions){
		jctx->subsess = jctx->subsess->next;
		rtspclient_send_setup(rc, jctx->subsess, cb_setup, arg);
		++ ref;
	} else {
		// all subsessions have done setting up.
		// now start playing.
		rtspclient_send_play(rc, jctx->sess, 1.0f, cb_play, jctx->sess);
		++ ref;
		free(jctx);
	}
	return 0;
}
int cb_describe(rtsp_client_t rc, rtsp_context_t ctx, void *arg)
{
	cb(rc, ctx, arg);
	// get sdp info from payload.
	rtsp_mediasession_t sess = rtspclient_create_mediasession(ctx->rtsp_ctx.payload, ctx->rtsp_ctx.payload_len);
	if(sess){
		// now begin stream setup.
		rtsp_subsession_t subsess = sess->subsessions;
		struct JobContext* ctx = calloc(1, sizeof(*ctx));
		ctx->rc = rc;
		ctx->sess = sess;
		ctx->subsess = subsess;
		rtspclient_send_setup(rc, subsess, cb_setup, ctx);
		++ref;
	}
	return 0;
}

int on_rtp(rtsp_client_t rc, rtsp_context_t ctx, void *arg)
{
	time_t now = time(NULL);
	printf(".");
	fflush(stdout);
	return 0;
}
int main(int argc, char *argv[])
{
	reactor_t r = reactor_create(NULL);
	rtsp_client_t rc1 = rtspclient_create(r, "rtsp://172.16.10.42/h264/ch1/main/av_stream", "admin", "fhjt12345");
	rtspclient_on_rtp_call(rc1, on_rtp, NULL);
	rtspclient_send_options(rc1, cb, NULL);
	rtspclient_send_describe(rc1, cb_describe, NULL);
	++ ref;
	++ ref;
	while(ref > 0){
		reactor_loop_once(r, 1000);
	}
	rtspclient_free(rc1);
	reactor_free(r);
	return 0;
}

