#include "libmad/Mad.h"
#include "util.h"
#include <jni.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#include <android/log.h>
#include "net.h"

#ifndef SHRT_MAX
#define SHRT_MAX (32767)
#endif
#define INPUT_BUFFER_SIZE   2048

typedef struct mp3_handle_tag{
	struct mad_stream stream;
	struct mad_frame frame;
	struct mad_synth synth;
	mad_timer_t timer;
	int leftSamples;
	int offset;
	unsigned char inputBuffer[INPUT_BUFFER_SIZE];
} mp3_handle_t;

static mp3_handle_t* g_mp3Handle;
unsigned int g_Samplerate;
unsigned int g_Channels;

static http_client_t g_httpClient;
static int read_size = 0;
static char meta_data[255*16];
static char* song_title = NULL;
static int read_meta_size = 0;
static int meta_len;

static int stop_flag = 0;

static JavaVM *g_jvm = NULL;
static jobject g_feed = NULL;


static inline int readNextFrame(JNIEnv *env, mp3_handle_t* mp3);

void exception(JNIEnv *env, int code, const char* str) {
	int ret = -1;
	jclass cls;
	jmethodID mid;
	jstring jstr;
	LOGW("exception is %s, env is %s", str, env==NULL?"不存在":"正常");
	if (env == NULL)
		return;
	do {
		// find class
		cls = (*env)->GetObjectClass(env, g_feed);
		if (cls == NULL) {
			LOGE("FindClass() failed");
			break;
		}

		// get exception method to
		mid = (*env)->GetMethodID(env, cls, "exception",
				"(ILjava/lang/String;)V");
		if (mid == NULL) {
			LOGE("GetMethodID() exception failed");
			break;
		}

		jstr = (*env)->NewStringUTF(env, str);
		(*env)->CallNonvirtualVoidMethod(env, g_feed, cls, mid, code, jstr);
		(*env)->DeleteLocalRef(env, jstr);
		ret = 0;

	} while (0);
}

void title_change(JNIEnv *env, const char* str) {
	jclass cls;
	jmethodID mid;
	jstring jstr;

	do {
		// find class
		cls = (*env)->GetObjectClass(env, g_feed);
		if (cls == NULL) {
			LOGE("FindClass() failed");
			break;
		}

		// get exception method to
		mid = (*env)->GetMethodID(env, cls, "updateTitle",
				"(Ljava/lang/String;)V");
		if (mid == NULL) {
			LOGE("GetMethodID() exception failed");
			break;
		}

		jstr = (*env)->NewStringUTF(env, str);
		(*env)->CallNonvirtualVoidMethod(env, g_feed, cls, mid, jstr);
		(*env)->DeleteLocalRef(env, jstr);

	} while (0);
}

static inline void closeHandle() {
	LOGI("CloseHandle");
    if (NULL == g_mp3Handle)
        return;
    
	mad_synth_finish(&g_mp3Handle->synth);
	mad_frame_finish(&g_mp3Handle->frame);
	mad_stream_finish(&g_mp3Handle->stream);
	free(g_mp3Handle);
	g_mp3Handle = NULL;
	if (song_title){
		free(song_title);
		song_title = NULL;
	}
}

static inline signed short fixedToShort(mad_fixed_t Fixed) {
	if (Fixed >= MAD_F_ONE)
		return (SHRT_MAX);
	if (Fixed <= -MAD_F_ONE)
		return (-SHRT_MAX);

	Fixed = Fixed >> (MAD_F_FRACBITS - 15);
	return ((signed short) Fixed);
}

int NativeMP3Decoder_init() {
	mp3_handle_t* mp3Handle = (mp3_handle_t*) malloc(sizeof(mp3_handle_t));
	memset(mp3Handle, 0, sizeof(mp3_handle_t));

	mad_stream_init(&mp3Handle->stream);
	mad_frame_init(&mp3Handle->frame);
	mad_synth_init(&mp3Handle->synth);
	mad_timer_reset(&mp3Handle->timer);

	g_mp3Handle = mp3Handle;



	return 0;
}

static inline int read_full_data(JNIEnv *env, char* dest, int len) {
	int count = 0;
	while (rb_filled(&g_httpClient.recv_data) < len) {
		usleep(100000);
		count++;
		if (g_httpClient.recv_thread_sts != THREAD_RUNNING){
			if (g_httpClient.err_code == SOCK_ERROR)
				exception(env, -1, "网络错误");
			else
				exception(env, -2, "未知错误");
			return 0;
		}
		else if (count > 30){
			exception(env, -3, "网络不稳定，无法获得数据");
			return 0;
		}
		else if (stop_flag == 1){
			return 0;
		}
		LOGI("have not recv data wait %d", count);
	}
	int ret = rb_read(&g_httpClient.recv_data, dest, len);
	return ret;
}

static inline int readMp3(JNIEnv *env, char *dest, unsigned int len){
	int ret = 0;
	int read = 0;
	if (g_httpClient.metaInterval > 0){
		if (read_size + len < g_httpClient.metaInterval){
			ret = read_full_data(env, dest, len);
			read_size += ret;
			return ret;
		}
		else{
			// 读取的MP3数据中包含meta信息， 读取前半MP3部分
			int cut = g_httpClient.metaInterval - read_size;
			ret = read_full_data(env, dest, cut);
			if (ret < cut){
				LOGE("recv data error, return");
				return ret;
			}
			read_size += ret;
			read = ret;
			if (ret == cut){
				// 读取meta信息
				ret = read_full_data(env, meta_data, 1);
				if(ret != 1){
					LOGE("recv data error, return");
					return read;
				}
				read_size = 0;
				meta_len = meta_data[0]*16;
				LOGI("===================meta data len is=================== %d", meta_len);
				if (meta_len > 0){
					// 处理title
					ret = read_full_data(env, meta_data, meta_len);
					if (ret < meta_len){
						LOGE("recv data error, return");
						return read;
					}
					read_meta_size = ret;
					if (ret < meta_len)
						return read;
					// split title
					meta_data[meta_len] = '\0';
					char* titleStart = strstr(meta_data, "StreamTitle='");
					titleStart += 13;
					char* titleEnd = strstr(titleStart, "';");
					if (titleEnd-titleStart > 0){
						char* title = calloc(1, titleEnd-titleStart+1);
						if (title) {
							memcpy(title, titleStart, titleEnd-titleStart);
							LOGI("recv a meta ==%s==, title is ==%s==",meta_data, title);
							if (song_title)
								free(song_title);
							song_title = title;
							title_change(env ,song_title);
						}
					}
				}
				// 读取后半部分MP3数据
				ret = read_full_data(env, dest+read, len-read);
				read_size += ret;
				read += ret;
			}

			return read;
		}
	}else{
		LOGI("No meta in stream, decode directly");
		return rb_read(&g_httpClient.recv_data, dest, len);
	}
}

static inline int readNextFrame(JNIEnv *env, mp3_handle_t* mp3) {
	do {
		if (mp3->stream.buffer == 0 || mp3->stream.error == MAD_ERROR_BUFLEN) {
			int inputBufferSize = 0;

			if (mp3->stream.next_frame != 0) {

				int leftOver = mp3->stream.bufend - mp3->stream.next_frame;

				int i;
				for (i = 0; i < leftOver; i++)
					mp3->inputBuffer[i] = mp3->stream.next_frame[i];
				int readBytes = readMp3(env, mp3->inputBuffer + leftOver, INPUT_BUFFER_SIZE - leftOver);
				if (readBytes != INPUT_BUFFER_SIZE - leftOver){
					LOGE("readMp3 error, return");
					return -1;
				}
				inputBufferSize = leftOver + readBytes;
			} else {
				int readBytes = readMp3(env, mp3->inputBuffer, INPUT_BUFFER_SIZE);
				if (readBytes !=  INPUT_BUFFER_SIZE){
					LOGE("readMp3 error, return");
					return -1;
				}
				inputBufferSize = readBytes;
			}

			mad_stream_buffer(&mp3->stream, mp3->inputBuffer, inputBufferSize);
			mp3->stream.error = MAD_ERROR_NONE;

		}

		if (mad_frame_decode(&mp3->frame, &mp3->stream)) {
			if (mp3->stream.error == MAD_ERROR_BUFLEN
					|| (MAD_RECOVERABLE(mp3->stream.error)))
				continue;
			else
				return -1;
		} else
			break;
	} while (1 && stop_flag == 0);

	mad_timer_add(&mp3->timer, mp3->frame.header.duration);
	mad_synth_frame(&mp3->synth, &mp3->frame);
	mp3->leftSamples = mp3->synth.pcm.length;
	mp3->offset = 0;
	return 0;
}

int NativeMP3Decoder_readSamples(JNIEnv *env, short *target, int size) {

	mp3_handle_t* mp3 = g_mp3Handle;

	int pos = 0;
	int idx = 0;
	int ret = 0;
	while (idx != size && stop_flag == 0) {
		if (mp3->leftSamples > 0) {
			for (; idx < size && mp3->offset < mp3->synth.pcm.length;
					mp3->leftSamples--, mp3->offset++) {
				int value = fixedToShort(mp3->synth.pcm.samples[0][mp3->offset]);

				if (MAD_NCHANNELS(&mp3->frame.header) == 2) {
					value += fixedToShort(mp3->synth.pcm.samples[1][mp3->offset]);
					value /= 2;
				}

				target[idx++] = value;
			}
		} else {
			ret = readNextFrame(env, mp3);
			if (ret != 0)
				return 0;
		}
	}

	return mp3->timer.seconds;

}

int NativeMP3Decoder_getAduioSamplerate() {
	LOGI("audio samplerate : %d", g_Samplerate);
	if (g_Samplerate == 0 && stop_flag == 0){
		if (readNextFrame(NULL, g_mp3Handle) == 0){
			g_Samplerate = g_mp3Handle->synth.pcm.samplerate;
			g_Channels = g_mp3Handle->synth.pcm.channels;
		}
	}
	return g_Samplerate;

}

int NativeMP3Decoder_getAduioChannels() {
	LOGI("audio channels : %d", g_Channels);
	if (g_Channels == 0 && stop_flag == 0) {
		if (readNextFrame(NULL, g_mp3Handle) == 0){
			g_Samplerate = g_mp3Handle->synth.pcm.samplerate;
			g_Channels = g_mp3Handle->synth.pcm.channels;
		}
	}
	return g_Channels;

}

void NativeMP3Decoder_closeAduioPlayer() {
	http_close(&g_httpClient);

	if (g_mp3Handle != NULL) {
		closeHandle();
	}
}

/*
 * Class:     com_dashu_open_audio_core_NativeMP3Decoder
 * Method:    initAudioPlayer
 * Signature: (Ljava/lang/String;I)I
 */

JNIEXPORT jint JNICALL Java_com_dashu_open_audio_core_NativeMP3Decoder_initAudioPlayer(
		JNIEnv *env, jobject obj, jstring jurl, jobject jfeed) {

     // get java vm
	(*env)->GetJavaVM(env, &g_jvm);
     g_feed = (*env)->NewGlobalRef(env, jfeed);
		
	const char* url = (*env)->GetStringUTFChars(env, jurl, NULL);

	LOGI("initAudioPlayer is : %s", url);

	g_Samplerate = 0;
	g_Channels = 0;
	read_size = 0;
	stop_flag = 0;

	int ret = http_connect(&g_httpClient, url);
	if (0 == ret){
		ret = NativeMP3Decoder_init();
	}

	(*env)->ReleaseStringUTFChars(env, jurl, url);

	return ret;
}

/*
 * Class:     com_dashu_open_audio_core_NativeMP3Decoder
 * Method:    getAudioBuf
 * Signature: ([SI)I
 */
JNIEXPORT jint JNICALL Java_com_dashu_open_audio_core_NativeMP3Decoder_getAudioBuf(
		JNIEnv *env, jobject obj, jshortArray audioBuf, jint len) {
	int bufsize = 0;
	int ret = 0;
	if (audioBuf != NULL) {
		bufsize = (*env)->GetArrayLength(env, audioBuf);
		jshort *_buf = (*env)->GetShortArrayElements(env, audioBuf, 0);

		memset(_buf, 0, bufsize*sizeof(short));
		ret = NativeMP3Decoder_readSamples(env, _buf, len);
		(*env)->ReleaseShortArrayElements(env, audioBuf, _buf, 0);
	}
	else{
			LOGE("getAudio failed");
	}
	return ret;
}

/*
 * Class:     com_dashu_open_audio_core_NativeMP3Decoder
 * Method:    closeAduioFile
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_dashu_open_audio_core_NativeMP3Decoder_closeAduioPlayer(
		JNIEnv *env, jobject obj) {
	LOGI("close audio player");
	stop_flag = 1;
    if (NULL != g_feed) {
		(*env)->DeleteGlobalRef(env, g_feed);
		g_feed = NULL;
	}
	NativeMP3Decoder_closeAduioPlayer();
}

/*
 * Class:     com_dashu_open_audio_core_NativeMP3Decoder
 * Method:    getAudioSamplerate
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_dashu_open_audio_core_NativeMP3Decoder_getAudioSamplerate(
		JNIEnv *env, jobject obj) {
	LOGI("Get audio samplerate");
	return NativeMP3Decoder_getAduioSamplerate();
}
JNIEXPORT jint JNICALL Java_com_dashu_open_audio_core_NativeMP3Decoder_getAudioChannels(
		JNIEnv *env, jobject obj) {
	LOGI("Get audio channels");
	return NativeMP3Decoder_getAduioChannels();
}
