
#include "binary_io.h"

#define CHUNKSIZE 1024
static int _bio_chunk_size(int newsize)
{
	return CHUNKSIZE * ((newsize + CHUNKSIZE - 1) / CHUNKSIZE);
}

bio_buf_t bio_buf_dup(const void* src, int size){
	bio_buf_t b = calloc(1, sizeof(struct bio_buf_st));
	int chunked = _bio_chunk_size(size);
	b->capacity = chunked;
	if(chunked){
		b->tail = b->head = b->heap = malloc(chunked);
		if(src){
			memcpy(b->head, src, size);
			b->size = size;
			b->tail = b->head + size;
		}
	}
	return b;
}

bio_buf_t bio_buf_new(void* src, int size){
	bio_buf_t b = calloc(1, sizeof(struct bio_buf_st));
	if(src){
		b->head = b->heap = (char *)src;
		b->capacity = b->size = size;
		b->tail = b->head + size;
	} else {
		int chunked = _bio_chunk_size(size);
		b->capacity = chunked;
		if(chunked){
			b->tail = b->head = b->heap = malloc(chunked);
		}
	}
	return b;
}

void bio_buf_free(bio_buf_t b){
	if(b->heap)
		free(b->heap);
	free(b);
}

/*we will not malloc another  if this buf is enough */
int bio_buf_extend(bio_buf_t b, int grow){
	int begin = b->head - b->heap;
	int newsize;
	if(begin > b->capacity / 2){
		memmove(b->heap, b->head, b->size);
		b->head = b->heap;
		b->tail = b->head + b->size;
		begin = 0;
	}
	newsize = begin + b->size + grow;
	if(newsize > b->capacity){
		char* tmp;
		newsize = _bio_chunk_size(newsize);
		tmp = realloc(b->heap, newsize);
		while(!tmp){
			usleep(10000);
			tmp = realloc(b->heap, newsize);
		}
		b->heap = tmp;
		b->head = b->heap + begin;
		b->tail = b->head + b->size;
		b->capacity = newsize;
	}
	return 1;
}
void bio_buf_append(bio_buf_t b, const void* src, int len)
{
	bio_buf_extend(b, len);
	memcpy(b->tail, src, len);
	b->tail += len;
	b->size += len;
}


/* use < 0  eat char from the tail to header*/
/* use > 0  eat char from the header to tail*/
void bio_buf_consume(bio_buf_t b, int use)
{
	if(use == 0){
		if(b->tail != b->heap + b->capacity)
			*(b->tail) = '\0'; // complete the string (give the terminal '\0' char).
		return;
	}
	if(use < 0){
		/* eat char from the tail to header */
		b->size += use;
		if(b->size < 0){
			b->head = b->tail = b->heap;
			b->size = 0;
		} else {
			b->tail -= -use;
		}
		*(b->tail) = '\0'; // also truncate the string.
		return;
	}
	b->size -= use;
	if(b->size <= 0){
		b->head = b->tail = b->heap;
		b->size = 0;
	} else {
		b->head += use;
	}
}
