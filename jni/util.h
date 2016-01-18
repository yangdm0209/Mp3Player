#ifndef _UTIL_H__
#define _UTIL_H__

#include <android/log.h>
#define LOG_TAG "Dashu.MAD.Core"
#define LOGI(format, args...) do{__android_log_print(ANDROID_LOG_INFO, LOG_TAG, "[%s:%d] -> "format, __FUNCTION__, __LINE__, ##args);}while(0);
#define LOGD(format, args...) do{__android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, "[%s:%d] -> "format, __FUNCTION__, __LINE__, ##args);}while(0);
#define LOGW(format, args...) do{__android_log_print(ANDROID_LOG_WARN, LOG_TAG, "[%s:%d] -> "format, __FUNCTION__, __LINE__, ##args);}while(0);
#define LOGE(format, args...) do{__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "[%s:%d] -> "format, __FUNCTION__, __LINE__, ##args);}while(0);

typedef struct util_dict_tag {
	char *key;
	char *val;
	struct util_dict_tag *next;
} util_dict_t;

util_dict_t *util_dict_new(void);
void util_dict_free(util_dict_t *dict);
/* dict, key must not be NULL. */
int util_dict_set(util_dict_t *dict, const char *key, const char *val);
const char *util_dict_get(util_dict_t *dict, const char *key);

void util_dict_print(util_dict_t *dict);

#endif // _UTIL_H__
