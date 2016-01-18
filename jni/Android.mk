

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE:= libmad

LOCAL_SRC_FILES:= \
	./libmad/version.c \
	./libmad/fixed.c \
	./libmad/bit.c \
	./libmad/timer.c \
	./libmad/stream.c \
	./libmad/frame.c  \
	./libmad/synth.c \
	./libmad/decoder.c \
	./libmad/layer12.c \
	./libmad/layer3.c \
	./libmad/huffman.c \
	./util.c \
	./net.c \
	./ringbuf.c \
	./player.c

LOCAL_LDLIBS    := -lm -llog

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/libmad/android \
    $(LOCAL_PATH)/libmad

LOCAL_CFLAGS := \
    -DHAVE_CONFIG_H=1 \
    -DFPM_DEFAULT

include $(BUILD_SHARED_LIBRARY)
