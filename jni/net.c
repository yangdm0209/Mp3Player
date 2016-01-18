#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <netdb.h>
#include <string.h>
#include <pthread.h>

#include "net.h"
#include "util.h"

#define DEAULT_PORT 80
#define RING_BUFFER_SIZE 960000
#define RECV_BUFFER_SIZE 4096
#define MAX_HEADERS 32


#define HTTP_VERSION "http-version"
#define HTTP_CODE "http-code"
#define HTTP_MESSAGE "http-message"


static int g_recv_running = 0;
static pthread_t g_recv_tid = 0;

const char* HTTP_REQUEST =
        "%s %s HTTP/1.1\r\nHost: %s:%d\r\nConnection: keep-alive\r\nUser-Agent: MAD-DecodeStreamPlayer/1.0 (Android) dashuqinzi\r\n"
        "Icy-MetaData:1\r\n\r\n";

int http_connect(http_client_t* client, const char* url) {
	int len = 0;
	int ret = 0;
	char ip[17];
	char path[255];
	int port;
	char buffer[RECV_BUFFER_SIZE];
	char* headerEnd = NULL;

	LOGI("start http connect, url is: %s", url);

	memset(ip, 0, sizeof(ip));
	memset(path, 0, sizeof(path));

	rb_init(&client->recv_data, RING_BUFFER_SIZE);
    client->http_parser = util_dict_new();

	if (parse_url(url, ip, &port, path) != 0) {
		http_close(client);
		return URL_ERROR;
	}

	LOGI("request ip is: %s", ip);
	LOGI("request port is: %d", port);
	LOGI("request path is: %s", path);

	client->sock = sock_connect(ip, port);
	if (client->sock == SOCK_ERROR) {
		http_close(client);
		return SOCK_ERROR;
	}

	snprintf(buffer, RECV_BUFFER_SIZE, HTTP_REQUEST, "GET", path, ip, port);
	LOGI("http request header is: %s", buffer);
	len = strlen(buffer);

	ret = sock_write_bytes(client->sock, buffer, len);
	if (ret < 0) {
		LOGE("send http request error");
		http_close(client);
		return SOCK_ERROR;
	}

	memset(buffer, 0, RECV_BUFFER_SIZE);
	ret = read_http_header(client->sock, buffer, RECV_BUFFER_SIZE);
	if (ret < 0) {
		LOGE("read http header error");
		http_close(client);
		return SOCK_ERROR;
	}

	LOGI("http response header: ====%s====", buffer);
	parse_httpp_response(client->http_parser, buffer, strlen(buffer));

	const char* resp_code = util_dict_get(client->http_parser, HTTP_CODE);
	if (NULL != resp_code)
		client->resp_code = atoi(resp_code);
	if (client->resp_code != 200) {
		LOGE("response error %d", client->resp_code);
		http_close(client);
		return SOCK_ERROR;
	}

	const char* metaint = util_dict_get(client->http_parser, "icy-metaint");
	if (NULL != metaint)
		client->metaInterval = atoi(metaint);

	LOGI("metaInterval is: %d", client->metaInterval);

	// start recv stream data thread
	pthread_create(&g_recv_tid, NULL, &http_recv_thread, client);

	return SUCCESS;
}

int http_close(http_client_t* client) {

	LOGI("http close release memory");

	if (!client){
		return -1;
	}

	void* thread_val;
	g_recv_running = THREAD_STOPING;
	pthread_join(g_recv_tid, &thread_val);

	sock_close(client->sock);
	client->sock = 0;
	rb_free(&client->recv_data);
    util_dict_free(client->http_parser);

    LOGI("release http connect over");

	return SUCCESS;
}

void* http_recv_thread(void *arg)
{
	http_client_t* client = (http_client_t*)arg;
	int allDataLen = client->metaInterval ? client->metaInterval : RECV_BUFFER_SIZE;
	char metaByte;
	int metaLen;
	int ret = 0;
	char recvData[allDataLen];
	int recvDataLen = 0;

	g_recv_running = THREAD_RUNNING;
	client->recv_thread_sts = THREAD_RUNNING;
	client->err_code = SUCCESS;

	LOGI("start recv data");

	while (THREAD_RUNNING == g_recv_running) {
		memset(recvData, 0, sizeof(recvData));
		LOGI("test %d, %d, %d", client->sock, recvData[0], allDataLen);
		ret = recv(client->sock, recvData, allDataLen, 0);
		if (ret <= 0) {
			LOGE("recv data error %d", ret);
			client->err_code=SOCK_ERROR;
			break;
		}
		while ((rb_space(&client->recv_data) < ret) &&
				(THREAD_RUNNING == g_recv_running)) {
			LOGI("ring buff is not enough, wait 1 sec");
			usleep(500000);
		}
		rb_write(&client->recv_data, recvData, ret);
	}

	LOGI("stop recv data");

	g_recv_running = THREAD_STOPED;
	client->recv_thread_sts = THREAD_STOPED;
	g_recv_tid = 0;
	pthread_exit(0);
	return NULL;
}

int parse_url(const char* _url, char* ip, int* port, char* path)
{
	// http(s)://host:port/XXX.mp3
	char *hoststart = NULL;
	char *hostend = NULL;
	char *portstart = NULL;
	char *portend = NULL;
	char *pathstart = NULL;
	char *url = calloc(1, strlen(_url)+1);
	strcpy(url, _url);

	LOGI("start parse_url: %s", url);

	hoststart = strstr(url, "://");
	if (hoststart == NULL)
		hoststart = url;
	else
		hoststart += 3;

	portstart = strchr(hoststart, ':');
	if (portstart == NULL) {
		hostend = strchr(hoststart, '/');
		*port = DEAULT_PORT;
	} else {
		hostend = portstart;
		portstart++;
		portend = strchr(portstart, '/');
	}

	pathstart = strchr(hoststart, '/');

	if (NULL == hostend)
		get_ip(hoststart, ip);
	else {
		char tmp = *hostend;
		*hostend = '\0';
		get_ip(hoststart, ip);
		*hostend = tmp;
	}

	if (NULL != portstart) {
		if (NULL == portend) {
			*port = atoi(portstart);
		} else {
			char tmp = *portend;
			*portend = '\0';
			*port = atoi(portstart);
			*portend = tmp;
		}
	}

	if (NULL == pathstart)
		pathstart = "/";
	strcpy(path, pathstart);

	free(url);
	return 0;
}

int get_ip(char* host, char* ip) {
	struct hostent *he;
	struct in_addr **addr_list;

	if ((he = gethostbyname(host)) == NULL) { // get the host info
		LOGE("host(%s) is disable", host);
		return -1;
	}

	addr_list = (struct in_addr **) he->h_addr_list;
	strcpy(ip, inet_ntoa(*addr_list[0]));
	return 0;
}

int read_http_header(sock_t sock, char* buff, int len) {
	int read_bytes, ret;
	unsigned long pos;
	char c;

	read_bytes = 1;
	pos = 0;
	ret = -1;

	while ((read_bytes == 1) && (pos < (len - 1))) {
		read_bytes = 0;

		if ((read_bytes = recv(sock, &c, 1, 0))) {
			if (c != '\r')
				buff[pos++] = c;

			if ((pos > 1) && (buff[pos - 1] == '\n' && buff[pos - 2] == '\n')) {
				ret = 0;
				break;
			}
		}
	}

	if (ret == 0)
		buff[pos] = '\0';

	return ret;
}

static int split_headers(char *data, unsigned long len, char **line) {
	/* first we count how many lines there are
	 ** and set up the line[] array
	 */
	int lines = 0;
	unsigned long i;
	line[lines] = data;
	for (i = 0; i < len && lines < MAX_HEADERS; i++) {
		if (data[i] == '\r')
			data[i] = '\0';
		if (data[i] == '\n') {
			lines++;
			data[i] = '\0';
			if (lines >= MAX_HEADERS)
				return MAX_HEADERS;
			if (i + 1 < len) {
				// 连着2个\n，表示http头结束
				if (data[i + 1] == '\n' || data[i + 1] == '\r')
					break;
				line[lines] = &data[i + 1];
			}
		}
	}
	return lines;
}

static void parse_headers(util_dict_t *parser, char **line, int lines) {
	int i, l;
	int whitespace, slen;
	char *name = NULL;
	char *value = NULL;

	/* parse the name: value lines. */
	for (l = 1; l < lines; l++) {
		whitespace = 0;
		// 得到key
		name = line[l];
		value = NULL;
		slen = strlen(line[l]);
		for (i = 0; i < slen; i++) {
			if (line[l][i] == ':') {
				whitespace = 1;
				line[l][i] = '\0';
			} else {
				if (whitespace) {
					whitespace = 0;
					// 去掉空格
					while (i < slen && line[l][i] == ' ')
						i++;
					// 得到value
					if (i < slen)
						value = &line[l][i];
					break;
				}
			}
		}

		if (name != NULL && value != NULL) {
			util_dict_set(parser, name, value);
			name = NULL;
			value = NULL;
		}
	}
}

int parse_httpp_response(util_dict_t *parser, const char *http_data,
		unsigned long len) {
	char *data;
	char *line[MAX_HEADERS];
	int lines, slen, i, whitespace = 0, where = 0, code;
	char *version = NULL, *resp_code = NULL, *message = NULL;

	if (http_data == NULL)
		return 0;

	/* make a local copy of the data, including 0 terminator */
	data = (char *) malloc(len + 1);
	if (data == NULL)
		return 0;
	memcpy(data, http_data, len);
	data[len] = 0;

	lines = split_headers(data, len, line);

	/* In this case, the first line contains:
	 * VERSION RESPONSE_CODE MESSAGE, such as HTTP/1.0 200 OK
	 */
	slen = strlen(line[0]);
	version = line[0];
	for (i = 0; i < slen; i++) {
		if (line[0][i] == ' ') {
			line[0][i] = 0;
			whitespace = 1;
		} else if (whitespace) {
			whitespace = 0;
			where++;
			if (where == 1)
				resp_code = &line[0][i];
			else {
				message = &line[0][i];
				break;
			}
		}
	}
	if (version == NULL || resp_code == NULL || message == NULL) {
		free(data);
		return -1;
	}

	util_dict_set(parser, HTTP_VERSION, version);
	util_dict_set(parser, HTTP_CODE, resp_code);
	code = atoi(resp_code);
	if (code < 200 || code >= 300) {
		util_dict_set(parser, HTTP_MESSAGE, message);
	}

	parse_headers(parser, line, lines);

	free(data);
	return 0;
}

/* Connection related socket functions */
sock_t sock_connect(const char *hostname, const int port) {
	sock_t sock;
	struct sockaddr_in server;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == SOCK_ERROR)
		return SOCK_ERROR;

	memset(&server, 0, sizeof(struct sockaddr_in));
	server.sin_family = AF_INET;
	inet_pton(AF_INET, hostname, &server.sin_addr);
	server.sin_port = htons(port);

	if (connect(sock, (struct sockaddr*) &server, sizeof(server)) < 0) {
		sock_close(sock);
		return SOCK_ERROR;
	}
	return sock;
}

/* Socket write functions */
int sock_write_bytes(sock_t sock, const void *buff, const size_t len) {
	int send_num = 0;
	int ret = 0;
	while (send_num < len) {
		ret = send(sock, buff + send_num, len - send_num, 0);
		if (ret < 0)
			break;
		send_num += ret;
	}

	return send_num == len ? 0 : -1;
}

/* Socket read functions */
int sock_read_bytes(sock_t sock, char *buff, const int len) {
	int recv_num = 0;
	int ret = 0;
	while (recv_num < len) {
		ret = recv(sock, buff + recv_num, len - recv_num, 0);
		if (ret <= 0)
			break;
		recv_num += ret;
	}

	return recv_num == len ? 0 : -1;
}

/* Socket close functions */
int sock_close(sock_t sock) {
	return close(sock);
}
