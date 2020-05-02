/*
 * reactor.c
 * reactor creator
 *
 *  Created on: 2014年10月21日
 *      Author: wangzhen
 */

#include <stdio.h>
#include "reactor.h"
#include <event2/event.h>
#include <string.h>

reactor_t reactor_event_create(struct event_base *eb);

reactor_t reactor_create(const char* type){
	reactor_t r = NULL;
	//r = reactor_epoll_create();
	//if(!r) return r;
	if(!type || '\0' == type[0] || 0 == strcmp(type, "event"))
		r = reactor_event_create(NULL);
	if(!r) return r;
	//r = reactor_ev_create();
	//if(!r) return r;
	//r = reactor_select_create();
	//if(!r) return r;
	return r;
}
