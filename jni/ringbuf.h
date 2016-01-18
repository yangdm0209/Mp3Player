#ifndef _RINGBUFFER_H__
#define _RINGBUFFER_H__

#include <pthread.h>
#include <jni.h>

typedef struct ringbuf_tag {
	char *buf;
	char *w_ptr;
	char *r_ptr;
	unsigned int size;
	unsigned int full;
    pthread_mutex_t mutex;
} ringbuf;


int rb_init(ringbuf *rb, unsigned int size);

int rb_filled(ringbuf *rb);

int rb_space(ringbuf *rb);

unsigned int rb_read(ringbuf *rb, char *dest, unsigned int len);

int rb_write(ringbuf *rb, char* src, unsigned int size);

int rb_free(ringbuf *rb);

#endif /*_RINGBUFFER_H__*/
