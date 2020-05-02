/*
 * jarray.c
 *
 *  Created on: 2014年12月13日
 *      Author: wangz
 */

#include <util/util.h>

jarray_t jarray_new(){
	pool_t p = pool_new();
	jarray_t arr = pmalloco(p, sizeof(struct J_array_st));
	arr->p = p;
	arr->capacity = 10;
	arr->zen = pmalloco(p, 10 * sizeof(void*));
	return arr;
}

jarray_t jarray_new_p(pool_t p){
	jarray_t arr = pmalloco(p, sizeof(struct J_array_st));
	arr->p = p;
	arr->capacity = 10;
	arr->zen = pmalloco(p, 10 * sizeof(void*));
	return arr;
}

void jarray_clear(jarray_t arr){
	arr->size = 0;
}

void jarray_free(jarray_t arr){
	pool_free(arr->p);
}

int jarray_size(jarray_t arr){
	return arr->size;
}

void jarray_push(jarray_t arr, void *value){
	if(arr->size == arr->capacity){
		void *old_zen = (void *)arr->zen;
		arr->capacity += 64;
		arr->zen = pmalloco(arr->p, arr->capacity * arr->size);
		memcpy(arr->zen, old_zen, sizeof(void*) * arr->size);
		pfree(arr->p, old_zen);
	}
	arr->zen[arr->size] = value;
	arr->size ++;
}

void jarray_push_at(jarray_t arr, int pos, void *value)
{
	if(pos < 0){
		pos += arr->size + 1;
	}
	if(pos < 0){
		pos = 0;
	}
	if(pos > arr->size)
		pos = arr->size;

	if(arr->size == arr->capacity){
		void *old_zen = (void *)arr->zen;
		arr->capacity += 64;
		arr->zen = pmalloco(arr->p, arr->capacity);
		memcpy(arr->zen, old_zen, sizeof(void*) * arr->size);
		pfree(arr->p, old_zen);
	}
	if(pos != arr->size){
		memmove(&arr->zen[pos + 1], &arr->zen[pos], sizeof(void*) * (arr->size - pos));
	}
	arr->zen[pos] = value;
	arr->size ++;
}

void *jarray_get(jarray_t arr, int pos)
{
	if(pos < 0) pos += arr->size;
	if(pos < 0) return NULL;
	if(pos >= arr->size) return NULL;
	return arr->zen[pos];
}

void jarray_put(jarray_t arr, int pos, void *value)
{
	if(pos < 0) pos += arr->size;
	if(pos < 0) pos = 0;
	if(pos >= arr->size){
		while(pos > arr->size)
			jarray_push(arr, NULL);
		jarray_push(arr, value);
	} else {
		arr->zen[pos] = value;
	}
}

int jarray_find(jarray_t arr, void *value){
	int i;
	for(i = 0; i < arr->size; ++i){
		if(arr->zen[i] == value)
			return i;
	}
	return -1;
}

void *jarray_pop_at(jarray_t arr, int pos)
{
	void *value;
	if(pos < 0){
		pos += arr->size;
	}
	if(pos < 0){
		return NULL;
	}
	if(pos >= arr->size)
		return NULL;

	value = arr->zen[pos];
	if(pos != arr->size - 1){
		memmove(&arr->zen[pos], &arr->zen[pos + 1], sizeof(void*) * (arr->size - pos));
	}
	arr->size --;
	return value;
}
