#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "ringbuf.h"

int rb_init(ringbuf *rb, unsigned int size){
	rb->buf = (char*)malloc(size * sizeof(char));
	if(!rb->buf)
		return -1;

	memset(rb->buf, 0, size*sizeof(char));
	rb->r_ptr = rb->buf;
	rb->w_ptr = rb->buf;
	rb->size = size;
	rb->full = 0;

	pthread_mutex_init(&rb->mutex, NULL);

	return 0;
}

int rb_filled(ringbuf *rb){
	int filled;
	char *end_ptr;

	if(rb->w_ptr == rb->r_ptr && rb->full)
		filled = rb->size;
	else if(rb->w_ptr == rb->r_ptr && !rb->full)
		filled = 0;
	else if(rb->w_ptr > rb->r_ptr)
		filled = rb->w_ptr - rb->r_ptr;
	else
	{
		end_ptr = rb->buf + rb->size;
		filled = end_ptr - rb->r_ptr;
		filled += rb->w_ptr - rb->buf;
	}

	return filled;
}

int rb_space(ringbuf *rb){
	int space;
	char *end_ptr;

	if(rb->r_ptr == rb->w_ptr && rb->full)
		space = 0;
	else if(rb->r_ptr == rb->w_ptr && !rb->full)
		space = rb->size;
	else if(rb->r_ptr > rb->w_ptr)
		space = rb->r_ptr - rb->w_ptr;
	else
	{
		end_ptr = rb->buf + rb->size;
		space = end_ptr - rb->w_ptr;
		space += rb->r_ptr - rb->buf;
	}

	return space;
}

unsigned int rb_read(ringbuf *rb, char *dest, unsigned int len){
	char *end_ptr;

	if(!dest || !rb->buf)
		return 0;
	if(len > rb->size)
		return 0;

	pthread_mutex_lock(&rb->mutex);

	end_ptr = rb->buf + rb->size;
	//len = rb_filled(rb);

	if(rb->r_ptr + len < end_ptr ) {
		memcpy(dest, rb->r_ptr, len);
		rb->r_ptr += len;
	}
	/*buf content crosses the start point of the ring*/
	else {
		/*copy from r_ptr to start of ringbuffer*/
		memcpy(dest, rb->r_ptr, end_ptr - rb->r_ptr);
		/*copy from start of ringbuffer to w_ptr*/
		memcpy(dest + (end_ptr - rb->r_ptr), rb->buf, len - (end_ptr - rb->r_ptr));
		rb->r_ptr = rb->buf + (len - (end_ptr - rb->r_ptr));
	}

	pthread_mutex_unlock(&rb->mutex);

	if(rb->w_ptr == rb->r_ptr)
		rb->full = 0;

	return len;
}

int rb_write(ringbuf *rb, char* src, unsigned int len){
	char *end_ptr;

	if(!src || !rb->buf)
		return -1;
	if(len > rb->size)
		return -1;
	if(len == 0)
		return 0;

	pthread_mutex_lock(&rb->mutex);

	end_ptr = rb->buf + rb->size;

	if(rb->w_ptr + len < end_ptr ) {
		memcpy(rb->w_ptr, src, len);
		rb->w_ptr += len;
	}
	else {
		memcpy(rb->w_ptr, src, end_ptr - rb->w_ptr);
		memcpy(rb->buf, src + (end_ptr - rb->w_ptr), len - (end_ptr - rb->w_ptr));
		rb->w_ptr = rb->buf + (len - (end_ptr - rb->w_ptr));
	}

	if(rb->w_ptr == rb->r_ptr)
		rb->full = 1;

	pthread_mutex_unlock(&rb->mutex);

	return 0;
}

int rb_free(ringbuf *rb){
	if (NULL != rb->buf){
		free(rb->buf);
		rb->buf = NULL;
	}
	pthread_mutex_destroy(&rb->mutex);
	return 0;
}
