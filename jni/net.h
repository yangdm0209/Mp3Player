#ifndef _NET_H__
#define _NET_H__

#include <unistd.h>
#include "ringbuf.h"
#include "util.h"

/* thread status */
#define THREAD_RUNNING 1 // thread is running
#define THREAD_STOPING 2 // thread is stoping
#define THREAD_STOPED 3 // thread has stoped

/* return value */
#define SUCCESS 0
#define SOCK_ERROR -1
#define CALLOC_ERROR -2
#define URL_ERROR -3


typedef int sock_t;

typedef struct http_client_tag{
	sock_t sock;
    util_dict_t* http_parser;
	int resp_code;
	int err_code;
	ringbuf recv_data;
	int metaInterval;
	int recv_thread_sts;
//	char* song_title;
//    void (*exception)(int code, const char* message);
//    void (*title_change)(const char* title);
}http_client_t;

int http_connect(http_client_t* client, const char* url);

int http_close(http_client_t* client);

void* http_recv_thread(void *arg);

int parse_url(const char* url, char* ip, int* port, char* path);

int get_ip(char* host, char* ip);

int read_http_header(sock_t sock, char* buff, int len);
int parse_httpp_response(util_dict_t *parser, const char *http_data, unsigned long len);


/* Connection related socket functions */
sock_t sock_connect(const char *hostname, const int port);

/* Socket write functions */
int sock_write_bytes(sock_t sock, const void *buff, const size_t len);

/* Socket read functions */
int sock_read_bytes(sock_t sock, char *buff, const int len);

/* Socket close functions */
int sock_close(sock_t  sock);

#endif  /* _NET_H__ */
