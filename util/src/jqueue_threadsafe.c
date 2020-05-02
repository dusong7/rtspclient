/*
 * jqueue_threadsafe.c
 *
 *  Created on: 2016年11月10日
 *      Author: WangZhen
 */


#include <util/util.h>
#include <pthread.h>

typedef struct _jqueue_threadsafe_st{
	struct _jqueue_st queue;
	pthread_mutex_t lock;
	pthread_cond_t cond;
	int exiting;
}*jqueue_threadsafe_t;

jqueue_t jqueue_new_safe(){
	pool_t p;
	jqueue_t q;

	p = pool_new();
	q = (jqueue_t) pmalloco(p, sizeof(struct _jqueue_threadsafe_st));
	q->p = p;
	q->init_time = time(NULL);
	pthread_mutex_init(&((jqueue_threadsafe_t)q)->lock, NULL);
	pthread_cond_init(&((jqueue_threadsafe_t)q)->cond, NULL);
	pool_cleanup(p, (pool_cleanup_t)pthread_mutex_destroy, &((jqueue_threadsafe_t)q)->lock);
	pool_cleanup(p, (pool_cleanup_t)pthread_cond_destroy, &((jqueue_threadsafe_t)q)->cond);
	return q;
}

void* jqueue_pull_safe(jqueue_t q, int wait){
	void *ret;
	pthread_mutex_lock(&((jqueue_threadsafe_t)q)->lock);
	ret = jqueue_pull(q);
	if(wait){
		if(wait == -1){
			while(!ret && 0 == ((jqueue_threadsafe_t)q)->exiting){
				pthread_cond_wait(&((jqueue_threadsafe_t)q)->cond, &((jqueue_threadsafe_t)q)->lock);
				ret = jqueue_pull(q);
			}
		} else if(!ret){
			struct timespec ts;
			j_clock_gettime(J_CLOCK_REALTIME, &ts);
			ts.tv_sec += wait / 1000;
			ts.tv_nsec += (wait%1000) * 1000000;
			if(ts.tv_nsec > 1000000000){
				ts.tv_sec ++;
				ts.tv_nsec -= 1000000000;
			}
			pthread_cond_timedwait(&((jqueue_threadsafe_t)q)->cond, &((jqueue_threadsafe_t)q)->lock, &ts);
			ret = jqueue_pull(q);
		}
	}
	if(!ret && ((jqueue_threadsafe_t)q)->exiting)
		((jqueue_threadsafe_t)q)->exiting = 0;
	pthread_mutex_unlock(&((jqueue_threadsafe_t)q)->lock);
	return ret;
}

void jqueue_push_safe(jqueue_t q, void* v, int pri){
	pthread_mutex_lock(&((jqueue_threadsafe_t)q)->lock);
	jqueue_push(q, v, pri);
	pthread_cond_signal(&((jqueue_threadsafe_t)q)->cond);
	pthread_mutex_unlock(&((jqueue_threadsafe_t)q)->lock);
}

void jqueue_cancel_wait(jqueue_t q){
	pthread_mutex_lock(&((jqueue_threadsafe_t)q)->lock);
	((jqueue_threadsafe_t)q)->exiting = 1;
	pthread_cond_signal(&((jqueue_threadsafe_t)q)->cond);
	pthread_mutex_unlock(&((jqueue_threadsafe_t)q)->lock);
}

int jqueue_size_safe(jqueue_t q){
	int s ;
	pthread_mutex_lock(&((jqueue_threadsafe_t)q)->lock);
	s = jqueue_size(q);
	pthread_mutex_unlock(&((jqueue_threadsafe_t)q)->lock);
	return s;
}

void *jqueue_peek_safe(jqueue_t q){
	void* v;
	pthread_mutex_lock(&((jqueue_threadsafe_t)q)->lock);
	v = jqueue_peek(q);
	pthread_mutex_unlock(&((jqueue_threadsafe_t)q)->lock);
	return v;
}
void jqueue_remove_safe(jqueue_t q, void *data){
	pthread_mutex_lock(&((jqueue_threadsafe_t)q)->lock);
	jqueue_remove(q, data);
	pthread_mutex_unlock(&((jqueue_threadsafe_t)q)->lock);
}
int jqueue_locate_safe(jqueue_t q, void *data){
	int s;
	pthread_mutex_lock(&((jqueue_threadsafe_t)q)->lock);
	s = jqueue_locate(q, data);
	pthread_mutex_unlock(&((jqueue_threadsafe_t)q)->lock);
	return s;
}
